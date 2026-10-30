'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const context = {window: {POPX_DESIGN: JSON.parse(fs.readFileSync("dashboard/static/design.json", "utf8"))}};
vm.runInNewContext(fs.readFileSync('dashboard/static/labels.js', 'utf8'), context);
const current = context.window.sensorLabelsForSize(57);
const legacy = context.window.sensorLabelsForSize(49);
const previousModel = context.window.sensorLabelsForSnapshot({brain: {sizeS: 57}});
for (const [index, type] of ['FOOD', 'DANGER', 'MATE', 'HOME'].entries())
    assert.match(current[24 + index], new RegExp(`${type} intorno`));
assert.match(current[37], /Grad\. DANGER N/);
assert.match(current[41], /Grad\. MATE N/);
assert.match(current[45], /Grad\. HOME N/);
assert.match(current[32], /Passo fallito/);
assert.match(legacy[32], /Ingresso riservato/);
assert.match(previousModel[24], /Grad\. FOOD N \(precedente\)/);
assert.match(previousModel[27], /Grad\. FOOD S/);
assert.equal(current[57], undefined);
assert.match(context.window.sensorLabelsForSize(58)[57], /non implementato/);
assert.match(context.window.ACTION_LABELS[20], /sensibilità/);
console.log('PASS: pheromone channels, directional gradients, legacy slot and sensor sentinel labels');
