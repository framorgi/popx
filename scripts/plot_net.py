#!/usr/bin/env python3
"""Neural network history viewer for popx.

Usage:
    python3 plot_net.py [nnets_dir]

    nnets_dir  Path to the directory containing JSON network snapshots.
               Defaults to  ../data_out/nnets/

Keyboard controls (when the window is focused):
    →  or  l   Next network
    ←  or  h   Previous network
    q           Quit
"""

import json
import os
import sys

import matplotlib
matplotlib.use("Qt5Agg")   # interactive backend; requires: pip install pyqt5
import matplotlib.pyplot as plt
import networkx as nx
import numpy as np
from scipy.sparse import coo_matrix

# ---------------------------------------------------------------------------
# Label maps derived from neuron.h enums
# ---------------------------------------------------------------------------

SENSOR_LABELS = {
    # 0-7: position / movement
    0:  "LOC_X",         1:  "LOC_Y",
    2:  "BDIST_X",       3:  "BDIST",
    4:  "BDIST_Y",       5:  "GENSIM_FWD",
    6:  "MOVE_DIR_X",    7:  "MOVE_DIR_Y",
    # 8-11: population density
    8:  "POP_N",         9:  "POP_W",
    10: "POP_E",         11: "POP_S",
    # 12-19: temperature
    12: "TEMP_AVG_N",    13: "TEMP_AVG_W",
    14: "TEMP_AVG_E",    15: "TEMP_AVG_S",
    16: "TEMP_DRV_N",    17: "TEMP_DRV_W",
    18: "TEMP_DRV_E",    19: "TEMP_DRV_S",
    # 20-27: feromones
    20: "SIG_FOOD",      21: "SIG_DNG",
    22: "SIG_MATE",      23: "SIG_HOME",
    24: "SIG_DRV_N",     25: "SIG_DRV_W",
    26: "SIG_DRV_E",     27: "SIG_DRV_S",
    # 28-31: intrinsic / environmental  (moved here — within 5-bit address space)
    28: "OSC1",          29: "AGE",
    30: "TEMP",          31: "RANDOM",
    # 32 = NUM_SENSES sentinel (not a neuron)
    # 33-36: GLUCOSE_DENSITY N/W/E/S — extended, currently unreachable by genes
}

ACTION_LABELS = {
    # 0-7: movement
    0:  "MV_FWD",      1:  "MV_LEFT",
    2:  "MV_RIGHT",    3:  "MV_RAND",
    4:  "MV_EAST",     5:  "MV_WEST",
    6:  "MV_NORTH",    7:  "MV_SOUTH",
    # 8-11: feromone signalling
    8:  "EMIT_FOOD",   9:  "EMIT_DNG",
    10: "EMIT_MATE",   11: "EMIT_HOME",
    # 12-14: resource acquisition
    12: "GET_GLUC",    13: "GET_H2O",
    14: "GET_CALC",
    # 15-16: internal modulation
    15: "SET_OSC",     16: "SET_RESP",
}


# ---------------------------------------------------------------------------
# Loading helpers
# ---------------------------------------------------------------------------

def _load_all(nnets_dir: str):
    """Return a sorted list of (path, data_dict) for every JSON file."""
    if not os.path.isdir(nnets_dir):
        print(f"[plot_net] Directory not found: {nnets_dir}")
        sys.exit(1)

    records = []
    for fname in os.listdir(nnets_dir):
        if not fname.endswith(".json"):
            continue
        path = os.path.join(nnets_dir, fname)
        try:
            with open(path) as fp:
                data = json.load(fp)
            records.append((
                data.get("generation",   0),
                data.get("timestamp_ms", 0),
                path,
                data,
            ))
            print(f"[plot_net] Loaded: {path}")
        except (json.JSONDecodeError, OSError):
            print(f"[plot_net] Skipping unreadable file: {path}")

    if not records:
        print(f"[plot_net] No valid JSON files in {nnets_dir}")
        sys.exit(1)

    records.sort(key=lambda r: (r[0], r[1]))   # generation, then timestamp
    print(f"[plot_net] Loaded {len(records)} snapshot(s) from {nnets_dir}")
    return [(r[2], r[3]) for r in records]


def _entries_to_coo(entries, nrows: int, ncols: int):
    """Convert a list of {row, col, value} dicts to a COO sparse matrix."""
    if not entries:
        return coo_matrix((nrows, ncols))
    rows = [e["row"] for e in entries]
    cols = [e["col"] for e in entries]
    vals = [e["value"] for e in entries]
    return coo_matrix((vals, (rows, cols)), shape=(nrows, ncols))


# ---------------------------------------------------------------------------
# Drawing
# ---------------------------------------------------------------------------

