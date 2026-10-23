"""Optional LAN entry point for an already running loopback dashboard.

Run: .venv/bin/python -m dashboard.lan_proxy --port 8080
No database access; exits when the original dashboard process disappears.
"""
import argparse
import asyncio
from contextlib import asynccontextmanager, suppress
import ipaddress

import httpx
import uvicorn
from fastapi import FastAPI, Request
from starlette.responses import PlainTextResponse, StreamingResponse
from starlette.background import BackgroundTask

HOP_HEADERS = {'connection', 'keep-alive', 'proxy-authenticate', 'proxy-authorization',
               'te', 'trailer', 'transfer-encoding', 'upgrade'}


def clean_headers(headers):
    excluded = HOP_HEADERS | {x.strip().lower() for x in headers.get('connection', '').split(',')}
    items = headers.multi_items() if hasattr(headers, 'multi_items') else headers.items()
    return [(k, v) for k, v in items if k.lower() not in excluded]


def allowed_host(host):
    if host == 'localhost':
        return True
    try:
        address = ipaddress.ip_address(host)
        return address.is_private and not address.is_unspecified and not address.is_multicast
    except ValueError:
        return False


def create_app(upstream='http://127.0.0.1:8000', transport=None, on_disconnect=None):
    @asynccontextmanager
    async def lifespan(app):
        async with httpx.AsyncClient(transport=transport, timeout=httpx.Timeout(10, read=None),
                                     trust_env=False) as client:
            app.state.client = client
            response = await client.get(upstream + '/api/health')
            response.raise_for_status()
            original_pid = response.json()['pid']

            async def watch():
                failures = 0
                while True:
                    await asyncio.sleep(1)
                    try:
                        health = await client.get(upstream + '/api/health', timeout=2)
                        health.raise_for_status()
                        if health.json()['pid'] != original_pid:
                            break
                        failures = 0
                    except (httpx.HTTPError, ValueError, KeyError):
                        failures += 1
                        if failures >= 3:
                            break
                if on_disconnect:
                    on_disconnect()

            watcher = asyncio.create_task(watch())
            try:
                yield
            finally:
                watcher.cancel()
                with suppress(asyncio.CancelledError):
                    await watcher

    app = FastAPI(lifespan=lifespan, docs_url=None, redoc_url=None, openapi_url=None)

    @app.api_route('/{path:path}', methods=['GET', 'HEAD', 'POST', 'PUT', 'PATCH', 'DELETE', 'OPTIONS'])
    async def forward(request: Request, path: str):
        if not allowed_host(request.url.hostname):
            return PlainTextResponse('Host LAN non consentito', status_code=400)
        origin = request.headers.get('origin')
        if origin and origin != str(request.base_url).rstrip('/'):
            return PlainTextResponse('Origine non consentita', status_code=403)
        if request.headers.get('sec-fetch-site') == 'cross-site':
            return PlainTextResponse('Richiesta cross-site non consentita', status_code=403)
        headers = [(k, v) for k, v in clean_headers(request.headers)
                   if k.lower() not in {'host', 'origin', 'forwarded'} and not k.lower().startswith('x-forwarded-')]
        if origin:
            headers.append(('origin', upstream))
        # Fixed upstream authority: user paths can never select another server.
        url = httpx.URL(upstream).copy_with(raw_path=request.scope['raw_path'] +
                                          (b'?' + request.scope['query_string'] if request.scope['query_string'] else b''))
        client = request.app.state.client
        try:
            response = await client.send(client.build_request(request.method, url, headers=headers,
                                                              content=request.stream()), stream=True)
        except httpx.HTTPError:
            return PlainTextResponse('Dashboard terminata o non disponibile', status_code=503)
        result = StreamingResponse(response.aiter_raw(), status_code=response.status_code,
                                   background=BackgroundTask(response.aclose))
        result.raw_headers = [(k.encode('latin-1'), v.encode('latin-1'))
                              for k, v in clean_headers(response.headers)]
        return result

    return app


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8080)
    parser.add_argument('--upstream-port', type=int, default=8000)
    args = parser.parse_args()
    if not all(0 < p < 65536 for p in (args.port, args.upstream_port)) or args.port == args.upstream_port:
        parser.error('Specify distinct valid ports')
    app = create_app(f'http://127.0.0.1:{args.upstream_port}', on_disconnect=lambda: setattr(server, 'should_exit', True))
    server = uvicorn.Server(uvicorn.Config(app, host='0.0.0.0', port=args.port, proxy_headers=False,
                                          log_level='warning', timeout_graceful_shutdown=3))
    server.run()


if __name__ == '__main__':
    main()
