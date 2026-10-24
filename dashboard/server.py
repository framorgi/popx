"""Single-writer SQLite service. UDP acknowledgements follow committed transactions."""
import argparse
import asyncio
from collections import OrderedDict
from contextlib import asynccontextmanager
import hashlib
import json
import os
from pathlib import Path
import re
import sqlite3
import subprocess
import tempfile
import time
import uuid

from fastapi import FastAPI, HTTPException, Request
from .provenance import collect as provenance
from .metrics import catalog as metric_catalog
from .evolution import EvolutionService, install_routes as install_evolution_routes
from .checkpoints import Checkpoints, install_routes as install_checkpoint_routes
from .storage import RemovalService, StorageGuard, install_routes as install_storage_routes
from .genome_codec import pack_event, unpack_event
from fastapi.responses import StreamingResponse, FileResponse
from fastapi.staticfiles import StaticFiles
from starlette.middleware.trustedhost import TrustedHostMiddleware

ROOT = Path(__file__).resolve().parent.parent


def compact(value):
    return json.dumps(value, separators=(',', ':'), allow_nan=False)


class Store:
    def __init__(self, path, binary=None):
        self.binary = binary
        self.path = Path(path).resolve()
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        self.db = sqlite3.connect(path)
        self.db.execute('PRAGMA journal_mode=WAL')
        self.db.executescript('''
            CREATE TABLE IF NOT EXISTS runs(id TEXT PRIMARY KEY, created REAL, metadata TEXT, state TEXT, reason TEXT, summary TEXT);
            CREATE TABLE IF NOT EXISTS messages(process TEXT, sequence INTEGER, PRIMARY KEY(process,sequence));
            CREATE TABLE IF NOT EXISTS status_sequence(process TEXT PRIMARY KEY, sequence INTEGER);
            CREATE TABLE IF NOT EXISTS samples(run TEXT, tick INTEGER, data TEXT, PRIMARY KEY(run,tick));
            CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY, run TEXT, tick INTEGER, type TEXT, pop TEXT, data TEXT);
            CREATE INDEX IF NOT EXISTS events_run ON events(run,id);
            CREATE TABLE IF NOT EXISTS environments(run TEXT PRIMARY KEY, data TEXT);
            CREATE TABLE IF NOT EXISTS environment_parts(run TEXT, offset INTEGER, total INTEGER, data TEXT, PRIMARY KEY(run,offset));
            CREATE TABLE IF NOT EXISTS diagnostics(id INTEGER PRIMARY KEY, run TEXT, data TEXT);
            CREATE TABLE IF NOT EXISTS snapshots(id TEXT PRIMARY KEY, run TEXT, pop TEXT, tick INTEGER, path TEXT);
            CREATE TABLE IF NOT EXISTS gaps(id INTEGER PRIMARY KEY, run TEXT, tick INTEGER, dropped INTEGER, recorded REAL);
            CREATE TABLE IF NOT EXISTS removed_runs(run TEXT PRIMARY KEY, requested REAL, state TEXT,
                rows_deleted INTEGER, files_deleted INTEGER, files_retained INTEGER, error TEXT);
            PRAGMA user_version=1;
        ''')
        self.db.commit()

    def ingest(self, process, sequence, msg):
        db = self.db
        if db.execute('SELECT 1 FROM removed_runs WHERE run=?', (msg['run_id'],)).fetchone():
            return False
        with db:
            if not db.execute('INSERT OR IGNORE INTO messages VALUES(?,?)', (process, sequence)).rowcount:
                return False
            run, tick, kind, data = msg['run_id'], msg.get('tick', 0), msg['type'], msg['data']
            db.execute("INSERT OR IGNORE INTO runs VALUES(?,?,?,'unknown','',NULL)", (run, time.time(), '{}'))
            if kind == 'session':
                data = {**data, 'provenance': provenance(self.binary)}
                db.execute('UPDATE runs SET metadata=? WHERE id=?', (compact(data), run))
            elif kind == 'sample':
                db.execute('INSERT OR REPLACE INTO samples VALUES(?,?,?)', (run, tick, compact(data)))
            elif kind == 'event':
                db.execute('INSERT INTO events(run,tick,type,pop,data) VALUES(?,?,?,?,?)',
                           (run, tick, data['event_type'], data['agent'].get('pop_id'),
                            compact(pack_event(data['agent']))))
            elif kind == 'environment':
                if isinstance(data, dict) and 'cells' in data:
                    db.execute('INSERT OR REPLACE INTO environment_parts VALUES(?,?,?,?)', (run, data['offset'], data['total'], compact(data['cells'])))
                    db.execute('INSERT OR REPLACE INTO environments VALUES(?,?)', (run, compact({'format':'chunks','cells':data['total']})))
                else:
                    db.execute('INSERT OR REPLACE INTO environments VALUES(?,?)', (run, compact(data)))
            elif kind == 'diagnostic':
                db.execute('INSERT INTO diagnostics(run,data) VALUES(?,?)', (run, compact(data)))
            elif kind == 'end':
                db.execute('INSERT INTO status_sequence VALUES(?,?) ON CONFLICT(process) DO UPDATE SET sequence=max(sequence,excluded.sequence)',(process,sequence))
                db.execute("UPDATE runs SET state='finished',reason=?,summary=? WHERE id=?", (data['reason'], compact(data['summary']), run))
            elif kind == 'status':
                previous_sequence = db.execute('SELECT sequence FROM status_sequence WHERE process=?', (process,)).fetchone()
                if previous_sequence and sequence <= previous_sequence[0]:
                    return True
                db.execute('INSERT OR REPLACE INTO status_sequence VALUES(?,?)', (process,sequence))
                previous_metadata = json.loads(db.execute('SELECT metadata FROM runs WHERE id=?',(run,)).fetchone()[0])
                data = {**data, 'metadata':{**previous_metadata, **data.get('metadata',{})}}
                if data['metadata'] != previous_metadata:
                    db.execute('UPDATE runs SET metadata=? WHERE id=?',(compact(data['metadata']),run))
                db.execute('UPDATE runs SET state=?,reason=? WHERE id=?', (data['state'],data.get('reason',''),run))
                dropped = data.get('transport', {}).get('dropped_messages', 0)
                gap=data.get('transport',{}).get('last_gap') or {}
                gap_run=gap.get('run_id',run)
                previous = db.execute('SELECT coalesce(max(dropped),0) FROM gaps WHERE run=?', (gap_run,)).fetchone()[0]
                if dropped > previous:
                    db.execute('INSERT INTO gaps(run,tick,dropped,recorded) VALUES(?,?,?,?)', (gap_run, gap.get('through_tick',tick), dropped, time.time()))
                    db.execute('INSERT INTO events(run,tick,type,pop,data) VALUES(?,?,?,?,?)',(gap_run,tick,'telemetry_gap',None,compact(gap)))
        return True

    def close(self):
        self.db.close()


