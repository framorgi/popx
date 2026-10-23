'use strict';
const assert = require('node:assert/strict');
const inspector = require('../dashboard/static/inspector.js');

assert.deepEqual(inspector.geneticColor([]), [0, 0, 0]);
assert.equal(inspector.geneticColor(undefined), null);
assert.deepEqual(inspector.geneticColor([[3, 0xffffffff, 7, 0x8000000e, -32768]]), [182, 252, 254]);
assert.deepEqual(inspector.geneticColor([[0, 1, 1, 2, 1], [2, 4, 7, 5, 0]]), [4, 60, 21]);

const brain = {sizeS: 49, sizeN: 16, sizeY: 22, num_hidden_layers: 4,
    connections: [{from_layer: 0, to_layer: 5, entries: [{row: 21, col: 48, value: 0.125}]}]};
const snapshot = {genes: [
    [0, 48, 7, 21, 8192], [0, 48, 7, 21, -8192], [0, 48, 7, 21, 0],
    [0, 49, 7, 0, 1], [0, 0, 7, 22, 1], [4, 16, 7, 0, 1],
    [7, 0, 7, 0, 1], [5, 0, 7, 0, 1], [0, 0, 5, 0, 1],
    [1, 0, 0, 0, 1], [2, 0, 2, 0, 1], [3, 0, 2, 0, 1],
    [4, 15, 7, 21, -1], [0, 0xffffffff, 7, 0, 1]
], brain, traits: {chloroplasts: 12, mitochondria: 12}, direction: [3, -4],
    totals: {movement_successes: 2}, alive: true};
const entries = inspector.genome(snapshot, {actions: {21: 'DO_NOTHING'}});
assert.equal(entries.length, snapshot.genes.length);
assert.deepEqual(entries.map(e => e.status), ['wired', 'wired', 'wired', 'silent', 'silent', 'silent',
    'discarded', 'discarded', 'discarded', 'discarded', 'discarded', 'discarded', 'wired', 'silent']);
assert.deepEqual(entries[0].group, {count: 3, weight: 0});
assert.equal(entries[2].currentWeight, 0.125); // A zero gene need not mean a zero learned edge.
assert.equal(entries[2].weight, 0);
assert.equal(entries[12].weight, -1 / 8192);
assert.match(entries[0].target, /DO_NOTHING/);
assert.equal(inspector.classify([0, 0, 7, 0, 0], null).status, 'unknown');
assert.equal(inspector.classify([6, 15, 7, 21, 1], {...brain, num_hidden_layers: 6}).status, 'wired');
const circle = inspector.avatar(snapshot);
assert.equal(circle.shape, 'circle'); // Equal organelles are a circle in SFML.
assert.equal(circle.points.length, 11);
assert.deepEqual(circle.unit, [0.6, -0.8]); // North means negative y, east positive x.
assert.equal(circle.initial, false);
const triangle = inspector.avatar({...snapshot, traits: {chloroplasts: 13, mitochondria: 12}, alive: false,
    totals: {movement_successes: 0}, direction: [0, 0]});
assert.equal(triangle.shape, 'triangle');
assert.deepEqual(triangle.points, [[0, -1], [1, 1], [-1, 1]]);
assert.deepEqual(triangle.color, [0, 0, 0]);
assert.equal(triangle.unit, null);
assert.equal(triangle.initial, true);
assert.equal(inspector.avatar({}).direction, null);

// Minimal DOM contract to exercise actual render handlers, selection and reset without browser dependencies.
class Element {
    constructor(tag) {
        this.tag = tag; this.children = []; this.attrs = {}; this.style = {}; this.classes = new Set();
        this.classList = {toggle: (name, on) => on ? this.classes.add(name) : this.classes.delete(name)};
    }
    append(...children) { this.children.push(...children); }
    replaceChildren(...children) { this.children = children; }
    setAttribute(key, value) { this.attrs[key] = value; }
}
global.document = {createElement: tag => new Element(tag), createElementNS: (_, tag) => new Element(tag)};
const container = new Element('div');
inspector.renderGenome(container, snapshot);
const [sortLabel, directionLabel] = container.children[1].children;
const sort = sortLabel.children[0], direction = directionLabel.children[0];
const [bars, detail] = container.children[2].children;
assert.equal(bars.children.length, snapshot.genes.length);
bars.children[0].onmouseenter();
assert.match(detail.children[0].textContent, /Gene 1 · anteprima/);
bars.children[0].onclick();
bars.children[1].onmouseenter();
assert.equal(bars.children[0].attrs['aria-pressed'], 'true');
assert.match(detail.children[0].textContent, /Gene 2 · anteprima/);
bars.children[1].onmouseleave();
assert.match(detail.children[0].textContent, /Gene 1 · selezionato/);
bars.children[2].onfocus();
assert.match(detail.children[0].textContent, /Gene 3/);
bars.children[2].onclick();
bars.children[2].onblur();
assert.match(detail.children[0].textContent, /Gene 3 · selezionato/);
const originalButtons = [...bars.children];
sort.value = 'weight';
sort.onchange();
assert.equal(bars.children[0], originalButtons[1]);
assert.equal(bars.children.at(-1), originalButtons[0]);
assert.equal(originalButtons[2].attrs['aria-pressed'], 'true');
assert.match(detail.children[0].textContent, /Gene 3 · selezionato/);
direction.value = 'desc';
direction.onchange();
assert.equal(bars.children[0], originalButtons[0]);
assert.equal(bars.children.at(-1), originalButtons[1]);
bars.children.at(-1).onclick();
assert.match(detail.children[0].textContent, /Gene 2 · selezionato/);
assert.equal(originalButtons[1].attrs['aria-pressed'], 'true');
assert.equal(originalButtons[2].attrs['aria-pressed'], 'false');
sort.value = 'index'; direction.value = 'asc'; sort.onchange();
assert.deepEqual(bars.children, originalButtons);
assert.equal(inspector.sortGenes(entries, 'absolute', 'desc')[0].index, 0);
assert.equal(inspector.sortGenes(entries, 'absolute', 'desc')[1].index, 1); // Stable original order on ties.
assert.equal(inspector.sortGenes(entries, 'source', 'desc')[0].gene[0], 7);
assert.equal(inspector.sortGenes(entries, 'target')[0].gene[2], 0);
assert.equal(inspector.sortGenes(entries, 'status')[0].status, 'wired');
assert.equal(inspector.sortGenes(entries, 'status', 'desc')[0].status, 'discarded');
assert.deepEqual(entries.map(e => e.index), snapshot.genes.map((_, i) => i)); // Never mutate the genome.
inspector.renderGenome(container, null);
assert.equal(container.children.length, 1);
assert.match(container.children[0].textContent, /Seleziona/);
inspector.renderGenome(container, {});
assert.match(container.children[0].textContent, /non è disponibile/);
inspector.renderAvatar(container, snapshot);
const svg = container.children[0].children[0];
assert.equal(svg.tag, 'svg');
assert.equal(svg.children.filter(e => e.tag === 'polygon').length, 2); // Body and direction arrow.
inspector.renderAvatar(container, null);
assert.equal(container.children.length, 0);
console.log('Inspector regression passed: color, SFML shape, direction, all genes, weights, hover/click/focus/reset.');
