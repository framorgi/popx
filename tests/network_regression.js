'use strict';
const assert = require('node:assert/strict');
const {prepare, formatWeight} = require('../dashboard/static/network.js');
const cytoscape = require('../dashboard/static/vendor/cytoscape.min.js');
const brain = {connections: [{from_layer: 0, to_layer: 1, entries: [
    {col: 0, row: 0, value: 0}, {col: 1, row: 0, value: 1e-12},
    {col: 2, row: 0, value: -2}, {col: 3, row: 0, value: 4}
]}]};
const full = prepare(brain, 0, true);
assert.equal(full.edges.length, 4);
assert.equal(full.zeroCount, 1);
assert.equal(full.edges[0].data.lineStyle, 'dashed');
assert.equal(prepare(brain, 0, false).edges.length, 3);
assert.equal(prepare(brain, 1e-12, false).edges.length, 3); // Inclusive boundary, tiny nonzero preserved.
assert.equal(prepare(brain, 1e-11, false).edges.length, 2);
const filtered = prepare(brain, 2, false);
assert.deepEqual(filtered.edges.map(e => e.data.weight), [-2, 4]);
assert.equal(filtered.edges[0].data.width, 3);
assert.equal(filtered.edges[1].data.width, 6);
assert.equal(filtered.edges[0].data.width, full.edges[2].data.width); // Filtering does not rescale.
assert.equal(prepare(brain, 5, true).edges.length, 0);
assert.throws(() => prepare(brain, NaN, true));
assert.throws(() => prepare(brain, -1, true));
assert.match(formatWeight(1e-12), /e-12$/);
assert.equal(formatWeight(0), '0 (esatto)');
const zero = prepare({connections: [{from_layer: 0, to_layer: 1, entries: [{col: 0, row: 0, value: 0}]}]}, 0, true);
assert.equal(zero.edges[0].data.width, 0.4);
assert.equal(zero.maxAbs, 0);
const pathways = {num_hidden_layers:2,connections:[
    {from_layer:0,to_layer:1,entries:[
        {col:0,row:0,value:1}, {col:1,row:1,value:3}, {col:4,row:3,value:1}]},
    {from_layer:1,to_layer:2,entries:[{col:3,row:1,value:1}]},
    {from_layer:0,to_layer:2,entries:[{col:3,row:0,value:0}]},
    {from_layer:1,to_layer:3,entries:[
        {col:0,row:0,value:2}, {col:2,row:1,value:4}]},
    {from_layer:2,to_layer:3,entries:[
        {col:0,row:3,value:6}, {col:1,row:4,value:1}, {col:2,row:5,value:-1}]},
    {from_layer:0,to_layer:3,entries:[{col:2,row:2,value:5}]}
]};
const allPaths=prepare(pathways,0,true);
const outputPaths=prepare(pathways,0,true,'output');
const sensorPaths=prepare(pathways,0,true,'sensor_path');
assert.equal(allPaths.edges.length,11);
assert.equal(outputPaths.outputCount,9);
assert.equal(sensorPaths.pathCount,6);
assert.equal(outputPaths.edges.length,9);
assert.equal(sensorPaths.edges.length,6);
assert(outputPaths.edges.some(e=>e.data.id==='1:2>3:1')); // Hidden sigmoid(0) can reach an output.
assert(!sensorPaths.edges.some(e=>e.data.id==='1:2>3:1'));
assert(!outputPaths.edges.some(e=>e.data.id==='0:1>1:1')); // Dead-end branch.
assert(!sensorPaths.edges.some(e=>e.data.id==='0:3>2:0')); // Zero cannot complete a path.
assert.deepEqual(prepare(pathways,2,false,'sensor_path').edges.map(e=>e.data.id),
                 ['1:0>3:0','0:2>3:2']);
assert.throws(()=>prepare(pathways,0,false,'unknown'));
// The actual vendored Cytoscape accepts the same directed graph and numeric weight data.
const graph = cytoscape({headless: true, elements: [
    ...[...filtered.connected].map(id => ({data: {id}})), ...filtered.edges
]});
assert.equal(graph.edges().length, 2);
assert.equal(graph.nodes().length, 3);
assert.equal(graph.edges('[weight < 0]').length, 1);
graph.destroy();
console.log('PASS: exact zeros, weight thresholds, output influence, complete sensor paths and Cytoscape graph');
