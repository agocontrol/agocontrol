# -*- coding: utf-8 -*-
#try:
#    from urllib.request import Request, urlopen
#except:
#    from urllib2 import Request, urlopen

from base64 import b64encode
import json
import os
from mimetypes import MimeTypes
import requests


class PushBulletError():
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return self.value

class PushBullet():

    HOST = "https://api.pushbullet.com/v2"

    def __init__(self, api_key):
        self.api_key = api_key

    def _request(self, url, payload=None, post=True):
        auth = "%s:" % (self.api_key)
        auth = auth.encode('ascii')
        auth = b64encode(auth)
        auth = b"Basic "+auth
        headers = {}
        headers['Accept'] = 'application/json'
        headers['Content-type'] = 'application/json'
        headers['Authorization'] = auth
        headers['User-Agent'] = 'pyPushBullet'
        if post:
            if payload:
                payload = json.dumps(payload)
                resp = requests.post(url, data=payload, headers=headers)
            else:
                resp = requests.post(url, headers=headers)
        else:
            if payload:
                payload = json.dumps(payload)
                resp = requests.get(url, headers=headers)
            else:
                resp = requests.get(url, data=payload, headers=headers)
        return resp.json()

    @staticmethod
    def request_multiform(url, payload, files):
        return requests.post(url, data=payload, files=files)

    def get_devices(self):
        resp = self._request(self.HOST + "/devices", None, False)
        if 'devices' in resp:
            return resp['devices']
        return []

    def push_note(self, device, title, body):
        data = {
            'type': 'note',
            'device_iden': device,
            'title': title,
            'body': body
        }
        return self._request(self.HOST + "/pushes", data)

    def push_address(self, device, name, address):
        data = {
            'type': 'address',
            'device_iden': device,
            'name': name,
            'address': address
        }
        return self._request(self.HOST + "/pushes", data)

    def push_list(self, device, title, items):
        data = {
            'type': 'list',
            'device_iden': device,
            'title': title,
            'items': items
        }
        return self._request(self.HOST + "/pushes", data)


    def push_link(self, device, title, url):
        data = {
            'type': 'link',
            'device_iden': device,
            'title': title,
            'url': url
        }
        return self._request(self.HOST + "/pushes", data)

    def push_file(self, device, filepath):
        #get upload file authorization
        mimes = MimeTypes()
        mimetype = mimes.guess_type(filepath)[0]
        filename = os.path.basename(filepath)
        if not mimetype:
            mimetype = ''
        data = {
            'file_name' : filename,
            'file_type' : mimetype
        }
        try:
            auth_resp = self._request(self.HOST + "/upload-request", data)

            #upload file now
            resp = PushBullet.request_multiform(auth_resp['upload_url'], auth_resp['data'], {'file': open(filepath, 'rb')})
            if resp and resp.status_code == 204:
                #file uploaded successfully, push file now
                data = {
                    'type': 'file',
                    'device': device,
                    'file_name': filename,
                    'file_type': mimetype,
                    'file_url' : auth_resp['file_url']
                }
                return self._request(self.HOST + '/pushes', data)
            return None
        except Exception:
            return None

