#!/usr/bin/env python3
"""Render README illustrations; optionally read an initial environment from SQLite.

Usage: python3 scripts/render_readme_figures.py [--environment-db PATH] [--run-id ID]
Requires NumPy and Matplotlib. Does not run or modify the simulator/database.
"""

import argparse
import json
from pathlib import Path
import sqlite3

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Rectangle
import numpy as np


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs/assets"
INK, MUTED, TEAL, CORAL = "#182d3d", "#536778", "#087f8c", "#d65d4c"


def save(figure, name, description):
    metadata = {"Description": description}
    if name.endswith(".svg"):
        metadata["Date"] = None
    figure.savefig(OUTPUT / name, dpi=160, facecolor="#f7fafc", bbox_inches="tight",
                   metadata=metadata)
    plt.close(figure)


def actions():
    figure, axes = plt.subplots(1, 3, figsize=(12, 3.8))
    titles = ["01 / FORWARD MOVEMENT", "02 / BLOCKED MOVEMENT", "03 / SIGNAL EMISSION"]
    for ax, title in zip(axes, titles):
        ax.set(xlim=(-.55, 4.55), ylim=(4.55, -.55), aspect="equal")
        ax.set_xticks(np.arange(-.5, 5, 1), minor=True)
        ax.set_yticks(np.arange(-.5, 5, 1), minor=True)
        ax.grid(which="minor", color="#d8e3eb")
        ax.tick_params(which="both", bottom=False, left=False, labelbottom=False, labelleft=False)
        for spine in ax.spines.values():
            spine.set_visible(False)
        ax.set_title(title, loc="left", fontsize=11, color=INK, fontweight="bold", pad=16)
        ax.set_facecolor("white")
    ax = axes[0]
    ax.add_patch(Circle((1, 3), .24, color=TEAL, zorder=3))
    ax.add_patch(Circle((2, 2), .24, fill=False, edgecolor=TEAL, linewidth=2, linestyle="--"))
    ax.annotate("", (1.8, 2.2), (1.2, 2.8), arrowprops={"arrowstyle": "->", "color": TEAL, "lw": 2})
    ax.text(.7, 3.7, "Pop", color=INK, fontsize=11)
    ax.text(2.3, 1.9, "free\ndestination", color=TEAL, fontsize=10)
    ax = axes[1]
    ax.add_patch(Circle((1, 2), .24, color=TEAL, zorder=3))
    ax.add_patch(Circle((2, 2), .24, color=CORAL, zorder=3))
    ax.annotate("", (1.7, 2), (1.3, 2), arrowprops={"arrowstyle": "->", "color": CORAL, "lw": 2})
    ax.text(.3, 3.1, "position unchanged", color=INK, fontsize=10)
    ax.text(2.1, 1.3, "another Pop", color=CORAL, fontsize=10)
    ax = axes[2]
    ax.add_patch(Rectangle((1.5, 1.5), 1, 1, color="#8bddbd", zorder=2))
    ax.add_patch(Circle((2, 2), .24, color=TEAL, zorder=3))
    ax.text(.3, 3.3, "+70…100, capped at 100", color=INK, fontsize=10)
    ax.text(.3, 3.8, "decay, without diffusion", color=MUTED, fontsize=9)
    figure.subplots_adjust(wspace=.24, bottom=.04, top=.85)
    save(figure, "popx-actions.svg", "Illustrations, not measured trajectories. Movement with responsiveness=1.")


def respiration():
    temperature = np.linspace(10, 65, 551)
    z = (temperature - 37) / 10
    thermal_yield = 2 ** (-z * z)
    hot = np.clip(z, 0, 1)
    inhibition = 1 - 3 * hot ** 2 + 2 * hot ** 3
    figure, axes = plt.subplots(1, 2, figsize=(11, 3.6))
    axes[0].plot(temperature, thermal_yield, color=TEAL, lw=2.5, label="Yield Y(T)")
    axes[0].plot(temperature, thermal_yield * inhibition, color=CORAL, lw=2.5, label="Activity A(T)")
    for value in (37, 47):
        axes[0].axvline(value, color=MUTED, alpha=.4, linestyle=":")
    axes[0].set(xlabel="Pop temperature (°C)", ylabel="Dimensionless factor", ylim=(-.04, 1.07))
    axes[0].set_xticks([10, 25, 37, 47, 65])
    axes[0].legend(frameon=False, fontsize=9)
    energy = np.linspace(0, 125, 251)
    axes[1].plot(energy, np.clip((100 - energy) / 25, 0, 1), color=TEAL, lw=2.5)
    axes[1].set(xlabel="Energy reserve (model units)", ylabel="Demand D(E)", ylim=(-.04, 1.07))
    axes[1].set_xticks([0, 25, 50, 75, 100, 125])
    axes[0].set_title("Thermal response", loc="left", color=INK, fontweight="bold")
    axes[1].set_title("Reserve regulation", loc="left", color=INK, fontweight="bold")
    for ax in axes:
        ax.spines[["top", "right"]].set_visible(False)
        ax.grid(alpha=.15)
    figure.tight_layout(w_pad=3)
    save(figure, "popx-respiration.svg", "Analytical model curves: Topt=37, w=10, E*=100, b=25. Not experimental data.")


def environment(database, run_id):
    with sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True) as connection:
        if run_id is None:
            run_id = connection.execute("SELECT id FROM runs ORDER BY created DESC LIMIT 1").fetchone()[0]
        metadata = json.loads(connection.execute("SELECT metadata FROM runs WHERE id=?", (run_id,)).fetchone()[0])
        side = metadata["config"]["map_size"]
        cells = []
        for offset, total, raw in connection.execute(
                "SELECT offset,total,data FROM environment_parts WHERE run=? ORDER BY offset", (run_id,)):
            if offset != len(cells) or total != side * side:
                raise ValueError("Initial environment has missing or inconsistent parts")
            cells.extend(json.loads(raw))
        if len(cells) != side * side:
            raise ValueError("Initial environment is incomplete")
    specs = [("Elevation", "elevation", None, "terrain", "model units"),
             ("Temperature", "temperature", None, "coolwarm", "°C"),
             ("Glucose", "resources", 0, "YlGn", "model units"),
             ("Water", "resources", 1, "Blues", "model units")]
    figure, axes = plt.subplots(1, 4, figsize=(13, 3.5))
    for ax, (title, key, index, cmap, units) in zip(axes, specs):
        values = np.array([cell[key] if index is None else cell[key][index] for cell in cells]).reshape(side, side)
        img = ax.imshow(values, cmap=cmap, interpolation="nearest", origin="upper", vmin=0)
        ax.set_title(title, loc="left", fontsize=12, color=INK, fontweight="bold")
        ax.set(xlabel="x (cell)", ylabel="y (cell)")
        bar = figure.colorbar(img, ax=ax, orientation="horizontal", pad=.24, fraction=.055)
        bar.set_label(units, fontsize=8)
        bar.ax.tick_params(labelsize=8)
    figure.tight_layout(w_pad=1.5)
    save(figure, "popx-environment.png",
         f"Actual initial environment, run {run_id}; grid {side}×{side}. Not a screenshot.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment-db", type=Path)
    parser.add_argument("--run-id")
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10, "text.color": INK,
                         "axes.labelcolor": INK, "svg.fonttype": "path", "svg.hashsalt": "popx-readme"})
    actions()
    respiration()
    if args.environment_db:
        environment(args.environment_db, args.run_id)


if __name__ == "__main__":
    main()
