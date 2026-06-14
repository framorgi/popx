#!/usr/bin/env python3
"""Neural network history viewer for popx.

Usage:
    python3 plot_net.py [nnets_dir]

    nnets_dir  Path to the directory containing JSON network snapshots.
               Defaults to  ../data_out/nnets/

Keyboard controls (when the window is focused):
    →  or  l   Next network
    ←  or  h   Previous network
    ↑  or  k   Highlight next connection
    ↓  or  j   Highlight previous connection
    f           Toggle incomplete-connection filter
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

_SUBPLOT_KW = dict(left=0.18, right=0.82, top=0.91, bottom=0.04)

def _draw(ax, fig, data: dict, index: int, total: int,
          filter_incomplete: bool = False, highlight_conn_idx=None):
    ax.clear()
    ax.axis("off")
    ax.set_facecolor("lavenderblush")
    # Lock layout so suptitle text changes never cause a window resize.
    fig.subplots_adjust(**_SUBPLOT_KW)

    # ── filter-mode banner (drawn in axes-normalised coordinates) ─────────
    if filter_incomplete:
        ax.text(
            0.5, 0.995,
            "  ▶  FILTER ON — showing only connections that reach an output  ◀  ",
            ha="center", va="top", transform=ax.transAxes,
            fontsize=10, fontweight="bold", color="white", clip_on=False,
            bbox=dict(boxstyle="round,pad=0.25", facecolor="steelblue", alpha=0.88),
        )

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

    # ── compute productive set: only nodes on COMPLETE sensor→output paths ──
    # A node is useful iff it is both reachable FROM a sensor AND can reach an
    # output.  This excludes dead-end hidden chains and sensor-side stubs.
    _sensor_nodes = {n for n, d in G.nodes(data=True) if d["layer"] == 0}
    _out_nodes    = {n for n, d in G.nodes(data=True)
                     if d["layer"] == num_hidden + 1}
    _can_reach_output = set(_out_nodes)
    for _on in _out_nodes:
        _can_reach_output |= nx.ancestors(G, _on)
    _reachable_from_sensor = set(_sensor_nodes)
    for _sn in _sensor_nodes:
        _reachable_from_sensor |= nx.descendants(G, _sn)
    _productive = _can_reach_output & _reachable_from_sensor
    useful_edge_count = sum(1 for u, v in G.edges()
                            if u in _productive and v in _productive)
    total_edge_count  = G.number_of_edges()

    # ── filter: keep only nodes/edges on complete sensor→output paths ──────
    if filter_incomplete:
        G.remove_edges_from([(u, v) for u, v in list(G.edges())
                              if u not in _productive or v not in _productive])
        G.remove_nodes_from([n for n in list(G.nodes()) if n not in _productive])
        # Rebuild style lists to match the filtered edge set
        edge_widths.clear()
        edge_colors.clear()
        for _u, _v, _d in G.edges(data=True):
            _w = _d.get("weight", 0.0)
            _a = float(np.clip(abs(_w), 0.15, 1.0))
            edge_widths.append(float(np.clip(abs(_w) * 2.5, 0.3, 3.0)))
            edge_colors.append((1.0, 0.0, 0.0, _a) if _w < 0
                               else (0.0, 0.75, 0.0, _a))

    # ── prune disconnected SENSOR and OUTPUT nodes; keep ALL hidden nodes ────
    # This ensures every hidden layer column is always rendered, even if silent.
    endpoints = {u for u, _ in G.edges()} | {v for _, v in G.edges()}
    for node in list(G.nodes()):
        nd = G.nodes[node]
        L  = nd["layer"]
        if (L == 0 or L == num_hidden + 1) and node not in endpoints:
            G.remove_node(node)

    # ── apply per-connection / per-path highlight (↑/↓ scrolling) ────────
    n_drawn_edges = len(edge_widths)
    if filter_incomplete:
        # Build an ordered edge→index map that matches edge_widths/edge_colors.
        _edge_index = {(u, v): i for i, (u, v) in enumerate(G.edges())}
        # Enumerate all simple sensor→output paths in the filtered graph.
        _useful_paths: list = []
        for _sn in sorted(_sensor_nodes & set(G.nodes())):
            for _tn in sorted(_out_nodes & set(G.nodes())):
                for _p in nx.all_simple_paths(G, _sn, _tn, cutoff=num_layers + 1):
                    _useful_paths.append(_p)
        n_highlight_items = len(_useful_paths)
        if highlight_conn_idx is not None and n_highlight_items > 0:
            hi      = highlight_conn_idx % n_highlight_items
            hi_path = _useful_paths[hi]
            hi_set  = {_edge_index[(hi_path[k], hi_path[k + 1])]
                       for k in range(len(hi_path) - 1)
                       if (hi_path[k], hi_path[k + 1]) in _edge_index}
            edge_colors = [(r, g, b, a) if i in hi_set else (r, g, b, 0.04)
                           for i, (r, g, b, a) in enumerate(edge_colors)]
            edge_widths = [w if i in hi_set else 0.3
                           for i, w in enumerate(edge_widths)]
    else:
        _useful_paths     = []
        n_highlight_items = n_drawn_edges
        if highlight_conn_idx is not None and n_drawn_edges > 0:
            h = highlight_conn_idx % n_drawn_edges
            edge_colors = [(r, g, b, 0.04) if i != h else (r, g, b, a)
                           for i, (r, g, b, a) in enumerate(edge_colors)]
            edge_widths = [0.3 if i != h else w
                           for i, w in enumerate(edge_widths)]

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
            f"  {'[filter: ON]' if filter_incomplete else ''}"
            f"   useful: {useful_edge_count}/{total_edge_count}"
            f"\n[← / →] navigate    [↑ / ↓] highlight connection    [f] filter    [q] quit",
            fontsize=10,
        )
        fig.canvas.draw_idle()
        return n_highlight_items

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

    if filter_incomplete and n_highlight_items > 0 and highlight_conn_idx is not None:
        conn_info = f"   path {(highlight_conn_idx % n_highlight_items) + 1}/{n_highlight_items}"
    elif not filter_incomplete and n_drawn_edges > 0 and highlight_conn_idx is not None:
        conn_info = f"   conn {(highlight_conn_idx % n_drawn_edges) + 1}/{n_drawn_edges}"
    else:
        conn_info = ""
    fig.suptitle(
        f"Network {index+1}/{total}   |   pop:{pop_id}   |   generation:{generation}"
        f"  {'[filter: ON]' if filter_incomplete else ''}"
        f"   useful: {useful_edge_count}/{total_edge_count}{conn_info}"
        f"\n[← / →] navigate    [↑ / ↓] {'path' if filter_incomplete else 'conn'}    [f] filter    [q] quit",
        fontsize=10,
    )
    fig.canvas.draw_idle()
    return n_highlight_items



# ---------------------------------------------------------------------------
# Main viewer class
# ---------------------------------------------------------------------------

class NetworkViewer:
    def __init__(self, nnets_dir: str):
        self._snapshots = _load_all(nnets_dir)
        self._idx        = 0
        self._filter     = False
        self._conn_idx   = None   # None = no highlight
        self._conn_total = 0
        plt.rcParams["figure.autolayout"] = False
        self._fig, self._ax = plt.subplots(figsize=(16, 9))
        self._fig.patch.set_facecolor("lightblue")
        self._fig.subplots_adjust(**_SUBPLOT_KW)
        # Full-screen window — called once; never called again so no resize on redraws.
        try:
            plt.get_current_fig_manager().window.showFullScreen()
        except Exception:
            pass
        self._fig.canvas.mpl_connect("key_press_event", self._on_key)
        self._refresh()

    def _refresh(self):
        self._conn_total = _draw(
            self._ax, self._fig,
            self._snapshots[self._idx][1],
            self._idx,
            len(self._snapshots),
            filter_incomplete=self._filter,
            highlight_conn_idx=self._conn_idx,
        )

    def _on_key(self, event):
        if event.key in ("right", "l"):
            self._idx      = (self._idx + 1) % len(self._snapshots)
            self._conn_idx = None
            self._refresh()
        elif event.key in ("left", "h"):
            self._idx      = (self._idx - 1) % len(self._snapshots)
            self._conn_idx = None
            self._refresh()
        elif event.key == "f":
            self._filter = not self._filter
            # Preserve _conn_idx so scrolling continues in filtered mode.
            # _conn_total is refreshed by _refresh(); modulo keeps idx in range.
            self._refresh()
        elif event.key in ("up", "k"):
            if self._conn_total > 0:
                self._conn_idx = (0 if self._conn_idx is None
                                  else (self._conn_idx + 1) % self._conn_total)
                self._refresh()
        elif event.key in ("down", "j"):
            if self._conn_total > 0:
                self._conn_idx = (self._conn_total - 1 if self._conn_idx is None
                                  else (self._conn_idx - 1) % self._conn_total)
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
