#!/usr/bin/env python3
"""Read-only live NN probe: does one newly passable direction cause movement toward it?

The counterfactual changes only MOVE_OPEN_N..NW while holding the last sensed
environment fixed. It measures the policy circuit, not realized movement or fitness.
"""

import argparse
import json
import math
import random
import urllib.error
import urllib.request


DIRECTIONS = ((0, -1), (1, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1))


def get_json(url, payload=None):
    body = None if payload is None else json.dumps(payload).encode()
    request = urllib.request.Request(url, body, {"Content-Type": "application/json"})
    with urllib.request.urlopen(request, timeout=20) as response:
        return json.load(response)


def connections(brain):
    edges = [[] for _ in range(brain["num_hidden_layers"] + 2)]
    for matrix in brain["connections"]:
        for edge in matrix["entries"]:
            if edge["value"]:
                edges[matrix["to_layer"]].append(
                    (matrix["from_layer"], edge["col"], edge["row"], edge["value"])
                )
    return edges


def choice(brain, edges, sensors):
    hidden = [sensors]
    output_layer = brain["num_hidden_layers"] + 1
    for layer in range(1, output_layer + 1):
        sums = [0.0] * (brain["sizeY"] if layer == output_layer else brain["sizeN"])
        for source, column, row, weight in edges[layer]:
            sums[row] += weight * hidden[source][column]
        hidden.append([1.0 / (1.0 + math.exp(-max(-700.0, min(700.0, value)))) for value in sums])
    outputs = hidden[-1]
    best = max(outputs)
    winners = [index for index, value in enumerate(outputs) if abs(value - best) < 1e-12]
    return winners[0] if len(winners) == 1 and best != 0.5 else 21


def intended_direction(snapshot, action):
    dx, dy = snapshot["direction"]
    dx, dy = (dx > 0) - (dx < 0), (dy > 0) - (dy < 0)
    if action == 0:
        return dx, dy
    if action == 1:
        return -dy, dx
    if action == 2:
        return dy, -dx
    return {4: (1, 0), 5: (-1, 0), 6: (0, -1), 7: (0, 1)}.get(action)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8000")
    parser.add_argument("--sample", type=int, default=80)
    parser.add_argument("--seed", type=int, default=20261021)
    args = parser.parse_args()
    if args.sample < 1:
        parser.error("--sample must be positive")
    url = args.url.rstrip("/")
    status = get_json(url + "/api/status")
    run = status["run_id"]

    def command(name, arguments=None):
        return get_json(url + "/api/commands", {
            "command": name, "run_id": run, "arguments": arguments or {}
        })["result"]

    roster = command("agents")
    randomizer = random.Random(args.seed)
    selected = randomizer.sample(roster["agents"], min(args.sample, len(roster["agents"])))
    counts = {key: 0 for key in ("inspected", "matched_action", "opened_cases", "switches",
                                  "movement_choices", "correct", "causal_correct")}
    unavailable = 0
    ticks = []
    for agent in selected:
        try:
            snapshot = command("inspect", {"pop_id": agent["pop_id"]})
        except urllib.error.HTTPError as error:
            if error.code == 409:  # The agent died after the roster was read.
                unavailable += 1
                continue
            raise
        brain = snapshot["brain"]
        if brain["sizeS"] < 57:
            raise RuntimeError("The active brain lacks MOVE_OPEN_N..NW")
        edges = connections(brain)
        sensors = snapshot["sensors"]
        counts["inspected"] += 1
        ticks.append(snapshot["tick"])
        counts["matched_action"] += choice(brain, edges, sensors) == snapshot["last_action"]
        blocked = sensors.copy()
        blocked[49:57] = [0.0] * 8
        baseline = choice(brain, edges, blocked)
        for direction, vector in enumerate(DIRECTIONS):
            opened = blocked.copy()
            opened[49 + direction] = 1.0
            action = choice(brain, edges, opened)
            counts["opened_cases"] += 1
            counts["switches"] += action != baseline
            counts["movement_choices"] += action in range(8)
            correct = intended_direction(snapshot, action) == vector
            counts["correct"] += correct
            counts["causal_correct"] += correct and action != baseline

    print(f"run={run} roster_tick={roster['tick']} inspected={counts['inspected']} "
          f"unavailable={unavailable} snapshot_ticks={min(ticks) if ticks else 0}..{max(ticks) if ticks else 0}")
    print(" ".join(f"{name}={value}" for name, value in counts.items() if name != "inspected"))
    print("Il test cambia solo gli otto ingressi di percorribilità. "
          "Un movimento corretto è causale solo se differisce dall'azione con tutte le direzioni bloccate.")


if __name__ == "__main__":
    main()
