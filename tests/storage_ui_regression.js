'use strict';
const assert = require('assert'), fs = require('fs'), vm = require('vm');
class Element {
    constructor(tag = 'div') { this.tag = tag; this.value = ''; this.children = []; this.classList = {add() {}}; }
    append(...items) { this.children.push(...items); }
    replaceChildren(...items) { this.children = items; if (this.tag === 'select') this.value = items[0]?.value || ''; }
    setAttribute() {}
}
const nodes = new Map(), get = id => {
    if (!nodes.has(id)) nodes.set(id, new Element(['storage-run', 'storage-checkpoint'].includes(id) ? 'select' : 'div'));
    return nodes.get(id);
};
const runs = [{id: 'live', state: 'running', created: 1}, {id: 'old', state: 'finished', created: 0}];
let confirmed = false, deletes = [], cleans = 0;
const report = {disk_total: 1000, disk_available: 700, disk_unavailable: 300, database_files_bytes: 100,
    database_reusable_bytes: 20, measured_at: 1, run_count: 2, removals: [], guard: {limit_bytes: 1000, limit_gb: 10}};
const context = {
    document: {getElementById: get, createElement: tag => new Element(tag),
        querySelector: () => get('aside'), addEventListener() {}},
    window: {confirm: () => confirmed}, setInterval() {},
    fetch: async (url, options) => {
        let data = url === '/api/storage' ? report : url === '/api/status' ?
            {run_id: 'live', state: 'running', connected: true} :
            url === '/api/checkpoints' ? {checkpoints: [{id: 'cp', created: 1, tick: 10, bytes: 100, automatic: false}],
                operation: {stage: 'idle'}} : url.startsWith('/api/snapshots?') ? [] : runs;
        if (options?.method === 'DELETE') {
            const run = url.split('/').pop(); assert.strictEqual(JSON.parse(options.body).confirm_run, run);
            deletes.push(run); runs.splice(runs.findIndex(item => item.id === run), 1);
            report.removals = [{run, state: 'deleted', rows_deleted: 1}]; data = {state: 'deleting'};
        } else if (url === '/api/storage/clean') {
            assert.strictEqual(JSON.parse(options.body).confirm, 'clean_history'); cleans++;
            data = {run_id: 'new', operation: {stage: 'done'}};
        }
        return {ok: true, json: async () => data};
    }
};
vm.runInNewContext(fs.readFileSync('dashboard/static/storage.js', 'utf8'), context);
(async () => {
    for (let index = 0; index < 10; index++) await new Promise(resolve => setImmediate(resolve));
    assert.strictEqual(get('storage-meter').children[0].value, 300);
    assert.strictEqual(get('storage-delete').disabled, false, 'connected run can be removed');
    get('storage-run').value = 'old'; get('storage-run').onchange();
    await get('storage-delete').onclick(); assert.strictEqual(deletes.length, 0);
    confirmed = true; await get('storage-delete').onclick(); assert.deepStrictEqual(deletes, ['old']);
    assert(get('storage-jobs').children[0].textContent.includes('rimossa'));
    await get('storage-delete').onclick(); assert.deepStrictEqual(deletes, ['old', 'live']);
    await get('storage-clean').onclick(); assert.strictEqual(cleans, 1);
    console.log('PASS: unified storage widget, connected-run removal, confirmation and full-clean request');
})().catch(error => { console.error(error); process.exitCode = 1; });
