'use strict';
// Read-only views of a manual snapshot. Keep these rules aligned with Genome::compute_genetic_color_value,
// Pop::get_render_state, Renderer::update_entity and Brain::wire; never infer genotype from learned edges.
const PopXInspector = (() => {
    const design = typeof window === 'undefined' ? require('./design.json') : window.POPX_DESIGN;
    const layerColors = design.layers;
    let sortField = 'index', sortDirection = 'asc';
    function sortGenes(entries, field = 'index', direction = 'asc') {
        const states = {wired: 0, silent: 1, discarded: 2, unknown: 3};
        const compare = (a, b) => {
            switch (field) {
                case 'source': return a.gene[0] - b.gene[0] || a.gene[1] - b.gene[1];
                case 'target': return a.gene[2] - b.gene[2] || a.gene[3] - b.gene[3];
                case 'weight': return a.weight - b.weight;
                case 'absolute': return Math.abs(a.weight) - Math.abs(b.weight);
                case 'status': return states[a.status] - states[b.status];
                default: return a.index - b.index;
            }
        };
        return [...entries].sort((a, b) => (direction === 'desc' ? -1 : 1) * compare(a, b) || a.index - b.index);
    }
    function geneticColor(genes) {
        if (!Array.isArray(genes)) return null;
        if (!genes.length) return [0, 0, 0];
        const first = genes[0], last = genes[genes.length - 1];
        return [((genes.length & 1) << 7) | ((first[0] & 7) << 4) | ((last[0] & 7) << 1),
            ((first[2] & 7) << 5) | ((last[2] & 7) << 2), ((first[1] & 15) << 4) | (last[3] & 15)];
    }
    function avatar(snapshot) {
        const traits = snapshot.traits || {};
        const color = snapshot.alive === false ? [0, 0, 0] :
            (snapshot.genetics?.color_model === 'projection_v1' ? snapshot.genetics.color : geneticColor(snapshot.genes));
        const shape = Number.isFinite(traits.chloroplasts) && Number.isFinite(traits.mitochondria)
            ? (traits.chloroplasts > traits.mitochondria ? 'triangle' : 'circle') : null;
        const d = snapshot.direction;
        const direction = Array.isArray(d) && d.length === 2 && d.every(Number.isFinite) ? d : null;
        const length = direction ? Math.hypot(...direction) : 0;
        const unit = length ? direction.map(v => v / length) : null;
        // SFML uses an upright triangle or an 11-segment circle; heading does not rotate the shape.
        const points = shape === 'triangle' ? [[0, -1], [1, 1], [-1, 1]] :
            Array.from({length: 11}, (_, i) => [Math.cos(2 * Math.PI * i / 11), Math.sin(2 * Math.PI * i / 11)]);
        return {color, shape, points, direction, unit,
            initial: snapshot.totals?.movement_successes === 0};
    }
    function endpoint(type, index, labels = {}) {
        return type === 0 ? `Input ${index}${labels.sensors?.[index] ? ` · ${labels.sensors[index]}` : ''}` :
            type === 7 ? `Output ${index}${labels.actions?.[index] ? ` · ${labels.actions[index]}` : ''}` :
            `Hidden ${type} · neurone ${index}`;
    }
    function classify(gene, brain) {
        if (!brain || ![brain.num_hidden_layers, brain.sizeS, brain.sizeN, brain.sizeY].every(Number.isFinite))
            return {status: 'unknown', reason: 'Dimensioni della rete non disponibili'};
        const [source, col, sink, row] = gene, hidden = brain.num_hidden_layers;
        if (!(source === 0 || (source >= 1 && source <= hidden)))
            return {status: 'discarded', reason: 'Sorgente non ammessa o strato non attivo'};
        if (!(sink === 7 || (sink >= 1 && sink <= hidden)))
            return {status: 'discarded', reason: 'Destinazione non ammessa o strato non attivo'};
        const target = sink === 7 ? hidden + 1 : sink;
        if (target <= source) return {status: 'discarded', reason: 'Connessione laterale o all’indietro: rete feedforward'};
        if (col >= (source === 0 ? brain.sizeS : brain.sizeN))
            return {status: 'silent', reason: 'Indice sorgente fuori dimensione (nessun modulo)'};
        if (row >= (sink === 7 ? brain.sizeY : brain.sizeN))
            return {status: 'silent', reason: 'Indice destinazione fuori dimensione (nessun modulo)'};
        return {status: 'wired', reason: 'Cablato nella rete', key: `${source}:${col}>${target}:${row}`};
    }
    function genome(snapshot, labels = {}) {
        const edges = new Map(), groups = new Map();
        for (const connection of snapshot.brain?.connections || []) {
            for (const entry of connection.entries || []) {
                edges.set(`${connection.from_layer}:${entry.col}>${connection.to_layer}:${entry.row}`, entry.value);
            }
        }
        const entries = (snapshot.genes || []).map((gene, index) => {
            const entry = {...classify(gene, snapshot.brain), index, gene, weight: gene[4] / 8192,
                source: endpoint(gene[0], gene[1], labels), target: endpoint(gene[2], gene[3], labels)};
            if (entry.key) {
                if (!groups.has(entry.key)) groups.set(entry.key, {count: 0, weight: 0});
                const group = groups.get(entry.key);
                group.count++;
                group.weight += entry.weight;
            }
            return entry;
        });
        for (const entry of entries) {
            if (entry.key) Object.assign(entry, {group: groups.get(entry.key), currentWeight: edges.get(entry.key)});
        }
        return entries;
    }
    function node(tag, text, className) {
        const element = document.createElement(tag);
        if (text !== undefined) element.textContent = text;
        if (className) element.className = className;
        return element;
    }
    function svgNode(tag, attrs, text) {
        const element = document.createElementNS('http://www.w3.org/2000/svg', tag);
        for (const [key, value] of Object.entries(attrs)) element.setAttribute(key, String(value));
        if (text !== undefined) element.textContent = text;
        return element;
    }
    function renderAvatar(container, snapshot) {
        container.replaceChildren();
        if (!snapshot) return;
        const model = avatar(snapshot), figure = node('figure', undefined, 'pop-avatar');
        const svg = svgNode('svg', {viewBox: '0 0 240 240', role: 'img', 'aria-label': 'Avatar e bussola del pop'});
        svg.append(svgNode('title', {}, 'Coordinate del mondo: x verso est, y verso sud. Freccia: last_direction.'));
        svg.append(svgNode('circle', {cx: 120, cy: 120, r: 89, fill: '#d6deea', stroke: '#92a3b9'}));
        svg.append(svgNode('path', {d: 'M31 120H209 M120 31V209', stroke: '#92a3b9', 'stroke-dasharray': '3 5'}));
        for (const [text, x, y] of [['N · −y', 120, 17], ['S · +y', 120, 236], ['O', 14, 125], ['E', 226, 125]])
            svg.append(svgNode('text', {x, y, fill: '#e7eef7', 'text-anchor': 'middle', 'font-size': 13}, text));
        if (model.color && model.shape) {
            svg.append(svgNode('polygon', {points: model.points.map(([x, y]) => `${120 + x * 37},${120 + y * 37}`).join(' '),
                fill: `rgb(${model.color.join(',')})`}));
        }
        if (model.unit) {
            const [dx, dy] = model.unit, x = 120 + 73 * dx, y = 120 + 73 * dy;
            svg.append(svgNode('line', {x1: 120, y1: 120, x2: x, y2: y, stroke: '#0e131c', 'stroke-width': 5}));
            svg.append(svgNode('line', {x1: 120, y1: 120, x2: x, y2: y, stroke: '#ffffff', 'stroke-width': 2}));
            svg.append(svgNode('polygon', {points: `${x},${y} ${x - 13 * dx + 6 * dy},${y - 13 * dy - 6 * dx} ` +
                `${x - 13 * dx - 6 * dy},${y - 13 * dy + 6 * dx}`, fill: '#ffffff', stroke: '#0e131c'}));
        }
        figure.append(svg);
        const caption = node('div', undefined, 'avatar-caption');
        caption.append(node('h3', 'Aspetto nel mondo'));
        caption.append(node('p', model.shape ? (model.shape === 'triangle' ? 'Triangolo' : 'Cerchio (11 segmenti)') +
            ` · RGB ${model.color?.join(', ') ?? 'non disponibile'}` : 'Aspetto non disponibile per questo snapshot.'));
        if (snapshot.genetics?.fingerprint)
            caption.append(node('p', `Firma del genoma alla nascita: ${snapshot.genetics.fingerprint}. Colore: proiezione dell’intero genoma; la somiglianza esatta si legge nella pagina Linee evolutive.`, 'muted'));
        caption.append(node('p', `Cloroplasti ${snapshot.traits?.chloroplasts ?? '—'} · mitocondri ${snapshot.traits?.mitochondria ?? '—'}`));
        caption.append(node('p', `${model.initial ? 'Direzione iniziale' : 'Ultima direzione (last_direction)'}: ` +
            `${model.direction ? `(${model.direction.join(', ')})` : 'non disponibile'}`));
        caption.append(node('p', 'Calcolo: la forma confronta cloroplasti e mitocondri; il colore deriva dal genoma ' +
            'con la stessa regola di SFML. La freccia normalizza il vettore last_direction a lunghezza fissa. ' +
            'Cosa rappresenta: l’aspetto dell’agente e la direzione dell’ultimo spostamento riuscito; prima del primo ' +
            'spostamento viene mostrata la direzione iniziale casuale.', 'muted'));
        caption.append(node('p', 'Come leggerlo: il triangolo indica più cloroplasti che mitocondri, il cerchio indica ' +
            'mitocondri in numero almeno pari ai cloroplasti. Il nero identifica un agente morto. La freccia indica ' +
            'il verso nello spazio: nord in alto (−y), est a destra (+x). Avatar e freccia usano una scala visiva fissa.', 'muted'));
        figure.append(caption);
        container.append(figure);
    }
    function renderGenome(container, snapshot, labels = {}) {
        container.replaceChildren();
        if (!snapshot) {
            container.append(node('p', 'Seleziona un agente per esplorare il genoma.', 'muted'));
            return;
        }
        if (!Array.isArray(snapshot.genes)) {
            container.append(node('p', 'Il genoma originale non è disponibile in questo snapshot.', 'muted'));
            return;
        }
        const entries = genome(snapshot, labels), counts = {wired: 0, silent: 0, discarded: 0, unknown: 0};
        entries.forEach(entry => counts[entry.status]++);
        container.append(node('p', `${entries.length} geni · ${counts.wired} cablati · ${counts.silent} silenti fuori dimensione · ` +
            `${counts.discarded} esclusi dal cablaggio${counts.unknown ? ` · ${counts.unknown} non classificabili` : ''}`, 'muted'));
        if (!entries.length) return;
        const controls = node('div', undefined, 'toolbar genome-sort');
        const sortLabel = node('label', 'Ordina geni: '), sort = node('select');
        const sortOptions = {index: 'Posizione originale', source: 'Sorgente (strato, neurone)',
            target: 'Destinazione (strato, neurone)', weight: 'Peso nel genoma', absolute: 'Modulo del peso nel genoma',
            status: 'Stato: cablati → silenti → esclusi → non classificabili'};
        for (const [value, text] of Object.entries(sortOptions)) {
            const option = node('option', text);
            option.value = value;
            sort.append(option);
        }
        sort.value = sortField;
        sortLabel.append(sort);
        const directionLabel = node('label', 'Verso: '), direction = node('select');
        for (const [value, text] of [['asc', 'Crescente'], ['desc', 'Decrescente']]) {
            const option = node('option', text);
            option.value = value;
            direction.append(option);
        }
        direction.value = sortDirection;
        directionLabel.append(direction);
        controls.append(sortLabel, directionLabel);
        container.append(controls);
        const layout = node('div', undefined, 'genome-layout'), bars = node('div', undefined, 'genome-bars');
        const detail = node('div', undefined, 'gene-detail');
        detail.setAttribute('role', 'region');
        detail.setAttribute('aria-label', 'Dettagli del gene');
        const hint = 'Passa su una barretta per leggerla. Clicca per fissare la selezione; usa Tab e Invio da tastiera.';
        let selected = null, preview = null;
        const buttons = [];
        function show(entry) {
            detail.replaceChildren();
            buttons.forEach((button, i) => {
                button.classList.toggle('gene-preview', entry?.index === i);
                button.setAttribute('aria-pressed', String(selected?.index === i));
            });
            if (!entry) { detail.append(node('p', hint, 'muted')); return; }
            detail.append(node('h3', `Gene ${entry.index + 1}${selected === entry ? ' · selezionato' : ' · anteprima'}`));
            const fields = {
                'Sorgente': entry.source, 'Destinazione': entry.target, 'Stato': entry.reason,
                'Peso nel genoma (int16)': entry.gene[4], 'Peso iniziale del gene (÷ 8192)': entry.weight,
                'Codifica [tipo src, indice src, tipo dst, indice dst, peso]': entry.gene.join(', ')
            };
            if (entry.group) {
                fields['Geni sullo stesso arco'] = entry.group.count;
                fields['Peso iniziale dell’arco (somma dei geni)'] = entry.group.weight;
                fields['Peso attuale dell’arco, dopo apprendimento'] = entry.currentWeight ?? 'Non disponibile';
            }
            const dl = node('dl');
            for (const [label, value] of Object.entries(fields)) dl.append(node('dt', label), node('dd', String(value)));
            detail.append(dl);
            if (entry.weight === 0) detail.append(node('p', 'Gene presente con peso esattamente zero. Il peso attuale dell’arco può differire.', 'muted'));
            detail.append(node('p', hint, 'muted'));
        }
        for (const entry of entries) {
            const button = node('button', undefined, `gene-bar gene-${entry.status}`);
            button.type = 'button';
            const title = `Gene ${entry.index + 1}: ${entry.source} → ${entry.target}; peso ${entry.weight}; ${entry.reason}`;
            button.title = title;
            button.setAttribute('aria-label', title);
            button.append(node('span', String(entry.index + 1), 'gene-index'));
            const color = node('span', undefined, 'gene-color');
            color.style.background = entry.weight === 0 ? design.signals.zero : entry.weight > 0 ? design.signals.positive : design.signals.negative;
            color.style.borderLeftColor = layerColors[entry.gene[0]];
            color.style.borderRightColor = layerColors[entry.gene[2]];
            button.append(color, node('span', `${entry.gene[0]}:${entry.gene[1]} → ${entry.gene[2]}:${entry.gene[3]}`, 'gene-route'));
            button.onmouseenter = () => { preview = entry; show(entry); };
            button.onmouseleave = () => { preview = null; show(selected); };
            button.onfocus = () => show(entry);
            button.onblur = () => show(preview || selected);
            button.onclick = () => { selected = entry; show(entry); };
            buttons.push(button);
            bars.append(button);
        }
        function applySort() {
            sortField = sort.value;
            sortDirection = direction.value;
            preview = null;
            bars.replaceChildren(...sortGenes(entries, sortField, sortDirection).map(entry => buttons[entry.index]));
            bars.setAttribute('aria-label', `Tutti i geni · ${sortOptions[sortField]} · ${sortDirection === 'asc' ? 'crescente' : 'decrescente'}`);
            bars.scrollTop = 0;
            show(selected);
        }
        sort.onchange = applySort;
        direction.onchange = applySort;
        applySort();
        layout.append(bars, detail);
        container.append(layout);
    }
    return {sortGenes, geneticColor, avatar, classify, genome, renderAvatar, renderGenome};
})();
if (typeof module !== 'undefined') module.exports = PopXInspector;
