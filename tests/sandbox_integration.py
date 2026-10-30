"""Exercise actual C++ commands through the backend; isolated data and bounded runs."""
import json
from pathlib import Path
import subprocess
import tempfile
import time
import unittest
from fastapi.testclient import TestClient
from dashboard.server import ROOT, create_app
from tests.dashboard_integration import free_port


class SandboxIntegration(unittest.TestCase):
    def test_runtime_step_checkpoint_and_interventions(self):
        with tempfile.TemporaryDirectory(prefix='popx-sandbox-') as tmp:
            root=Path(tmp);ini=root/'popx.ini';port=free_port()
            ini.write_text((ROOT/'config/popx.ini').read_text()+'\nmap_size=24\npopulation=8\nmax_population_allowed=20\nsource_move_interval_ticks=100000\nphy_alpha_min=0\nphy_alpha_max=0\nphy_init_temperature=37\n')
            with TestClient(create_app(root/'history.sqlite',ini,ROOT/'build-release/src/popx',port)) as client:
                proc=subprocess.Popen([str(ROOT/'build-release/src/popx'),'--headless','--config',str(ini),'--udp-port',str(port)],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
                def status():return client.get('/api/status').json()
                def wait(predicate):
                    end=time.monotonic()+15
                    while time.monotonic()<end:
                        s=status()
                        if predicate(s):return s
                        if proc.poll() is not None:raise AssertionError(proc.stderr.read().decode())
                        time.sleep(.02)
                    self.fail(str(s))
                def command(name,args=None,ok=True):
                    r=client.post('/api/commands',json={'command':name,'arguments':args or {},'run_id':status()['run_id']})
                    if ok:self.assertEqual(r.status_code,200,r.text)
                    else:self.assertGreaterEqual(r.status_code,400,r.text)
                    return r.json()
                try:
                    wait(lambda s:s.get('connected'))
                    self.assertIn('window.POPX_DESIGN=',client.get('/api/design.js').text)
                    self.assertEqual(client.get('/static/sandbox.html').status_code,200)
                    command('sandbox_spawn',{'x':0,'y':0},False) # Must pause first.
                    command('pause');s=wait(lambda s:s['state']=='paused');start=s['tick']
                    command('step',{'ticks':7});s=wait(lambda s:s['tick']==start+7 and not s['step_remaining'])
                    self.assertEqual(s['state'],'paused');time.sleep(.1);self.assertEqual(status()['tick'],start+7)
                    command('step',{'ticks':1.5},False);command('step',{'ticks':10001},False)
                    before=s['runtime']
                    command('runtime_apply',{'learning_rate':.2,'map_size':40},False)
                    self.assertEqual(status()['runtime'],before)
                    command('runtime_apply',{'learning_rate':-.1},False)
                    command('runtime_apply',{'learning_rate':.02,'reward_energy_weight':.8})
                    command('runtime_apply',{'climate_rotation_speed':0})
                    s=wait(lambda s:s.get('runtime',{}).get('learning_rate')==.02 and
                           s.get('runtime',{}).get('climate_rotation_speed')==0)
                    self.assertEqual(s['runtime']['reward_energy_weight'],.8)
                    self.assertEqual(s['runtime']['climate_rotation_speed'],0)
                    command('appearance',{'theme':'slate','agent_scale':1.1});self.assertEqual(json.loads((root/'appearance.json').read_text())['theme'],'slate')
                    command('appearance',{'theme':'invalid'},False)
                    agents=command('agents')['result']['agents'];occupied={tuple(p['position']) for p in agents}
                    born=None
                    for y in range(24):
                        for x in range(24):
                            if (x,y) in occupied:continue
                            r=client.post('/api/commands',json={'command':'sandbox_spawn','arguments':{'x':x,'y':y},'run_id':status()['run_id']})
                            if r.status_code==200:born=r.json()['result'];break
                        if born:break
                    self.assertIsNotNone(born);self.assertTrue(born['sandbox_spawn']);self.assertTrue(born['forced'])
                    command('sandbox_spawn',{'x':born['position'][0],'y':born['position'][1]},False)
                    command('sandbox_cell',{'x':-1,'y':0,'field':'temperature','value':30},False)
                    command('sandbox_cell',{'x':0,'y':0,'field':'temperature','value':30})
                    command('sandbox_cell',{'x':0,'y':0,'field':'temperature','erase':True})
                    command('sandbox_cell',{'x':0,'y':0,'field':'temperature','erase':True},False)
                    command('sandbox_cell',{'x':0,'y':0,'field':'food','value':10})
                    command('sandbox_cell',{'x':0,'y':0,'field':'danger','value':10})
                    command('sandbox_cell',{'x':0,'y':0,'field':'danger','erase':True})
                    for field in ('glucose','water','oxygen','co2','lipids','nitrogen','calcium'):
                        command('sandbox_cell',{'x':0,'y':0,'field':field,'value':1})
                        command('sandbox_cell',{'x':0,'y':0,'field':field,'erase':True})
                    source=None
                    for y in range(24):
                        for x in range(24):
                            r=client.post('/api/commands',json={'command':'sandbox_spawn','arguments':{'x':x,'y':y,'kind':'source'},'run_id':status()['run_id']})
                            if r.status_code==200:source=(x,y);break
                        if source:break
                    self.assertIsNotNone(source)
                    command('sandbox_remove',{'x':source[0],'y':source[1],'kind':'pop'},False)
                    command('sandbox_remove',{'x':source[0],'y':source[1],'kind':'source'})
                    command('sandbox_spawn',{'x':source[0],'y':source[1],'kind':'source'})
                    p=command('inspect',{'pop_id':born['pop_id']})['result'];blocks=p['brain']['connections'];self.assertTrue(blocks)
                    block=next(b for b in blocks if b['entries']);edge=block['entries'][0]
                    args={'pop_id':born['pop_id'],'from_layer':block['from_layer'],'to_layer':block['to_layer'],'row':edge['row'],'col':edge['col'],'value':.12345}
                    command('sandbox_weight',args)
                    edited=command('inspect',{'pop_id':born['pop_id']})['result']
                    values=[e['value'] for b in edited['brain']['connections'] if b['from_layer']==args['from_layer'] and b['to_layer']==args['to_layer'] for e in b['entries'] if e['row']==args['row'] and e['col']==args['col']]
                    self.assertAlmostEqual(values[0],.12345,places=6)
                    command('sandbox_weight',{**args,'value':5},False)
                    cp=client.post('/api/checkpoints',json={});self.assertEqual(cp.status_code,200,cp.text)
                    checkpoint=cp.json();identifier=checkpoint['id']
                    loaded=client.post(f'/api/checkpoints/{identifier}/load',json={'confirm':identifier});self.assertEqual(loaded.status_code,200,loaded.text)
                    s=wait(lambda s:s.get('config_locked') and s['tick']==0)
                    self.assertEqual(s['runtime']['learning_rate'],.02);self.assertEqual(s['runtime']['reward_energy_weight'],.8)
                    self.assertEqual(s['runtime']['climate_rotation_speed'],0)
                    command('pause');wait(lambda s:s['state']=='paused')
                    command('sandbox_remove',{'x':source[0],'y':source[1],'kind':'source'})
                    restored=command('inspect',{'pop_id':born['pop_id']})['result'];self.assertEqual(restored['brain']['connections'],edited['brain']['connections'])
                    command('sandbox_remove',{'x':born['position'][0],'y':born['position'][1],'kind':'pop'})
                    command('inspect',{'pop_id':born['pop_id']},False)
                    # Finished state blocks intervention; empty/oversized tick requests rejected.
                    command('stop');command('runtime_apply',{'learning_rate':.1},False)
                    command('exit');proc.wait(timeout=5);self.assertEqual(proc.returncode,0)
                finally:
                    if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
                    proc.stderr.close()

if __name__=='__main__':unittest.main()
