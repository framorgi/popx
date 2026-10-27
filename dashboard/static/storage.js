'use strict';
(() => {
    const $ = id => document.getElementById(id);
    const page = !!$('storage-page');
    const state = {runs: [], current: null, status: null, report: null, snapshots: [], snapshotOffset: 0,
        busy: false, refreshing: false};
    let closed = false;
    const node = (tag, value) => { const element = document.createElement(tag);
        if (value !== undefined) element.textContent = value; return element; };
    const bytes = value => { const units = ['B', 'KiB', 'MiB', 'GiB', 'TiB']; let index = 0;
        while (value >= 1024 && index < units.length - 1) { value /= 1024; index++; }
        return `${value.toLocaleString('it-IT', {maximumFractionDigits: 2})} ${units[index]}`; };
    async function api(path, options = {}) {
        const response = await fetch(path, {cache: 'no-store', headers: {'Content-Type': 'application/json'}, ...options});
        if (closed) throw new Error('Dashboard chiusa');
        if (!response.ok) { let message = `Richiesta fallita (${response.status})`;
            try { message = (await response.json()).detail || message; } catch {}
            throw new Error(message); }
        return response.json();
    }
    const widget = node('section'); widget.className = 'storage-widget';
    const link = node('a', 'Spazio, run e snapshot'); link.href = '/static/storage.html'; widget.append(link);
    const mini = node('div'); widget.append(mini); document.querySelector('aside')?.append(widget);
    function message(value) { if (!closed && page) $('storage-message').textContent = value; }
    function bar(report, limit = false) {
        const element = node('progress'); element.className = 'storage-bar';
        element.max = limit ? report.guard.limit_bytes : report.disk_total;
        element.value = limit ? Math.min(report.database_files_bytes, element.max) : report.disk_unavailable;
        element.setAttribute('aria-label', limit ? 'Database rispetto al limite' : 'Quota del filesystem occupata');
        if (!limit && report.disk_available / report.disk_total < .1) element.classList.add('storage-low');
        return element;
    }
    function selection() {
        if (!page) return;
        const run = state.runs.find(item => item.id === $('storage-run').value);
        const deleting = state.report?.removals.some(item => item.state === 'deleting');
        $('storage-delete').disabled = state.busy || !!deleting || !run;
        $('storage-clean').disabled = state.busy || !!deleting || !state.report?.run_count;
        $('storage-checkpoint-save').disabled = state.busy || !state.status?.connected;
        $('storage-checkpoint-load').disabled = state.busy || !$('storage-checkpoint').value || !state.status?.connected;
        $('storage-selection').textContent = run ? `${run.id} · ${run.state === 'finished' ? 'conclusa' : 'aperta'}` +
            (run.id === state.current ? ' · collegata al simulatore' : '') : 'Nessuna run selezionata';
    }
    function render(report) {
        state.report = report;
        const percent = 100 * report.disk_unavailable / report.disk_total;
        mini.replaceChildren(bar(report), node('p', `${percent.toFixed(1)}% del filesystem occupato o riservato`),
            node('p', `${bytes(report.disk_available)} disponibili`),
            node('p', `Database e scritture: ${bytes(report.database_files_bytes)}`));
        if (report.guard) mini.append(bar(report, true), node('p', `Rotazione a ${report.guard.limit_gb} GB`));
        if (report.guard?.error) mini.append(node('p', report.guard.error));
        if (!page) return;
        const cards = node('div'); cards.className = 'storage-numbers';
        for (const [label, value] of [['Disponibile nel filesystem', report.disk_available],
            ['File SQLite, WAL e SHM', report.database_files_bytes],
            ['Pagine interne riutilizzabili', report.database_reusable_bytes]]) {
            const card = node('div', label); card.append(node('strong', bytes(value))); cards.append(card);
        }
        $('storage-meter').replaceChildren(bar(report), cards,
            node('p', `${report.run_count} run nello storico · aggiornamento ogni 15 secondi`));
        $('storage-jobs').replaceChildren(...report.removals.map(job => {
            const line = node('p', `${job.run}: ${({deleting: 'rimozione in corso', deleted: 'rimossa',
                error: 'rimozione da completare'})[job.state] || job.state} · ${job.rows_deleted.toLocaleString('it-IT')} righe` +
                (job.error ? ` · ${job.error}` : ''));
            if (job.state === 'error') { const retry = node('button', 'Riprova');
                retry.onclick = () => remove(job.run); line.append(retry); }
            return line;
        }));
        selection();
    }
    async function snapshots(more = false) {
        if (!page) return;
        const run = $('storage-run').value;
        if (!run) { state.snapshots = []; $('storage-snapshots').replaceChildren(node('p', 'Nessuna run selezionata.')); return; }
        const offset = more ? state.snapshotOffset : 0;
        const rows = await api(`/api/snapshots?run=${encodeURIComponent(run)}&offset=${offset}`);
        if (run !== $('storage-run').value) return;
        state.snapshots = more ? [...state.snapshots, ...rows] : rows;
        state.snapshotOffset = offset + rows.length;
        const list = $('storage-snapshots'); list.replaceChildren();
        if (!state.snapshots.length) list.append(node('p', 'Nessuno snapshot neurale salvato per questa run.'));
        for (const snapshot of state.snapshots) {
            const button = node('button', `${snapshot.pop_id} · tick ${snapshot.tick.toLocaleString('it-IT')}`);
            button.onclick = async () => { try { const data = await api(`/api/snapshots/${snapshot.id}`);
                const detail = $('storage-snapshot-detail'); detail.hidden = false;
                detail.textContent = JSON.stringify(data, null, 2); } catch (error) { message(error.message); } };
            list.append(button);
        }
        $('storage-snapshots-more').disabled = rows.length < 100;
        if (!more) { $('storage-snapshot-detail').hidden = true; $('storage-snapshot-detail').textContent = ''; }
    }
    async function runs(more = false) {
        if (!page) return;
        const selected = $('storage-run').value;
        const rows = await api(`/api/runs?offset=${more ? state.runs.length : 0}`);
        state.runs = more ? [...state.runs, ...rows] : rows;
        $('storage-run').replaceChildren(...state.runs.map(run => {
            const option = node('option', `${new Date(run.created * 1000).toLocaleString('it-IT')} · ` +
                `${run.state === 'finished' ? 'conclusa' : 'aperta'} · ${run.id}`);
            option.value = run.id; return option;
        }));
        if (state.runs.some(run => run.id === selected)) $('storage-run').value = selected;
        else if (state.runs.some(run => run.id === state.current)) $('storage-run').value = state.current;
        $('storage-more').disabled = rows.length < 50;
        selection(); await snapshots();
    }
    async function checkpoints() {
        if (!page) return;
        const result = await api('/api/checkpoints');
        const selected = $('storage-checkpoint').value;
        $('storage-checkpoint').replaceChildren(...result.checkpoints.map(checkpoint => {
            const option = node('option', `${new Date(checkpoint.created * 1000).toLocaleString('it-IT')} · ` +
                `tick ${checkpoint.tick.toLocaleString('it-IT')} · ${bytes(checkpoint.bytes)} · ` +
                (checkpoint.automatic ? 'automatico' : 'manuale'));
            option.value = checkpoint.id; return option;
        }));
        if (result.checkpoints.some(item => item.id === selected)) $('storage-checkpoint').value = selected;
        if (result.operation.error) $('storage-checkpoint-message').textContent = result.operation.error;
        else if (!['idle', 'done'].includes(result.operation.stage))
            $('storage-checkpoint-message').textContent = `Operazione: ${result.operation.stage}`;
        selection();
    }
    async function refresh() {
        if (closed || state.refreshing || state.busy) return;
        state.refreshing = true;
        try { state.status = await api('/api/status'); state.current = state.status.run_id;
            render(await api('/api/storage')); if (page) await checkpoints(); }
        catch (error) { message(error.message); }
        finally { state.refreshing = false; }
    }
    async function perform(work) {
        if (state.busy || closed) return;
        state.busy = true; selection();
        try { await work(); await refreshAfterWork(); } catch (error) { message(error.message); }
        finally { state.busy = false; selection(); }
    }
    async function refreshAfterWork() {
        state.status = await api('/api/status'); state.current = state.status.run_id;
        render(await api('/api/storage'));
        await runs(); await checkpoints();
    }
    async function remove(run) {
        const selected = state.runs.find(item => item.id === run);
        const connected = run === state.current;
        if (!window.confirm(`Eliminare definitivamente la run ${run}${selected ? ` del ${new Date(selected.created * 1000).toLocaleString('it-IT')}` : ''}?\n` +
            (connected ? 'La simulazione collegata verrà fermata. ' : '') +
            'Saranno eliminati storico e snapshot neurali riconosciuti della run.')) return;
        await perform(async () => { await api(`/api/runs/${encodeURIComponent(run)}`, {
            method: 'DELETE', body: JSON.stringify({confirm_run: run})});
            message('Rimozione avviata. Lo stato viene aggiornato qui.'); });
    }
    if (page) {
        $('storage-refresh').onclick = async () => { await refresh(); try { await runs(); } catch (error) { message(error.message); } };
        $('storage-more').onclick = () => runs(true).catch(error => message(error.message));
        $('storage-run').onchange = () => { selection(); snapshots().catch(error => message(error.message)); };
        $('storage-delete').onclick = () => remove($('storage-run').value);
        $('storage-snapshots-more').onclick = () => snapshots(true).catch(error => message(error.message));
        $('storage-clean').onclick = async () => {
            if (!window.confirm('Pulire completamente lo storico SQLite? Verrà salvato e verificato un checkpoint, poi la simulazione ripartirà da esso senza dati precedenti.')) return;
            await perform(async () => { message('Salvataggio, verifica e pulizia in corso…');
                const result = await api('/api/storage/clean', {method: 'POST',
                    body: JSON.stringify({confirm: 'clean_history'})});
            message(result.run_id ? `Storico pulito. Nuova run ${result.run_id} attiva.` :
                'Storico pulito. Nessuna simulazione avviata.'); });
        };
        $('storage-checkpoint-save').onclick = () => perform(async () => {
            $('storage-checkpoint-message').textContent = 'Salvataggio e verifica…';
            await api('/api/checkpoints', {method: 'POST'});
            $('storage-checkpoint-message').textContent = 'Checkpoint salvato e verificato.';
        });
        $('storage-checkpoint-load').onclick = async () => {
            const id = $('storage-checkpoint').value;
            if (!id || !window.confirm('Caricare il checkpoint selezionato? Avvierà una nuova run con configurazione salvata e grafici vuoti.')) return;
            await perform(async () => { $('storage-checkpoint-message').textContent = 'Caricamento…';
                await api(`/api/checkpoints/${id}/load`, {method: 'POST', body: JSON.stringify({confirm: id})});
                $('storage-checkpoint-message').textContent = 'Checkpoint caricato in una nuova run.';
            });
        };
    }
    refresh().then(() => { if (page) return runs().catch(error => message(error.message)); });
    const timer = setInterval(() => { if (!document.hidden) refresh(); }, 15000);
    document.addEventListener('popx:exit', () => { closed = true; clearInterval(timer);
        mini.replaceChildren(node('p', 'Aggiornamenti terminati.')); });
})();