class ExtinctionIndex:
    """Exact recovery ticks, cached by run and advanced from the last event id."""

    def __init__(self, path):
        self.path = Path(path).resolve()
        self.cache = {}
        self.locks = {}

    async def ticks(self, run, start, end):
        lock = self.locks.setdefault(run, asyncio.Lock())
        async with lock:
            previous_id, known = self.cache.get(run, (0, []))

            def read_increment():
                with sqlite3.connect(f'file:{self.path}?mode=ro', uri=True) as db:
                    latest = db.execute('SELECT coalesce(max(id),0) FROM events WHERE run=?', (run,)).fetchone()[0]
                    additions = []
                    if latest > previous_id:
                        additions = [row[0] for row in db.execute(
                            "SELECT DISTINCT tick FROM events WHERE run=? AND id>? AND id<=? AND type='birth' "
                            "AND json_extract(data,'$.recovery_seed')=1 ORDER BY tick",
                            (run, previous_id, latest))]
                    terminal = db.execute('SELECT reason FROM runs WHERE id=?', (run,)).fetchone()
                    final_tick = None
                    if terminal and terminal[0] == 'extinction':
                        row = db.execute('SELECT max(tick) FROM samples WHERE run=?', (run,)).fetchone()
                        final_tick = row[0]
                    return latest, additions, final_tick

            latest, additions, final_tick = await asyncio.to_thread(read_increment)
            ticks = sorted(set(known).union(additions))
            if final_tick is not None:
                ticks = sorted(set(ticks).union((final_tick,)))
            self.cache[run] = (latest, ticks)
            return [tick for tick in ticks if start <= tick <= end]


