"""Deterministic genealogies, censoring, genetic distance and read-only API checks."""
import json
import tempfile
import time
import unittest
import threading
import concurrent.futures
from unittest.mock import patch
from pathlib import Path
import sqlite3

from fastapi import FastAPI
from fastapi.testclient import TestClient
from dashboard.evolution import EvolutionService, LineageIndex, install_routes, survival
from dashboard.genetics import canonical, color, fingerprint, similarity
from dashboard.genome_codec import pack_event


class EvolutionTests(unittest.TestCase):
    def test_shutdown_cancels_genealogy_work(self):
        cancel=threading.Event();cancel.set()
        with self.assertRaises(concurrent.futures.CancelledError):
            LineageIndex(cancel=cancel).update('/not/opened.sqlite','r')
        service=EvolutionService('/unused.sqlite');started=threading.Event()
        def work():
            started.set()
            if not service.closing.wait(3):raise AssertionError('Shutdown failed to cancel worker')
        future=service.pool.submit(work);self.assertTrue(started.wait(1))
        service.close();self.assertTrue(future.done());self.assertTrue(service.closing.is_set())

    def test_genetics(self):
        a=canonical([[0,0xffffffff,7,0x8000000e,-32768],[1,2,7,3,32767],[0,4,1,5,8192]])
        self.assertEqual(similarity(a,canonical(reversed(a))),1)
        self.assertEqual(similarity(a,canonical([*a,a[0]])),.75)
        self.assertEqual(similarity((),()),1)
        self.assertEqual(similarity(a,()),0)
        self.assertEqual(color(a),color(canonical(reversed(a))))
        b=canonical([[0,0x7fffffff,7,0x8000000e,-32768],[1,2,7,3,32767],[0,4,1,5,8192]])
        self.assertEqual(similarity(a,b),2/3)
        self.assertNotEqual(fingerprint(a),fingerprint(b))
        self.assertEqual(fingerprint(a),'f390b98f0fc55cfc')
        self.assertEqual(color(a),[106,130,89])

    @staticmethod
    def fixture():
        genes=[[0,5,7,4,8192]]
        rows=[(1,0,'founder','f',dict(genes=genes,parent_id='',forced=False)),
              (2,10,'birth','a',dict(genes=genes,parent_id='f',forced=False)),
              (3,20,'birth','b',dict(genes=[[0,6,7,4,8192]],parent_id='a',forced=False)),
              (4,30,'birth','x',dict(genes=genes,parent_id='a',forced=True)),
              (5,40,'death','a',dict(age=30,death_cause='energy')),
              (6,50,'cull','f',dict(age=50,death_cause='external'))]
        return rows

    def index(self):
        index=LineageIndex(.9)
        for row in self.fixture():index.ingest(*row)
        index.cutoff=100
        index.sample=dict(alive=2,counts=dict(founders=1,natural_births=2,forced_births=1,deaths=2),
                          reproduction=dict(eligible_agent_ticks=20,cap_blocked_agent_ticks=10))
        index.bins=[dict(tick=0),dict(tick=50),dict(tick=100,reproduction=index.sample['reproduction'])]
        index.summary=index.build_summary('run')
        return index

    def test_groups_and_censoring(self):
        index=self.index();s=index.summary
        self.assertTrue(s['integrity']['consistent'])
        self.assertEqual(index.nodes['b'].root,'f')
        self.assertEqual(index.nodes['b'].strain,'b')
        self.assertEqual(index.nodes['x'].root,'x')
        self.assertEqual(index.nodes['x'].depth,0)
        self.assertEqual(index.nodes['a'].children,1) # Forced x is separate.
        self.assertEqual(s['overview']['children_completed']['mean'],1)
        self.assertEqual(s['overview']['age_alive']['mean'],75)
        self.assertEqual(s['overview']['age_completed']['mean'],40)
        self.assertEqual(s['overview']['causes'],{'cull':1,'energy':1})
        self.assertEqual(s['cap_fraction'],.5)
        self.assertEqual(s['cap_timeline'][-1]['blocked_fraction'],.5)
        self.assertEqual(s['survival'][-1]['survival'],.5)
        self.assertEqual(sum(b['censored'] for b in s['age_histogram']),2)
        self.assertEqual(index.detail('f')['summary']['alive'],0)
        self.assertEqual({n['id'] for n in index.detail('f')['genealogy']},{'f','b'})
        self.assertEqual(sum(s['timeline'][-1]['counts'].values()),2)
        self.assertEqual(s['timeline'][-1]['effective_strains'],2)

    def test_missing_history(self):
        index=self.index()
        index.ingest(7,60,'birth','orphan',dict(parent_id='absent',genes=[],forced=False))
        self.assertFalse(index.build_summary('run')['integrity']['consistent'])
        self.assertEqual(index.missing_parents,1)

    def test_inherited_recovery_continues_family_without_natural_children(self):
        index=self.index()
        genes=[[0,5,7,4,8192]]
        index.ingest(7,60,'birth','r',dict(parent_id='a',genes=genes,forced=True,
                                       recovery_seed=True,recovery_from_archive=True))
        self.assertEqual(index.nodes['r'].root,'f')
        self.assertEqual(index.nodes['r'].strain,'f')
        self.assertEqual(index.nodes['r'].depth,2)
        self.assertEqual(index.nodes['a'].children,1)
        self.assertTrue(index.nodes['r'].recovery)
        self.assertEqual(index.summarize_group(list(index.nodes.values()))['inherited_recoveries'],1)
        index.ingest(8,70,'birth','r2',dict(parent_id='a',genes=[[0,9,7,4,8192]],forced=True,
                                        recovery_from_archive=True))
        self.assertEqual(index.strains['r2']['parent'],'f')
        self.assertEqual(index.strains['r2']['root'],'f')
        index.ingest(9,80,'birth','external',dict(parent_id='old_run_donor',genes=genes,forced=True,
            recovery_from_archive=True,recovery_parent_external=True))
        self.assertEqual(index.missing_parents,0)
        self.assertEqual(index.nodes['external'].parent,'old_run_donor')
        self.assertEqual(index.nodes['external'].depth,0)

    def test_concurrent_writer_cannot_advance_event_cursor(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'concurrent.sqlite'
            with sqlite3.connect(path) as writer:
                writer.execute('PRAGMA journal_mode=WAL')
                writer.executescript('CREATE TABLE events(id INTEGER PRIMARY KEY,run TEXT,tick INTEGER,type TEXT,pop TEXT,data TEXT);'
                    'CREATE TABLE samples(run TEXT,tick INTEGER,data TEXT);CREATE TABLE runs(id TEXT,metadata TEXT);'
                    'CREATE TABLE gaps(run TEXT,tick INTEGER);')
                writer.execute('INSERT INTO events VALUES(1,\'r\',0,\'founder\',\'f\',\'{"genes":[]}\')')
                writer.execute('INSERT INTO samples VALUES(\'r\',100,\'{"tick":100,"alive":1,"counts":{"founders":1,"deaths":0}}\')')
                writer.execute('INSERT INTO runs VALUES(\'r\',\'{}\')');writer.commit()
                reader=LineageIndex.open_database(path)
                class Interleaved:
                    def execute(self, sql, args=()):
                        cursor=reader.execute(sql,args)
                        if sql.startswith('SELECT min(id)'):
                            writer.execute('INSERT INTO events VALUES(2,\'r\',120,\'birth\',\'b\',\'{"genes":[],"parent_id":"f"}\')')
                            writer.commit()
                        return cursor
                    def close(self):reader.close()
                index=LineageIndex()
                with patch.object(index,'open_database',return_value=Interleaved()):index.update(path,'r')
                self.assertEqual(index.last_id,1)
                writer.execute('INSERT INTO samples VALUES(\'r\',120,\'{"tick":120,"alive":2,"counts":{"founders":1,"natural_births":1,"deaths":0}}\')')
                writer.commit();index.update(path,'r')
                self.assertTrue(index.summary['integrity']['consistent'])

    def test_read_only_service_and_incremental_refresh(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'history.sqlite'
            with sqlite3.connect(path) as db:
                db.executescript('CREATE TABLE events(id INTEGER PRIMARY KEY,run TEXT,tick INTEGER,type TEXT,pop TEXT,data TEXT);'
                                 'CREATE TABLE samples(run TEXT,tick INTEGER,data TEXT);'
                                 'CREATE TABLE runs(id TEXT,metadata TEXT);CREATE TABLE gaps(run TEXT,tick INTEGER);')
                db.executemany('INSERT INTO events VALUES(?,?,?,?,?,?)',
                               [(i,'run',t,k,p,json.dumps(pack_event(d) if i <= 3 else d))
                                for i,t,k,p,d in self.fixture()])
                db.execute('INSERT INTO samples VALUES(?,?,?)',('run',100,json.dumps(dict(tick=100,**self.index().sample))))
                db.execute('INSERT INTO runs VALUES(?,?)',('run','{}'))
            service=EvolutionService(path);app=FastAPI();install_routes(app,service)
            try:
                with TestClient(app) as client:
                    def ready():
                        deadline=time.monotonic()+10
                        while time.monotonic()<deadline:
                            reply=client.get('/api/evolution/run')
                            self.assertEqual(reply.status_code,200,reply.text)
                            result=reply.json()
                            if result['state']=='ready':return result
                            time.sleep(.02)
                        self.fail('analysis timeout')
                    self.assertTrue(ready()['integrity']['consistent'])
                    result=client.get('/api/evolution/run/compare?first=f&second=a').json()
                    self.assertEqual(result['similarity'],1)
                    self.assertEqual(client.get('/api/evolution/run/strain/f').status_code,200)
                    self.assertEqual(client.get('/api/evolution/run/compare?first=missing&second=a').status_code,422)
                    self.assertEqual(client.get('/api/evolution/run?threshold=0.2').status_code,422)
                    with sqlite3.connect(path) as db:
                        self.assertEqual(db.execute('SELECT count(*) FROM events').fetchone()[0],6)
                        db.execute('INSERT INTO events VALUES(?,?,?,?,?,?)',(7,'run',120,'death','b',json.dumps(dict(age=100,death_cause='energy'))))
                        sample=dict(tick=120,alive=1,counts=dict(founders=1,natural_births=2,forced_births=1,deaths=3))
                        db.execute('INSERT INTO samples VALUES(?,?,?)',('run',120,json.dumps(sample)))
                    client.get('/api/evolution/run?refresh=true')
                    final=ready()
                    self.assertTrue(final['integrity']['consistent'])
                    self.assertEqual(final['overview']['completed'],3)
                    with LineageIndex.open_database(path) as db:
                        with self.assertRaises(sqlite3.OperationalError):db.execute('DELETE FROM events')
            finally:service.close()


if __name__=='__main__':unittest.main()
