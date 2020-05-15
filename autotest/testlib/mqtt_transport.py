import json
import os
import threading
import time
import traceback
from collections import namedtuple

import pytest

import logging

try:
    import paho.mqtt.client as mqtt

    HAS_MQTT = True
except ImportError:
    HAS_MQTT = False

@pytest.fixture(scope='session')
def mqttc():
    assert HAS_MQTT

    #broker = os.environ.get('AGO_BROKER', 'localhost')
    #username = os.environ.get('AGO_USERNAME', 'agocontrol')
    #password = os.environ.get('AGO_PASSWORD', 'letmein')

    mqttc = mqtt.Client(client_id='ago-autotest', clean_session=True)
    mqttc.loop_start()

    ready_cond = threading.Condition()

    def mqtt_on_connect(client, userdata, flags, rc):
        #print("MQTT connected, subscibing")
        client.subscribe('com.agocontrol/legacy')

    def mqtt_on_subscribe(client, userdata, mid, granted_qos):
        #print("MQTT subscribed")
        ready_cond.acquire()
        ready_cond.notify()
        ready_cond.release()

    mqttc.on_connect = mqtt_on_connect
    mqttc.on_subscribe = mqtt_on_subscribe
    mqttc.connect('localhost')

    ready_cond.acquire()
    ready_cond.wait(10)
    ready_cond.release()
#    print("Setup ready")

    yield mqttc

    mqttc.disconnect()
    mqttc.loop_stop()


@pytest.fixture(scope='function')
def mqtt_transport_adapter(mqttc):
    adapter = MqttTransportAdapter(mqttc)
    return adapter

MqttTransportMessage = namedtuple('MqttTransportMessage', 'content,subject')

class MqttTransportAdapter:
    def __init__(self, mqttc, timeout=10):
        self.mqttc = mqttc

    def set_handler(self, handler):
        self.handler = handler
        self.mqttc.on_message = self.on_message

    def on_message(self, client, userdata, message):
        #print("Got message on ", message.topic, ":", message.payload)
        try:
            json_payload = json.loads(message.payload)

            if 'UT-EXP' not in json_payload['content']:
                # Ignore other spurious msgs
                #print("Ignoring message")
                return

            tpm = MqttTransportMessage(json_payload['content'], json_payload.get('subject', None))
            rep = self.handler(tpm)
            if rep:
                logging.info("Got msg %s, replying with %s", (json_payload, rep))
                self.mqttc.publish(json_payload['reply-to'], json.dumps(rep))
            #else:
                #print("Handler called, but no reply")
        except:
            traceback.print_exc()

    def send(self, content, subject):
        topic = 'com.agocontrol/legacy'
        self.mqttc.publish(topic, payload=json.dumps(dict(content=content, subject=subject)))

    def shutdown(self):
        self.mqttc.on_message = None


