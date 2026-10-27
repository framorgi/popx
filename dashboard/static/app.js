'use strict';
// Served as a static page so the current receiver can keep running during UI updates.
document.addEventListener('DOMContentLoaded', () => {
    const storageStyle=document.createElement('link');storageStyle.rel='stylesheet';storageStyle.href='/static/storage.css';document.head.append(storageStyle);
    const storageScript=document.createElement('script');storageScript.src='/static/storage.js';document.head.append(storageScript);
    const link = document.createElement('a');
    link.href = '/static/evolution.html';
    link.textContent = 'Linee evolutive';
    link.style.cssText = 'display:block;color:inherit;text-decoration:none;padding:9px 13px;border:1px solid #35465b;border-radius:7px';
    document.querySelector('aside nav').append(link);
});
const $ = id => document.getElementById(id);
const state = {
    status: {},
    run: null,
    runs: [],
    samples: [],
    extinctions: [],
    showExtinctions: true,
    historyRequest: 0,
    forcedBirthsSeen: 0,
    charts: [],
    agents: [],
    selected: null,
    snapshot: null,
    config: null,
    cy: null,
    eventAfter: 0
};
state.requests = new Set();
state.closed = false;
const palette = ['#48d9b8', '#88aaff', '#ffc778', '#fb8b83', '#c5a1f5', '#8dd7ef', '#e7a5c9'];
function metricInfo(path) {
    return state.metrics?.[path] || {label: `Misura: ${path}`, description: 'Definizione non disponibile per questa versione.'};
}
function describeMetric(container, info) {
    container.replaceChildren();
    for (const [label, key] of [['Calcolo', 'calculation'], ['Cosa rappresenta', 'meaning'], ['Come leggerlo', 'interpretation']]) {
        const paragraph = el('p');
        paragraph.append(el('strong', `${label}: `), document.createTextNode(info[key] || info.description));
        container.append(paragraph);
    }
}
function notice(text, error = false) {
    $('notice').hidden = false;
    $('notice').textContent = text;
    $('notice').style.borderColor = error ? '#fb8b83' : '#48d9b8';
}
async function api(url, options = {}) {
    if (state.closed) throw new DOMException('Dashboard chiusa', 'AbortError');
    const controller = new AbortController();state.requests.add(controller);
    try {
        const response = await fetch(url, {...options, signal:controller.signal, headers: {'Content-Type': 'application/json', ...options.headers}});
        const data = await response.json();
        if (state.closed) throw new DOMException('Dashboard chiusa', 'AbortError');
        if (!response.ok) throw new Error(typeof data.detail === 'string' ? data.detail : JSON.stringify(data.detail));
        return data;
    } finally {state.requests.delete(controller);}
}
function guard(fn) {
    return async (...args) => {
        try {
            await fn(...args);
        } catch (error) {
            if (!state.closed && error.name !== 'AbortError') notice(error.message, true);
        }
    };
}
function fmt(value) {
    if (typeof value === 'number' && value !== 0 && Math.abs(value) < .01)
        return value.toExponential(3);
    return typeof value === 'number' ? value.toLocaleString('it-IT', {maximumFractionDigits: 2}) : String(value ?? '—');
}
function at(object, path) {
    return path.split('.').reduce((x, k) => x?.[k], object) ?? null;
}
function el(tag, text, className) {
    const n = document.createElement(tag);
    if (text !== undefined) n.textContent = text;
    if (className) n.className = className;
    return n;
}
async function command(name, args = {}) {
    const reply = await api(
        '/api/commands',
        {method: 'POST', body: JSON.stringify({command: name, arguments: args, run_id: state.status.run_id})});
    if (name === 'exit') {clearDashboard();return reply;}
    if (reply.status) updateStatus({...state.status, ...reply.status, connected: true});
    return reply;
}
function clearDashboard() {
    state.closed = true;
    state.stream?.close();clearInterval(state.pollTimer);
    for (const request of state.requests) request.abort();
    state.requests.clear();
    state.run=null;state.runs=[];state.samples=[];state.extinctions=[];state.historyRequest++;
    state.agents=[];state.selected=null;state.snapshot=null;
    state.status={connected:false};state.config=null;state.diagnostics=[];state.diagnosticRows=[];state.eventAfter=0;
    state.cy?.destroy();state.cy=null;
    for (const chart of [...state.charts,state.metricChart,state.diagnosticChart]) {
        if (!chart) continue;
        for (const dataset of chart.data.datasets) dataset.data=[];
        chart.update('none');
    }
    for (const id of ['runs','agents','agent-detail','agent-avatar','agent-genome','events','diagnostics',
                     'config-form','metric','metric-description','diagnostic-description','diagnostic-metric','network'])
        $(id).replaceChildren();
    for (const id of ['alive','elapsed','ticks','cost','births','transport']) $(id).textContent='—';
    for (const id of ['snapshot-stamp','agent-raw','graph-detail','silent-genes','metric-value','active-config','agents-stamp'])
        $(id).textContent='';
    $('agent-name').textContent='Nessun agente selezionato';$('state').textContent='Simulatore chiuso';
    $('reason').textContent='';$('connection').textContent='Dashboard disconnessa';
    document.querySelectorAll('main button, main input, main select, [data-command]').forEach(e=>e.disabled=true);
    document.dispatchEvent(new Event('popx:exit'));
    notice('Simulatore chiuso e dashboard azzerata. Lo storico resta salvato. Con il launcher si chiude anche il server; la scheda del browser resta aperta.');
}
function updateStatus(status) {
    if (state.closed) return;
    state.status = status;
    if (status.reason === 'storage_limit' || status.storage_guard?.exceeded)
        notice('Limite database raggiunto: salvataggio del checkpoint e rinnovo dello storico. Consulta Spazio e storico.', true);
    $('connection').textContent = status.connected === false ? 'Simulatore non connesso' : 'Simulatore connesso';
    $('speed').textContent = status.speed === 0 ? 'Turbo headless · passa a 1×' : `Velocità ${status.speed || 1}× · ${status.speed === 2 ? 'passa a turbo headless' : 'passa a 2×'}`;
    $('gui').title = status.speed === 0 ? 'Aprire la GUI riporta la velocità a 2×' : '';
    $('gui').textContent = status.gui ? 'Passa a headless' : 'Mostra GUI';
    $('active-config').textContent = (status.config_locked ? 'Configurazione bloccata dal checkpoint.\n' : '') + JSON.stringify(status.metadata?.config || {}, null, 2);
    if (status.config_locked) {
        $('config-save').disabled = true;
        $('config-form').querySelectorAll('input,select').forEach(input => input.disabled = true);
    }
    document.querySelectorAll('[data-command]').forEach(b => {
        const c = b.dataset.command;
        b.disabled = (status.config_locked && (c === 'start' || c === 'restart')) || status.connected === false || (c === 'start' && status.state !== 'finished') ||
            (c === 'resume' && status.state !== 'paused') || (c === 'pause' && status.state !== 'running');
    });
    if (state.run === status.run_id) {
        if (status.performance) $('cost').textContent = `${fmt(status.performance.avg_tick_us)} µs`;
        $('alive').textContent = fmt(status.alive);
        $('elapsed').textContent = `${fmt((status.simulated_seconds || 0) / 60)} min`;
        $('ticks').textContent = `Tick ${fmt(status.tick)}`;
        $('state').textContent =
            ({running: 'In esecuzione', paused: 'In pausa', finished: 'Conclusa'})[status.state] || '—';
        $('reason').textContent = status.reason === 'storage_limit' ? 'Arresto automatico: limite database raggiunto' : status.reason || 'Run evolutiva';
        const t = status.transport || {};
        $('transport').textContent = `${fmt(t.dropped_messages || 0)} messaggi persi · ${fmt(t.queued || 0)} in coda`;
    }
}
async function loadRuns(more = false) {
    const offset = more ? state.runs.length : 0;
    const page = await api(`/api/runs?offset=${offset}`);
    state.runs = more ? [...state.runs, ...page] : page;
    $('runs-more').disabled = page.length < 50;
    const select = $('runs');
    select.replaceChildren();
    state.runs.forEach(run => {
        const option =
            el('option', `${run.metadata.kind || 'run'} · ${new Date(run.created * 1000).toLocaleString('it-IT')}`);
        option.value = run.id;
        select.append(option);
    });
    if (!state.run && state.runs.length) state.run = state.status.run_id || state.runs[0].id;
    select.value = state.run || '';
}
const chartSpecs = [
    [
        'Popolazione',
        [
            ['Vivi', 'alive'], ['Nascite naturali', 'counts.natural_births'],
            ['Nascite forzate', 'counts.forced_births'], ['Morti', 'counts.deaths']
        ]
    ],
    ['Apprendimento: ricompensa e segnale', [['Ricompensa', 'learning.reward'], ['Media recente', 'learning.baseline'], ['Segnale di apprendimento', 'learning.signal']]],
    ['Apprendimento: intensità e dispersione', [['Modulo medio del segnale', 'learning.mean_abs_signal'], ['Deviazione standard', 'learning.signal_std']]],
    ['Apprendimento: variazione dei pesi', [['Variazione effettiva', 'learning.mean_abs_delta'], ['Plasticità proposta', 'learning.mean_abs_plasticity'], ['Decadimento proposto', 'learning.mean_abs_decay']]],
    ['Riproduzione: tentativi e risultati', [['Tentativi', 'reproduction.attempts'], ['Senza spazio', 'reproduction.no_space'], ['Nascite', 'reproduction.births'], ['Oltre la prima corona', 'reproduction.dispersed_births']]],
    ['Riproduzione: attesa al cap', [['Idoneità (agenti × tick)', 'reproduction.eligible_agent_ticks'], ['Blocco al cap (agenti × tick)', 'reproduction.cap_blocked_agent_ticks']]],
    ['Movimento', [['Tentativi', 'totals.movement_attempts'], ['Riusciti', 'totals.movement_successes']]],
    [
        'Risorse nel mondo',
        [['Glucosio', 'world_resources.0'], ['Acqua', 'world_resources.1'], ['Calcio / carbonato', 'world_resources.2'],
         ['CO₂', 'world_resources.3'], ['Ossigeno', 'world_resources.4'], ['Lipidi', 'world_resources.5'],
         ['Azoto', 'world_nitrogen']]
    ],
    ['Barriere di calcio', [['Celle sature', 'calcium_saturated_cells']]],
    [
        'Energia cumulativa',
        [
            ['Respirazione', 'totals.respiration_gain'], ['Metabolismo', 'totals.metabolism_cost'],
            ['Movimento', 'totals.movement_cost'], ['Altre azioni', 'totals.other_action_cost'],
            ['Riproduzione', 'totals.reproduction_cost']
        ]
    ],
    [
        'Termica dei vivi',
        [['Temperatura media', 'averages.body_temperature'], ['Distanza da ottimo', 'averages.abs_temp_delta']]
    ],
    ['Prestazioni e riproduzione', [['Costo tick (µs)', 'avg_tick_us'], ['Backlog', 'backlog']]]
];
const extinctionLines = window.PopXChartMath.extinctionPlugin(() => state.extinctions, () => state.showExtinctions);
function makeChart(canvas, series, tickAxis = true) {
    return new Chart(canvas, {
        type: 'line',
        plugins: tickAxis ? [extinctionLines] : [],
        data: {
            datasets: series.map(([label, path], i) => ({
                                     label: metricInfo(path).label,
                                     path,
                                     borderColor: palette[i % palette.length],
                                     borderWidth: 1.5,
                                     pointRadius: 0,
                                     data: []
                                 }))
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false,
            parsing: false,
            normalized: true,
            interaction: {mode: 'nearest', intersect: false},
            plugins: {legend: {labels: {color: '#92a3b9', boxWidth: 12}},
                tooltip: {callbacks: {label: context => `${context.dataset.label}: ${fmt(context.parsed.y)}`}}},
            scales: {
                x: {
                    type: 'linear',
                    title: {display: true, text: 'Tick', color: '#92a3b9'},
                    ticks: {color: '#92a3b9', maxTicksLimit: 6},
                    grid: {color: '#273246'}
                },
                y: {ticks: {color: '#92a3b9', callback: value => fmt(value)}, grid: {color: '#273246'}}
            }
        }
    });
}
function setupCharts() {
    for (const [title, series] of chartSpecs) {
        const card = el('article', undefined, 'chart-card');
        const heading = el('div', undefined, 'chart-heading');
        heading.append(el('h2', title));
        const canDerive = series.every(([, path]) => window.PopXChartMath.isDerivable(path));
        if (canDerive) {
            const toggle = el('button', 'Mostra derivata /tick', 'derivative-toggle');
            toggle.type = 'button';
            toggle.setAttribute('aria-pressed', 'false');
            heading.append(toggle);
            toggle.onclick = () => {
                chart.derivative = !chart.derivative;
                toggle.textContent = chart.derivative ? 'Mostra valori' : 'Mostra derivata /tick';
                toggle.setAttribute('aria-pressed', String(chart.derivative));
                drawCharts();
            };
        }
        card.append(heading);
        const plot = el('div', undefined, 'plot'), canvas = el('canvas');
        plot.append(canvas);
        card.append(plot);
        const definitions = el('details'), list = el('dl');
        definitions.append(el('summary', 'Significato delle misure'));
        for (const [, path] of series) {
            const info = metricInfo(path);
            const guide = el('dd', undefined, 'metric-guide');
            describeMetric(guide, info);
            list.append(el('dt', info.label), guide);
        }
        definitions.append(list);
        if (canDerive) definitions.append(el('p',
            'Derivata: si divide la differenza fra due valori per i tick trascorsi. Rappresenta la variazione media per tick ' +
            'nell’intervallo osservato. Un valore positivo indica crescita, uno negativo diminuzione, zero stabilità; ' +
            'per i contatori cumulativi indica invece la frequenza media degli eventi. Il primo punto non è definito.'));
        card.append(definitions);
        $('charts').append(card);
        const chart = makeChart(canvas, series);
        state.charts.push(chart);
    }
    state.metricChart = makeChart($('metric-chart'), [['Misura', 'alive']]);
}
function flatten(value, prefix = '', result = []) {
    for (const [k, v] of Object.entries(value || {})) {
        const key = prefix ? `${prefix}.${k}` : k;
        if (typeof v === 'number' || (v === null && state.metrics?.[key]))
            result.push(key);
        else if (v && typeof v === 'object')
            flatten(v, key, result);
    }
    return result;
}
function drawCharts() {
    const last = state.samples.at(-1);
    if (last) {
        const previous = $('metric').value;
        const keys = flatten(last)
            .filter(key => state.metrics?.[key] && !key.startsWith('calcium_distance_correlation.'))
            .sort((a, b) => metricInfo(a).label.localeCompare(metricInfo(b).label, 'it'));
        const signature = keys.join('|');
        if (state.metricKeys !== signature) {
            $('metric').replaceChildren(...keys.map(key => {
                const info = metricInfo(key), option = el('option', info.label);
                option.value = key;
                option.title = info.description;
                return option;
            }));
            $('metric').value = keys.includes(previous) ? previous : 'alive';
            state.metricKeys = signature;
        }
        const path = $('metric').value, info = metricInfo(path);
        const derivativeToggle = $('metric-derivative');
        derivativeToggle.disabled = !window.PopXChartMath.isDerivable(path);
        if (derivativeToggle.disabled) derivativeToggle.checked = false;
        state.metricChart.derivative = derivativeToggle.checked;
        state.metricChart.data.datasets[0].path = path;
        state.metricChart.data.datasets[0].label = info.label + (state.metricChart.derivative ? ' · Δ/tick' : '');
        describeMetric($('metric-description'), info);
        if (state.metricChart.derivative) $('metric-description').append(el('p',
            'Derivata: differenza fra due campioni divisa per i tick trascorsi. Rappresenta la variazione media per tick; ' +
            'valori positivi indicano crescita, negativi diminuzione e zero stabilità. Per un contatore cumulativo è ' +
            'la frequenza media degli eventi; il primo punto non è definito.'));
        const latest = window.PopXChartMath.points(state.samples, path, state.metricChart.derivative).at(-1)?.y;
        $('metric-value').textContent = `${state.metricChart.derivative ? 'Ultima variazione media per tick' : 'Ultimo valore'}: ${fmt(latest)}${latest === null ? ' (servono due campioni validi)' : ''}`;
    }
    for (const chart of [...state.charts, state.metricChart]) {
        for (const dataset of chart.data.datasets) {
            dataset.label = metricInfo(dataset.path).label + (chart.derivative ? ' · Δ/tick' : '');
            dataset.data = window.PopXChartMath.points(state.samples, dataset.path, chart.derivative);
        }
        chart.options.scales.y.title = {display: Boolean(chart.derivative),
                                        text: 'Variazione media per tick', color: '#92a3b9'};
        chart.update('none');
    }
    if (!last) return;
    $('cost').textContent = `${fmt(last.avg_tick_us)} µs`;
    $('births').textContent =
        `${fmt(last.counts?.natural_births)} naturali · ${fmt(last.counts?.forced_births)} forzate`;
    if (state.run !== state.status.run_id) {
        $('alive').textContent = fmt(last.alive);
        $('elapsed').textContent = `${fmt(last.simulated_seconds / 60)} min`;
        $('ticks').textContent = `Tick ${fmt(last.tick)}`;
        const run = state.runs.find(r => r.id === state.run);
        $('state').textContent = run?.state || 'Archivio';
        $('reason').textContent = run?.reason || run?.metadata?.kind || 'Archivio';
    }
}
async function loadHistory() {
    if (!state.run) return;
    const request = ++state.historyRequest, run = state.run;
    const start = $('from-tick').value || 0, end = $('to-tick').value || '9223372036854775807';
    const samples = await api(`/api/runs/${encodeURIComponent(run)}/samples?start=${start}&end=${end}&limit=1000`);
    if (request !== state.historyRequest || run !== state.run) return;
    state.samples = samples;
    state.extinctions = [];
    state.forcedBirthsSeen = samples.at(-1)?.counts?.forced_births ?? 0;
    drawCharts();
    guard(() => refreshExtinctions(request, run, start, end))();
}
async function refreshExtinctions(request = state.historyRequest, run = state.run,
                                  start = $('from-tick').value || 0,
                                  end = $('to-tick').value || '9223372036854775807') {
    if (!run) return;
    const result = await api(`/api/runs/${encodeURIComponent(run)}/extinctions?start=${start}&end=${end}`);
    if (request !== state.historyRequest || run !== state.run) return;
    state.extinctions = result.ticks;
    drawCharts();
}
function drawAgents() {
    const query = $('agent-search').value.toLowerCase(), sort = $('agent-sort').value;
    $('agents').replaceChildren();
    [...state.agents]
        .filter(a => a.pop_id.toLowerCase().includes(query))
        .sort((a, b) => sort === 'age' ? b.age - a.age : a.pop_id.localeCompare(b.pop_id, undefined, {numeric: true}))
        .forEach(agent => {
            const button = el('button', `${agent.pop_id} · età ${agent.age}`);
            button.onclick = guard(async () => {
                state.selected = agent.pop_id;
                const reply = await command('inspect', {pop_id: agent.pop_id});
                showSnapshot(reply.result);
            });
            $('agents').append(button);
        });
}
function showSnapshot(snapshot) {
    state.snapshot = snapshot;
    $('agent-name').textContent = snapshot.pop_id;
    $('snapshot-stamp').textContent = `Tick ${fmt(snapshot.tick)} · run ${snapshot.run_id}`;
    $('agent-detail').replaceChildren();
    const info = {
        Età: snapshot.age,
        Energia: snapshot.totals.energy,
        Temperatura: snapshot.totals.body_temperature,
        Posizione: snapshot.position.join(', '),
        Genitore: snapshot.parent_id || 'Fondatore',
        Discendenti: snapshot.totals.offspring,
        ...snapshot.traits,
        'Raggio sensoriale effettivo': snapshot.sensing_radius
    };
    for (const [key, value] of Object.entries(info)) {
        const field = el('div');
        field.append(el('small', key), el('strong', fmt(value)));
        $('agent-detail').append(field);
    }
    $('agent-raw').textContent = JSON.stringify({...snapshot, brain: undefined}, null, 2);
    PopXInspector.renderAvatar($('agent-avatar'), snapshot);
    PopXInspector.renderGenome($('agent-genome'), snapshot, {
        sensors: window.sensorLabelsForSnapshot(snapshot), actions: window.ACTION_LABELS
    });
    drawNetwork();
}
function drawNetwork() {
    if (!state.snapshot) return;
    if (!$('weight-min').reportValidity()) return;
    const brain = state.snapshot.brain, elements = [], threshold = Number($('weight-min').value);
    const graph = PopXNetwork.prepare(brain, threshold, $('graph-zero').checked, $('graph-path-filter').value || 'all');
    const hideIsolated = $('graph-isolated').checked;
    const sensorLabels = window.sensorLabelsForSnapshot(state.snapshot);
    const sizes = [brain.sizeS, ...Array(brain.num_hidden_layers).fill(brain.sizeN), brain.sizeY];
    sizes.forEach((size, layer) => {
        for (let i = 0; i < size; i++) {
            if (hideIsolated && !graph.connected.has(`${layer}:${i}`)) continue;
            const label = layer === 0 ? (sensorLabels[i] || `INPUT_${i}`) :
                layer === sizes.length - 1 ? (window.ACTION_LABELS[i] || `OUTPUT_${i}`) :
                                             `H${layer}.${i}`;
            elements.push({
                data: {id: `${layer}:${i}`, label, activation: brain.activations?.[layer]?.[i] ?? null, layer},
                position: {x: layer * 260, y: (i - (size - 1) / 2) * 40}
            });
        }
    });
    elements.push(...graph.edges);
    if (state.cy) state.cy.destroy();
    state.cy = cytoscape({
        container: $('network'),
        elements,
        layout: {name: 'preset', fit: true, padding: 40},
        style: [
            {
                selector: 'node',
                style: {
                    label: 'data(label)',
                    'background-color': '#7d9ccc',
                    color: '#dce7f5',
                    'font-size': 10,
                    width: 15,
                    height: 15,
                    'text-valign': 'top'
                }
            },
            {
                selector: 'edge',
                style: {
                    width: 'data(width)',
                    'line-style': 'data(lineStyle)',
                    'line-color': 'data(color)',
                    'target-arrow-color': 'data(color)',
                    'target-arrow-shape': 'triangle',
                    'curve-style': 'bezier',
                    opacity: .8
                }
            },
            {selector: ':selected', style: {'background-color': '#ffffff', 'line-color': '#ffffff', 'line-opacity': 1}},
            {selector: 'edge:selected', style: {label: 'data(weightLabel)', 'font-size': 13, color: '#ffffff'}}
        ],
        minZoom: .05,
        maxZoom: 4
    });
    state.cy.on('tap', 'node, edge', event => {
        const data = event.target.data();
        $('graph-detail').textContent = event.target.isEdge()
            ? `${data.source} → ${data.target} · Peso: ${PopXNetwork.formatWeight(data.weight)} (valore completo: ${data.weight})`
            : `${data.label} · Attivazione: ${data.activation === null ? 'non ancora calcolata' : PopXNetwork.formatWeight(data.activation)}`;
    });
    $('graph-detail').textContent = `${graph.edges.length} / ${graph.total} connessioni mostrate · ${graph.outputCount} possono influire su un output · ${graph.pathCount} sono su percorsi input → output · ${graph.zeroCount} pesi esattamente zero · |peso| massimo: ${PopXNetwork.formatWeight(graph.maxAbs)}.${graph.edges.length ? '' : ' Nessuna connessione soddisfa il filtro.'}`;
    $('silent-genes').textContent = `${brain.silent_genes?.length || 0} geni silenti fuori dimensione · ${
        brain.discarded_genes?.length || 0} geni esclusi dai vincoli della rete`;
}
async function loadConfig() {
    state.config = await api('/api/config');
    $('config-save').disabled = !!state.config.locked;
    $('config-form').replaceChildren();
    const groups = new Map();
    const fields = [
        ...state.config.schema.map(s => ({...s, value: state.config.entries[s.name] ?? state.config.values[s.name]})),
        ...Object.entries(state.config.entries)
            .filter(([k]) => k.includes('@'))
            .map(([k, value]) => ({
                     ...state.config.schema.find(s => s.name === k.split('@')[0]),
                     name: k,
                     value,
                     description: 'Override legacy; non applicato durante la run'
                 }))
    ];
    for (const spec of fields) {
        if (!spec.type) continue;
        if (!groups.has(spec.group)) {
            const section = el('section', undefined, 'config-group');
            section.append(el('h2', spec.group));
            const grid = el('div', undefined, 'config-fields');
            section.append(grid);
            $('config-form').append(section);
            groups.set(spec.group, grid);
        }
        const label = el('label', undefined, spec.effective ? '' : 'legacy');
        label.append(el('span', spec.label || spec.name));
        let input;
        if (spec.type === 'boolean') {
            input = el('select');
            for (const value of ['true', 'false']) {
                const option = el('option', value);
                option.value = value;
                input.append(option);
            }
            input.value = String(spec.value) === '1' ? 'true' :
                String(spec.value) === '0'           ? 'false' :
                                                       String(spec.value);
        } else {
            input = el('input');
            input.type = spec.type === 'string' ? 'text' : 'number';
            if (spec.min !== undefined) input.min = spec.min;
            if (spec.max !== undefined) input.max = spec.max;
            input.step = spec.type === 'integer' ? '1' : 'any';
            input.value = spec.value;
        }
        input.dataset.key = spec.name;
        input.dataset.original = input.value;
        input.required = true;
        input.disabled = !!state.config.locked;
        label.append(input, el('small', spec.description));
        groups.get(spec.group).append(label);
    }
}
async function loadEvents(reset) {
    if (!state.run) return;
    if (reset) {
        state.eventAfter = 0;
        $('events').replaceChildren();
    }
    const rows = await api(`/api/runs/${state.run}/events?after=${state.eventAfter}`);
    if (!reset) $('events').replaceChildren();
    for (const row of rows) {
        const tr = el('tr');
        for (const value
                 of [row.tick, row.type, row.data.pop_id, row.data.age, row.data.totals?.offspring,
                     row.type === 'death'    ? row.data.death_cause :
                         row.type === 'cull' ? 'NewGen' :
                                               '—'])
            tr.append(el('td', fmt(value)));
        $('events').append(tr);
        state.eventAfter = row.id;
    }
    $('events-more').disabled = rows.length < 100;
}
document.querySelectorAll('[data-tab]')
    .forEach(button => button.onclick = guard(async () => {
                 document.querySelectorAll('.tab').forEach(tab => tab.hidden = tab.id !== button.dataset.tab);
                 document.querySelectorAll('[data-tab]').forEach(b => b.classList.toggle('active', b === button));
                 $('title').textContent = {
                     overview: 'La simulazione, nel tempo.',
                     inspector: 'Dentro ogni agente.',
                     configuration: 'Parametri, sotto controllo.'
                 }[button.dataset.tab];
                 if (button.dataset.tab === 'configuration' && !state.config) await loadConfig();
                 if (state.cy) state.cy.resize();
             }));