def _draw(ax, fig, data: dict, index: int, total: int):
    ax.clear()
    ax.axis("off")
    ax.set_facecolor("lavenderblush")

    size_s     = data.get("sizeS", 0)
    size_n     = data.get("sizeN", 0)
    size_y     = data.get("sizeY", 0)
    num_hidden = data.get("num_hidden_layers", 1)
    num_layers = num_hidden + 2
    pop_id     = data.get("pop_id", "?")
    generation = data.get("generation", "?")

    G               = nx.DiGraph()
    node_colors_map = {}
    labels          = {}
    edge_widths     = []
    edge_colors     = []

    # ── add all nodes ──────────────────────────────────────────────────────
    for i in range(size_s):
        G.add_node(f"S{i}", layer=0, idx=i)
        node_colors_map[f"S{i}"] = "gold"
        labels[f"S{i}"] = SENSOR_LABELS.get(i, f"S{i}")

    for h in range(1, num_hidden + 1):
        for i in range(size_n):
            G.add_node(f"H{h}_{i}", layer=h, idx=i)
            node_colors_map[f"H{h}_{i}"] = "violet"
            labels[f"H{h}_{i}"] = f"H{h}.{i}"

    for i in range(size_y):
        G.add_node(f"Y{i}", layer=num_hidden + 1, idx=i)
        node_colors_map[f"Y{i}"] = "orange"
        labels[f"Y{i}"] = ACTION_LABELS.get(i, f"Y{i}")

    # ── add edges ──────────────────────────────────────────────────────────
    def _node_name(layer: int, idx: int) -> str:
        if layer == 0:
            return f"S{idx}"
        if layer == num_hidden + 1:
            return f"Y{idx}"
        return f"H{layer}_{idx}"

    def _add_entries(entries, from_layer: int, to_layer: int):
        for e in entries:
            src = _node_name(from_layer, e["col"])
            dst = _node_name(to_layer,   e["row"])
            v   = float(e["value"])
            G.add_edge(src, dst, weight=v)
            alpha = float(np.clip(abs(v), 0.15, 1.0))
            edge_widths.append(float(np.clip(abs(v) * 2.5, 0.3, 3.0)))
            edge_colors.append((1.0, 0.0, 0.0, alpha) if v < 0
                               else (0.0, 0.75, 0.0, alpha))

    if "connections" in data:
        for conn in data["connections"]:
            _add_entries(conn.get("entries", []),
                         conn["from_layer"], conn["to_layer"])
    else:
        # Backward compat: old W/V/D format (single hidden layer)
        W = _entries_to_coo(data.get("W", []), size_n, size_s).tocoo()
        V = _entries_to_coo(data.get("V", []), size_y, size_n).tocoo()
        D = _entries_to_coo(data.get("D", []), size_y, size_s).tocoo()
        for mat, fl, tl in [(W, 0, 1), (V, 1, 2), (D, 0, 2)]:
            for r, c, v in zip(mat.row, mat.col, mat.data):
                src = _node_name(fl, int(c))
                dst = _node_name(tl, int(r))
                v   = float(v)
                G.add_edge(src, dst, weight=v)
                alpha = float(np.clip(abs(v), 0.15, 1.0))
                edge_widths.append(float(np.clip(abs(v) * 2.5, 0.3, 3.0)))
                edge_colors.append((1.0, 0.0, 0.0, alpha) if v < 0
                                   else (0.0, 0.75, 0.0, alpha))

    # ── prune disconnected SENSOR and OUTPUT nodes; keep ALL hidden nodes ────
    # This ensures every hidden layer column is always rendered, even if silent.
    endpoints = {u for u, _ in G.edges()} | {v for _, v in G.edges()}
    for node in list(G.nodes()):
        nd = G.nodes[node]
        L  = nd["layer"]
        if (L == 0 or L == num_hidden + 1) and node not in endpoints:
            G.remove_node(node)

    # ── layout: fixed column per layer, each column spaced independently ────
    # Build per-layer node lists; guarantee every layer 0..num_layers-1 exists.
    by_layer: dict = {L: [] for L in range(num_layers)}
    for node, nd in G.nodes(data=True):
        by_layer[nd["layer"]].append((nd["idx"], node))
    for items in by_layer.values():
        items.sort()

    # Each column distributes its own nodes evenly over y=[0,1].
    # This avoids sensor/output nodes clustering at the top when hidden
    # layers are much larger.
    pos = {}
    for L in range(num_layers):
        items = by_layer[L]
        n = len(items)
        x = L / (num_layers - 1) if num_layers > 1 else 0.5
        for rank, (_, node) in enumerate(items):
            y = 1.0 - rank / (n - 1) if n > 1 else 0.5
            pos[node] = (x, y)

    if not pos:
        ax.text(0.5, 0.5, "No connections in this network",
                ha="center", va="center", transform=ax.transAxes, fontsize=11)
        fig.suptitle(
            f"Network {index+1}/{total}   |   pop:{pop_id}   |   gen:{generation}"
            f"\n[← / →] navigate    [q] quit",
            fontsize=10,
        )
        fig.canvas.draw_idle()
        return

    # ── node / font sizing ─────────────────────────────────────────────────
    max_col = max(len(v) for v in by_layer.values())  # tallest column
    node_area    = max(20, int(1400 / max(max_col, 1)))
    font_sz_io   = max(7.0, min(11.0, 300.0 / max(size_s, size_y, 1)))
    font_sz_hid  = max(3.5, min(7.0,  200.0 / max(max_col, 1)))
    label_offset = 0.04   # data-unit gap between node centre and label edge

    node_list = list(G.nodes())
    colors    = [node_colors_map.get(n, "gray") for n in node_list]

    nx.draw_networkx_nodes(G, pos, ax=ax, nodelist=node_list,
                           node_size=node_area, node_color=colors)

    if G.number_of_edges() > 0:
        nx.draw_networkx_edges(
            G, pos, ax=ax,
            connectionstyle="arc3,rad=0.05",
            arrowsize=8,
            arrowstyle="-|>",
            width=edge_widths,
            edge_color=edge_colors,
        )

    # ── draw labels: S/Y face outward, hidden centered above ───────────────
    for node in G.nodes():
        if node not in pos:
            continue
        x, y = pos[node]
        nd  = G.nodes[node]
        L   = nd["layer"]
        lbl = labels.get(node, "")
        if L == 0:
            ax.text(x - label_offset, y, lbl,
                    ha="right", va="center",
                    fontsize=font_sz_io, color="darkgoldenrod",
                    clip_on=False, transform=ax.transData)
        elif L == num_hidden + 1:
            ax.text(x + label_offset, y, lbl,
                    ha="left", va="center",
                    fontsize=font_sz_io, color="darkorange",
                    clip_on=False, transform=ax.transData)
        else:
            col_n = len(by_layer[L])
            if col_n <= 20:
                ax.text(x, y + 0.015, lbl,
                        ha="center", va="bottom",
                        fontsize=font_sz_hid, color="purple",
                        clip_on=False, transform=ax.transData)

    # ── layer headers for ALL layers (always visible) ─────────────────────
    layer_names = {0: "Sensors"}
    for h in range(1, num_hidden + 1):
        layer_names[h] = f"Hidden {h}"
    layer_names[num_hidden + 1] = "Outputs"

    for L in range(num_layers):
        x = L / (num_layers - 1) if num_layers > 1 else 0.5
        ax.text(x, 1.06, layer_names.get(L, f"L{L}"),
                ha="center", va="bottom",
                fontsize=font_sz_io, fontweight="bold",
                clip_on=False, transform=ax.transData)

    # ── axis limits: generous margins for side labels ─────────────────────
    ax.set_xlim(-0.30, 1.30)
    ax.set_ylim(-0.06, 1.15)

    fig.suptitle(
        f"Network {index+1}/{total}   |   pop:{pop_id}   |   generation:{generation}"
        f"\n[← / →] navigate    [q] quit",
        fontsize=10,
    )
    fig.canvas.draw_idle()



