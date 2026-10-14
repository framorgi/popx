"""Real C++/UDP/HTTP/SQLite integration, isolated INI and database."""
import json
import os
from pathlib import Path
import socket
import sqlite3
import subprocess
import tempfile
import time
import unittest
from fastapi.testclient import TestClient
from dashboard.server import create_app, ROOT


def free_port():
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1',0))
        return sock.getsockname()[1]


class Integration(unittest.TestCase):
    def test_runtime(self):
        with tempfile.TemporaryDirectory(prefix='popx-integration-') as temp:
            folder=Path(temp)
            config=folder/'popx.ini'
            raw=(ROOT/'config/popx.ini').read_text().replace('map_size = 120','map_size = 24')
            raw=raw.replace('population           = 80','population           = 20')
            raw=raw.replace('nnets_dir = ../data_out/nnets/',f'nnets_dir = {folder}/nnets/')
            raw += "\nphy_alpha_min=0\nphy_alpha_max=0\nphy_init_temperature=37\n"
            raw += '# Unicode protocol boundary: '+('è🌿'*600)+'\n'
            config.write_text(raw)
            port=free_port()
            database=folder/'history.sqlite'
            app=create_app(database,config,ROOT/'build-release/src/popx',port)
            env={**os.environ};env.pop('DISPLAY',None)
            with TestClient(app) as client:
                self.assertEqual(client.get('/').status_code,200)
                self.assertEqual(client.get('/static/app.js').status_code,200)
                proc=subprocess.Popen([str(ROOT/'build-release/src/popx'),'--headless','--config',str(config),'--udp-port',str(port)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                try:
                    def until(predicate,timeout=8):
                        end=time.monotonic()+timeout
                        while time.monotonic()<end:
                            value=predicate()
                            if value:return value
                            if proc.poll() is not None:raise AssertionError(proc.communicate())
                            time.sleep(.05)
                        self.fail('Timed out')
                    status=until(lambda: (s if (s:=client.get('/api/status').json()).get('connected') else None))
                    run=status['run_id']
                    def cmd(name,args=None,expected=200,run_id=None):
                        response=client.post('/api/commands',json={'command':name,'arguments':args or {},'run_id':run_id or run})
                        self.assertEqual(response.status_code,expected,response.text)
                        return response.json()
                    paused=cmd('pause')['status']['tick']
                    time.sleep(.1)
                    self.assertEqual(cmd('agents')['status']['tick'],paused)
                    listing=cmd('agents')['result']
                    self.assertTrue(listing['agents'])
                    pop=listing['agents'][0]['pop_id']
                    inspected=cmd('inspect',{'pop_id':pop})['result']
                    self.assertEqual(inspected['tick'],paused)
                    self.assertEqual(inspected['brain']['sizeS'],49)
                    self.assertIn('connections',inspected['brain'])
                    saved=cmd('snapshot',{'pop_id':pop})['result']
                    self.assertTrue(list((folder/'nnets').rglob('*.json')))
                    self.assertEqual(client.get('/api/snapshots/'+saved['snapshot_id']).json()['pop_id'],pop)
                    # Send exactly the same UDP command twice: NewGen must only cull once.
                    async def duplicate():
                        import asyncio
                        receiver=app.state.receiver
                        request='duplicate-newgen'
                        future=asyncio.get_running_loop().create_future()
                        receiver.pending[request]=future
                        packet=json.dumps({'version':1,'type':'command','process_id':receiver.process,
                            'run_id':run,'request_id':request,'command':'newgen','arguments':{},
                            'expires_ms':int(time.time()*1000)+5000}).encode()
                        receiver.transport.sendto(packet,receiver.peer)
                        receiver.transport.sendto(packet,receiver.peer)
                        try:return await asyncio.wait_for(future,3)
                        finally:receiver.pending.pop(request,None)
                    duplicate_reply=client.portal.call(duplicate)
                    self.assertTrue(duplicate_reply['ok'])
                    time.sleep(.1)
                    self.assertEqual(cmd('agents')['status']['alive'],duplicate_reply['status']['alive'])
                    cmd('show_gui',expected=409)
                    self.assertFalse(client.get('/api/status').json()['gui'])
                    cmd('speed',{'value':2})
                    cmd('speed',{'value':3},expected=409)
                    cmd('resume')
                    time.sleep(.2)
                    advanced=cmd('pause')['status']['tick']
                    self.assertGreaterEqual(advanced-paused,20)
                    cfg=client.get('/api/config').json()
                    response=client.put('/api/config',json={'revision':cfg['revision'],'changes':{'map_size':'16','num_threads':'1','brain_size_n':'8','max_duration_minutes':'0.001'}})
                    self.assertEqual(response.status_code,200,response.text)
                    self.assertEqual(cmd('agents')['status']['tick'],advanced)
                    stale=client.put('/api/config',json={'revision':cfg['revision'],'changes':{'map_size':'18'}})
                    self.assertEqual(stale.status_code,409)
                    restarted=cmd('restart')['status']
                    self.assertNotEqual(restarted['run_id'],run)
                    old_run,run=run,restarted['run_id']
                    until(lambda: client.get('/api/status').json().get('state')=='finished')
                    stopped=client.get('/api/status').json()
                    self.assertEqual(stopped['tick'],6)
                    self.assertEqual(stopped['reason'],'duration')
                    self.assertEqual(stopped['metadata']['config']['map_size'],16)
                    self.assertEqual(stopped['metadata']['config']['brain_size_n'],8)
                    cmd('stop',expected=409,run_id=old_run)
                    samples=until(lambda:client.get(f'/api/runs/{run}/samples').json())
                    until(lambda: any(s['tick']==6 for s in client.get(f'/api/runs/{run}/samples').json()))
                    self.assertEqual(client.get('/api/snapshots/'+saved['snapshot_id']).status_code,200)
                    config.write_text(config.read_text()+'\nmap_size = -1\n')
                    cmd('start',expected=409)
                    self.assertEqual(client.get('/api/status').json()['run_id'],run)
                    cmd('exit')
                    proc.wait(timeout=5)
                    self.assertEqual(proc.returncode,0,proc.stderr.read().decode())
                finally:
                    if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
                    proc.stdout.close();proc.stderr.close()
            with sqlite3.connect(database) as db:
                self.assertEqual(db.execute('SELECT count(*) FROM runs').fetchone()[0],2)
                self.assertEqual(db.execute('SELECT sum(json_array_length(data)) FROM environment_parts WHERE run=?',(old_run,)).fetchone()[0],24*24)
                self.assertEqual(db.execute('SELECT sum(json_array_length(data)) FROM environment_parts WHERE run=?',(run,)).fetchone()[0],16*16)
                final=json.loads(db.execute('SELECT data FROM samples WHERE run=? AND tick=6',(run,)).fetchone()[0])
                self.assertEqual(final['resource_ledger']['residual'],[0]*6)
                self.assertAlmostEqual(final['energy_ledger']['residual'],0,places=5)
                self.assertGreater(db.execute("SELECT count(*) FROM events WHERE type='censored'").fetchone()[0],0)
            self.assertFalse(list(folder.glob('stats*.json')))
            # Persistence remains queryable after backend restart.
            with TestClient(create_app(database,config,ROOT/'build-release/src/popx',port)) as client:
                self.assertEqual(len(client.get('/api/runs').json()),2)

    def test_backend_outage(self):
        with tempfile.TemporaryDirectory(prefix='popx-outage-') as temp:
            folder=Path(temp);config=folder/'popx.ini';port=free_port()
            raw=(ROOT/'config/popx.ini').read_text().replace('map_size = 120','map_size = 24')
            config.write_text(raw+'\nphy_alpha_min=0\nphy_alpha_max=0\nphy_init_temperature=37\n')
            proc=subprocess.Popen([str(ROOT/'build-release/src/popx'),'--headless','--config',str(config),'--udp-port',str(port)],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
            try:
                time.sleep(7)
                self.assertIsNone(proc.poll())
                with TestClient(create_app(folder/'history.sqlite',config,ROOT/'build-release/src/popx',port)) as client:
                    for _ in range(100):
                        status=client.get('/api/status').json()
                        if status.get('connected') and status.get('transport',{}).get('dropped_messages',0)>0:break
                        time.sleep(.1)
                    else:self.fail('No reconnection/gap status')
                    self.assertGreater(status['tick'],500)
                    response=client.post('/api/commands',json={'command':'exit','run_id':status['run_id']})
                    self.assertEqual(response.status_code,200,response.text)
                    proc.wait(timeout=5)
                with sqlite3.connect(folder/'history.sqlite') as db:
                    self.assertGreater(db.execute('SELECT count(*) FROM gaps').fetchone()[0],0)
            finally:
                if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
                proc.stderr.close()


if __name__=='__main__':unittest.main()
