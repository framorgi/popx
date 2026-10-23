#!/usr/bin/env python3
"""Read a fixed prefix of a live PopX SQLite run; emit a text demographic audit.

No commands are sent to the simulator. Censored agents remain distinct from
completed lives; natural-edge depth resets at every forced birth.
"""
import argparse
import collections
import json
import sqlite3
import statistics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('database')
    parser.add_argument('--run')
    parser.add_argument('--tick', type=int)
    parser.add_argument('--plot', help='Optional PNG/SVG chart (requires matplotlib).')
    args = parser.parse_args()
    db = sqlite3.connect(f'file:{args.database}?mode=ro', uri=True)
    run = args.run or db.execute('SELECT id FROM runs ORDER BY created DESC LIMIT 1').fetchone()[0]
    cutoff = args.tick if args.tick is not None else db.execute('SELECT max(tick) FROM samples WHERE run=?', (run,)).fetchone()[0]
    metadata = json.loads(db.execute('SELECT metadata FROM runs WHERE id=?', (run,)).fetchone()[0])
    print('RUN', run, 'CUTOFF', cutoff, flush=True)
    print('CONFIG', metadata['config'])
    print('MODEL', metadata['model'])
    prov = metadata.get('provenance', {})
    print('PROVENANCE', {k:v for k,v in prov.items() if k not in ('diff', 'untracked_sources')})
    print('GAPS', db.execute('SELECT count(*),sum(dropped) FROM gaps WHERE run=? AND tick<=?', (run, cutoff)).fetchone())
    nodes, living = {}, set()
    births = collections.Counter()
    causes = collections.Counter()
    ages = collections.defaultdict(list)
    energies = collections.defaultdict(list)
    glucose = collections.defaultdict(list)
    natural_children = collections.Counter()
    forced_children = collections.Counter()
    death_groups = collections.Counter()
    epoch_births = collections.Counter()
    recoveries = set()
    root_counts = collections.Counter()
    root_last = {}
    root_start = {}
    max_depth = max_natural_depth = missing = 0
    forced_parent_ages = []
    forced_parent_energy = []
    zero_substrates = collections.Counter()
    parent_counts = collections.Counter()
    longest_unassisted = (0, 0, 0)
    unassisted_start = 0
    # Remove large neural payloads inside SQLite; retain life-history evidence.
    query = "SELECT tick,type,pop,json_remove(data,'$.genes','$.sensors') FROM events WHERE run=? AND tick<=? ORDER BY tick,id"
    for tick, kind, pid, raw in db.execute(query, (run, cutoff)):
        d = json.loads(raw)
        if kind in ('founder', 'birth'):
            parent = d.get('parent_id', '')
            group = 'recovery' if d.get('recovery_seed') else ('founder' if kind == 'founder' else ('forced' if d.get('forced') else 'natural'))
            births[group] += 1
            epoch_births[tick // 500000, group] += 1
            energies[group].append(d['totals']['energy'])
            glucose[group].append(d['resources'][0])
            if d['resources'][0] == 0: zero_substrates[group, 'glucose'] += 1
            if d['resources'][1] == 0: zero_substrates[group, 'water'] += 1
            if group in ('forced', 'recovery'):
                interval = tick - unassisted_start
                if interval > longest_unassisted[0]: longest_unassisted = (interval, unassisted_start, tick)
                unassisted_start = tick
            if parent and parent not in nodes:
                missing += 1
            pn = nodes.get(parent)
            if group == 'forced' and pn:
                forced_parent_ages.append(tick-pn[0])
                forced_parent_energy.append(8*d['totals']['energy'] + metadata['config']['phy_energy_per_respiration'])
                parent_counts['juvenile' if tick-pn[0] <= 1000 else 'adult'] += 1
            root = pn[3] if pn else pid
            depth = pn[4]+1 if pn else 0
            nd = (pn[5]+1 if pn else 1) if group == 'natural' else 0
            nodes[pid] = (tick, group, parent, root, depth, nd)
            living.add(pid)
            max_depth = max(max_depth, depth)
            max_natural_depth = max(max_natural_depth, nd)
            root_counts[root] += 1
            root_start.setdefault(root, tick)
            root_last[root] = tick
            if group == 'recovery': recoveries.add(tick)
            if parent:
                (natural_children if group == 'natural' else forced_children)[parent] += 1
        elif kind in ('death', 'cull'):
            n = nodes.get(pid)
            group = n[1] if n else 'missing'
            age = d['age']
            cause = 'cull' if kind == 'cull' else d['death_cause']
            causes[cause] += 1
            ages[group].append(age)
            death_groups[group, cause, 'juvenile' if age <= 1000 else 'adult'] += 1
            living.discard(pid)
            if n: root_last[n[3]] = tick
    print('BIRTHS', dict(births), 'RECOVERY_EPISODES', len(recoveries), 'MISSING_PARENTS', missing)
    print('DEATH_CAUSES', dict(causes), 'LIVING_AT_CUTOFF', len(living))
    def distribution(v):
        v = sorted(v)
        return dict(n=len(v), mean=statistics.mean(v), p50=v[len(v)//2], p90=v[int(len(v)*.9)], p99=v[int(len(v)*.99)], maximum=v[-1]) if v else {}
    for group in births:
        completed = [p for p,n in nodes.items() if n[1] == group and p not in living]
        print('COHORT', group, 'death_age', distribution(ages[group]), 'birth_energy', distribution(energies[group]), 'birth_glucose', distribution(glucose[group]))
        print('REPRODUCTION_COMPLETED', group, 'n',len(completed),'natural_parents',sum(natural_children[p]>0 for p in completed),'natural_children',sum(natural_children[p] for p in completed),'forced_children',sum(forced_children[p] for p in completed))
    print('DEATH_GROUPS', dict(death_groups))
    print('ZERO_BIRTH_SUBSTRATES', dict(zero_substrates))
    print('FORCED_PARENT_AGES', distribution(forced_parent_ages), dict(parent_counts))
    print('FORCED_PARENT_ENERGY_BEFORE_DONATION', distribution(forced_parent_energy))
    if cutoff-unassisted_start > longest_unassisted[0]: longest_unassisted = (cutoff-unassisted_start, unassisted_start, cutoff)
    print('LONGEST_INTERVAL_WITHOUT_FORCED_OR_RECOVERY_BIRTH', longest_unassisted)
    print('MAX_DEPTH', max_depth, 'MAX_CONSECUTIVE_NATURAL_EDGES', max_natural_depth)
    print('LIVING_ROOTS', dict(collections.Counter(nodes[p][3] for p in living)))
    print('LIVING_NATURAL_DEPTHS', sorted(nodes[p][5] for p in living))
    print('TOP_ROOTS', [(r,n,root_start[r],root_last[r],r in {nodes[p][3] for p in living}) for r,n in root_counts.most_common(10)])
    print('EPOCH_BIRTHS', sorted(epoch_births.items()))
    windows = collections.defaultdict(list)
    last = None
    first = None
    resource_windows = {}
    for tick, raw in db.execute('SELECT tick,data FROM samples WHERE run=? AND tick<=? ORDER BY tick', (run,cutoff)):
        d = json.loads(raw)
        windows[tick//500000].append(d['alive'])
        if first is None: first = d
        resource_windows[tick//500000] = (tick, d['resource_ledger']['final'], d['totals']['photosynthesis'])
        last = d
    for epoch, counts in windows.items():
        print('POPULATION_WINDOW',epoch*500000,'samples',len(counts),'mean',statistics.mean(counts),'at_cap',sum(n>=120 for n in counts),'below20',sum(n<20 for n in counts),'at20',sum(n==20 for n in counts))
    print('RESOURCE_WINDOWS', resource_windows)
    print('INITIAL_RESOURCE_LEDGER', first['resource_ledger'])
    print('LAST_COUNTS', last['counts'])
    print('LAST_LEDGERS', last['energy_ledger'], last['resource_ledger'])
    print('LAST_TRANSPORT', last['transport'])
    checks = {
        'living': len(living) == last['alive'],
        'natural_births': births['natural'] == last['counts']['natural_births'],
        'forced_births': births['forced'] + births['recovery'] == last['counts']['forced_births'],
        'deaths': sum(causes.values()) == last['counts']['deaths'],
        'completed_age_sum': sum(map(sum, ages.values())) == last['completed_lives']['age_sum'],
    }
    print('EVENT_SAMPLE_CONSISTENCY', checks)
    if not all(checks.values()):
        raise RuntimeError('Event/sample disagreement: inspect transport gaps and cutoff.')
    if args.plot:
        plot_samples(db, run, cutoff, args.plot)
    print('LIMITS: sampled cap occupancy is not denied births; observed offspring counts are not causal fitness; live individuals are right censored; natural-edge chains can have forced ancestors.')


def plot_samples(db, run, cutoff, path):
    """Plot bounded sample aggregates, without reading or changing live agents."""
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt

    buckets = collections.defaultdict(list)
    first = None
    ends = {}
    for tick, raw in db.execute('SELECT tick,data FROM samples WHERE run=? AND tick<=? ORDER BY tick', (run, cutoff)):
        d = json.loads(raw)
        if first is None:
            first = d
        buckets[tick // 10000].append(d['alive'])
        ends[tick // 10000] = (tick, d)
    x = [ends[b][0] / 1e6 for b in buckets]
    fig, axes = plt.subplots(3, 1, figsize=(11, 9), sharex=True)
    axes[0].fill_between(x, [min(v) for v in buckets.values()], [max(v) for v in buckets.values()], alpha=.2)
    axes[0].plot(x, [statistics.mean(v) for v in buckets.values()], label='Mean alive / 10,000 ticks')
    axes[0].axhline(120, color='gray', ls='--', label='Population cap')
    axes[0].set_ylabel('Agents')
    for i, label in [(1, 'Total water'), (0, 'Total glucose')]:
        axes[1].plot(x, [d['resource_ledger']['final'][i] / first['resource_ledger']['initial'][i] for _, d in ends.values()], label=label)
    axes[1].set_ylabel('Fraction of initial stock')
    previous = {'natural_births': 0, 'forced_births': 0}
    changes = collections.defaultdict(list)
    for _, d in ends.values():
        for key in previous:
            changes[key].append(d['counts'][key]-previous[key])
            previous[key] = d['counts'][key]
    axes[2].plot(x, changes['natural_births'], label='Natural births')
    axes[2].plot(x, changes['forced_births'], label='Forced births, including recovery seeds', alpha=.8)
    axes[2].set_ylabel('Births / bin')
    axes[2].set_xlabel('Millions of ticks (last bin may be partial)')
    for ax in axes:
        ax.legend(loc='upper right')
        ax.grid(alpha=.2)
    fig.suptitle(f'PopX run {run} — cutoff {cutoff:,} ticks')
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


if __name__ == '__main__':
    main()
