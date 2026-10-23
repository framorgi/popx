#!/usr/bin/env python3
"""One-time, explicitly partial migration from the paused pre-checkpoint inspector capture.
No attempt is made to infer current cell contents from old aggregate telemetry.
"""
import argparse
import json
from pathlib import Path
import re
import sqlite3
import struct
import subprocess


def cbor(value):
    def size(kind, n):
        if n < 24: return bytes([kind * 32 + n])
        for code, fmt, maximum in [(24, '>B', 256), (25, '>H', 65536), (26, '>I', 2**32), (27, '>Q', 2**64)]:
            if n < maximum: return bytes([kind * 32 + code]) + struct.pack(fmt, n)
        raise ValueError('Integer too large')
    if value is None: return b'\xf6'
    if isinstance(value, bool): return b'\xf5' if value else b'\xf4'
    if isinstance(value, int): return size(0, value) if value >= 0 else size(1, -1 - value)
    if isinstance(value, float): return b'\xfb' + struct.pack('>d', value)
    if isinstance(value, str):
        raw = value.encode(); return size(3, len(raw)) + raw
    if isinstance(value, list): return size(4, len(value)) + b''.join(map(cbor, value))
    if isinstance(value, dict): return size(5, len(value)) + b''.join(cbor(k)+cbor(v) for k, v in value.items())
    raise TypeError(type(value))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--binary', type=Path, default=Path('build-release/src/popx'))
    parser.add_argument('--limit-gb', type=float, default=10)
    args = parser.parse_args()
    db = sqlite3.connect(args.source.resolve().as_uri()+'?mode=ro', uri=True)
    status = json.loads(db.execute("SELECT value FROM metadata WHERE key='status'").fetchone()[0])
    pops = [json.loads(r[0]) for r in db.execute('SELECT data FROM agents ORDER BY rowid')]
    raw = re.sub(r'(?m)^database_limit_gb\s*=.*$', f'database_limit_gb = {args.limit_gb:g}', status['metadata']['ini'])
    raw += '\n# Checkpoint resolved paths\n' + ''.join(f"{key} = {status['metadata']['config'][key]}\n" for key in ('nnets_dir', 'log_dir', 'image_dir'))
    ini = args.destination.with_suffix('.ini')
    with ini.open('x') as f: f.write(raw)
    try:
        config = json.loads(subprocess.check_output([str(args.binary.resolve()), '--validate-config', str(ini.resolve())]))
    finally: ini.unlink()
    terrain = []
    for row in db.execute('SELECT data FROM environment_initial ORDER BY offset'):
        terrain.extend(c['elevation'] for c in json.loads(row[0]))
    document = dict(version=1, model='popx_state_v3', partial=True, ini=raw, config=config,
        source_run=status['run_id'], source_tick=status['tick'], speed=status['speed'],
        next_id=max(int(p['pop_id'][4:]) for p in pops)+1, width=config['map_size'], height=config['map_size'],
        pops=pops, terrain=terrain,
        reconstruction='Current world resources, pheromones, temperature RBFs, neural eligibility, respiration credit and reproduction FIFO reconstructed; original terrain retained. Individual neural weights, birth genomes, known physical state, positions and baseline retained. Global RNG continuation is not deterministic.')
    body = cbor(document)
    checksum = 14695981039346656037
    for byte in body: checksum = ((checksum ^ byte) * 1099511628211) % 2**64
    with args.destination.open('xb') as output:
        output.write(f'POPXCP1\n{checksum:x}\n'.encode()+body)
    print(args.destination, len(pops), 'Pop; partial migration; limit', config['database_limit_gb'])


if __name__ == '__main__': main()
