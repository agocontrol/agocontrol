import functools
import json
import logging
import re
import sys
import threading
import uuid
import paho.mqtt.client as mqtt

import agoclient.agotransport

from agoclient import agoproto

TOPIC_BASE = 'com.agocontrol'
PUBLISH_TOPIC = TOPIC_BASE + '/legacy'


class AgoMqttTransport(agoclient.agotransport.AgoTransport):
    def __init__(self, client_id, broker, username, password):
        self.log = logging.getLogger('transport')
        self.client_id = client_id
        self.broker = broker
        self.username = username
        self.password = password
        self.connection_uuid = str(uuid.uuid4())
        self.reply_topic_base = TOPIC_BASE + "/" + self.connection_uuid + "/"
        self.reply_seq = 1
        self.mqtt = None  # type: mqtt.Client
        self._shutting_down = False

        self.lock = threading.Lock()

        self.ready_event = threading.Event()

        self.queue = []
        self.queue_condition = threading.Condition(self.lock)

        self.pending_replies = {}

    def start(self):
        self.mqtt = mqtt.Client(self.broker, clean_session=True)
        if self.username and self.password:
            self.mqtt.username_pw_set(self.username, self.password)

        self.mqtt.on_connect = self._on_connect
        self.mqtt.on_subscribe = self._on_subscribe
        self.mqtt.on_message = self._on_message
        self.mqtt.enable_logger(logging.getLogger('mqtt'))  # Ues custom logger for lowlevel mqtt

        host = self.broker
        port = 1883
        m = re.match('^([^:]+)(?::?(\d+))?$', self.broker)
        if m:
            host = m.group(1)
            if m.group(2):
                port = int(m.group(2))

        try:
            self.log.info("Connecting to MQTT broker %s:%d", host, port)
            self.mqtt.connect_async(self.broker, port)
        except ValueError as e:
            self.log.error("Invalid MQTT broker configuration (%s:%d): %s", host, port, e)
            return False

        self.mqtt.loop_start()
        # Block until connected
        while not self._shutting_down:
            try:
                self.log.trace("Waiting for MQTT connection readiness")
                if self.ready_event.wait(5):
                    self.log.trace("MQTT ready")
                    return True

            except KeyboardInterrupt:
                break

        return False

    def _on_connect(self, client, userdata, flags, rc):
        self.pending_subscribes = 2
        self.ready_event.clear()
        self.log.debug("Connected to MQTT broker, subscribing")
        self.mqtt.subscribe(PUBLISH_TOPIC)
        self.mqtt.subscribe(self.reply_topic_base + '+')

    def _on_subscribe(self, client, userdata, mid, granted_qos):
        self.pending_subscribes -= 1
        if self.pending_subscribes > 0:
            return

        self.log.debug("All MQTT topics subscribed, ready")
        self.ready_event.set()

    def _on_message(self, client, userdata, message):
        try:
            if not message.topic.startswith(TOPIC_BASE):
                self.log.error("Got a message on topic %s, which should not be subscribed to", self.topic)
                return

            with self.lock:
                if message.topic in self.pending_replies:
                    pending = self.pending_replies[message.topic]
                    c = pending['condition']  # type: threading.Condition
                    pending['reply'] = json.loads(message.payload.decode('utf-8'))
                    c.notify()
                    return
                elif message.topic.startswith(self.reply_topic_base):
                    self.log.warning("Received too late response for %s", message.topic)
                    return

                elif message.topic != PUBLISH_TOPIC:
                    # Should not see anything not subscribed too
                    self.log.warning("on_message for unknown topic %s", message.topic)
                    return

                self.queue.append(json.loads(message.payload.decode('utf-8')))
                self.queue_condition.notify()

        except:
            self.log.exception("Unhandled exception in MQTT on_message")
            sys.exit(1)

    def prepare_shutdown(self):
        self._shutting_down = True
        if self.mqtt:
            self.log.debug("Preparing MQTT shutdown")
            self.mqtt.disconnect()

        # wakeup thread
        with self.lock:
            for pending in list(self.pending_replies.values()):
                pending['condition'].notify()
            self.queue_condition.notify()

    def shutdown(self):
        self.prepare_shutdown()
        if self.mqtt:
            self.log.debug("Stopping MQTT loop")
            self.mqtt.loop_stop()
            self.log.trace("MQTT loop stopped")
            self.mqtt = None

    def is_active(self):
        return self.mqtt is not None and not self._shutting_down

    def send_message(self, message, wait=False):
        self.log.trace("Sending message: %s", message)
        self._send_message(PUBLISH_TOPIC, message, wait)

    def _send_message(self, topic, message, wait=False):
        msginfo = self.mqtt.publish(topic, json.dumps(message))
        rc = msginfo.rc
        if rc == mqtt.MQTT_ERR_SUCCESS:
            if wait:
                msginfo.wait_for_publish()
            return True
        elif rc == mqtt.MQTT_ERR_QUEUE_SIZE:
            self.log.warning("Failed to send message, MQTT queue full")
        elif rc == mqtt.MQTT_ERR_NO_CONN:
            self.log.warning("Failed to send message, MQTT not connected")
            return False

    def send_request(self, message, timeout):
        if self._shutting_down:
            return agoproto.AgoResponse(identifier='no.reply', message='Client shutting down')

        reply_seq = self.reply_seq
        self.reply_seq += 1
        reply_topic = self.reply_topic_base + str(reply_seq)
        message['reply-to'] = reply_topic

        pending = dict(condition=threading.Condition(self.lock), reply=None)

        self.pending_replies[reply_topic] = pending
        try:
            self.send_message(message, wait=True)

            with self.lock:
                pending['condition'].wait(timeout)
                if not pending['reply']:
                    self.log.warning('Timeout waiting for reply to %s', message)
                    return agoproto.AgoResponse(identifier='no.reply', message='Timeout')

                reply = pending['reply']
        finally:
            del self.pending_replies[reply_topic]

        response = agoproto.AgoResponse(reply)

        if self.log.isEnabledFor(logging.TRACE):
            self.log.trace("Received response: %s", response.response)

        return response

    def fetch_message(self, timeout):
        try:
            if self._shutting_down:
                return None

            with self.lock:
                if not self.queue:
                    self.queue_condition.wait(timeout)
                    if not self.queue:
                        return None

                message = self.queue.pop()

            reply_function = None
            if 'reply-to' in message:
                reply_function = functools.partial(self._sendreply, message['reply-to'])
                del message['reply-to']

            return agoclient.agotransport.AgoTransportMessage(message, reply_function)
        except:
            raise

    def _sendreply(self, reply_to, content):
        """Internal used to send a reply."""
        self.log.trace("Sending reply to %s: %s", reply_to, content)
        self._send_message(reply_to, content)