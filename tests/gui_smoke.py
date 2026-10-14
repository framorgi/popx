"""Optional real-display smoke test: same paused run, GUI/headless CPU and window lifecycle."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
from fastapi.testclient import TestClient
from dashboard.server import ROOT,create_app
from tests.dashboard_integration import free_port


def main():
    if not os.environ.get('DISPLAY'):
        raise SystemExit('DISPLAY unavailable; GUI smoke not run')
    with tempfile.TemporaryDirectory(prefix='popx-gui-') as temp:
        folder=Path(temp);config=folder/'popx.ini'
        config.write_text((ROOT/'config/popx.ini').read_text()+'\nphy_alpha_min=0\nphy_alpha_max=0\nphy_init_temperature=37\n')
        port=free_port()
        with TestClient(create_app(folder/'history.sqlite',config,ROOT/'build-release/src/popx',port)) as client:
            proc=subprocess.Popen([str(ROOT/'build-release/src/popx'),'--headless','--config',str(config),'--udp-port',str(port)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            try:
                for _ in range(150):
                    status=client.get('/api/status').json()
                    if status.get('connected'):break
                    if proc.poll() is not None:raise RuntimeError(proc.communicate())
                    time.sleep(.1)
                else:raise RuntimeError('No simulator status')
                run=status['run_id']
                def command(name):
                    r=client.post('/api/commands',json={'command':name,'run_id':run})
                    if r.status_code!=200:
                        print('process status',proc.poll(),flush=True)
                        if proc.poll() is not None: print(proc.stderr.read().decode(),flush=True)
                        raise RuntimeError(r.text)
                    return r.json()
                tick=command('pause')['status']['tick']
                def cpu():
                    fields=Path(f'/proc/{proc.pid}/stat').read_text().split()
                    return (int(fields[13])+int(fields[14]))/os.sysconf('SC_CLK_TCK')
                before=cpu();time.sleep(2);headless=cpu()-before
                shown=command('show_gui');assert shown['status']['gui'] and shown['status']['tick']==tick
                time.sleep(.5)
                before=cpu();time.sleep(2);visible=cpu()-before
                # Capture only the window created by this child process, if the optional system Pillow is present.
                subprocess.run(['python3',str(ROOT/'tests/capture_window.py'),str(proc.pid),'/tmp/popx-gui.png'],check=True)
                hidden=command('headless');assert not hidden['status']['gui'] and hidden['status']['tick']==tick
                command('show_gui');command('headless');command('resume');time.sleep(.15)
                assert command('pause')['status']['tick']>tick
                command('exit');proc.wait(timeout=5)
                assert proc.returncode==0,proc.stderr.read().decode()
                print(json.dumps({'paused_wall_seconds_each':2,'headless_cpu_seconds':headless,'gui_cpu_seconds':visible,'same_run':run,'screenshot':'/tmp/popx-gui.png'}))
            finally:
                if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
                proc.stdout.close();proc.stderr.close()

if __name__=='__main__':main()
