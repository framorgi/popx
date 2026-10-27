"""Checkpoint operations. Simulator owns state files; dashboard owns the SQLite history."""
import asyncio
import hashlib
import os
from pathlib import Path
import re
import sqlite3
import time
import uuid

from fastapi import HTTPException, Request


class Checkpoints:
    def __init__(self, app, evolution):
        self.app, self.evolution = app, evolution
        self.root = app.state.store.path.parent / 'checkpoints'
        self.root.mkdir(parents=True, exist_ok=True)
        self.index = sqlite3.connect(self.root / 'catalog.sqlite')
        self.index.executescript('''
            CREATE TABLE IF NOT EXISTS checkpoints(id TEXT PRIMARY KEY, created REAL, path TEXT,
                source_run TEXT, tick INTEGER, bytes INTEGER, sha256 TEXT, automatic INTEGER);
            CREATE TABLE IF NOT EXISTS operation(id INTEGER PRIMARY KEY CHECK(id=1), checkpoint TEXT,
                stage TEXT, error TEXT);
        ''')
        self.index.commit()
        self.lock = asyncio.Lock()
        self.state = {'stage': 'idle', 'error': ''}
        self.recovery = self.index.execute('SELECT checkpoint FROM operation WHERE id=1 AND stage<>?', ('done',)).fetchone()
        self.restored_run = None

    @property
    def receiver(self):
        return self.app.state.receiver

    def listing(self):
        return [dict(zip(('id', 'created', 'source_run', 'tick', 'bytes', 'automatic'), row)) for row in
                self.index.execute('SELECT id,created,source_run,tick,bytes,automatic FROM checkpoints ORDER BY created DESC')]

    def path(self, ident):
        if not re.fullmatch(r'[a-f0-9]{32}', ident):
            raise HTTPException(400, 'Identificativo checkpoint non valido')
        row = self.index.execute('SELECT path,sha256 FROM checkpoints WHERE id=?', (ident,)).fetchone()
        if not row:
            raise HTTPException(404, 'Checkpoint non disponibile')
        path = Path(row[0])
        if path.parent != self.root or path.is_symlink() or not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != row[1]:
            raise HTTPException(409, 'Checkpoint mancante o alterato: storico conservato')
        return path

    def stage(self, ident, stage, error=''):
        with self.index:
            self.index.execute('INSERT OR REPLACE INTO operation VALUES(1,?,?,?)', (ident, stage, error))
        self.state = dict(stage=stage, error=error, checkpoint=ident)

    async def save(self, automatic=False):
        ident = uuid.uuid4().hex
        path = self.root / (ident + '.popx')
        reply = await self.receiver.command('checkpoint_save', {'path': str(path), 'pause': automatic})
        data = reply['result']
        # Confirm both physical integrity and reconstruction before any destructive operation.
        await self.receiver.command('checkpoint_verify', {'path': str(path)})
        with self.index:
            self.index.execute('INSERT INTO checkpoints VALUES(?,?,?,?,?,?,?,?)',
                (ident, time.time(), str(path), data['run_id'], data['tick'], path.stat().st_size,
                 hashlib.sha256(path.read_bytes()).hexdigest(), int(automatic)))
        return ident

    async def wait_status(self, run):
        for _ in range(300):
            s = self.receiver.status
            if self.receiver.run == run and s.get('state') == 'paused' and s.get('transport', {}).get('queued', 1) == 0:
                return
            await asyncio.sleep(.1)
        raise RuntimeError('Attesa conferma della nuova run scaduta; checkpoint e storico conservati')

    async def load(self, ident, resume=True):
        path = self.path(ident)
        reply = await self.receiver.command('checkpoint_load', {'path': str(path)})
        run = reply['status']['run_id']
        await self.wait_status(run)
        if resume:
            self.app.state.storage_guard.ensure_capacity()
            await self.receiver.command('resume', {}, run)
        return run

    async def reset_history(self, keep_run=None):
        """Replace history; keep only the restored run when a simulator is active."""
        store = self.app.state.store
        await self.app.state.removals.close()
        await asyncio.to_thread(self.evolution.close)
        try:
            from .server import Store
            fresh_path = store.path.with_name(store.path.name + '.rotation-new')
            for suffix in ('', '-wal', '-shm'):
                Path(str(fresh_path) + suffix).unlink(missing_ok=True)
            fresh = Store(fresh_path, store.binary)
            try:
                tables = ['runs', 'samples', 'events', 'environments', 'environment_parts', 'diagnostics', 'snapshots', 'gaps']
                for table in tables:
                    key = 'id' if table == 'runs' else 'run'
                    rows = store.db.execute(f'SELECT * FROM {table} WHERE {key}=?', (keep_run,)).fetchall()
                    if rows:
                        fresh.db.executemany(f'INSERT INTO {table} VALUES({",".join("?" for _ in rows[0])})', rows)
                # Durable tombstones acknowledge late UDP packets without recreating discarded history.
                if keep_run is None:
                    old_runs = [r[0] for r in store.db.execute('SELECT id FROM runs')]
                else:
                    old_runs = [r[0] for r in store.db.execute('SELECT id FROM runs WHERE id<>?', (keep_run,))]
                fresh.db.executemany("INSERT INTO removed_runs VALUES(?,?,'done',0,0,0,'')", [(r, time.time()) for r in old_runs])
                fresh.db.commit()
                if fresh.db.execute('PRAGMA wal_checkpoint(TRUNCATE)').fetchone()[0]:
                    raise RuntimeError('Checkpoint WAL del nuovo database occupato: storico conservato')
                fresh.close()
                if any(Path(str(fresh_path) + suffix).exists() for suffix in ('-wal', '-shm')):
                    raise RuntimeError('Il nuovo database ha ancora lettori SQLite attivi: storico conservato')
                with open(fresh_path, 'rb') as saved:
                    os.fsync(saved.fileno())
                if store.db.execute('PRAGMA wal_checkpoint(TRUNCATE)').fetchone()[0]:
                    raise RuntimeError('Checkpoint WAL dello storico occupato: storico conservato')
                store.close()
                try:
                    # Replacing the main file while old WAL/SHM readers exist can retain
                    # gigabytes on an unlinked inode and attach an old journal to the new DB.
                    if any(Path(str(store.path) + suffix).exists() for suffix in ('-wal', '-shm')):
                        raise RuntimeError('Lettori SQLite ancora attivi sul vecchio storico: rotazione rinviata')
                    os.replace(fresh_path, store.path)
                    fd = os.open(store.path.parent, os.O_RDONLY | os.O_DIRECTORY)
                    try: os.fsync(fd)
                    finally: os.close(fd)
                finally:
                    # Open the old file again if replacement failed, otherwise the new file.
                    replacement = Store(store.path, store.binary)
                    store.db = replacement.db
                self.receiver.parts.clear()
                self.receiver.partial_bytes = 0
                self.app.state.extinctions.cache.clear()
            finally:
                try: fresh.close()
                except sqlite3.Error: pass
                for suffix in ('', '-wal', '-shm'):
                    Path(str(fresh_path) + suffix).unlink(missing_ok=True)
        finally:
            # Keep the same service object referenced by the registered routes.
            self.evolution.reopen()

    async def rotate(self):
        if self.lock.locked():
            return
        async with self.lock:
            ident = self.recovery[0] if self.recovery else None
            try:
                self.state = dict(stage='saving', error='')
                if ident is None:
                    ident = await self.save(automatic=True)
                    self.stage(ident, 'saved')
                self.stage(ident, 'loading')
                if (self.restored_run and self.receiver.run == self.restored_run and
                        self.receiver.status.get('state') == 'paused'):
                    run = self.restored_run
                else:
                    run = await self.load(ident, resume=False)
                    self.restored_run = run
                self.stage(ident, 'reclaiming')
                await self.reset_history(run)
                self.stage(ident, 'resuming')
                self.app.state.storage_guard.ensure_capacity()
                await self.receiver.command('resume', {}, run)
                self.stage(ident, 'done')
                self.recovery = None
                self.restored_run = None
                # Bound automatic checkpoint storage; manual checkpoints are retained.
                obsolete = self.index.execute('SELECT id,path FROM checkpoints WHERE automatic=1 ORDER BY created DESC LIMIT -1 OFFSET 2').fetchall()
                for old, path in obsolete:
                    Path(path).unlink(missing_ok=True)
                    with self.index: self.index.execute('DELETE FROM checkpoints WHERE id=?', (old,))
            except Exception as error:
                message = str(getattr(error, 'detail', error))
                self.state = dict(stage='error', error=message, checkpoint=ident)
                if ident:
                    self.stage(ident, 'error', message)
                    self.recovery = (ident,)
                # Preserve live state if saving failed, and never erase history before a verified load.
                if self.receiver.status.get('state') == 'running':
                    try: await self.receiver.command('pause', {})
                    except Exception: pass
                raise

    def close(self):
        self.index.close()


def install_routes(app):
    @app.get('/api/checkpoints')
    async def listing():
        service = app.state.checkpoints
        return {'checkpoints': service.listing(), 'operation': service.state}

    @app.post('/api/checkpoints')
    async def save():
        service = app.state.checkpoints
        if service.lock.locked(): raise HTTPException(409, 'Operazione checkpoint in corso')
        async with service.lock:
            return {'id': await service.save()}

    @app.post('/api/checkpoints/{ident}/load')
    async def load(ident: str, request: Request):
        if (await request.json()).get('confirm') != ident:
            raise HTTPException(400, 'Confermare il checkpoint selezionato')
        service = app.state.checkpoints
        if service.lock.locked(): raise HTTPException(409, 'Operazione checkpoint in corso')
        async with service.lock:
            return {'run_id': await service.load(ident)}
