"""Compare renderers serially from one checkpoint, in isolated SQLite stores (2000 ticks, 30 s ceiling).

Biological RNG is not deterministic; this is a performance probe, not an evolutionary experiment.
Run with the production simulator paused/closed to avoid contention. No statistical JSON export.
"""
import argparse
import os
from pathlib import Path
import subprocess
import time
from fastapi.testclient import TestClient
from dashboard.server import ROOT,create_app
from tests.dashboard_integration import free_port


def probe(binary,checkpoint,folder):
    folder.mkdir(parents=True,exist_ok=True);ini=folder/'popx.ini';ini.write_text((ROOT/'config/popx.ini').read_text());port=free_port()
    with TestClient(create_app(folder/'history.sqlite',ini,binary,port)) as client:
        proc=subprocess.Popen([str(binary),'--config',str(ini),'--checkpoint',str(checkpoint),'--udp-port',str(port),'--headless'],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
        def status():return client.get('/api/status').json()
        def command(name,args=None):
            r=client.post('/api/commands',json={'command':name,'arguments':args or {},'run_id':status()['run_id']});assert r.status_code==200,r.text;return r.json()
        try:
            end=time.monotonic()+15
            while not status().get('connected'):
                if proc.poll() is not None:raise RuntimeError(proc.stderr.read().decode())
                if time.monotonic()>end:raise TimeoutError('startup')
                time.sleep(.05)
            assert status()['tick']==0
            command('show_gui');time.sleep(.2)
            begin=time.monotonic();command('step',{'ticks':2000})
            while True:
                s=status()
                if s.get('tick')==2000 and s.get('step_remaining')==0:break
                if time.monotonic()-begin>30:raise TimeoutError('30 second probe exceeded')
                time.sleep(.02)
            elapsed=time.monotonic()-begin
            rss=int(Path(f'/proc/{proc.pid}/status').read_text().split('VmRSS:')[1].split()[0])
            print(f'{folder.name}: 2000 ticks in {elapsed:.3f}s; alive={s["alive"]}; tick_us={s["performance"]["avg_tick_us"]:.1f}; frame_us={s["performance"]["avg_frame_us"]:.1f}; fps={s["performance"]["fps"]:.2f}; RSS_KiB={rss}',flush=True)
            subprocess.run(['python3',str(ROOT/'tests/capture_window.py'),str(proc.pid),str(folder/'gui.png')],check=True)
            command('exit');proc.wait(timeout=5);assert proc.returncode==0
        finally:
            if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
            proc.stderr.close()

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('checkpoint',type=Path);p.add_argument('before',type=Path);p.add_argument('after',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
    if not os.environ.get('DISPLAY'):raise SystemExit('DISPLAY required')
    for name,binary in [('before',a.before),('after',a.after)]:probe(binary.resolve(),a.checkpoint.resolve(),a.output.resolve()/name)
