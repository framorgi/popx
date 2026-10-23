#!/usr/bin/env python3
"""Fixed-prefix, read-only audit of the pre-weathering model; prints text only.

Full initial cells and death overflow prove a lower bound on permanent barriers:
an empty saturated cell cannot be entered or mined remotely in water_v2.
Live inspection is optional and does not persist neural snapshots.
"""
import argparse
import collections
import json
import sqlite3
import urllib.request


def command(run, name, arguments=None):
    request = urllib.request.Request('http://127.0.0.1:8000/api/commands',
        data=json.dumps(dict(run_id=run, command=name, arguments=arguments or {})).encode(),
        headers={'Content-Type': 'application/json'})
    return json.load(urllib.request.urlopen(request, timeout=20))['result']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('database'); parser.add_argument('--run', required=True)
    parser.add_argument('--tick', type=int, required=True); parser.add_argument('--live', action='store_true')
    args = parser.parse_args()
    db = sqlite3.connect(f'file:{args.database}?mode=ro', uri=True)
    metadata = json.loads(db.execute('select metadata from runs where id=?', (args.run,)).fetchone()[0])
    print('RUN', args.run, 'MODEL', metadata.get('model'), 'CUTOFF', args.tick, flush=True)
    if metadata.get('model') != 'reserve_regulated_water_v2':
        raise SystemExit('Permanent-barrier inference is valid only for water_v2')
    print('PROVENANCE', metadata.get('provenance', {}).get('binary_sha256'))
    last = None
    for endpoint in [0, 4690000, 8000000, 12000000, 16000000, args.tick]:
        row = db.execute('select tick,data from samples where run=? and tick<=? order by tick desc limit 1',
                         (args.run, endpoint)).fetchone()
        if not row: continue
        tick, raw = row; data = json.loads(raw)
        print('SAMPLE', tick, 'alive', data['alive'], 'counts', data['counts'],
              'world', data['world_resources'], 'water_total', data['resource_ledger']['final'][1],
              'energy', data['averages']['energy'], 'moves', data['totals']['movement_successes'],
              'attempts', data['totals']['movement_attempts'])
        last = data
    print('LEDGERS', last['resource_ledger'], last['energy_ledger'])
    print('GAPS', db.execute('select count(*) from gaps where run=? and tick<=?', (args.run, args.tick)).fetchone()[0])
    cells = []
    for (raw,) in db.execute('select data from environment_parts where run=? order by offset', (args.run,)):
        cells.extend(json.loads(raw))
    width = metadata['config']['map_size']
    if len(cells) != width * width:
        raise SystemExit('Unexpected map dimensions: do not infer barriers')
    saturated = {(i % width, i // width) for i,c in enumerate(cells) if c['resources'][2] == 500}
    print('INITIAL_BARRIERS', len(saturated))
    roots = {}; depth = {}; living = set(); recoveries = collections.Counter(); first_forced = None
    for tick, kind, pid, raw in db.execute(
        "select tick,type,pop,json_remove(data,'$.genes','$.sensors') from events where run=? and tick<=? order by tick,id",
        (args.run, args.tick)):
        data = json.loads(raw)
        if kind in ('birth', 'founder'):
            parent = data.get('parent_id')
            roots[pid] = roots.get(parent, pid); depth[pid] = depth.get(parent, -1) + 1; living.add(pid)
            if kind == 'birth' and data.get('forced'):
                recoveries[tick] += 1
                if first_forced is None: first_forced = tick
        elif kind in ('death', 'cull'):
            living.discard(pid)
            if data['resources'][2] > 0:
                saturated.add(tuple(data['position']))
    print('CONFIRMED_BARRIERS_LOWER_BOUND', len(saturated), 'FIRST_FORCED', first_forced,
          'RECOVERY_TICKS', len(recoveries), 'FORCED_COUNT', sum(recoveries.values()))
    print('LIVING_ROOTS', dict(collections.Counter(roots[p] for p in living)),
          'DEPTHS', dict(collections.Counter(depth[p] for p in living)))
    if not args.live: return
    census = command(args.run, 'agents'); agents = census['agents']; occupied = {tuple(p['position']) for p in agents}
    causes = collections.Counter(); weights = []; stationary = 0; adjacent = collections.Counter()
    for pop in agents:
        try: p = command(args.run, 'inspect', {'pop_id': pop['pop_id']})
        except Exception as error:
            print('INSPECT_MISSING', pop['pop_id'], str(error)); continue
        x,y = p['position']; action = p['last_action']; dx,dy = p['direction']
        adjacent[sum((x+u,y+v) in saturated for u in (-1,0,1) for v in (-1,0,1) if u or v)] += 1
        stationary += p['totals']['distance'] == 0
        weights.extend(e['value'] for c in p['brain']['connections'] for e in c['entries'])
        # Responsiveness is not exposed by v2. Only attribute actions with no changes to it.
        if p['actions'][20]: causes['unknown_responsiveness'] += 1; continue
        if action not in (0,1,2,4,5,6,7): causes['random_or_nonmovement'] += 1; continue
        moves = {0:(dx,dy),1:(-dy,dx),2:(dy,-dx),4:(1,0),5:(-1,0),6:(0,-1),7:(0,1)}
        u,v = moves[action]; target = (x+u,y+v)
        if not (0 <= target[0] < width and 0 <= target[1] < len(cells)//width):
            causes['out_of_bounds'] += 1; continue
        slope = int(cells[target[1]*width+target[0]]['elevation']) - int(cells[y*width+x]['elevation'])
        if slope >= 6*(1-p['lazyness']): causes['slope'] += 1
        elif target in saturated: causes['confirmed_calcium'] += 1
        elif target in occupied: causes['occupied'] += 1
        else: causes['unresolved_or_free'] += 1
    print('LIVE', census['tick'], 'count', len(agents), 'last_action_causes', dict(causes),
          'zero_lifetime_distance', stationary, 'saturated_neighbours_histogram', dict(adjacent))
    nonzero = [abs(w) for w in weights if w]
    print('LIVE_WEIGHTS', len(weights), 'exact_zero', len(weights)-len(nonzero),
          'max_abs', max(nonzero, default=0), 'min_nonzero_abs', min(nonzero, default=0))


if __name__ == '__main__':
    main()
