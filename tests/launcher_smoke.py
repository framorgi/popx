"""Launcher works from another CWD and owns/cleans up both processes."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sqlite3
import tempfile
import time
from urllib.request import urlopen,Request
ROOT=Path(__file__).resolve().parent.parent

def port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1',0));return sock.getsockname()[1]

def main():
    with tempfile.TemporaryDirectory(prefix='popx-launcher-') as temp:
        folder=Path(temp);(folder/'test.ini').write_text((ROOT/'config/popx.ini').read_text()+'\nmax_duration_minutes=0\nphy_alpha_min=0\nphy_alpha_max=0\nphy_init_temperature=37\n')
        http,udp=port(),port()
        proc=subprocess.Popen(['bash',str(ROOT/'scripts/run_popx.sh'),'--speed','0','--config','test.ini','--db','test.sqlite','--http-port',str(http),'--udp-port',str(udp)],cwd=folder,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        stream = None
        try:
            base=f'http://127.0.0.1:{http}'
            for _ in range(150):
                try:
                    status=json.load(urlopen(base+'/api/status',timeout=.3))
                    if status.get('connected'):break
                except OSError:pass
                if proc.poll() is not None:raise RuntimeError(proc.communicate())
                time.sleep(.1)
            else:raise RuntimeError('Launcher not ready')
            assert status['speed']==0 and not status['gui']
            health=json.load(urlopen(base+'/api/health'))
            # Reproduce an open dashboard tab: Exit must finish even with a persistent EventSource.
            stream = urlopen(base+'/api/stream', timeout=15)
            request=Request(base+'/api/commands',data=json.dumps({'command':'exit','run_id':status['run_id']}).encode(),headers={'Content-Type':'application/json'})
            reply=json.load(urlopen(request,timeout=5))
            proc.wait(timeout=8)
            assert proc.returncode==0,proc.communicate()
            assert (folder/'test.sqlite').exists()
            with sqlite3.connect(folder/'test.sqlite') as db:
                reason,summary=db.execute('SELECT reason,summary FROM runs WHERE id=?',(status['run_id'],)).fetchone()
            assert reason=='exit',reason
            assert json.loads(summary)['tick']==reply['status']['tick'],'Tick advanced after Exit acknowledgment'
            try:os.kill(health['pid'],0)
            except ProcessLookupError:pass
            else:raise AssertionError('Backend child still alive')
            print('PASS: launcher from external CWD, readiness, Exit with open SSE, process cleanup')
        finally:
            if stream is not None:stream.close()
            if proc.poll() is None:proc.terminate();proc.wait(timeout=8)
            proc.stdout.close();proc.stderr.close()

if __name__=='__main__':main()
