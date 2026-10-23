'use strict';
// Pure graph preparation, shared by the browser and standalone regression checks.
const PopXNetwork = (() => {
    function formatWeight(value) {
        if (value === 0) return '0 (esatto)';
        return Math.abs(value) < 0.001 ? value.toExponential(6) : Number(value.toPrecision(7)).toString();
    }
    function reachable(starts, neighbors) {
        const seen = new Set(starts), queue = [...starts];
        for (let i = 0; i < queue.length; ++i) {
            for (const next of neighbors.get(queue[i]) || []) {
                if (seen.has(next)) continue;
                seen.add(next);
                queue.push(next);
            }
        }
        return seen;
    }
    function prepare(brain, threshold, includeZero, mode = 'all') {
        if (!Number.isFinite(threshold) || threshold < 0) throw new Error('Inserisci una soglia finita maggiore o uguale a zero.');
        if (!['all', 'output', 'sensor_path'].includes(mode)) throw new Error('Filtro dei percorsi neurali sconosciuto.');
        if (mode !== 'all' && !Number.isInteger(brain.num_hidden_layers))
            throw new Error('Lo snapshot non contiene il numero di strati nascosti.');
        const entries = [];
        for (const connection of brain.connections || []) {
            for (const entry of connection.entries || []) {
                if (!Number.isFinite(entry.value)) continue;
                entries.push({
                    id: `${connection.from_layer}:${entry.col}>${connection.to_layer}:${entry.row}`,
                    source: `${connection.from_layer}:${entry.col}`,
                    target: `${connection.to_layer}:${entry.row}`,
                    weight: entry.value
                });
            }
        }
        const maxAbs = entries.reduce((max, e) => Math.max(max, Math.abs(e.weight)), 0);
        const zeroCount = entries.filter(e => e.weight === 0).length;
        const forward = new Map(), backward = new Map(), inputNodes = new Set(), outputNodes = new Set();
        const outputLayer = brain.num_hidden_layers + 1;
        for (const entry of entries) {
            if (entry.weight === 0) continue; // A zero-weight edge cannot carry a signal.
            if (!forward.has(entry.source)) forward.set(entry.source, []);
            if (!backward.has(entry.target)) backward.set(entry.target, []);
            forward.get(entry.source).push(entry.target);
            backward.get(entry.target).push(entry.source);
            if (entry.source.startsWith('0:')) inputNodes.add(entry.source);
            if (entry.target.startsWith(`${outputLayer}:`)) outputNodes.add(entry.target);
        }
        const fromInput = reachable(inputNodes, forward);
        const toOutput = reachable(outputNodes, backward);
        const outputEdges = entries.filter(e => e.weight !== 0 && toOutput.has(e.target));
        const pathEdges = outputEdges.filter(e => fromInput.has(e.source));
        const allowed = new Set((mode === 'output' ? outputEdges : pathEdges).map(e => e.id));
        const edges = entries.filter(e => Math.abs(e.weight) >= threshold && (includeZero || e.weight !== 0) &&
            (mode === 'all' || allowed.has(e.id)))
            .map(e => ({data: {
                ...e, weightLabel: formatWeight(e.weight),
                // Scale uses the full snapshot, so filtering cannot inflate the remaining weights.
                width: maxAbs ? Math.max(0.4, 6 * Math.abs(e.weight) / maxAbs) : 0.4,
                color: e.weight === 0 ? '#8792a2' : e.weight > 0 ? '#48d9b8' : '#fb8b83',
                lineStyle: e.weight === 0 ? 'dashed' : 'solid'
            }}));
        return {edges, total: entries.length, zeroCount, maxAbs,
            outputCount: outputEdges.length, pathCount: pathEdges.length,
            connected: new Set(edges.flatMap(e => [e.data.source, e.data.target]))};
    }
    return {formatWeight, prepare};
})();
if (typeof module !== 'undefined') module.exports = PopXNetwork;
