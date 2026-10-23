'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const context = {window: {}};
vm.runInNewContext(fs.readFileSync('dashboard/static/labels.js', 'utf8'), context);
const current = context.window.sensorLabelsForSize(57);
const legacy = context.window.sensorLabelsForSize(49);
const previousModel = context.window.sensorLabelsForSnapshot({brain: {sizeS: 57}});
for (const [index, type] of ['FOOD', 'DANGER', 'MATE', 'HOME'].entries())
    assert.match(current[24 + index], new RegExp(`DENSITY_${type}`));
assert.match(current[37], /DANGER_DRV_N/);
assert.match(current[41], /MATE_DRV_N/);
assert.match(current[45], /HOME_DRV_N/);
assert.match(current[32], /LAST_MOVE_FAILED/);
assert.match(legacy[32], /LEGACY_RESERVED_INPUT_32/);
assert.match(previousModel[24], /gradiente FOOD \(legacy\)/);
assert.match(previousModel[27], /DRV_S/);
assert.equal(current[57], undefined);
assert.match(context.window.sensorLabelsForSize(58)[57], /non implementato/);
assert.match(context.window.ACTION_LABELS[20], /raggio sensoriale/);
console.log('PASS: pheromone channels, directional gradients, legacy slot and sensor sentinel labels');
