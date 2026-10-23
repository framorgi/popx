"""Bounded headless runtime comparison; isolated launcher, SQLite history and CSV timings."""
import argparse
import csv
import json
from pathlib import Path
import socket
import sqlite3
import subprocess
import time
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parent.parent


def free_port(kind):
    with socket.socket(socket.AF_INET, kind) as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    folder = args.directory.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    database = folder / 'history.sqlite'
    if database.exists():
        raise SystemExit('Use a fresh benchmark directory')
    config = folder / 'popx.ini'
    config.write_text((ROOT / 'config/popx.ini').read_text() + '\nmax_duration_minutes=0\n')
    http, udp = free_port(socket.SOCK_STREAM), free_port(socket.SOCK_DGRAM)
    base = f'http://127.0.0.1:{http}'

    def get(path):
        return json.load(urlopen(base + path, timeout=3))

    run = None

    def command(name, arguments=None):
        request = Request(base + '/api/commands', data=json.dumps(
            {'command': name, 'arguments': arguments or {}, 'run_id': run}).encode(),
            headers={'Content-Type': 'application/json'})
        reply = json.load(urlopen(request, timeout=5))
        assert reply['ok'], reply
        return reply['status']

    with (folder / 'launcher.log').open('wb') as log:
        proc = subprocess.Popen(['bash', str(ROOT / 'scripts/run_popx.sh'), '--headless', '--speed', '2',
                                 '--config', str(config), '--db', str(database), '--http-port', str(http),
                                 '--udp-port', str(udp)], cwd=ROOT, stdin=subprocess.DEVNULL,
                                stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                if proc.poll() is not None:
                    raise RuntimeError('Launcher exited; inspect launcher.log')
                try:
                    status = get('/api/status')
                    if status.get('connected') and status.get('tick', 0) > 0:
                        run = status['run_id']
                        break
                except OSError:
                    pass
                time.sleep(.1)
            else:
                raise RuntimeError('Startup deadline exceeded')
            rows = []
            command('pause')
            for speed in (2, 0, 2):
                status = command('speed', {'value': speed})
                assert status['speed'] == speed and not status['gui']
                start = command('resume')['tick']
                begin = time.monotonic()
                time.sleep(1.5)
                status = command('pause')
                elapsed = time.monotonic() - begin
                ticks = status['tick'] - start
                assert ticks > 0 and status['transport']['dropped_messages'] == 0
                assert command('agents')['tick'] == status['tick'], 'Paused run advanced'
                rows.append(dict(run=run, speed=speed, ticks=ticks, seconds=elapsed,
                                 ticks_per_second=ticks / elapsed,
                                 queued=status['transport']['queued']))
            command('stop')
            command('exit')
            proc.wait(timeout=8)
            assert proc.returncode == 0
            with sqlite3.connect(f'file:{database}?mode=ro', uri=True) as db:
                state, raw = db.execute('SELECT state,summary FROM runs WHERE id=?', (run,)).fetchone()
                final = json.loads(raw)
                assert state == 'finished'
                assert db.execute('SELECT COUNT(*) FROM gaps WHERE run=?', (run,)).fetchone()[0] == 0
                reproduction = final['reproduction']
                assert reproduction['attempts'] == reproduction['births'] + reproduction['no_space']
                assert reproduction['births'] == final['counts']['natural_births']
                assert final['learning']['n'] == final['alive']
                assert final['totals']['learning_updates'] > 0
                assert all(value == 0 for value in final['resource_ledger']['residual'])
                assert abs(final['energy_ledger']['residual']) < 1e-3
            with (folder / 'timings.csv').open('w', newline='') as output:
                writer = csv.DictWriter(output, fieldnames=list(rows[0]))
                writer.writeheader()
                writer.writerows(rows)
            for row in rows:
                print(f"speed={row['speed']}: {row['ticks_per_second']:.1f} ticks/s; {row['ticks']} ticks; "
                      f"queued={row['queued']}")
            print('Same evolving population; descriptive timing, not a biological efficacy comparison.')
            print('History and provenance:', database)
        finally:
            if proc.poll() is None:
                try:
                    command('exit')
                    proc.wait(timeout=8)
                except (OSError, subprocess.TimeoutExpired):
                    proc.terminate()
                    proc.wait(timeout=8)


if __name__ == '__main__':
    main()
