# Execute with
#
#   python -m unittest rpc-test
#
import os
import threading
import json
import urllib2
import random
import threading
import time
import pytest

import logging

try:
    from qpid.messaging import Connection, Message, Empty, LinkClosed
    HAS_QPID=True
except ImportError:
    HAS_QPID=False

try:
    from todo import mqtt
    HAS_MQTT = True
except ImportError:
    HAS_MQTT = False

logging.basicConfig()


RPC_PARSE_ERROR = -32700
RPC_INVALID_REQUEST = -32600
RPC_METHOD_NOT_FOUND = -32601
RPC_INVALID_PARAMS = -32602
RPC_INTERNAL_ERROR = -32603
# -32000 to -32099 impl-defined server errors
RPC_NO_EVENT = -32000
RPC_MESSAGE_ERROR = -32001
RPC_COMMAND_ERROR = -31999


#@pytest.fixture(scope='session')
#def variables():
url = os.environ.get('AGO_URL', 'http://localhost:8008')
url_jsonrpc = url + '/jsonrpc'

@pytest.fixture(scope='session')
def qpid_details():
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

@pytest.fixture(scope='session')
def qpid_session(qpid_details):
    return qpid_details['session']

@pytest.fixture(scope='session')
def qpid_sender(qpid_details):
    return qpid_details['sender']

def bus_message(subject, content, expect_rpc_error_code=None, timeout=None):
    """Send a Agocontrol qpid Bus message via the RPC interface"""
    params = {
        'content': content,
        'subject': subject
        }

    if timeout is not None:
        params['replytimeout'] = timeout
        # Give HTTP request a bit more time.
        timeout = timeout + 0.5

    return jsonrpc_request('message', params, expect_rpc_error_code = expect_rpc_error_code, timeout=timeout)

def jsonrpc_request(method, params=None, req_id=None, expect_rpc_error_code=None, timeout=None):
    """Execute a JSON-RPC 2.0 request with the specified method, and specified params
    dict. If req_id is None, a random ID number is selected.
    
    Spec: http://www.jsonrpc.org/specification
    """
    if req_id is None:
        req_id = random.randint(1, 10000)

    msg = {'jsonrpc': '2.0',
            'id': req_id, 
            'method': method
            }

    if params:
        msg['params'] = params

    req_raw = json.dumps(msg)
    dbg_msg = "REQ: %s" % req_raw

    http_req = urllib2.Request(url_jsonrpc, req_raw)
    if not timeout: timeout = 5
    http_rep = urllib2.urlopen(http_req, timeout=timeout)

    assert http_rep.code == 200, dbg_msg
    assert 'application/json' == http_rep.info()['Content-Type'], dbg_msg

    rep_raw = http_rep.read()
    dbg_msg+= ", REP : %s" % rep_raw

    rep_body = json.loads(rep_raw)

    assert '2.0' == rep_body['jsonrpc'], dbg_msg
    assert req_id == rep_body['id'], dbg_msg

    if expect_rpc_error_code:
        assert 'error' in rep_body, dbg_msg
        assert 'result' not in rep_body, dbg_msg

        err = rep_body['error']
        assert 'code' in err, dbg_msg
        assert expect_rpc_error_code == err['code'], dbg_msg
    else:
        assert 'error' not in rep_body, dbg_msg
        assert 'result' in rep_body, dbg_msg


    if expect_rpc_error_code == None:
        return rep_body['result']
    else:
        return rep_body['error']


transport_types = []
if HAS_QPID:
    transport_types.append('qpid')
if HAS_MQTT:
    transport_types.append('mqtt')

