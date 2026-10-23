#!/usr/bin/env python3
"""Read-only fixed-prefix demography and optional synchronized live movement census.

Print text; optionally save CSV observations and a PNG. No neural snapshots or
simulation control commands. Live agent lists use the existing main-thread API.
"""
import argparse
import collections
import csv
import hashlib
import json
from pathlib import Path
import sqlite3
import statistics
import time
import urllib.request


def samples(db, run, cutoff):
    return [(t, json.loads(raw)) for t, raw in db.execute(
        'SELECT tick,data FROM samples WHERE run=? AND tick<=? ORDER BY tick', (run, cutoff))]


def compare(label, rows):
    first, last = rows[0][1], rows[-1][1]
    print(label, 'cutoff', rows[-1][0], 'counts', last['counts'], 'alive', last['alive'])
    print('STOCKS', 'initial', first['resource_ledger']['final'], 'final', last['resource_ledger']['final'])
    print('BALANCES',last['resource_ledger'],last['energy_ledger'])
    print('AT_CAP', sum(d['alive'] == 120 for _,d in rows), '/', len(rows))
    prev = rows[0]
    endpoints = [row for i,row in enumerate(rows) if row[0] and (row[0] % 50000 == 0 or i == len(rows)-1)]
    for tick,d in endpoints:
        pt,p = prev
        dt = tick-pt
        attempts = d['totals']['movement_attempts']-p['totals']['movement_attempts']
        success = d['totals']['movement_successes']-p['totals']['movement_successes']
        exposure = (d['totals']['metabolism_cost']-p['totals']['metabolism_cost'])/.02
        birth = d['counts']['natural_births']-p['counts']['natural_births']
        deaths = d['counts']['deaths']-p['counts']['deaths']
        print('WINDOW',pt,tick,'births',birth,'deaths',deaths,'attempts',attempts,'successes',success,
              'success_pct',100*success/attempts if attempts else 0,
              'attempts_per_1000_agent_ticks',1000*attempts/exposure,
              'successes_per_1000_agent_ticks',1000*success/exposure,
              'photo',d['totals']['photosynthesis']-p['totals']['photosynthesis'],
              'resp',d['totals']['respirations']-p['totals']['respirations'],
              'mean_live_energy',d['averages']['energy'],'backlog',d['backlog'],'dropped',d['backlog_dropped'])
        prev = (tick,d)


