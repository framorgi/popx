"""Real simulator/backend checkpoint save/load and storage rotation in temporary databases."""
import json
from contextlib import closing
import os
from pathlib import Path
import socket
import sqlite3
import subprocess
import tempfile
import time
import unittest
from fastapi.testclient import TestClient
from dashboard.server import ROOT, create_app


class CheckpointIntegration(unittest.TestCase):
    def test_remove_disconnected_open_run(self):
        with tempfile.TemporaryDirectory(prefix='popx-remove-') as tmp:
            root=Path(tmp);ini=root/'config.ini';db=root/'history.sqlite'
            ini.write_text((ROOT/'config/popx.ini').read_text())
            with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
                sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
            with TestClient(create_app(db,ini,ROOT/'build-release/src/popx',port)) as client:
                with closing(sqlite3.connect(db)) as con:
                    con.execute("INSERT INTO runs VALUES('orphan',0,'{}','running','',NULL)")
                    con.execute('INSERT INTO samples VALUES(?,?,?)',('orphan',0,'{}'))
                    con.commit()
                response=client.request('DELETE','/api/runs/orphan',json={'confirm_run':'orphan'})
                self.assertEqual(response.status_code,202,response.text)
                deadline=time.monotonic()+5
                while time.monotonic()<deadline:
                    removal=client.get('/api/storage/removals/orphan').json()
                    if removal['state']=='deleted':break
                    time.sleep(.02)
                self.assertEqual(removal['state'],'deleted',removal)
                self.assertEqual(client.get('/api/runs').json(),[])

    def test_clean_history_without_connected_run(self):
        with tempfile.TemporaryDirectory(prefix='popx-clean-') as tmp:
            root=Path(tmp);ini=root/'config.ini';db=root/'history.sqlite'
            ini.write_text((ROOT/'config/popx.ini').read_text())
            with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
                sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
            with TestClient(create_app(db,ini,ROOT/'build-release/src/popx',port)) as client:
                with closing(sqlite3.connect(db)) as con:
                    con.execute("INSERT INTO runs VALUES('old',0,'{}','finished','stop',NULL)")
                    con.execute('INSERT INTO diagnostics(run,data) VALUES(?,?)',('old','x'*100_000))
                    con.commit()
                self.assertEqual(client.post('/api/storage/clean',json={}).status_code,422)
                response=client.post('/api/storage/clean',json={'confirm':'clean_history'})
                self.assertEqual(response.status_code,200,response.text)
                self.assertIsNone(response.json()['run_id'])
                self.assertEqual(client.get('/api/runs').json(),[])
                self.assertLess(db.stat().st_size,100_000)

    def test_save_load_locked_config_and_rotation(self):
        with tempfile.TemporaryDirectory(prefix='popx-checkpoints-') as tmp:
            root=Path(tmp);ini=root/'config.ini';db=root/'history.sqlite'
            ini.write_text((ROOT/'config/popx.ini').read_text()+'\nmap_size = 24\npopulation = 12\nmax_duration_minutes = 0\nnewgen_enabled = false\ndatabase_limit_gb = 0.01\nphy_alpha_min = 0\nphy_alpha_max = 0\nphy_init_temperature = 37\n')
            with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
                sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
            binary=ROOT/'build-release/src/popx'
            with TestClient(create_app(db,ini,binary,port)) as client:
                proc=subprocess.Popen([str(binary),'--headless','--config',str(ini),'--udp-port',str(port)],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
                def status():return client.get('/api/status').json()
                def wait(predicate,seconds=30):
                    deadline=time.monotonic()+seconds
                    while time.monotonic()<deadline:
                        s=status()
                        if predicate(s):return s
                        if proc.poll() is not None:raise AssertionError(proc.stderr.read().decode())
                        time.sleep(.05)
                    raise AssertionError(str(s)+' '+client.get('/api/checkpoints').text)
                def command(name):
                    response=client.post('/api/commands',json={'command':name,'run_id':status()['run_id']})
                    self.assertEqual(response.status_code,200,response.text);return response.json()
                try:
                    old=wait(lambda s:s.get('tick',0)>=10)['run_id'];command('pause')
                    before=command('agents')['result']['agents']
                    # A later INI edit must not prevent a checkpoint of the active run.
                    ini.write_text(ini.read_text()+'\nnewgen_enabled = true\n')
                    response=client.post('/api/checkpoints');self.assertEqual(response.status_code,200,response.text)
                    ident=response.json()['id']
                    # Corrupt-file rejection must preserve the current run and INI.
                    file=root/'checkpoints'/f'{ident}.popx';original=file.read_bytes();file.write_bytes(original[:-1]+bytes([original[-1]^1]))
                    r=client.post(f'/api/checkpoints/{ident}/load',json={'confirm':ident});self.assertEqual(r.status_code,409,r.text)
                    self.assertEqual(status()['run_id'],old);file.write_bytes(original)
                    r=client.post(f'/api/checkpoints/{ident}/load',json={'confirm':ident});self.assertEqual(r.status_code,200,r.text)
                    new=wait(lambda s:s.get('run_id')!=old and s.get('state')=='running');self.assertTrue(new['config_locked'])
                    self.assertEqual(client.put('/api/config',json={}).status_code,409)
                    command('pause')
                    after=command('agents')['result']['agents']
                    self.assertEqual({a['pop_id'] for a in before},{a['pop_id'] for a in after})
                    # Simulate old historical volume above the 10 MB test limit.
                    with closing(sqlite3.connect(db)) as con:
                        con.execute('INSERT INTO diagnostics(run,data) VALUES(?,?)',(old,'x'*12_000_000));con.commit()
                    rotated=wait(lambda s:s.get('run_id') not in (old,new['run_id']) and s.get('state')=='running',45)
                    time.sleep(.2)
                    runs=client.get('/api/runs').json();self.assertEqual(len(runs),1,runs)
                    self.assertEqual(runs[0]['id'],rotated['run_id'])
                    self.assertLess(db.stat().st_size,10_000_000)
                    self.assertFalse(any(root.glob('history.sqlite.rotation-new*')))
                    self.assertLess(sum(path.stat().st_size for path in root.glob('history.sqlite*')),
                                    10_000_000)
                    checkpoints=client.get('/api/checkpoints').json();self.assertEqual(checkpoints['operation']['stage'],'done',checkpoints)
                    self.assertEqual(len(checkpoints['checkpoints']),2)
                    with closing(sqlite3.connect(db)) as con:
                        sample=json.loads(con.execute('SELECT data FROM samples WHERE run=? ORDER BY tick LIMIT 1',(rotated['run_id'],)).fetchone()[0])
                    self.assertEqual(sample['tick'],0);self.assertEqual(sample['resource_ledger']['residual'],[0]*6)
                    self.assertLess(abs(sample['energy_ledger']['residual']),1e-5)
                    # Manual full cleanup uses the same verified rotation before the size limit.
                    response=client.post('/api/storage/clean',json={'confirm':'clean_history'})
                    self.assertEqual(response.status_code,200,response.text)
                    cleaned=response.json()['run_id'];self.assertNotEqual(cleaned,rotated['run_id'])
                    self.assertEqual([r['id'] for r in client.get('/api/runs').json()],[cleaned])
                    self.assertEqual(client.post('/api/storage/clean',json={}).status_code,422)
                    # A connected run can be stopped and removed; its command replies must still work.
                    response=client.request('DELETE',f'/api/runs/{cleaned}',json={'confirm_run':cleaned})
                    self.assertEqual(response.status_code,202,response.text)
                    deadline=time.monotonic()+10
                    while time.monotonic()<deadline:
                        removal=client.get(f'/api/storage/removals/{cleaned}').json()
                        if removal['state']=='deleted':break
                        time.sleep(.05)
                    self.assertEqual(removal['state'],'deleted',removal)
                    self.assertEqual(client.get('/api/runs').json(),[])
                    command('exit');proc.wait(timeout=5)
                finally:
                    if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
                    proc.stderr.close()


if __name__=='__main__':unittest.main()
