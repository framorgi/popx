"""Describe the saved pilots; do not interpret unpaired differences as causal effects."""
import json
from audit_data import read_report
import math
import statistics
import sys
from pathlib import Path


def correlation(xs, ys):
    if not xs:
        return None
    mx, my = statistics.mean(xs), statistics.mean(ys)
    vx = sum((v - mx) ** 2 for v in xs)
    vy = sum((v - my) ** 2 for v in ys)
    if vx * vy == 0:
        return None
    return sum((a - mx) * (b - my) for a, b in zip(xs, ys)) / math.sqrt(vx * vy)


def describe(report):
    run = report['run']
    initial = run.get('initial_genomes', [])
    people = run.get('individuals', [])
    births = run.get('birth_events', [])
    signatures = {p['id']: p['signature'] for p in initial}
    agreements = []
    for child in births:
        if child.get('parent_id') in signatures:
            parent_signature = signatures[child['parent_id']]
            agreements.append(sum(x == y for x, y in zip(parent_signature, child['signature'])) / len(parent_signature))
        signatures[child['id']] = child['signature']
    founder_ids = {p['id'] for p in initial}
    founders = [p for p in people if p['id'] in founder_ids]
    assays = run.get('inheritance_assay', [])
    fields = ['initial_alive', 'initial_registered', 'ticks_executed', 'stop_reason',
              'initial_unique_action_signatures', 'movement_attempts', 'movement_successes',
              'normal_births', 'forced_births', 'wall_seconds']
    result = {k: run[k] for k in fields if k in run}
    result['movement_success_fraction'] = (run['movement_successes'] / run['movement_attempts']
                                           if run.get('movement_attempts') else None)
    result['last_sample'] = run['samples'][-1] if run.get('samples') else None
    result['first_sample'] = run['samples'][0] if run.get('samples') else None
    result['founders_reproducing'] = sum(p['offspring'] > 0 for p in founders)
    result['founders_living_to_maturity'] = sum(p['age'] > 1000 for p in founders)
    result['raw_founder_distance_offspring_pearson'] = correlation(
        [p['distance'] for p in founders], [p['offspring'] for p in founders])
    result['age_normalized_founder_distance_offspring_pearson'] = correlation(
        [p['distance'] / max(p['age'], 1) for p in founders], [p['offspring'] for p in founders])
    mature = [p for p in founders if p['age'] > 1000]
    result['mature_founder_distance_rate_offspring_pearson'] = correlation(
        [p['distance'] / p['age'] for p in mature], [p['offspring'] for p in mature])
    result['observed_parent_child_signature_agreement_mean'] = statistics.mean(agreements) if agreements else None
    result['observed_parent_child_pairs'] = len(agreements)
    assay_fields = ['clone_original_agreement', 'offspring_vs_original_agreement', 'learned_vs_original_agreement']
    result['assay_means'] = {k: statistics.mean(p[k] for p in assays) for k in assay_fields} if assays else {}
    result['assay_sample_size'] = len(assays)
    result['normal_births_from_nonfounders'] = sum(
        not p.get('forced', p.get('forced_regime', False)) and p.get('parent_id') not in founder_ids for p in births)
    return result


if __name__ == '__main__':
    if len(sys.argv) < 2:
        raise SystemExit('usage: analyze_audit.py DATABASE [DATABASE ...]')
    summaries = {name: describe(read_report(name, 'audit')) for name in sys.argv[1:]}
    print(json.dumps(summaries, indent=2))
