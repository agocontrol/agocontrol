import os
import threading
import time
import pytest

import logging

try:
    from qpid.messaging import Connection, Message, Empty, LinkClosed
    HAS_QPID = True
except ImportError:
    HAS_QPID = False

@pytest.fixture(scope='session')
def qpid_details():
    assert HAS_QPID

    broker = os.environ.get('AGO_BROKER', 'localhost')
    username = os.environ.get('AGO_USERNAME', 'agocontrol')
    password = os.environ.get('AGO_PASSWORD', 'letmein')

    connection = Connection(broker,
                            username=username,
                            password=password,
                            reconnect=True)
    connection.open()

    session = connection.session()

    details = dict(
        connection=connection,
        session=session,
        sender=session.sender("agocontrol; {create: always, node: {type: topic}}")
    )

    yield details

    connection.close()

@pytest.fixture(scope='function')
def qpid_transport_adapter(qpid_details):
    adapter = QpidTransportAdapter(qpid_details['session'])
    return adapter

class QpidTransportAdapter(threading.Thread):
    """Simple class which will look for Qpid messages, and
    execute a handler fn which the test can use to generate a response.

    Only reacts to messages with key 'UT-EXP' in content!
    """
    def __init__(self, session, timeout=10):
        super(QpidTransportAdapter, self).__init__()
        self.session = session
        self.receiver = self.session.receiver("agocontrol; {create: always, node: {type: topic}}")
        self.sender = self.session.sender("agocontrol; {create: always, node: {type: topic}}")
        self.timeout = timeout

    def set_handler(self, handler):
        self.handler = handler
        self.start()

    def run(self):
        self.stop = False
        stop_after = time.time() + self.timeout
        while not self.stop and time.time() < stop_after:
            try:
                msg = self.receiver.fetch(timeout=10)
                self.session.acknowledge()
                if not 'UT-EXP' in msg.content:
                    # Ignore other spurious msgs
                    continue

                rep = self.handler(msg)
                if rep:
                    logging.info("Got msg %s, replying with %s", (msg.content, rep))
                    snd = self.session.sender(msg.reply_to)
                    snd.send(Message(rep))

            except (Empty, LinkClosed):
                continue

    def send(self, content, subject):
        message = Message(content=content, subject=subject)
        self.sender.send(message)

    def shutdown(self):
        self.stop = True
        self.receiver.close()

        if self.is_alive():
            self.join()


