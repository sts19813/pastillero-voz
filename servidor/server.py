"""Receptor HTTPS detrás de un proxy TLS. Python 3.9+, sin dependencias."""
import hmac
import json
import os
import re
import sqlite3
import urllib.error
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

TOKEN = os.environ['DEVICE_TOKEN']
ACCOUNT = os.environ['TWILIO_ACCOUNT_SID']
AUTH = os.environ['TWILIO_AUTH_TOKEN']
FROM = os.environ['TWILIO_FROM']
TO = os.environ['EMERGENCY_TO']
DB = os.environ.get('DB_PATH', 'alertas.sqlite3')
MESSAGE = 'señor su madre no responde a la alarma'


def init_db():
    with sqlite3.connect(DB, timeout=10) as db:
        db.execute('CREATE TABLE IF NOT EXISTS events (id TEXT PRIMARY KEY, state TEXT NOT NULL)')


def send_sms():
    url = 'https://api.twilio.com/2010-04-01/Accounts/' + urllib.parse.quote(ACCOUNT, safe='') + '/Messages.json'
    body = urllib.parse.urlencode({'To': TO, 'From': FROM, 'Body': MESSAGE}).encode()
    import base64
    authorization = base64.b64encode((ACCOUNT + ':' + AUTH).encode()).decode()
    req = urllib.request.Request(url, data=body, headers={'Authorization': 'Basic ' + authorization})
    with urllib.request.urlopen(req, timeout=10) as result:
        return result.status


class Handler(BaseHTTPRequestHandler):
    def reply(self, code, message):
        data = json.dumps({'status': message}).encode()
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        if self.path != '/alert':
            return self.reply(404, 'not_found')
        received = self.headers.get('Authorization', '')
        if not hmac.compare_digest(received.encode(), ('Bearer ' + TOKEN).encode()):
            return self.reply(401, 'unauthorized')
        try:
            length = int(self.headers.get('Content-Length', '0'))
            if not 0 < length <= 512:
                raise ValueError('length')
            payload = json.loads(self.rfile.read(length))
            event = payload['event_id']
            if not isinstance(event, str) or not re.fullmatch(r'\d{8}-\d{4}', event):
                raise ValueError('event_id')
        except (ValueError, KeyError, TypeError):
            return self.reply(400, 'invalid_request')
        with sqlite3.connect(DB, timeout=10) as db:
            try:
                db.execute('INSERT INTO events VALUES (?, ?)', (event, 'unknown'))
            except sqlite3.IntegrityError:
                state = db.execute('SELECT state FROM events WHERE id = ?', (event,)).fetchone()[0]
                return self.reply(200 if state == 'accepted' else 202, state)
        # Guardar antes de llamar al proveedor evita duplicados tras cortes ambiguos.
        try:
            send_sms()
        except Exception as exc:
            print('SMS no confirmado:', type(exc).__name__, flush=True)
            return self.reply(202, 'unknown')
        with sqlite3.connect(DB, timeout=10) as db:
            db.execute('UPDATE events SET state = ? WHERE id = ?', ('accepted', event))
        return self.reply(200, 'accepted')


if __name__ == '__main__':
    if not TOKEN or TOKEN == 'TOKEN_LARGO_ALEATORIO':
        raise SystemExit('Configure DEVICE_TOKEN con un valor aleatorio.')
    init_db()
    ThreadingHTTPServer(('127.0.0.1', int(os.environ.get('PORT', '8080'))), Handler).serve_forever()
