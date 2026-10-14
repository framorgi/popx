"""Launcher works from another CWD and owns/cleans up both processes."""
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
from urllib.request import urlopen,Request
ROOT=Path(__file__).resolve().parent.parent

def port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1',0));return sock.getsockname()[1]

def main():
    with tempfile.TemporaryDirectory(prefix='popx-launcher-') as temp:
        folder=Path(temp);(folder/'test.ini').write_text((ROOT/'config/popx.ini').read_text()+'\nmax_duration_minutes=0.001\n')
        http,udp=port(),port()
        proc=subprocess.Popen(['bash',str(ROOT/'scripts/run_popx.sh'),'--headless','--config','test.ini','--db','test.sqlite','--http-port',str(http),'--udp-port',str(udp)],cwd=folder,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
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
            health=json.load(urlopen(base+'/api/health'))
            request=Request(base+'/api/commands',data=json.dumps({'command':'exit','run_id':status['run_id']}).encode(),headers={'Content-Type':'application/json'})
            assert urlopen(request,timeout=5).status==200
            proc.wait(timeout=8)
            assert proc.returncode==0,proc.communicate()
            assert (folder/'test.sqlite').exists()
            try:os.kill(health['pid'],0)
            except ProcessLookupError:pass
            else:raise AssertionError('Backend child still alive')
            print('PASS: launcher from external CWD, readiness, Exit, process cleanup')
        finally:
            if proc.poll() is None:proc.terminate();proc.wait(timeout=8)
            proc.stdout.close();proc.stderr.close()

if __name__=='__main__':main()