class Receiver(asyncio.DatagramProtocol):
    def __init__(self, store):
        self.store = store
        self.parts = OrderedDict()
        self.completed = OrderedDict()
        self.partial_bytes = 0
        self.transport = None
        self.peer = None
        self.process = None
        self.run = None
        self.status = {}
        self.last_seen = 0
        self.pending = {}
        self.subscribers = set()
        self.errors = 0
        self.latest_sequence = {}

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, packet, address):
        if address[0] != '127.0.0.1':
            return
        try:
            obj = json.loads(packet)
            p, seq, i, n, chunk = obj['p'], obj['s'], obj['i'], obj['n'], obj['d']
            if obj['v'] != 1 or not isinstance(p, str) or len(p) > 128 or not isinstance(seq, int) or not 0 < seq < 2**63:
                return
            if not isinstance(n, int) or not isinstance(i, int) or not 0 <= i < n <= 32768 or not isinstance(chunk, str) or len(chunk) > 800:
                return
            now = time.monotonic()
            for key, entry in list(self.parts.items()):
                if now - entry['created'] > 15:
                    self.partial_bytes -= entry['bytes']
                    del self.parts[key]
            key = (p, seq)
            if key in self.completed or (key not in self.parts and self.store.db.execute('SELECT 1 FROM messages WHERE process=? AND sequence=?',key).fetchone()):
                self.completed[key] = True
                if len(self.completed)>4096: self.completed.popitem(last=False)
                if i == 0 or i == n-1: self.transport.sendto(compact({'ack':seq,'p':p}).encode(),address)
                return
            if key not in self.parts:
                while self.parts and (len(self.parts) >= 128 or self.partial_bytes > 32*1024*1024):
                    _, old = self.parts.popitem(last=False)
                    self.partial_bytes -= old['bytes']
                self.parts[key] = {'created': now, 'n': n, 'chunks': {}, 'bytes': 0}
            entry = self.parts[key]
            if n != entry['n']:
                return
            if i not in entry['chunks']:
                while self.parts and self.partial_bytes+len(chunk)>32*1024*1024:
                    evicted, old = self.parts.popitem(last=False)
                    self.partial_bytes -= old['bytes']
                    if evicted == key: return
                entry['chunks'][i] = chunk
                entry['bytes'] += len(chunk)
                self.partial_bytes += len(chunk)
            if len(entry['chunks']) != n:
                return
            body = ''.join(entry['chunks'][j] for j in range(n))
            self.partial_bytes -= entry['bytes']
            del self.parts[key]
            msg = json.loads(body)
            if not isinstance(msg, dict):
                return
            if msg.get('version') != 1 or msg.get('process_id') != p or not isinstance(msg.get('run_id'), str):
                return
            if len(msg['run_id']) > 128 or msg.get('type') not in ('session','sample','event','environment','diagnostic','end','status','reply'):
                return
            if not isinstance(msg.get('tick',0),int) or not 0 <= msg.get('tick',0) < 2**63:
                return
            if msg['type'] != 'environment' and not isinstance(msg.get('data'), dict):
                return
            if msg['type'] == 'event' and (not isinstance(msg['data'].get('agent'), dict) or not isinstance(msg['data'].get('event_type'), str)):
                return
            fresh = self.store.ingest(p, seq, msg)
            self.completed[key] = True
            if len(self.completed)>4096: self.completed.popitem(last=False)
            self.transport.sendto(compact({'ack': seq, 'p': p}).encode(), address)
            if fresh:
                self.deliver(msg, address, seq)
        except (ValueError, KeyError, TypeError, sqlite3.Error, OSError):
            self.errors += 1

    def deliver(self, msg, address, sequence):
        kind, data = msg['type'], msg['data']
        if kind == 'status':
            process = msg['process_id']
            if sequence > self.latest_sequence.get(process, 0):
                self.latest_sequence[process] = sequence
                self.peer, self.process, self.run = address, process, msg['run_id']
                self.status = data
                self.last_seen = time.monotonic()
        elif kind == 'reply':
            future = self.pending.get(data.get('request_id'))
            if future and not future.done():
                future.set_result(data)
        if kind in ('sample', 'status', 'end'):
            for queue in tuple(self.subscribers):
                if queue.full():
                    queue.get_nowait()
                queue.put_nowait(msg)

    async def command(self, name, arguments, run=None):
        if not self.peer or time.monotonic()-self.last_seen > 5:
            raise HTTPException(503, 'Simulatore non connesso')
        request_id = uuid.uuid4().hex
        packet = compact({'version': 1, 'type': 'command', 'process_id': self.process,
                          'run_id': run or self.run, 'request_id': request_id,
                          'command': name, 'arguments': arguments, 'expires_ms': int(time.time()*1000)+(60000 if name.startswith('checkpoint_') else 5000)}).encode()
        if len(packet) > 4096:
            raise HTTPException(400, 'Comando troppo grande')
        future = asyncio.get_running_loop().create_future()
        self.pending[request_id] = future
        try:
            for _ in range(100 if name.startswith('checkpoint_') else 8):
                self.transport.sendto(packet, self.peer)
                try:
                    reply = await asyncio.wait_for(asyncio.shield(future), .6)
                    if not reply['ok']:
                        raise HTTPException(409, reply.get('error', 'Comando rifiutato'))
                    return reply
                except asyncio.TimeoutError:
                    pass
            raise HTTPException(504, 'Conferma non ricevuta; controllare lo stato prima di riprovare')
        finally:
            self.pending.pop(request_id, None)


