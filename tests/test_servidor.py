import importlib.util
import json
import os
import pathlib
import tempfile
import threading
import unittest
import urllib.error
import urllib.request

os.environ.update(DEVICE_TOKEN='prueba', TWILIO_ACCOUNT_SID='ACtest', TWILIO_AUTH_TOKEN='test', TWILIO_FROM='+10000000000', EMERGENCY_TO='+10000000001')
spec = importlib.util.spec_from_file_location('servidor', pathlib.Path(__file__).parents[1] / 'servidor' / 'server.py')
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)

class ServerTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        server.DB = os.path.join(self.temp.name, 'events.sqlite3')
        server.init_db()
        self.http = server.ThreadingHTTPServer(('127.0.0.1', 0), server.Handler)
        self.thread = threading.Thread(target=self.http.serve_forever, daemon=True)
        self.thread.start()
        self.calls = []
        server.send_sms = lambda: self.calls.append('sms')

    def tearDown(self):
        self.http.shutdown(); self.http.server_close(); self.thread.join(); self.temp.cleanup()

    def request(self, event, token='prueba'):
        req = urllib.request.Request('http://127.0.0.1:%d/alert' % self.http.server_port,
            data=json.dumps({'event_id': event}).encode(), headers={'Authorization': 'Bearer ' + token, 'Content-Type': 'application/json'})
        try:
            with urllib.request.urlopen(req) as res:
                return res.status, json.load(res)['status']
        except urllib.error.HTTPError as exc:
            with exc:
                return exc.code, json.load(exc)['status']

    def test_auth_validation_and_deduplication(self):
        self.assertEqual(self.request('20261120-0800', 'incorrecto')[0], 401)
        self.assertEqual(self.request('incorrecto')[0], 400)
        self.assertEqual(self.request('20261120-0800'), (200, 'accepted'))
        self.assertEqual(self.request('20261120-0800'), (200, 'accepted'))
        self.assertEqual(len(self.calls), 1)

    def test_uncertain_is_not_retried(self):
        def fail():
            self.calls.append('sms')
            raise TimeoutError()
        server.send_sms = fail
        self.assertEqual(self.request('20261120-0801'), (202, 'unknown'))
        self.assertEqual(self.request('20261120-0801'), (202, 'unknown'))
        self.assertEqual(len(self.calls), 1)

if __name__ == '__main__':
    unittest.main()
