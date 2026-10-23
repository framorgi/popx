"""Storage accounting and bounded deletion on the dashboard's single SQLite writer."""
import asyncio
import json
import math
import platform
from pathlib import Path
import re
import shutil
import time

from fastapi import HTTPException, Request


def database_files(path):
    files = {}
    for name, suffix in [('database', ''), ('journal', '-wal'), ('shared_index', '-shm')]:
        try:
            files[name] = Path(str(path)+suffix).stat().st_size
        except FileNotFoundError:
            if not suffix:
                raise
            files[name] = 0
    return files


class StorageGuard:
    """Rotate history through a verified checkpoint at the measured threshold; this is not a filesystem quota."""
    def __init__(self, store, receiver, config_path):
        self.store, self.receiver, self.config_path = store, receiver, Path(config_path)
        self.task = None
        self.stopped_run = None
        self.checkpoints = None
        self.retry_at = 0
        self.state = dict(limit_gb=10.0, limit_bytes=10_000_000_000, exceeded=False,
                          error='', stop_requested=False, run=None, measured_at=None)

    def limit(self, configured=False):
        active = self.receiver.status.get('metadata', {}).get('config', {})
        if not configured and 'database_limit_gb' in active:
            value = active['database_limit_gb']
        else:
            value = 10.0
            for line in self.config_path.read_text().splitlines():
                pair = line.split('#', 1)[0].split('=', 1)
                if len(pair) == 2 and pair[0].strip() == 'database_limit_gb':
                    value = pair[1].strip()
        value = float(value)
        if not math.isfinite(value) or not .001 <= value <= 1_000_000:
            raise ValueError('database_limit_gb deve essere fra 0,001 e 1000000 GB.')
        return value

    def ensure_capacity(self, configured=False):
        try:
            limit = self.limit(configured)
            total = sum(database_files(self.store.path).values())
        except (OSError, ValueError, TypeError) as error:
            raise HTTPException(503, f'Impossibile verificare il limite del database: {error}') from error
        if total >= limit*1_000_000_000:
            raise HTTPException(409, f'Database al limite di {limit:g} GB. Libera spazio nei file o aumenta database_limit_gb prima di avviare la run.')

    async def check(self):
        run = self.receiver.run
        self.state.update(run=run, measured_at=time.time(), error='', stop_requested=self.stopped_run == run and run is not None)
        try:
            limit = self.limit()
            total = sum(database_files(self.store.path).values())
            self.state.update(limit_gb=limit, limit_bytes=int(limit*1_000_000_000),
                              measured_bytes=total, exceeded=total >= limit*1_000_000_000)
        except (OSError, ValueError, TypeError) as error:
            self.state.update(error=str(error), exceeded=False)
            return  # Report failure and retry; no automatic stop without a measured threshold crossing.
        if (self.state['exceeded'] or (self.checkpoints and self.checkpoints.recovery)) and time.monotonic() >= self.retry_at and run and run != self.stopped_run and self.receiver.status.get('state') in ('running','paused'):
            try:
                if self.checkpoints and self.receiver.status.get('checkpoint_supported'):
                    await self.checkpoints.rotate()
                    self.state['rotation'] = self.checkpoints.state
                else:
                    await self.receiver.command('stop', {'reason': 'storage_limit'}, run)
                    self.stopped_run = run
                    self.state['stop_requested'] = True
            except Exception as error:
                self.retry_at = time.monotonic() + 30
                self.state['error'] = str(error)  # Retry on the next check, including missing acknowledgements.

    def start(self):
        self.task = asyncio.create_task(self.watch())

    async def watch(self):
        while True:
            await self.check()
            await asyncio.sleep(.5)

    async def close(self):
        if self.task:
            self.task.cancel()
            try:
                await self.task
            except asyncio.CancelledError:
                pass


def disk_report(store):
    path = store.path
    usage = shutil.disk_usage(path.parent)
    files = database_files(path)
    page_size = store.db.execute('PRAGMA page_size').fetchone()[0]
    pages = store.db.execute('PRAGMA page_count').fetchone()[0]
    free_pages = store.db.execute('PRAGMA freelist_count').fetchone()[0]
    return dict(measured_at=time.time(), wsl='microsoft' in platform.release().lower(), windows_available=None,
                disk_total=usage.total, disk_available=usage.free,
                disk_unavailable=usage.total-usage.free, files=files, database_files_bytes=sum(files.values()),
                database_logical_bytes=pages*page_size, database_reusable_bytes=free_pages*page_size,
                database_occupied_pages_bytes=(pages-free_pages)*page_size,
                run_count=store.db.execute('SELECT count(*) FROM runs WHERE id NOT IN (SELECT run FROM removed_runs)').fetchone()[0])


