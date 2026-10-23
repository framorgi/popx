"""Destructive operations are verified only against disposable databases."""
import json
from pathlib import Path
import socket
import sqlite3
import tempfile
import time
import unittest
from concurrent.futures import Future
from dashboard.evolution import LineageIndex
from fastapi.testclient import TestClient
from dashboard.server import Store, create_app, ROOT


class StorageTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.path=self.root/'history.sqlite';self.snap=self.root/'nnets'/'old'/'pop_1'/'10_snap1.json'
        self.snap.parent.mkdir(parents=True);self.snap.write_text('{}')
        self.outside=self.root/'unrelated.json';self.outside.write_text('preserve')
        store=Store(self.path)
        with store.db as db:
            meta=json.dumps({'config':{'nnets_dir':str(self.root/'nnets')}})
            for run,state in [('old','finished'),('live','running'),('paused','paused')]:
                db.execute('INSERT INTO runs VALUES(?,0,?,?,?,NULL)',(run,meta,state,''))
                db.execute('INSERT INTO samples VALUES(?,10,?)',(run,'{"tick":10}'))
            db.executemany('INSERT INTO events(run,tick,type,pop,data) VALUES(?,10,\'birth\',\'p\',?)',
                [('old',json.dumps({'payload':'x'*5000}))]*1200+[('live','{"keep":true}')])
            for table in ['environments','environment_parts','diagnostics','gaps']:
                if table=='environment_parts':db.execute('INSERT INTO environment_parts VALUES(\'old\',0,1,\'{}\')')
                elif table=='gaps':db.execute('INSERT INTO gaps(run,tick,dropped,recorded) VALUES(\'old\',0,1,0)')
                else:db.execute(f'INSERT INTO {table}(run,data) VALUES(\'old\',\'{{}}\')')
            db.execute('INSERT INTO snapshots VALUES(\'snap1\',\'old\',\'pop_1\',10,?)',(str(self.snap),))
            db.execute('INSERT INTO snapshots VALUES(\'untrusted\',\'old\',\'pop_1\',10,?)',(str(self.outside),))
        store.close()
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
            sock.bind(('127.0.0.1',0));self.port=sock.getsockname()[1]

    def tearDown(self):self.temp.cleanup()

    def client(self):
        self.app=create_app(self.path,ROOT/'config/popx.ini',ROOT/'build-release/src/popx',self.port)
        return TestClient(self.app)

    def wait(self,client):
        deadline=time.monotonic()+5
        while time.monotonic()<deadline:
            job=client.get('/api/storage/removals/old').json()
            if job['state']=='deleted':return job
            self.assertNotEqual(job['state'],'error',job);time.sleep(.01)
        self.fail('deletion did not finish')

    def test_delete_isolated_run_and_space_accounting(self):
        with self.client() as client:
            before=client.get('/api/storage').json()
            self.assertEqual(before['disk_unavailable']+before['disk_available'],before['disk_total'])
            self.assertGreater(before['database_files_bytes'],0)
            self.assertEqual(client.request('DELETE','/api/runs/old',json={}).status_code,422)
            for run in ['live','paused']:
                self.assertEqual(client.request('DELETE',f'/api/runs/{run}',json={'confirm_run':run}).status_code,409)
            async def protect():self.app.state.receiver.run='old'
            client.portal.call(protect)
            self.assertEqual(client.request('DELETE','/api/runs/old',json={'confirm_run':'old'}).status_code,409)
            async def clear():self.app.state.receiver.run='live'
            client.portal.call(clear)
            self.assertEqual(client.request('DELETE','/api/runs/old',json={'confirm_run':'old'},headers={'Origin':'https://elsewhere.invalid'}).status_code,403)
            self.assertEqual(client.request('DELETE','/api/runs/old',json={'confirm_run':'old'}).status_code,202)
            job=self.wait(client)
            self.assertEqual(job['files_deleted'],1);self.assertEqual(job['files_retained'],1)
            self.assertFalse(self.snap.exists());self.assertEqual(self.outside.read_text(),'preserve')
            self.assertEqual(client.get('/api/runs/old').status_code,410)
            self.assertEqual(client.get('/api/runs/old/samples').status_code,410)
            self.assertEqual({r['id'] for r in client.get('/api/runs').json()},{'live','paused'})
            self.assertEqual(len(client.get('/api/runs/live/events').json()),1)
            after=client.get('/api/storage').json()
            self.assertGreater(after['database_reusable_bytes'],before['database_reusable_bytes'])
            async def late():
                return self.app.state.store.ingest('process',999,dict(run_id='old',tick=11,type='sample',data={}))
            self.assertFalse(client.portal.call(late))
        with sqlite3.connect(self.path) as db:
            for table in ['events','samples','environments','environment_parts','diagnostics','snapshots','gaps']:
                self.assertEqual(db.execute(f'SELECT count(*) FROM {table} WHERE run=\'old\'').fetchone()[0],0)

    def test_resume_durable_removal_after_restart(self):
        with sqlite3.connect(self.path) as db:
            db.execute("INSERT INTO removed_runs VALUES('old',0,'deleting',0,0,0,'')")
        with self.client() as client:
            self.wait(client)
            self.assertEqual(client.get('/api/evolution/old').status_code,422)
            self.assertEqual(client.get('/api/runs/live').status_code,200)

    def test_running_lineage_reader_blocks_removal(self):
        with self.client() as client:
            future=Future()
            async def reading():
                service=self.app.state.removals.evolution
                with service.lock:service.cache[('old',.9)]=(LineageIndex(),future)
            client.portal.call(reading)
            self.assertEqual(client.request('DELETE','/api/runs/old',json={'confirm_run':'old'}).status_code,409)
            self.assertEqual(client.get('/api/runs/old').status_code,200)
            future.set_result(None)
            self.assertEqual(client.request('DELETE','/api/runs/old',json={'confirm_run':'old'}).status_code,202)
            self.wait(client)


if __name__=='__main__':unittest.main()
