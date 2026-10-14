"""Run diagnostic producers against a temporary local receiver, persisting only SQLite."""
import argparse
import asyncio
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import tempfile
from .server import Store, Receiver, ROOT, compact


def provenance():
    from .provenance import collect
    result = collect()
    result['ini'] = (ROOT/'config/popx.ini').read_text()
    return result


from scripts.audit_data import read_report


async def run(kind, database, mode, corpus=None):
    store=Store(database)
    existing_runs={r[0] for r in store.db.execute("SELECT id FROM runs")}
    receiver=Receiver(store)
    transport,_=await asyncio.get_running_loop().create_datagram_endpoint(lambda:receiver,local_addr=('127.0.0.1',0))
    port=transport.get_extra_info('sockname')[1]
    objects=['brain','cell','config','console_logger','gene','genome','grid_world','pop','pops_manager','random_utility','simulator','stats','udp_communicator']
    source={'audit':'popx_audit','thermal':'popx_thermal_comparison','strategy':'popx_strategy_comparison'}[kind]
    try:
        with tempfile.TemporaryDirectory(prefix='popx-diagnostic-') as temp:
            binary=Path(temp)/source
            command=['clang++','-std=c++23','-O2','-Isrc','-Ibuild-release/_deps/eigen3-src','-Ibuild-release/_deps/nlohmann_json-src/include',
                     f'tools/{source}.cpp',*[f'build-release/src/CMakeFiles/popx.dir/{obj}.cpp.o' for obj in objects],'-ltbb','-lstdc++fs','-pthread','-o',str(binary)]
            proc=await asyncio.create_subprocess_exec(*command,cwd=ROOT)
            if await proc.wait():
                raise RuntimeError('Diagnostic build failed')
            metadata=provenance()
            metadata['harness_sha256']=hashlib.sha256(binary.read_bytes()).hexdigest()
            stdin=None
            args=[str(ROOT/'config/popx.ini'),'receiver',mode]
            if kind=='strategy':
                stdin=compact(read_report(corpus,'audit')).encode()
                args=['-','receiver',mode]
            env={**os.environ,'POPX_UDP_PORT':str(port)}
            proc=await asyncio.create_subprocess_exec(str(binary),*args,cwd=ROOT,env=env,stdin=asyncio.subprocess.PIPE if stdin else None,stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
            out,err=await proc.communicate(stdin)
            if out:
                # Console summary is not a data file.
                print(out.decode()[-4000:])
            if err: print(err.decode())
            rows=store.db.execute('SELECT id,metadata FROM runs').fetchall()
            with store.db:
                for run_id,existing in rows:
                    data=json.loads(existing)
                    if run_id not in existing_runs:
                        data['provenance']=metadata
                        store.db.execute("UPDATE runs SET metadata=?,state='finished',reason=CASE WHEN reason='' THEN 'diagnostic_completed' ELSE reason END WHERE id=?",(compact(data),run_id))
            if not any(row[0] not in existing_runs for row in store.db.execute('SELECT DISTINCT run FROM diagnostics')):
                raise RuntimeError('No committed diagnostic report')
            if proc.returncode:
                raise RuntimeError(f'Diagnostic failed: {proc.returncode}')
            print(f'Diagnostic persisted: {database}')
    finally:
        transport.close()
        store.close()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind',choices=['audit','thermal','strategy'])
    parser.add_argument('database')
    parser.add_argument('mode',nargs='?',default='2000')
    parser.add_argument('--corpus')
    args=parser.parse_args()
    if args.database.endswith('.json'):
        parser.error('Output is SQLite: use a .sqlite path, not .json')
    asyncio.run(run(args.kind,args.database,args.mode,args.corpus))

if __name__=='__main__': main()