##@pytest.mark.parametrize('transport', transport_types)
class TestRPC(object):
    def test_unknown_command(self):
        jsonrpc_request('no-real', expect_rpc_error_code = RPC_METHOD_NOT_FOUND)

    def test_bus_message(self, qpid_session):
        if not HAS_QPID: return
        def mock(msg):
            assert 'command' in msg.content
            assert 'some-command' == msg.content['command']
            assert 1234 == msg.content['int-param']
            return {'_newresponse':True, 'result':{'identifier': 'ok', 'data': {'int':4321, 'string':'test'}}}

        qpr = DummyQpidResponder(qpid_session, mock)
        try:
            qpr.start()
            rep = bus_message('', {'command':'some-command', 'int-param':1234, 'UT-EXP':True})
            # rep is result from response
            assert 4321 == rep['data']['int']
            assert 'test' == rep['data']['string']
        finally:
            qpr.shutdown()

    def test_bus_message_err(self, qpid_session):
        if not HAS_QPID: return
        def mock(msg):
            assert 'command' in msg.content
            assert 'some-err-command' ==  msg.content['command']
            assert 12345 == msg.content['int-param']
            return {'_newresponse':True, 'error':{'identifier': 'some.error', 'message': 'err', 'data': {'int':4321, 'string':'test'}}}

        qpr = DummyQpidResponder(qpid_session, mock)
        try:
            qpr.start()
            rep = bus_message('', {'command':'some-err-command', 'int-param':12345, 'UT-EXP':True})
            # rep is error from response; contains message, code and data.
            assert 'some.error' == rep['identifier']
            assert 'err' == rep['message']
            assert 4321 == rep['result']['data']['int']
            assert 'test' == rep['result']['data']['string']
        finally:
            qpr.shutdown()

    def test_empty_bus_message(self):
        jsonrpc_request('message', None, expect_rpc_error_code = RPC_INVALID_PARAMS)

    def test_no_reply_timeout(self):
        s = time.time()
        # Must take at least 0.5s
        ret = bus_message('', {'command':'non-existent'}, timeout=0.5)
        e = time.time()
        assert "error.no.reply" == ret['message']
        assert e-s >= 0.5
        assert e-s < 1.0 # but quite near 0.5..


    def test_inventory(self):
        """Executes an inventory request, assumes agoresolver is alive"""
        rep = bus_message('', {'command':'inventory'})
        dbg_msg = "REP: '%s'" % rep
        assert dict == type(rep), dbg_msg
        assert rep.get('identifier') == 'success', dbg_msg

        assert 'data' in rep, dbg_msg
        data = rep['data']
        assert 'devices' in data, dbg_msg
        assert 'schema' in data, dbg_msg
        assert 'rooms' in data, dbg_msg
        assert 'floorplans' in data, dbg_msg
        assert 'system' in data, dbg_msg
        assert 'variables' in data, dbg_msg
        assert 'environment' in data, dbg_msg

    def test_get_event_errors(self):
        jsonrpc_request('getevent', None, expect_rpc_error_code= RPC_INVALID_PARAMS)
        jsonrpc_request('getevent', {}, expect_rpc_error_code=RPC_INVALID_PARAMS)
        jsonrpc_request('getevent', {'uuid':'123'}, expect_rpc_error_code=RPC_INVALID_PARAMS)

    def test_subscribe(self, qpid_sender):
        if not HAS_QPID: return
        sub_id = jsonrpc_request('subscribe', None)
        assert type(sub_id) in (str,unicode)
        err = jsonrpc_request('getevent', {'uuid':sub_id, 'timeout':0}, expect_rpc_error_code = RPC_NO_EVENT)
        assert err['message'] == 'No messages available'

        # Send a message
        message = Message(content={'test':1, 'notify':True}, subject='event.something.subject')
        qpid_sender.send(message)

        while True:
            res = jsonrpc_request('getevent', {'uuid':sub_id, 'timeout':1})
            if res['event'] == 'event.device.announce':
                # Ignore announce from other devices.
                # Besides that, nothing shall talk on our test network
                continue

            assert 'notify' in res
            assert res['test'] == 1
            assert res['event'] =='event.something.subject'
            break

        jsonrpc_request('unsubscribe', None, expect_rpc_error_code=RPC_INVALID_PARAMS)
        jsonrpc_request('unsubscribe', {'uuid':None}, expect_rpc_error_code=RPC_INVALID_PARAMS)
        rep = jsonrpc_request('unsubscribe', {'uuid':sub_id})
        assert "success" == rep


    def test_subscribe_timeout(self):
        if not HAS_QPID: return
        # Ensure we can do subseccond precision on getevent timeouts
        sub_id = jsonrpc_request('subscribe', None)
        assert type(sub_id)in (str,unicode)
        s = time.time()
        err = jsonrpc_request('getevent', {'uuid':sub_id, 'timeout':0.5}, expect_rpc_error_code = RPC_NO_EVENT)
        e = time.time()
        assert err['message'] == 'No messages available'
        assert e-s >= 0.5
        assert e-s < 0.6


class DummyQpidResponder(threading.Thread):
    """Simple class which will look for Qpid messages, and
    execute a handler fn which the test can use to generate a response.

    Only reacts to messages with key 'UT-EXP' in content!
    """
    def __init__(self, session, handler, timeout=10):
        super(DummyQpidResponder, self).__init__()
        self.session = session
        self.receiver = session.receiver(
            "agocontrol; {create: always, node: {type: topic}}")
        self.sender = session.sender(
                "agocontrol; {create: always, node: {type: topic}}")
        self.handler = handler
        self.timeout = timeout

    def run(self):
        self.stop = False
        stop_after = time.time() + self.timeout
        while not self.stop and time.time() < stop_after:
            try:
                print "fetch.."
                msg = self.receiver.fetch(timeout=10)
                print "fetchret"
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

    def shutdown(self):
        self.stop = True
        self.receiver.close()
        self.join()


