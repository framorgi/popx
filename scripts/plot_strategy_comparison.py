"""Plot descriptive means and observed ranges, not confidence intervals."""
import json
import sys
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

folder = Path(sys.argv[2]) if len(sys.argv)>2 else Path('/tmp')
folder.mkdir(parents=True,exist_ok=True)
from analyze_strategy_comparison import summary
policies = ['stationary', 'constant_east', 'patrol', 'random_cardinal']
labels = ['Immobile', 'Sempre est', 'Pattuglia', 'Casuale']
fig, axes = plt.subplots(2, 2, figsize=(10, 7), constrained_layout=True, sharey=True)
for row, photo in enumerate([False, True]):
    for col, distribution in enumerate(['uniform', 'patch']):
        group = [r for r in summary['policy_comparisons'] if r['photosynthesis'] == photo and r['distribution'] == distribution]
        metrics = {r['strategy']: r['metrics']['descendants_alive'] for r in group}
        means = [metrics[p]['mean'] for p in policies]
        low = [metrics[p]['mean'] - metrics[p]['min'] for p in policies]
        high = [metrics[p]['max'] - metrics[p]['mean'] for p in policies]
        ax = axes[row, col]
        ax.bar(np.arange(4), means, color=['#626e7d', '#c67b42', '#4e88ac', '#489271'], yerr=[low, high], capsize=4)
        ax.set_xticks(np.arange(4), labels)
        for i, value in enumerate(means):
            ax.text(i, max(metrics[policies[i]]['max'], value) + 3, f'{value:g}', ha='center', fontsize=9)
        ax.set_title(('Con fotosintesi' if photo else 'Senza fotosintesi') + '\n' + ('Risorse uniformi' if distribution == 'uniform' else 'Risorse localizzate'))
        ax.set_ylim(0, 138)
        ax.grid(axis='y', alpha=0.2)
        ax.set_ylabel('Discendenti vivi al tick 3000')
fig.suptitle('Strategie prescritte, tratti uguali per blocco, ambiente a 37 °C\nMedie di 4 seed; barre: min–max osservato, non intervalli di confidenza', fontsize=12)
fig.savefig(folder / 'strategy_comparison.png', dpi=180)
fig.savefig(folder / 'strategy_comparison.svg')
