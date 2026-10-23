import unittest
import threading
import httpx
from fastapi.testclient import TestClient
from dashboard.lan_proxy import create_app


class EventStream(httpx.AsyncByteStream):
    async def __aiter__(self):
        yield b'data: hello\n\n'


class LanProxyTests(unittest.TestCase):
    def test_stops_when_backend_is_replaced(self):
        stopped = threading.Event()
        calls = []
        def upstream(request):
            calls.append(request)
            return httpx.Response(200, json={'pid': 123 if len(calls) == 1 else 456})
        app = create_app(transport=httpx.MockTransport(upstream), on_disconnect=stopped.set)
        with TestClient(app):
            self.assertTrue(stopped.wait(4))

    def test_forwarding_and_origin(self):
        seen = []
        def upstream(request):
            seen.append(request)
            if request.url.path == '/api/health':
                return httpx.Response(200, json={'pid': 123})
            return httpx.Response(200, stream=EventStream(), headers={'content-type': 'text/event-stream'})
        app = create_app(transport=httpx.MockTransport(upstream))
        with TestClient(app, base_url='http://192.168.1.5:8080') as client:
            response = client.get('/api/stream?x=1')
            self.assertEqual(response.content, b'data: hello\n\n')
            self.assertEqual(str(seen[-1].url), 'http://127.0.0.1:8000/api/stream?x=1')
            response = client.post('/api/command', json={'command': 'exit'}, headers={
                'origin': 'http://192.168.1.5:8080', 'x-forwarded-host': 'evil.example'})
            self.assertEqual(response.status_code, 200)
            self.assertEqual(seen[-1].headers['origin'], 'http://127.0.0.1:8000')
            self.assertEqual(seen[-1].headers['host'], '127.0.0.1:8000')
            self.assertNotIn('x-forwarded-host', seen[-1].headers)
            count = len(seen)
            self.assertEqual(client.post('/api/command', headers={'origin': 'https://evil.example'}).status_code, 403)
            self.assertEqual(client.get('/', headers={'host': 'evil.example'}).status_code, 400)
            self.assertEqual(len(seen), count)


if __name__ == '__main__':
    unittest.main()
