
import os
import json
import urllib2
import random
import time


RPC_PARSE_ERROR = -32700
RPC_INVALID_REQUEST = -32600
RPC_METHOD_NOT_FOUND = -32601
RPC_INVALID_PARAMS = -32602
RPC_INTERNAL_ERROR = -32603
# -32000 to -32099 impl-defined server errors
RPC_NO_EVENT = -32000
RPC_MESSAGE_ERROR = -32001
RPC_COMMAND_ERROR = -31999


url = os.environ.get('AGO_URL', 'http://localhost:8008')
url_jsonrpc = url + '/jsonrpc'


def bus_message(subject, content, expect_identifier=None, expect_rpc_error_code=None, timeout=None):
    """Send a Agocontrol Bus message via the RPC interface"""
    params = {
        'content': content,
        'subject': subject
    }

    if timeout is not None:
        params['replytimeout'] = timeout
        # Give HTTP request a bit more time.
        timeout = timeout + 0.5

    return jsonrpc_request('message', params, expect_identifier=expect_identifier, expect_rpc_error_code=expect_rpc_error_code, timeout=timeout)


def jsonrpc_request(method, params=None, req_id=None, expect_identifier=None, expect_rpc_error_code=None, timeout=None):
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

    http_req = urllib2.Request(url_jsonrpc, req_raw)
    if not timeout: timeout = 5
    http_rep = urllib2.urlopen(http_req, timeout=timeout)

    assert http_rep.code == 200
    assert 'application/json' == http_rep.info()['Content-Type']

    rep_raw = http_rep.read()

    rep_body = json.loads(rep_raw)

    assert '2.0' == rep_body['jsonrpc']
    assert req_id == rep_body['id']

    if expect_rpc_error_code:
        assert 'error' in rep_body
        assert 'result' not in rep_body

        err = rep_body['error']
        assert 'code' in err
        assert expect_rpc_error_code == err['code']
        if expect_identifier:
            assert expect_identifier == err['identifier']
        else:
            assert 'identifier' not in err
    else:
        assert 'error' not in rep_body
        assert 'result' in rep_body
        if expect_identifier:
            assert expect_identifier == rep_body['result']['identifier']
        else:
            assert 'identifier' not in rep_body['result']

    if not expect_rpc_error_code:
        return rep_body['result']
    else:
        return rep_body['error']


class TestRPC(object):
    def test_unknown_command(self):
        jsonrpc_request('no-real', expect_rpc_error_code=RPC_METHOD_NOT_FOUND)

    def test_bus_message(self, transport_adapter):
        def mock(msg):
            assert 'command' in msg.content
            assert 'some-command' == msg.content['command']
            assert 1234 == msg.content['int-param']
            return {'_newresponse':True, 'result':{'identifier': 'success', 'data': {'int':4321, 'string':'test'}}}

        transport_adapter.set_handler(mock)

        rep = bus_message('', {'command':'some-command', 'int-param':1234, 'UT-EXP':True}, expect_identifier='success')
        # rep is result from response
        assert 4321 == rep['data']['int']
        assert 'test' == rep['data']['string']

    def test_bus_message_err(self, transport_adapter):
        def mock(msg):
            assert 'command' in msg.content
            assert 'some-err-command' ==  msg.content['command']
            assert 12345 == msg.content['int-param']
            return {'_newresponse':True, 'error':{'identifier': 'some.error', 'message': 'err', 'data': {'int':4321, 'string':'test'}}}

        transport_adapter.set_handler(mock)

        rep = bus_message('', {'command':'some-err-command', 'int-param':12345, 'UT-EXP':True},
                          expect_identifier='some.error',
                          expect_rpc_error_code=RPC_COMMAND_ERROR)
        # rep is error from response; contains message, code and data.
        assert 'some.error' == rep['identifier']
        assert 'err' == rep['message']
        assert 4321 == rep['data']['int']
        assert 'test' == rep['data']['string']

    def test_empty_bus_message(self):
        jsonrpc_request('message', None, expect_rpc_error_code=RPC_INVALID_PARAMS)

    def test_no_reply_timeout(self):
        s = time.time()
        # Must take at least 0.5s
        ret = bus_message('', {'command':'non-existent'},
                          expect_identifier='error.no.reply',
                          expect_rpc_error_code=RPC_COMMAND_ERROR, timeout=0.5)
        e = time.time()
        assert "Timeout" == ret['message']
        assert e-s >= 0.5
        assert e-s < 1.0 # but quite near 0.5..


    def test_inventory(self):
        """Executes an inventory request, assumes agoresolver is alive"""
        rep = bus_message('', {'command':'inventory'}, expect_identifier='success')

        assert dict == type(rep)
        assert rep.get('identifier') == 'success'

        assert 'data' in rep
        data = rep['data']
        assert 'devices' in data
        assert 'schema' in data
        assert 'rooms' in data
        assert 'floorplans' in data
        assert 'system' in data
        assert 'variables' in data
        assert 'environment' in data

    def test_get_event_errors(self):
        jsonrpc_request('getevent', None, expect_rpc_error_code= RPC_INVALID_PARAMS)
        jsonrpc_request('getevent', {}, expect_rpc_error_code=RPC_INVALID_PARAMS)
        jsonrpc_request('getevent', {'uuid':'123'}, expect_rpc_error_code=RPC_INVALID_PARAMS)

    def test_subscribe(self, transport_adapter):
        sub_id = jsonrpc_request('subscribe', None)
        assert type(sub_id) in (str,unicode)
        err = jsonrpc_request('getevent', {'uuid':sub_id, 'timeout':0}, expect_rpc_error_code=RPC_NO_EVENT)
        assert err['message'] == 'No messages available'

        # Send a message
        transport_adapter.send(content={'test':1, 'notify':True}, subject='event.something.subject')

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
        # Ensure we can do subseccond precision on getevent timeouts
        sub_id = jsonrpc_request('subscribe', None)
        assert type(sub_id)in (str,unicode)
        s = time.time()
        err = jsonrpc_request('getevent', {'uuid':sub_id, 'timeout':0.5}, expect_rpc_error_code=RPC_NO_EVENT)
        e = time.time()
        assert err['message'] == 'No messages available'
        assert e-s >= 0.5
        assert e-s < 0.6
