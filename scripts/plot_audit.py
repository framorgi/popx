"""Static scientific summary of one pilot and the separate physiology controls."""
import json
from audit_data import read_report
import sys
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap

folder = Path(sys.argv[2]) if len(sys.argv)>2 else Path('/tmp')
folder.mkdir(parents=True,exist_ok=True)
report = read_report(sys.argv[1], 'audit')
pilot = report['run']
thermal = report['thermal_probe']
samples = pilot['samples']
fig, axes = plt.subplots(2, 1, figsize=(10, 9), constrained_layout=True)
x = [0] + [s['tick'] for s in samples]
axes[0].plot(x, [pilot['initial_alive']] + [s['alive'] for s in samples], label='Vivi', color='#224d91', linewidth=2)
axes[0].plot(x, [0] + [s['death_causes'][3] for s in samples], label='Morti termiche cumulative', color='#aa3232')
axes[0].plot(x, [0] + [s['death_causes'][1] for s in samples], label='Morti per energia cumulative', color='#d17815')
axes[0].plot(x, [0] + [s['births'] for s in samples], label='Nascite cumulative (incluse forzate)', color='#327e56')
axes[0].axvline(1001, color='#555555', linestyle=':', label='Età minima per riproduzione normale dei fondatori')
axes[0].set(xlabel='Tick di simulazione', ylabel='Numero di individui / eventi',
            title='Pilot dopo correzioni: una sola traiettoria, nessuna prova di adattamento')
axes[0].legend(fontsize=8, loc='upper left')
axes[0].grid(alpha=0.2)
axes[0].set_xlim(0, 2000)
conditions = [(m, lipid) for m in [1, 5, 25] for lipid in [20, 80]]
lookup = {(t['environment'], t['mitochondria'], t['lipids']): t for t in thermal}
rows = [0, 25, 37]
values = [[int(lookup[(env, m, lipid)]['alive']) for m, lipid in conditions] for env in rows]
axes[1].imshow(values, cmap=ListedColormap(['#e8b2b2', '#b5d2eb']), vmin=0, vmax=1, aspect='auto')
for row, env in enumerate(rows):
    for col, (m, lipid) in enumerate(conditions):
        t = lookup[(env, m, lipid)]
        label = '150\nvivo al termine' if t['alive'] else f"{t['ticks']}\nmorte termica"
        axes[1].text(col, row, label, ha='center', va='center', fontsize=9)
axes[1].set_xticks(range(6), [f'M={m}\nLipidi={lipid}' for m, lipid in conditions])
axes[1].set_yticks(range(3), [f'{env} °C' for env in rows])
axes[1].set(ylabel='Temperatura ambiente', title='Controlli fisiologici separati: tick osservati, limite 150 (non lifespan completo)')
fig.savefig(folder / 'dynamic_summary.png', dpi=180)
fig.savefig(folder / 'dynamic_summary.svg')