document.querySelectorAll('[data-command]').forEach(b => b.onclick = guard(async () => {
                                                        if (b.dataset.command === 'exit' && !window.confirm(
                                                            'Chiudere il simulatore? La run sarà conclusa e salvata, la GUI si chiuderà. Se hai usato il launcher, si chiuderà anche il server della dashboard; la scheda del browser resterà aperta.')) return;
                                                        await command(b.dataset.command);
                                                        if (state.closed) return;
                                                        await loadRuns();
                                                        notice('Comando applicato.');
                                                    }));
$('speed').onclick = guard(() => command('speed', {value: state.status.speed === 1 ? 2 : state.status.speed === 2 ? 0 : 1}));
$('gui').onclick = guard(() => command(state.status.gui ? 'headless' : 'show_gui'));
$('extinction-toggle').onclick = () => {
    state.showExtinctions = !state.showExtinctions;
    $('extinction-toggle').textContent = state.showExtinctions ? 'Nascondi linee rosse' : 'Mostra linee rosse';
    $('extinction-toggle').setAttribute('aria-pressed', String(state.showExtinctions));
    for (const chart of [...state.charts, state.metricChart, state.diagnosticChart]) chart?.update('none');
};
$('runs-more').onclick = guard(() => loadRuns(true));
$('runs').onchange = guard(async () => {
    state.run = $('runs').value;
    state.agents = [];
    state.selected = null;
    state.snapshot = null;
    PopXInspector.renderAvatar($('agent-avatar'), null);
    PopXInspector.renderGenome($('agent-genome'), null);
    state.cy?.destroy();
    state.cy = null;
    $('agent-name').textContent = 'Seleziona un agente';
    $('agent-detail').replaceChildren();
    for (const id of ['snapshot-stamp', 'agent-raw', 'graph-detail', 'silent-genes']) $(id).textContent = '';
    drawAgents();
    await loadHistory();
});
$('load-history').onclick = guard(loadHistory);
$('metric').onchange = drawCharts;
$('metric-derivative').onchange = drawCharts;
$('agents-refresh').onclick = guard(async () => {
    if (state.run !== state.status.run_id) throw new Error('Seleziona la run attiva per leggere gli agenti vivi');
    const reply = await command('agents');
    state.agents = reply.result.agents;
    $('agents-stamp').textContent = `${state.agents.length} vivi · tick ${fmt(reply.result.tick)}`;
    drawAgents();
});
$('agent-search').oninput = drawAgents;
$('agent-sort').onchange = drawAgents;
$('agent-refresh').onclick = guard(async () => {
    if (!state.selected) throw new Error('Seleziona un agente vivo');
    showSnapshot((await command('inspect', {pop_id: state.selected})).result);
});
$('snapshot-save').onclick = guard(async () => {
    if (!state.selected) throw new Error('Seleziona un agente vivo');
    showSnapshot((await command('snapshot', {pop_id: state.selected})).result);
    notice('Snapshot salvato e indicizzato.');
});
$('graph-filter').onclick = guard(drawNetwork);
$('weight-min').onchange = guard(drawNetwork);
$('graph-path-filter').onchange = guard(drawNetwork);
$('graph-zero').onchange = guard(drawNetwork);
$('graph-isolated').onchange = guard(drawNetwork);
$('graph-fit').onclick = () => state.cy?.fit(undefined, 40);
$('config-reload').onclick = guard(loadConfig);
$('config-save').onclick = guard(async () => {
    if (!$('config-form').reportValidity()) return;
    const changes = {};
    document.querySelectorAll('[data-key]').forEach(input => {
        if (input.value !== input.dataset.original) changes[input.dataset.key] = input.value;
    });
    await api('/api/config', {method: 'PUT', body: JSON.stringify({revision: state.config.revision, changes})});
    notice('INI salvato. Sarà applicato al prossimo Start o Restart.');
    await loadConfig();
});
$('config-form').onsubmit = e => e.preventDefault();
$('events-refresh').onclick = guard(() => loadEvents(true));
$('events-more').onclick = guard(() => loadEvents(false));
$('diagnostics-load').onclick = guard(async () => {
    state.diagnostics = await api(`/api/runs/${state.run}/diagnostics`);
    $('diagnostics').textContent = JSON.stringify(
        state.diagnostics.map(r => ({
                                  metadata: r.metadata,
                                  protocol: r.protocol,
                                  validation_passed: r.validation_passed,
                                  trials: r.trials?.length,
                                  gardens: r.gardens?.length,
                                  resource_probes: r.resource_probes
                              })),
        null, 2);
    diagnosticChoices();
});
function diagnosticChoices() {
    const report = state.diagnostics?.at(-1) || {};
    state.diagnosticRows = report[$('diagnostic-series').value] || [];
    const keys = flatten(state.diagnosticRows[0] || {});
    $('diagnostic-metric').replaceChildren(...keys.map(k => {
        const o = el('option', k);
        o.value = k;
        return o;
    }));
    drawDiagnostic();
}
function drawDiagnostic() {
    if (!state.diagnosticChart) {
        state.diagnosticChart = makeChart($('diagnostic-chart'), [['Misura', '']], false);
        state.diagnosticChart.options.scales.x.title.text = 'Indice condizione';
    }
    const key = $('diagnostic-metric').value;
    describeMetric($('diagnostic-description'), {
        calculation: key ? `Ogni punto riporta il valore del campo «${key}» registrato dalla fixture per una condizione. Il protocollo della prova specifica come viene misurato il campo.` : 'Selezionare una serie e una misura dai risultati caricati.',
        meaning: 'Il risultato di ciascuna condizione diagnostica, nell’ordine in cui compare nel rapporto della prova.',
        interpretation: 'L’asse orizzontale identifica la condizione e l’asse verticale il valore misurato. Confrontare punti riferiti a condizioni e unità compatibili, consultando il protocollo qui sotto; una quota più alta significa un valore maggiore del campo selezionato.'
    });
    state.diagnosticChart.data.datasets[0].label = key;
    state.diagnosticChart.data.datasets[0].data =
        (state.diagnosticRows || []).slice(0, 2000).map((row, i) => ({x: i, y: at(row, key)}));
    state.diagnosticChart.update('none');
}
$('diagnostic-series').onchange = diagnosticChoices;
$('diagnostic-metric').onchange = drawDiagnostic;
guard(async () => {
    // Static definitions permit wording hotfixes while the Python receiver keeps the run connected.
    state.metrics = window.POPX_METRICS || await api('/api/metrics');
    setupCharts();
    updateStatus(await api('/api/status'));
    await loadRuns();
    await loadHistory();
    const stream = new EventSource('/api/stream');
    state.stream = stream;
    stream.onmessage = guard(async event => {
        if (state.closed) return;
        const message = JSON.parse(event.data);
        if (message.type === 'status') {
            if (state.status.run_id && state.status.run_id !== message.run_id && message.data.metadata?.checkpoint) {
                location.reload();
                return;
            }
            updateStatus({...message.data, run_id: message.run_id, connected: true});
            if (!state.runs.some(r => r.id === message.run_id)) await loadRuns();
        }
        if (message.type === 'sample' && message.run_id === state.run && !$('to-tick').value) {
            const sample = message.data;
            const index = state.samples.findIndex(s => s.tick === sample.tick);
            if (index >= 0)
                state.samples[index] = sample;
            else
                state.samples.push(sample);
            state.samples.sort((a, b) => a.tick - b.tick);
            if (state.samples.length > 1000)
                state.samples = state.samples.filter((_, i) => i % 2 === 0 || i === state.samples.length - 1);
            drawCharts();
            const forced = sample.counts?.forced_births ?? 0;
            if (forced > state.forcedBirthsSeen) {
                state.forcedBirthsSeen = forced;
                guard(refreshExtinctions)();
            }
        }
    });
    stream.onerror = () => {
        if (state.closed) return;
        $('connection').textContent = 'Dashboard in riconnessione';
    };
    state.pollTimer = setInterval(guard(async () => updateStatus(await api('/api/status'))), 3000);
})();
