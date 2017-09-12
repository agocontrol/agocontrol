# Execute with
#
#   python -m unittest rpc-test
#
import unittest
import os
import threading
import json
import urllib2
import random
import threading
import time

import logging

from qpid.messaging import Connection, Message, Empty, LinkClosed

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

class RPCTest(unittest.TestCase):
    longMessage = True

    def setUp(self):
        self.url = os.environ.get('AGO_URL', 'http://localhost:8008')
        self.url_jsonrpc = self.url + '/jsonrpc'

        # lazy-inited
        self._qpid_connection = None

    @property
    def qpid_connection(self):
        if not self._qpid_connection:
            broker = os.environ.get('AGO_BROKER', 'localhost')
            username = os.environ.get('AGO_USERNAME', 'agocontrol')
            password = os.environ.get('AGO_PASSWORD', 'letmein')

            self._qpid_connection = Connection(broker,
                username=username, password=password, reconnect=True)
            self._qpid_connection.open()

            self._qpid_session = self._qpid_connection.session()
            self._qpid_sender = self._qpid_session.sender(
                "agocontrol; {create: always, node: {type: topic}}")

        return self._qpid_connection

    @property
    def qpid_session(self):
        if not self._qpid_connection:
            conn = self.qpid_connection
        return self._qpid_session

    @property
    def qpid_sender(self):
        if not self._qpid_connection:
            conn = self.qpid_connection
        return self._qpid_sender

    def testUnknownCommand(self):
        self.jsonrpc_request('no-real', rpc_error_code = RPC_METHOD_NOT_FOUND)

    def testBusMessage(self):
        def mock(msg):
            self.assertIn('command', msg.content)
            self.assertEquals('some-command', msg.content['command'])
            self.assertEquals(1234, msg.content['int-param'])
            return {'_newresponse':True, 'result':{'int':4321, 'string':'test'}}

        qpr = DummyQpidResponder(self.qpid_session, mock)
        try:
            qpr.start()
            rep = self.bus_message('', {'command':'some-command', 'int-param':1234, 'UT-EXP':True})
            # rep is result from response
            self.assertEquals(4321, rep['int'])
            self.assertEquals('test', rep['string'])
        finally:
            qpr.shutdown()

    def testBusMessageErr(self):
        def mock(msg):
            self.assertIn('command', msg.content)
            self.assertEquals('some-err-command', msg.content['command'])
            self.assertEquals(12345, msg.content['int-param'])
            return {'_newresponse':True, 'error':{'int':4321, 'string':'test'}}

        qpr = DummyQpidResponder(self.qpid_session, mock)
        try:
            qpr.start()
            rep = self.bus_message('', {'command':'some-err-command', 'int-param':12345, 'UT-EXP':True}, rpc_error_code=RPC_COMMAND_ERROR)
            # rep is error from response; contains message, code and data.
            self.assertEquals('Command returned error', rep['message'])
            self.assertEquals(4321, rep['data']['int'])
            self.assertEquals('test', rep['data']['string'])
        finally:
            qpr.shutdown()

    def testUnknownBusMessage(self):
        self.jsonrpc_request('message', None, rpc_error_code = RPC_INVALID_PARAMS)
        s = time.time()
        # Must take at least 0.5s
        ret = self.bus_message('', {'command':'non-existent'}, rpc_error_code = RPC_MESSAGE_ERROR, timeout=0.5)
        e = time.time()
        self.assertEquals("error.no.reply", ret['message'])
        self.assertGreaterEqual(e-s, 0.5)
        self.assertLessEqual(e-s, 1.0) # but quite near 0.5..


    def testInventory(self):
        """Executes an inventory request, assumes agoresolver is alive"""
        rep = self.bus_message('', {'command':'inventory'})
        dbg_msg = "REP: '%s'" % rep
        self.assertEquals(dict, type(rep), dbg_msg)
        self.assertEquals(rep.get('identifier'), 'success', dbg_msg)
        self.assertTrue('data' in rep, dbg_msg)
        data = rep['data']
        self.assertTrue('devices' in data, dbg_msg)
        self.assertTrue('schema' in data, dbg_msg)
        self.assertTrue('rooms' in data, dbg_msg)
        self.assertTrue('floorplans' in data, dbg_msg)
        self.assertTrue('system' in data, dbg_msg)
        self.assertTrue('variables' in data, dbg_msg)
        self.assertTrue('environment' in data, dbg_msg)

    def testGetEventErrors(self):
        err = self.jsonrpc_request('getevent', None, rpc_error_code= RPC_INVALID_PARAMS)
        err = self.jsonrpc_request('getevent', {}, rpc_error_code=RPC_INVALID_PARAMS)
        err = self.jsonrpc_request('getevent', {'uuid':'123'}, rpc_error_code=RPC_INVALID_PARAMS)

    def testSubscribe(self):
        sub_id = self.jsonrpc_request('subscribe', None)
        self.assertIn(type(sub_id), (str,unicode))
        err = self.jsonrpc_request('getevent', {'uuid':sub_id, 'timeout':0}, rpc_error_code = RPC_NO_EVENT)
        self.assertEquals(err['message'], 'No messages available')

        # Send a message
        message = Message(content={'test':1, 'notify':True}, subject='event.something.subject')
        self.qpid_sender.send(message)

        while True:
            res = self.jsonrpc_request('getevent', {'uuid':sub_id, 'timeout':1})
            if res['event'] == 'event.device.announce':
                # Ignore announce from other devices.
                # Besides that, nothing shall talk on our test network
                continue

            self.assertIn('notify', res)
            self.assertEquals(res['test'],1)
            self.assertEquals(res['event'],'event.something.subject')
            break

        rep = self.jsonrpc_request('unsubscribe', None, rpc_error_code=RPC_INVALID_PARAMS)
        rep = self.jsonrpc_request('unsubscribe', {'uuid':None}, rpc_error_code=RPC_INVALID_PARAMS)
        rep = self.jsonrpc_request('unsubscribe', {'uuid':sub_id})
        self.assertEquals("success", rep)


    def testSubscribeTimeout(self):
        # Ensure we can do subseccond precision on getevent timeouts
        sub_id = self.jsonrpc_request('subscribe', None)
        self.assertIn(type(sub_id), (str,unicode))
        s = time.time()
        err = self.jsonrpc_request('getevent', {'uuid':sub_id, 'timeout':0.5}, rpc_error_code = RPC_NO_EVENT)
        e = time.time()
        self.assertEquals(err['message'], 'No messages available')
        self.assertGreaterEqual(e-s, 0.5)
        self.assertLessEqual(e-s, 0.6)

    def bus_message(self, subject, content, rpc_error_code=None, timeout=None):
        """Send a Agocontrol qpid Bus message via the RPC interface"""
        params = {
            'content': content,
            'subject': subject
            }

        if timeout is not None:
            params['replytimeout'] = timeout
            # Give HTTP request a bit more time.
            timeout = timeout + 0.5

        return self.jsonrpc_request('message', params, rpc_error_code = rpc_error_code, timeout=timeout)

    def jsonrpc_request(self, method, params=None, req_id=None, rpc_error_code=None, timeout=None):
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

        http_req = urllib2.Request(self.url_jsonrpc, req_raw)
        if not timeout: timeout = 5
        http_rep = urllib2.urlopen(http_req, timeout=timeout)

        self.assertEquals(200, http_rep.code, dbg_msg)
        self.assertEquals('application/json', http_rep.info()['Content-Type'], dbg_msg)

        rep_raw = http_rep.read()
        dbg_msg+= ", REP : %s" % rep_raw

        rep_body = json.loads(rep_raw)


        self.assertEquals('2.0', rep_body['jsonrpc'], dbg_msg)
        self.assertEquals(req_id, rep_body['id'], dbg_msg)

        if rpc_error_code:
            self.assertTrue('error' in rep_body, dbg_msg)
            self.assertFalse('result' in rep_body, dbg_msg)

            err = rep_body['error']
            self.assertTrue('code' in err, dbg_msg)
            self.assertEquals(rpc_error_code, err['code'], dbg_msg)
        else:
            self.assertFalse('error' in rep_body, dbg_msg)
            self.assertTrue('result' in rep_body, dbg_msg)


        if rpc_error_code == None:
            return rep_body['result']
        else:
            return rep_body['error']



class DummyQpidResponder(threading.Thread):
    """Simple class which will look for Qpid messages, and
    execute a handler fn which the test can use to generate a response.

    Only reacts to messages with key 'UT-EXP' in content!
    """
    def __init__(self, session, handler):
        super(DummyQpidResponder, self).__init__()
        self.session = session
        self.receiver = session.receiver(
            "agocontrol; {create: always, node: {type: topic}}")
        self.sender = session.sender(
                "agocontrol; {create: always, node: {type: topic}}")
        self.handler = handler

    def run(self):
        self.stop = False
        while not self.stop:
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

    def shutdown(self):
        self.stop = True
        self.session.close()
        self.join()
