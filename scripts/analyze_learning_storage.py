#!/usr/bin/env python3
"""Read-only live learning/storage audit. Prints text; never deletes or rewrites history."""
import argparse
import json
import shutil
import sqlite3
import statistics
import time
import urllib.request
import zlib
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8000')
    parser.add_argument('--db', type=Path, default=Path('data_out/popx.sqlite'))
    parser.add_argument('--interval', type=float, default=10)
    args = parser.parse_args()
    if not 0 <= args.interval <= 30:
        parser.error('interval must be between 0 and 30 seconds')

    def api(path, data=None):
        request = urllib.request.Request(args.url+path, data=json.dumps(data).encode() if data else None,
                                         headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response)

    status = api('/api/status')
    run = status['run_id']
    print('Run:', run, 'model:', status.get('metadata', {}).get('model'), flush=True)
    db = sqlite3.connect(args.db.resolve().as_uri()+'?mode=ro', uri=True)
    db.execute('PRAGMA query_only=ON')
    start = time.monotonic()
    page_size = db.execute('PRAGMA page_size').fetchone()[0]
    pages_before = db.execute('PRAGMA page_count').fetchone()[0]
    for i in range(3):
        snapshot = api('/api/commands', {'command': 'agents', 'run_id': run})['result']
        agents = snapshot['agents']
        if not agents:
            print('No living agents'); break
        signals = [a['learning']['signal'] for a in agents]
        inferred = [.06*abs(a['learning']['signal'])*a['learning']['mean_abs_trace'] for a in agents]
        rate = status.get('metadata', {}).get('config', {}).get('learning_rate', .06)
        inferred = [v*rate/.06 for v in inferred]
        delta = statistics.fmean(a['learning']['mean_abs_delta'] for a in agents)
        print('Tick', snapshot['tick'], 'n', len(agents), 'signal_mean', statistics.fmean(signals),
              'signal_abs', statistics.fmean(map(abs, signals)), 'signal_std', statistics.pstdev(signals),
              'positive_fraction', sum(s>0 for s in signals)/len(signals),
              'near_zero_fraction', sum(abs(s)<1e-4 for s in signals)/len(signals),
              'plasticity_inferred', statistics.fmean(inferred), 'actual_weight_delta', delta,
              'plasticity_over_delta', statistics.fmean(inferred)/delta if delta else None, flush=True)
        if i < 2:
            time.sleep(args.interval)
    elapsed = time.monotonic()-start
    pages_after = db.execute('PRAGMA page_count').fetchone()[0]
    growth = max(0, pages_after-pages_before)*page_size/elapsed
    free = shutil.disk_usage(args.db.parent).free
    print('Disk: DB_bytes', pages_after*page_size, 'free_bytes', free,
          'observation_seconds', elapsed, 'allocated_GiB_per_day', growth*86400/2**30,
          'days_at_observed_rate', free/growth/86400 if growth else None, flush=True)
    print('Rates are a short observation, not a capacity guarantee; WAL and reused pages affect allocation.')
    counts = db.execute('SELECT type,count(*),sum(length(data)) FROM events WHERE run=? GROUP BY type', (run,)).fetchall()
    samples = db.execute('SELECT count(*),sum(length(data)) FROM samples WHERE run=?', (run,)).fetchone()
    print('Run event count/payload bytes:', counts, 'samples:', samples, flush=True)
    raw = [r[0].encode() for r in db.execute('SELECT data FROM events WHERE run=? ORDER BY id DESC LIMIT 1000', (run,))]
    print('Lossless zlib level 6 / raw bytes (last 1000 events, separate records):',
          sum(len(zlib.compress(b, 6)) for b in raw)/sum(map(len, raw)) if raw else None)
    print('SQLite freelist pages:', db.execute('PRAGMA freelist_count').fetchone()[0])
    db.close()


if __name__ == '__main__':
    main()
