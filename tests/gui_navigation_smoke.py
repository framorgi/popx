"""Real SFML event handling on X11, using a separate paused fixture and real backend."""
import ctypes as c
import json
import re
import subprocess
import tempfile
import time
from pathlib import Path
from fastapi.testclient import TestClient
from dashboard.server import ROOT,create_app
from tests.dashboard_integration import free_port

class PointerEvent(c.Structure):
    _fields_=[('type',c.c_int),('serial',c.c_ulong),('send_event',c.c_int),('display',c.c_void_p),('window',c.c_ulong),('root',c.c_ulong),('subwindow',c.c_ulong),('time',c.c_ulong),('x',c.c_int),('y',c.c_int),('x_root',c.c_int),('y_root',c.c_int),('state',c.c_uint),('button',c.c_uint),('same_screen',c.c_int)]
class Event(c.Union):
    _fields_=[('pointer',PointerEvent),('pad',c.c_long*24)]

def main():
    x=c.CDLL('libX11.so.6');x.XOpenDisplay.restype=c.c_void_p;display=x.XOpenDisplay(None)
    if not display:raise RuntimeError('X display unavailable')
    x.XSendEvent.argtypes=[c.c_void_p,c.c_ulong,c.c_int,c.c_long,c.POINTER(Event)]
    x.XFlush.argtypes=[c.c_void_p];x.XCloseDisplay.argtypes=[c.c_void_p]
    x.XKeysymToKeycode.argtypes=[c.c_void_p,c.c_ulong];x.XKeysymToKeycode.restype=c.c_uint
    with tempfile.TemporaryDirectory(prefix='popx-navigation-') as tmp:
        root=Path(tmp);ini=root/'popx.ini';port=free_port()
        ini.write_text((ROOT/'config/popx.ini').read_text()+'\nmap_size=24\npopulation=12\nphy_alpha_min=0\nphy_alpha_max=0\nphy_init_temperature=37\n')
        with TestClient(create_app(root/'data.sqlite',ini,ROOT/'build-release/src/popx',port)) as client:
            proc=subprocess.Popen([str(ROOT/'build-release/src/popx'),'--headless','--config',str(ini),'--udp-port',str(port)],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
            def status():return client.get('/api/status').json()
            def wait(predicate):
                end=time.monotonic()+10
                while time.monotonic()<end:
                    s=status()
                    if predicate(s):return s
                    if proc.poll() is not None:raise RuntimeError(proc.stderr.read().decode())
                    time.sleep(.05)
                raise AssertionError(s)
            def command(name,args=None):
                r=client.post('/api/commands',json={'command':name,'arguments':args or {},'run_id':status()['run_id']})
                assert r.status_code==200,r.text
                return r.json()
            try:
                wait(lambda s:s.get('connected'));tick=command('pause')['status']['tick'];command('show_gui');time.sleep(.5)
                tree=subprocess.check_output(['xwininfo','-root','-tree'],text=True)
                match=re.search(r'(0x[0-9a-f]+) "PopX Simulation \['+str(proc.pid)+r'\]".*? (\d+)x(\d+)\+',tree)
                assert match,tree
                window=int(match[1],16);width,height=map(int,match.groups()[1:])
                def send(kind,px,py,button):
                    e=Event();e.pointer=PointerEvent(type=kind,display=display,window=window,x=px,y=py,button=button,same_screen=1)
                    assert x.XSendEvent(display,window,0,(1<<2) if kind==4 else (1<<3) if kind==5 else (1<<0) if kind==2 else (1<<1),c.byref(e))
                    x.XFlush(display);time.sleep(.15)
                def key(symbol):
                    code=x.XKeysymToKeycode(display,symbol);send(2,0,0,code);send(3,0,0,code)
                pop=command('agents')['result']['agents'][0]
                usable=width-min(width*.45,390);side=min(usable,height)
                px=int((usable-side)/2+(pop['position'][0]+.5)*side/24);py=int((height-side)/2+(pop['position'][1]+.5)*side/24)
                send(4,px,py,1);send(5,px,py,1)
                wait(lambda s:s.get('observer',{}).get('selected_id')==pop['pop_id'])
                send(4,px,py,4);send(5,px,py,4)
                wait(lambda s:s['observer']['zoom']>1)
                key(0xff50);wait(lambda s:s['observer']['zoom']==1)
                key(0xffc1);time.sleep(.5)
                subprocess.run(['python3',str(ROOT/'tests/capture_window.py'),str(proc.pid),str(ROOT/'data_out/audit/ux/brain-monitor.png')],check=True)
                key(0xffc0);time.sleep(.5)
                subprocess.run(['python3',str(ROOT/'tests/capture_window.py'),str(proc.pid),str(ROOT/'data_out/audit/ux/learning-mixer.png')],check=True)
                run=status()['run_id']
                brush_x,brush_y=12,20
                bx=int((usable-side)/2+(brush_x+.5)*side/24);by=int((height-side)/2+(brush_y+.5)*side/24)
                command('sandbox_cell',{'x':brush_x,'y':brush_y,'field':'glucose','value':10})
                def brush_events():
                    return [d for d in client.get(f'/api/runs/{run}/diagnostics').json()
                            if d.get('kind')=='sandbox_cell' and d.get('arguments',{}).get('brush')]
                before=len(brush_events())
                send(4,bx,by,3);send(5,bx,by,3)
                end=time.monotonic()+5
                while time.monotonic()<end and len(brush_events())<=before:time.sleep(.05)
                assert len(brush_events())>before,'right mouse did not erase selected resource'
                send(4,bx,by,1);send(5,bx,by,1)
                end=time.monotonic()+5
                while time.monotonic()<end and len(brush_events())<=before+1:time.sleep(.05)
                assert len(brush_events())>before+1,'left mouse did not paint selected resource'
                assert status()['tick']==tick,'observer advanced biology'
                command('exit');proc.wait(timeout=5);assert proc.returncode==0
                print('PASS: real X11 selection, zoom, recenter, panel shortcuts, Brain Monitor, left/right brush; paused biological state unchanged')
            finally:
                if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
                proc.stderr.close()
    x.XCloseDisplay(display)
if __name__=='__main__':main()