def create_app(db_path, config_path, binary, udp_port):
    config_path, binary = Path(config_path).resolve(), Path(binary).resolve()
    evolution = EvolutionService(db_path)

    @asynccontextmanager
    async def lifespan(app):
        store = Store(db_path, binary)
        receiver = Receiver(store)
        transport, _ = await asyncio.get_running_loop().create_datagram_endpoint(lambda: receiver, local_addr=('127.0.0.1', udp_port))
        app.state.store, app.state.receiver = store, receiver
        app.state.extinctions = ExtinctionIndex(store.path)
        app.state.removals = RemovalService(store, evolution)
        for (run,) in store.db.execute('SELECT run FROM removed_runs'):
            evolution.retire(run)
        app.state.removals.resume()
        app.state.storage_guard = StorageGuard(store, receiver, config_path)
        app.state.checkpoints = Checkpoints(app, evolution)
        app.state.storage_guard.checkpoints = app.state.checkpoints
        app.state.storage_guard.start()
        try:
            yield
        finally:
            await app.state.storage_guard.close()
            app.state.checkpoints.close()
            transport.close()
            await app.state.removals.close()
            store.close()
            evolution.close()

    app = FastAPI(lifespan=lifespan)
    install_checkpoint_routes(app)
    install_evolution_routes(app, evolution)
    install_storage_routes(app)
    app.add_middleware(TrustedHostMiddleware, allowed_hosts=['localhost', '127.0.0.1', 'testserver'])

    @app.middleware('http')
    async def local_origin(request, call_next):
        origin = request.headers.get('origin')
        if request.method != 'GET' and origin and origin != str(request.base_url).rstrip('/'):
            from fastapi.responses import JSONResponse
            return JSONResponse({'detail': 'Origine non consentita'}, 403)
        return await call_next(request)

    def invoke(*args):
        try:
            result = subprocess.run([str(binary), *map(str, args)], capture_output=True, text=True, timeout=10)
            if result.returncode:
                raise HTTPException(422, result.stderr.strip())
            return json.loads(result.stdout)
        except (OSError, subprocess.TimeoutExpired) as error:
            raise HTTPException(503, str(error)) from error

    @app.get('/api/health')
    async def health():
        return {'ok': True, 'pid': os.getpid()}

    @app.get('/api/metrics')
    async def metrics():
        return metric_catalog()

    @app.get('/api/status')
    async def status():
        receiver = app.state.receiver
        return {**receiver.status, 'run_id': receiver.run,
                'storage_guard': app.state.storage_guard.state,
                'connected': bool(receiver.peer) and time.monotonic()-receiver.last_seen < 5,
                'receiver_errors': receiver.errors}

    @app.post('/api/commands')
    async def command(request: Request):
        data = await request.json()
        name = data.get('command')
        if app.state.checkpoints.lock.locked() and name not in ('agents', 'inspect', 'snapshot', 'exit'):
            raise HTTPException(409, 'Rotazione checkpoint in corso: attendere')
        if name not in ('start', 'restart', 'pause', 'resume', 'stop', 'newgen', 'speed', 'headless', 'show_gui', 'exit', 'agents', 'inspect', 'snapshot'):
            raise HTTPException(400, 'Comando sconosciuto')
        if name in ('start', 'restart', 'resume'):
            app.state.storage_guard.ensure_capacity(configured=name in ('start', 'restart'))
        reply = await app.state.receiver.command(name, data.get('arguments', {}), data.get('run_id'))
        if name == 'snapshot':
            result = reply['result']
            row = app.state.store.db.execute('SELECT metadata FROM runs WHERE id=?', (result['run_id'],)).fetchone()
            metadata = json.loads(row[0])
            root = Path(metadata['config']['nnets_dir'])
            for key in ('run_id', 'pop_id', 'snapshot_id'):
                if not re.fullmatch(r'[a-zA-Z0-9_-]+', str(result[key])):
                    raise HTTPException(400, 'Identificativo snapshot invalido')
            path = root/result['run_id']/result['pop_id']/f"{result['tick']}_{result['snapshot_id']}.json"
            path.parent.mkdir(parents=True, exist_ok=True)
            temporary = path.with_suffix('.tmp')
            temporary.write_text(compact(result))
            os.replace(temporary, path)
            with app.state.store.db:
                app.state.store.db.execute('INSERT INTO snapshots VALUES(?,?,?,?,?)',
                    (result['snapshot_id'], result['run_id'], result['pop_id'], result['tick'], str(path)))
        return reply

    @app.get('/api/runs')
    async def runs(offset: int = 0, limit: int = 50):
        rows = app.state.store.db.execute('SELECT id,created,metadata,state,reason FROM runs WHERE id NOT IN (SELECT run FROM removed_runs) ORDER BY created DESC LIMIT ? OFFSET ?',
                                         (max(1, min(limit, 100)), max(0, offset))).fetchall()
        return [dict(id=r[0], created=r[1], metadata={k:v for k,v in json.loads(r[2]).items() if k not in ('provenance','ini')}, state=r[3], reason=r[4]) for r in rows]

    @app.get('/api/runs/{run}')
    async def run_metadata(run: str):
        if app.state.store.db.execute('SELECT 1 FROM removed_runs WHERE run=?', (run,)).fetchone():
            raise HTTPException(410, 'Run rimossa o in rimozione')
        row = app.state.store.db.execute('SELECT metadata,state,reason,summary FROM runs WHERE id=?', (run,)).fetchone()
        if not row:
            raise HTTPException(404, 'Run non disponibile')
        return {'metadata':json.loads(row[0]), 'state':row[1], 'reason':row[2], 'summary':json.loads(row[3]) if row[3] else None}

    @app.get('/api/runs/{run}/samples')
    async def samples(run: str, start: int = 0, end: int = 9223372036854775807, limit: int = 1000):
        ensure_run_available(run)
        limit = max(2, min(limit, 2000))
        db = app.state.store.db
        lo, hi, count = db.execute('SELECT min(tick),max(tick),count(*) FROM samples WHERE run=? AND tick BETWEEN ? AND ?', (run, start, end)).fetchone()
        if not count:
            return []
        # Uniform buckets bound database results and browser memory, retaining the final sample.
        width = max(1, (hi-lo)//(limit-1)+1)
        rows = db.execute('SELECT data FROM samples WHERE run=? AND tick IN (SELECT max(tick) FROM samples WHERE run=? AND tick BETWEEN ? AND ? GROUP BY (tick-?)/?) ORDER BY tick',
                          (run, run, start, end, lo, width)).fetchall()
        return [json.loads(r[0]) for r in rows]

    @app.get('/api/runs/{run}/events')
    async def events(run: str, after: int = 0, limit: int = 100):
        ensure_run_available(run)
        rows = app.state.store.db.execute('SELECT id,tick,type,data FROM events WHERE run=? AND id>? ORDER BY id LIMIT ?', (run, after, max(1,min(limit,200)))).fetchall()
        return [dict(id=r[0],tick=r[1],type=r[2],data=unpack_event(json.loads(r[3]))) for r in rows]

    @app.get('/api/runs/{run}/extinctions')
    async def extinctions(run: str, start: int = 0, end: int = 9223372036854775807):
        ensure_run_available(run)
        if start < 0 or end < start or end >= 2**63:
            raise HTTPException(400, 'Intervallo tick non valido')
        return {'ticks': await app.state.extinctions.ticks(run, start, end)}

    @app.get('/api/runs/{run}/diagnostics')
    async def diagnostics(run: str):
        ensure_run_available(run)
        return [json.loads(r[0]) for r in app.state.store.db.execute('SELECT data FROM diagnostics WHERE run=?', (run,))]

    @app.get('/api/snapshots')
    async def snapshots(run: str, offset: int = 0):
        ensure_run_available(run)
        return [dict(id=r[0],pop_id=r[1],tick=r[2]) for r in app.state.store.db.execute('SELECT id,pop,tick FROM snapshots WHERE run=? ORDER BY tick DESC LIMIT 100 OFFSET ?', (run,max(0,offset)))]

    def ensure_run_available(run):
        if app.state.store.db.execute('SELECT 1 FROM removed_runs WHERE run=?', (run,)).fetchone():
            raise HTTPException(410, 'Run rimossa o in rimozione')

    @app.get('/api/snapshots/{snapshot}')
    async def snapshot(snapshot: str):
        row = app.state.store.db.execute('SELECT path,run FROM snapshots WHERE id=?', (snapshot,)).fetchone()
        if row:
            ensure_run_available(row[1])
        if not row or not Path(row[0]).is_file():
            raise HTTPException(404, 'Snapshot non disponibile')
        return json.loads(Path(row[0]).read_text())

    @app.get('/api/config')
    async def configuration():
        raw = config_path.read_text()
        entries = {}
        for line in raw.splitlines():
            text = line.split('#',1)[0]
            if '=' in text:
                key, value = text.split('=',1)
                entries[key.strip()] = value.strip()
        return {'locked': app.state.receiver.status.get('config_locked', False), 'schema': invoke('--config-schema'), 'values': invoke('--validate-config', config_path),
                'entries': entries, 'revision': hashlib.sha256(raw.encode()).hexdigest()}

    @app.put('/api/config')
    async def save_configuration(request: Request):
        if app.state.receiver.status.get('config_locked') or app.state.checkpoints.lock.locked():
            raise HTTPException(409, 'Configurazione bloccata dal checkpoint')
        data = await request.json()
        raw = config_path.read_text()
        revision = hashlib.sha256(raw.encode()).hexdigest()
        if data.get('revision') != revision:
            raise HTTPException(409, 'INI modificato esternamente: ricaricare prima di salvare')
        specs = {s['name']: s for s in invoke('--config-schema')}
        edits = data.get('changes', {})
        for key, value in edits.items():
            if key.split('@')[0] not in specs or not re.fullmatch(r'\w+(?:@\d+)?',key) or any(c in str(value) for c in '\r\n#'):
                raise HTTPException(422, f'Parametro non valido: {key}')
        found = set()
        lines = []
        for line in raw.splitlines(keepends=True):
            match = re.match(r'^(\s*)([\w@]+)(\s*=\s*)([^#\r\n]*)(.*)$', line.rstrip('\r\n'))
            if match and match[2] in edits:
                key = match[2]
                value = str(edits[key]).lower() if isinstance(edits[key],bool) else str(edits[key])
                line = f'{match[1]}{key}{match[3]}{value}  {match[5]}\n'
                found.add(key)
            lines.append(line)
        for key in edits.keys()-found:
            value = str(edits[key]).lower() if isinstance(edits[key],bool) else str(edits[key])
            lines.append(f'\n{key} = {value}\n')
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(mode='w', dir=config_path.parent, suffix='.ini', delete=False) as f:
                temporary = Path(f.name)
                f.write(''.join(lines)); f.flush(); os.fsync(f.fileno())
            invoke('--validate-config', temporary)
            if hashlib.sha256(config_path.read_bytes()).hexdigest() != revision:
                raise HTTPException(409, 'INI modificato durante il salvataggio')
            os.replace(temporary, config_path)
        finally:
            if temporary:
                temporary.unlink(missing_ok=True)
        return {'saved': True, 'applies': 'next_start'}

    @app.get('/api/stream')
    async def stream(request: Request):
        async def generate():
            queue = asyncio.Queue(maxsize=8)
            app.state.receiver.subscribers.add(queue)
            try:
                while not await request.is_disconnected():
                    try:
                        message = await asyncio.wait_for(queue.get(), 10)
                        yield f'data: {compact(message)}\n\n'
                    except asyncio.TimeoutError:
                        yield ': heartbeat\n\n'
            finally:
                app.state.receiver.subscribers.discard(queue)
        return StreamingResponse(generate(), media_type='text/event-stream', headers={'Cache-Control':'no-cache'})

    app.mount('/static', StaticFiles(directory=Path(__file__).parent/'static'), name='static')

    @app.get('/')
    async def index():
        return FileResponse(Path(__file__).parent/'static/index.html')
    return app


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--db', default=str(ROOT/'data_out/popx.sqlite'))
    parser.add_argument('--config', default=str(ROOT/'config/popx.ini'))
    parser.add_argument('--binary', default=str(ROOT/'build-release/src/popx'))
    parser.add_argument('--http-port', type=int, default=8000)
    parser.add_argument('--udp-port', type=int, default=8001)
    args = parser.parse_args()
    import uvicorn
    # Open EventSource streams must not hold the UDP port indefinitely after Exit.
    uvicorn.run(create_app(args.db,args.config,args.binary,args.udp_port),host='127.0.0.1',port=args.http_port,
                log_level='warning', timeout_graceful_shutdown=3)

if __name__ == '__main__':
    main()