class RemovalService:
    """Small committed batches yield to telemetry; durable intent resumes after shutdown."""
    def __init__(self, store, evolution):
        self.store, self.evolution = store, evolution
        self.task = None

    def status(self, run):
        row = self.store.db.execute('SELECT state,rows_deleted,files_deleted,files_retained,error FROM removed_runs WHERE run=?', (run,)).fetchone()
        if row is None:
            raise HTTPException(404, 'Rimozione non disponibile')
        return dict(zip(('state','rows_deleted','files_deleted','files_retained','error'), row), run=run)

    def resume(self):
        self.task = asyncio.create_task(self.process())

    async def close(self):
        if self.task:
            self.task.cancel()
            try:
                await self.task
            except asyncio.CancelledError:
                pass

    def start(self, run, current):
        db = self.store.db
        if run == current:
            raise HTTPException(409, 'La run collegata al simulatore è protetta. Seleziona una run precedente.')
        previous = db.execute('SELECT state FROM removed_runs WHERE run=?', (run,)).fetchone()
        if previous:
            if previous[0] == 'error':
                if db.execute("SELECT 1 FROM removed_runs WHERE state='deleting' AND run<>?", (run,)).fetchone():
                    raise HTTPException(409, 'Attendi la rimozione già in corso.')
                with db:
                    db.execute("UPDATE removed_runs SET state='deleting',error='' WHERE run=?", (run,))
                if not self.task or self.task.done(): self.resume()
            return self.status(run)
        row = db.execute('SELECT state FROM runs WHERE id=?', (run,)).fetchone()
        if row is None:
            raise HTTPException(404, 'Run non disponibile')
        if row[0] != 'finished':
            raise HTTPException(409, 'Si possono rimuovere le run concluse. Questa run risulta ancora aperta.')
        if db.execute("SELECT 1 FROM removed_runs WHERE state='deleting'").fetchone():
            raise HTTPException(409, 'Attendi la rimozione già in corso.')
        try:
            self.evolution.retire(run)
        except ValueError as error:
            raise HTTPException(409, str(error)) from error
        with db:
            db.execute("INSERT INTO removed_runs VALUES(?,?,'deleting',0,0,0,'')", (run, time.time()))
        if not self.task or self.task.done(): self.resume()
        return self.status(run)

    @staticmethod
    def remove_snapshot(row, run, metadata):
        """Only unlink the exact generated path; leave unrecognised paths and symlinks intact."""
        snapshot, pop, tick, saved = row
        root = metadata.get('config', {}).get('nnets_dir')
        if not root or not all(re.fullmatch(r'[a-zA-Z0-9_-]+', str(s)) for s in (run,pop,snapshot)):
            return False
        root = Path(root).absolute()
        expected = root/run/pop/f'{tick}_{snapshot}.json'
        path = Path(saved).absolute()
        if path != expected or any(p.is_symlink() for p in (path, *path.parents)):
            return False
        try:
            path.unlink(missing_ok=True)
            return True
        except OSError:
            return False

    async def process(self):
        db = self.store.db
        # Tombstones prevent delayed telemetry from re-creating erased runs.
        for (run,) in db.execute("SELECT run FROM removed_runs WHERE state='deleting'").fetchall():
            try:
                row = db.execute('SELECT metadata FROM runs WHERE id=?', (run,)).fetchone()
                metadata = json.loads(row[0]) if row else {}
                while True:
                    rows = db.execute('SELECT id,pop,tick,path FROM snapshots WHERE run=? LIMIT 100', (run,)).fetchall()
                    if not rows: break
                    deleted = sum(self.remove_snapshot(row, run, metadata) for row in rows)
                    with db:
                        db.executemany('DELETE FROM snapshots WHERE id=?', [(row[0],) for row in rows])
                        db.execute('UPDATE removed_runs SET files_deleted=files_deleted+?,files_retained=files_retained+?,rows_deleted=rows_deleted+? WHERE run=?',
                                   (deleted,len(rows)-deleted,len(rows),run))
                    await asyncio.sleep(.01)
                for table in ('events','samples','environment_parts','environments','diagnostics','gaps'):
                    while True:
                        with db:
                            count = db.execute(f'DELETE FROM {table} WHERE rowid IN (SELECT rowid FROM {table} WHERE run=? LIMIT 400)', (run,)).rowcount
                            db.execute('UPDATE removed_runs SET rows_deleted=rows_deleted+? WHERE run=?', (count,run))
                        if not count: break
                        await asyncio.sleep(.01)
                with db:
                    db.execute('DELETE FROM runs WHERE id=?', (run,))
                    db.execute("UPDATE removed_runs SET state='deleted' WHERE run=?", (run,))
            except Exception as error:
                with db:
                    db.execute("UPDATE removed_runs SET state='error',error=? WHERE run=?", (str(error),run))


def install_routes(app):
    @app.get('/api/storage')
    async def storage():
        report = disk_report(app.state.store)
        report['guard'] = app.state.storage_guard.state
        report['removals'] = [app.state.removals.status(row[0]) for row in
            app.state.store.db.execute('SELECT run FROM removed_runs ORDER BY requested DESC LIMIT 10')]
        return report

    @app.delete('/api/runs/{run}', status_code=202)
    async def remove_run(run: str, request: Request):
        if getattr(app.state, 'checkpoints', None) and app.state.checkpoints.lock.locked():
            raise HTTPException(409, 'Rotazione checkpoint in corso: attendere')
        body = await request.json()
        if body.get('confirm_run') != run:
            raise HTTPException(422, 'Conferma l’identificativo della run da rimuovere.')
        return app.state.removals.start(run, app.state.receiver.run)

    @app.get('/api/storage/removals/{run}')
    async def removal(run: str):
        return app.state.removals.status(run)
