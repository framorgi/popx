(() => {
    let closed = false, busy = false;
    const box = document.createElement('section'); box.className = 'card';
    const title = document.createElement('h2'); title.textContent = 'Checkpoint';
    const help = document.createElement('p');
    help.textContent = 'Salva ambiente e Pop nello stato attuale. Caricare avvia una nuova run, con grafici vuoti e configurazione INI della snapshot bloccata. Al limite del database il sistema salva, verifica, elimina tutti gli storici e riparte; conserva gli ultimi due checkpoint automatici e tutti quelli manuali.';
    const select = document.createElement('select'), save = document.createElement('button'), load = document.createElement('button');
    save.textContent = 'Salva checkpoint'; load.textContent = 'Carica checkpoint';
    const message = document.createElement('p'); message.setAttribute('role', 'status');
    box.append(title, help, select, save, load, message);
    (document.querySelector('main') || document.body).append(box);
    async function api(path, method = 'GET', body) {
        const r = await fetch(path, {method, headers: {'Content-Type': 'application/json'}, body: body ? JSON.stringify(body) : undefined});
        const d = await r.json(); if (!r.ok) throw Error(d.detail || r.statusText); return d;
    }
    async function refresh() {
        if (closed || busy) return;
        try {
            const d = await api('/api/checkpoints'); if (closed) return;
            const selected = select.value; select.replaceChildren();
            for (const c of d.checkpoints) {
                const o = document.createElement('option'); o.value = c.id;
                o.textContent = `${new Date(c.created * 1000).toLocaleString()} · tick ${c.tick} · ${(c.bytes / 1e6).toFixed(1)} MB · ${c.automatic ? 'automatico' : 'manuale'}`;
                select.append(o);
            }
            if ([...select.options].some(o => o.value === selected)) select.value = selected;
            load.disabled = !select.value;
            if (d.operation.error) message.textContent = `Rotazione sospesa: ${d.operation.error}`;
            else if (!['idle', 'done'].includes(d.operation.stage)) message.textContent = `Rotazione: ${d.operation.stage}`;
        } catch (e) { if (!closed) message.textContent = e.message; }
    }
    async function perform(fn) {
        if (closed || busy) return; busy = true; save.disabled = load.disabled = true;
        try { await fn(); } catch (e) { if (!closed) message.textContent = e.message; }
        finally { busy = false; if (!closed) { save.disabled = false; await refresh(); } }
    }
    save.onclick = () => perform(async () => { message.textContent = 'Salvataggio e verifica…'; await api('/api/checkpoints', 'POST'); if (!closed) message.textContent = 'Checkpoint salvato e verificato.'; });
    load.onclick = () => {
        const id = select.value;
        if (!id || !confirm('Caricare questo checkpoint? La run attuale sarà conclusa e la configurazione sovrascritta con quella salvata.')) return;
        perform(async () => { message.textContent = 'Caricamento…'; await api(`/api/checkpoints/${id}/load`, 'POST', {confirm: id}); if (!closed) location.reload(); });
    };
    const timer = setInterval(refresh, 5000);
    document.addEventListener('popx:exit', () => { closed = true; clearInterval(timer); box.replaceChildren(); });
    refresh();
})();
