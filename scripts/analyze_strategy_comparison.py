"""Summarise paired policy interventions; repeated deterministic controls are not independent replicas."""
import json
from audit_data import read_report
import statistics
import sys
from pathlib import Path

data = read_report(sys.argv[1], 'strategy_fixture')
fields = ['movement_attempts', 'movement_successes', 'boundary_failures', 'occupancy_failures',
          'founder_glucose_uptake', 'glucose_uptake', 'water_uptake', 'energy_income', 'movement_cost',
          'basal_cost', 'founder_energy', 'founder_offspring', 'descendants_born', 'descendants_alive',
          'descendants_mature', 'energy_deaths', 'population_cap_ticks', 'energy_discarded_at_cap']
rows = []
assert len(data['trials']) == 64
for photo in [False, True]:
    for distribution in ['uniform', 'patch']:
        for strategy in ['stationary', 'constant_east', 'patrol', 'random_cardinal']:
            group = [r for r in data['trials'] if r['photosynthesis'] == photo
                     and r['distribution'] == distribution and r['strategy'] == strategy]
            assert len(group) == 4
            for r in group:
                assert r['initial_glucose_world'] == 32768
                assert r['thermal_deaths'] == 0 and r['founder_alive'] and r['founder_age'] == 3000
                assert (r['movement_successes'] + r['boundary_failures'] + r['occupancy_failures']
                        + r['other_movement_failures']) == r['movement_attempts']
                assert r['descendants_born'] == len(r['births'])
                assert r['descendants_alive'] == sum(p['alive'] for p in r['individuals'] if p['serial'] != 0)
            rows.append({'photosynthesis': photo, 'distribution': distribution, 'strategy': strategy,
                         'n_seeds': 4, 'metrics': {f: {'mean': statistics.mean(r[f] for r in group),
                                                     'min': min(r[f] for r in group),
                                                     'max': max(r[f] for r in group)} for f in fields}})
trace = data['network_replay']
for r in trace:
    assert r['movement_requests'] == r['movement_successes'] + r['boundary_failures'] + r['zero_displacement_requests']
summary = {'policy_comparisons': rows, 'network_trace': {
    'n_genomes': len(trace), 'constant_action_genomes': sum(r['distinct_actions'] == 1 for r in trace),
    'no_movement_genomes': sum(r['movement_requests'] == 0 for r in trace),
    'requests': sum(r['movement_requests'] for r in trace), 'successful_moves': sum(r['movement_successes'] for r in trace),
    'boundary_failures': sum(r['boundary_failures'] for r in trace),
    'zero_displacement_requests': sum(r['zero_displacement_requests'] for r in trace),
    'constant_movers_visiting_at_most_4_cells': sum(r['distinct_actions'] == 1 and r['movement_requests'] > 0
                                                  and r['visited_cells'] <= 4 for r in trace)}}

if __name__ == '__main__':
    print(json.dumps(summary, indent=2))
