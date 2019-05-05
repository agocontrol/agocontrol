import errno
import functools
import logging
import select
import time
import uuid
import qpid.log
import qpid.messaging
import qpid.util

import agoclient.agotransport

from agoclient import agoproto


class AgoQpidTransport(agoclient.agotransport.AgoTransport):
    def __init__(self, broker, username, password):
        self.log = logging.getLogger('transport')
        self.broker = broker
        self.username = username
        self.password = password
        self.connection = None
        self.session = None
        self.receiver = None
        self.sender = None

    def start(self):
        self.connection = qpid.messaging.Connection(self.broker, username=self.username, password=self.password, reconnect=True)
        try:
            self.connection.open()
            self.session = self.connection.session()
            self.receiver = self.session.receiver(
                "agocontrol; {create: always, node: {type: topic}}")
            self.sender = self.session.sender(
                "agocontrol; {create: always, node: {type: topic}}")
        except KeyboardInterrupt:
            return False

        return True

    def prepare_shutdown(self):
        if self.receiver:
            self.log.trace("Preparing Qpid shutdown")
            self.receiver.close()
            self.receiver = None

    def shutdown(self):
        self.prepare_shutdown()
        if self.session:
            self.log.trace("Shutting down QPid session")
            self.session.acknowledge()
            self.session.close()
            self.session = None

        if self.connection:
            self.log.trace("Shutting down QPid connection")
            self.connection.close()
            self.connection = None

    def is_active(self):
        return self.receiver is not None

    def send_message(self, message):
        self.log.trace("Sending message: %s", message)

        try:
            message = qpid.messaging.Message(content=message['content'], subject=message.get('subject', None))
            self.sender.send(message)
            return True
        except qpid.messaging.SendError as e:
            self.log.error("Failed to send message", e)
            return False

    def send_request(self, message, timeout):
        replyreceiver = None
        try:
            replyuuid = str(uuid.uuid4())
            content = message['content']
            replyreceiver = self.session.receiver(
                "reply-%s; {create: always, delete: always}" % replyuuid)
            qpid_message = qpid.messaging.Message(content=content)
            qpid_message.reply_to = 'reply-%s' % replyuuid

            self.log.trace("Sending message [reply-to=%s]: %s", qpid_message.reply_to, content)

            self.sender.send(qpid_message)

            replymessage = replyreceiver.fetch(timeout=3)
            self.session.acknowledge()

            response = agoproto.AgoResponse(replymessage.content)

            self.log.trace("Received response: %s", response.response)

            return response

        except qpid.messaging.Empty as e:
            self.log.warning("Timeout waiting for reply (request: %s)", content)
            return agoproto.AgoResponse(identifier="no.reply", message='Timeout')
        except qpid.messaging.ReceiverError as e:
            self.log.warning("ReceiverError waiting for reply: %s (request: %s)", e, content)
            return agoproto.AgoResponse(identifier="receiver.error", message=str(e))
        except qpid.messaging.SendError as e:
            self.log.warning("SendError when sending: %s (request: %s)", e, content)
            return agoproto.AgoResponse(identifier="send.error", message=str(e))
        except select.error as e:
            # Modern QPID guards against this itself, but older qpid does not.
            # When SIGINT is used to shutdown, this bubbles up here on older qpid.
            # See http://svn.apache.org/viewvc/qpid/trunk/qpid/python/qpid/compat.py?r1=926766&r2=1558503
            if e[0] == errno.EINTR:
                #self.log.trace("EINTR while waiting for message, ignoring")
                return agoproto.AgoResponse(identifier="receiver.error", message='interrupted')
            else:
                raise e
        finally:
            if replyreceiver:
                replyreceiver.close()

    def fetch_message(self, timeout):
        if not self.receiver:
            return None

        try:
            qpid_message = self.receiver.fetch()
            self.session.acknowledge()

            reply_function = None
            if qpid_message.reply_to:
                reply_function = functools.partial(self._sendreply, qpid_message.reply_to)

            message = {"content": qpid_message.content}
            if qpid_message.subject:
                message["subject"] = qpid_message.subject

            return agoclient.agotransport.AgoTransportMessage(message, reply_function)

        except qpid.messaging.Empty:
            return None

        except qpid.messaging.ReceiverError as e:
            self.log.error("Error while receiving message: %s", e)
            time.sleep(0.05)

        except select.error as e:
            # Modern QPID guards against this itself, but older qpid does not.
            # When SIGINT is used to shutdown, this bubbles up here on older qpid.
            # See http://svn.apache.org/viewvc/qpid/trunk/qpid/python/qpid/compat.py?r1=926766&r2=1558503
            if e[0] == errno.EINTR:
               #self.log.trace("EINTR while waiting for message, ignoring")
                time.sleep(0.05)
            else:
                raise e

        except qpid.messaging.LinkClosed:
            # connection explicitly closed
            self.log.debug("LinkClosed exception")
            return None

    def _sendreply(self, addr, content):
        """Internal used to send a reply."""
        self.log.trace("Sending reply to %s: %s", addr, content)

        replysession = None
        try:
            replysession = self.connection.session()
            replysender = replysession.sender(addr)
            response = qpid.messaging.Message(content)
            replysender.send(response)
        except qpid.messaging.SendError as exception:
            self.log.error("Failed to send reply: %s", exception)
        except qpid.messaging.AttributeError as exception:
            self.log.error("Failed to encode reply: %s", exception)
        except qpid.messaging.MessagingError as exception:
            self.log.error("Failed to send reply message: %s", exception)
        finally:
            if replysession:
                replysession.close()