# ---------------------------------------------------------------------------
# Main viewer class
# ---------------------------------------------------------------------------

class NetworkViewer:
    def __init__(self, nnets_dir: str):
        self._snapshots = _load_all(nnets_dir)
        self._idx = 0
        self._fig, self._ax = plt.subplots(figsize=(16, 9))
        self._fig.patch.set_facecolor("lightblue")
        # Maximize the window (works with Qt5Agg backend)
        try:
            plt.get_current_fig_manager().window.showMaximized()
        except Exception:
            pass
        self._fig.canvas.mpl_connect("key_press_event", self._on_key)
        self._refresh()

    def _refresh(self):
        _draw(self._ax, self._fig,
              self._snapshots[self._idx][1],
              self._idx,
              len(self._snapshots))

    def _on_key(self, event):
        if event.key in ("right", "l"):
            self._idx = (self._idx + 1) % len(self._snapshots)
            self._refresh()
        elif event.key in ("left", "h"):
            self._idx = (self._idx - 1) % len(self._snapshots)
            self._refresh()
        elif event.key == "q":
            plt.close(self._fig)

    def show(self):
        plt.show()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    nnets_dir = sys.argv[1] if len(sys.argv) > 1 else "../data_out/nnets/"
    viewer = NetworkViewer(nnets_dir)
    viewer.show()


if __name__ == "__main__":
    main()