def agents(run):
    body=json.dumps({'command':'agents','run_id':run}).encode()
    req=urllib.request.Request('http://127.0.0.1:8000/api/commands',data=body,headers={'Content-Type':'application/json'})
    with urllib.request.urlopen(req,timeout=15) as response:
        reply=json.load(response)
    return reply['result']


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('database'); p.add_argument('--run',required=True);p.add_argument('--baseline')
    p.add_argument('--tick',type=int);p.add_argument('--live-seconds',type=float,default=0)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    db=sqlite3.connect(f'file:{Path(a.database).resolve()}?mode=ro',uri=True)
    cutoff=a.tick if a.tick is not None else db.execute('SELECT max(tick) FROM samples WHERE run=?',(a.run,)).fetchone()[0]
    rows=samples(db,a.run,cutoff)
    meta=json.loads(db.execute('SELECT metadata FROM runs WHERE id=?',(a.run,)).fetchone()[0])
    print('RUN',a.run,'MODEL',meta.get('model'),'CUTOFF',cutoff,flush=True)
    print('PROVENANCE',meta.get('provenance',{}).get('binary_sha256'),meta.get('provenance',{}).get('commit'))
    prov=meta.get('provenance',{})
    print('CURRENT_SOURCE_MISMATCHES',[f for f,h in prov.get('source_sha256',{}).items()
          if Path(f).is_file() and hashlib.sha256(Path(f).read_bytes()).hexdigest()!=h])
    print('GAPS',db.execute('SELECT count(*) FROM gaps WHERE run=? AND tick<=?',(a.run,cutoff)).fetchone()[0])
    compare('CURRENT',rows)
    baseline=samples(db,a.baseline,cutoff) if a.baseline else []
    if baseline: compare('BASELINE_INDEPENDENT_RUN',baseline)
    nodes={};living=set();natural_parents=set();causes=collections.Counter()
    for tick,kind,pid,raw in db.execute("SELECT tick,type,pop,json_remove(data,'$.genes','$.sensors') FROM events WHERE run=? AND tick<=? ORDER BY tick,id",(a.run,cutoff)):
        d=json.loads(raw)
        if kind in ('birth','founder'):
            parent=d.get('parent_id');pn=nodes.get(parent)
            nodes[pid]=(pn[0] if pn else pid,pn[1]+1 if pn else 0,tick,kind,d['traits'])
            living.add(pid)
            if kind=='birth' and not d['forced']:natural_parents.add(parent)
        elif kind in ('death','cull'):
            living.discard(pid);causes['cull' if kind=='cull' else d['death_cause']]+=1
    roots=collections.Counter(nodes[pid][0] for pid in living)
    print('GENEALOGY','live_roots',dict(roots),'max_depth_all',max(n[1] for n in nodes.values()),
          'live_depths',dict(collections.Counter(nodes[pid][1] for pid in living)),
          'founders_still_alive',sum(nodes[pid][3]=='founder' for pid in living),
          'natural_parents_total',len(natural_parents),'natural_parents_alive',len(natural_parents&living),
          'death_causes',dict(causes))
    if a.live_seconds:
        start=agents(a.run);time.sleep(min(a.live_seconds,30));end=agents(a.run)
        one={d['pop_id']:d for d in start['agents']};two={d['pop_id']:d for d in end['agents']}
        print('LIVE_WINDOW',start['tick'],end['tick'],'n_start',len(one),'n_end',len(two))
        records=[]
        for pid,d in two.items():
            before=one.get(pid)
            if not before:continue
            attempts=d['totals']['movement_attempts']-before['totals']['movement_attempts']
            successes=d['totals']['movement_successes']-before['totals']['movement_successes']
            records.append(dict(pop_id=pid,x=d['position'][0],y=d['position'][1],age=d['age'],
                attempts=attempts,successes=successes,energy=d['totals']['energy'],glucose=d['resources'][0],
                water=d['resources'][1],chloroplasts=d['traits']['chloroplasts'],mitochondria=d['traits']['mitochondria'],
                photo=d['totals']['photosynthesis']-before['totals']['photosynthesis'],
                resp=d['totals']['respirations']-before['totals']['respirations'],
                offspring=d['totals']['offspring']-before['totals']['offspring']))
            records[-1].update({f'action_{i}': value-before['actions'][i] for i,value in enumerate(d['actions'])})
        with (a.output/'live_movement.csv').open('w') as f:
            w=csv.DictWriter(f,fieldnames=list(records[0]));w.writeheader();w.writerows(records)
        print('LIVE_MATCHED',len(records),'no_success',sum(r['successes']==0 for r in records),
              'attempted_but_no_success',sum(r['attempts']>0 and r['successes']==0 for r in records),
              'no_attempt',sum(r['attempts']==0 for r in records),
              'attempts',sum(r['attempts'] for r in records),'successes',sum(r['successes'] for r in records),
              'photosynthetic',sum(r['photo']>0 for r in records),
              'naturally_eligible',sum(r['age']>1000 and r['energy']>50 and r['glucose']>20 for r in records),
              'energy_median',statistics.median(r['energy'] for r in records),
              'age_median',statistics.median(r['age'] for r in records),
              'glucose_median',statistics.median(r['glucose'] for r in records),
              'water_median',statistics.median(r['water'] for r in records))
        print('LIVE_ACTION_REQUESTS', [sum(r[f'action_{i}'] for r in records) for i in range(len(end['agents'][0]['actions']))])
        print('LIVE_BIRTH_TRAITS_CHLOROPLASTS',dict(collections.Counter(r['chloroplasts'] for r in records)))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,axes=plt.subplots(3,1,figsize=(10,9),sharex=True)
    for name,data in [('Current water v2',rows),('Previous independent run',baseline)]:
        if not data:continue
        x=[t/1000 for t,d in data]
        axes[0].plot(x,[d['alive'] for t,d in data],label=name)
        axes[1].plot(x,[d['resource_ledger']['final'][1]/data[0][1]['resource_ledger']['initial'][1] for t,d in data],label=name)
        xx=[];yy=[]
        for i in range(25,len(data),25):
            t,d=data[i];_,b=data[i-25]
            exposure=(d['totals']['metabolism_cost']-b['totals']['metabolism_cost'])/.02
            xx.append(t/1000); yy.append(1000*(d['totals']['movement_successes']-b['totals']['movement_successes'])/exposure)
        axes[2].plot(xx,yy,label=name)
    for ax in axes:ax.legend();ax.grid(alpha=.2)
    axes[0].set_ylabel('Alive');axes[1].set_ylabel('Water / initial stock')
    axes[2].set_ylabel('Successful moves / 1000 agent-ticks');axes[2].set_xlabel('Thousands of ticks')
    fig.suptitle('Independent runs at equal tick horizon; not a causal experiment')
    fig.tight_layout();fig.savefig(a.output/'comparison.png',dpi=150)


if __name__=='__main__':main()
