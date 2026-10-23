'use strict';
(() => {
    const $=id=>document.getElementById(id),page=!!$('storage-page');
    const state={runs:[],current:null,report:null,busy:false,refreshing:false};
    let closed=false;
    const node=(tag,text)=>{const n=document.createElement(tag);if(text!==undefined)n.textContent=text;return n;};
    const bytes=n=>{const units=['B','KiB','MiB','GiB','TiB'];let i=0;while(n>=1024&&i<4){n/=1024;i++;}return `${n.toLocaleString('it-IT',{maximumFractionDigits:2})} ${units[i]}`;};
    async function api(path,options){
        const r=await fetch(path,{cache:'no-store',headers:{'Content-Type':'application/json'},...options});
        if(closed)throw new Error('Dashboard chiusa');
        if(!r.ok){let message=`Richiesta fallita (${r.status})`;try{message=(await r.json()).detail||message;}catch{}
            if(r.status===404&&path==='/api/storage')message='Le funzioni di spazio e rimozione saranno disponibili dopo il riavvio del backend aggiornato.';
            throw new Error(message);}return r.json();
    }
    const widget=node('section');widget.className='storage-widget';
    const link=node('a','Spazio e storico');link.href='/static/storage.html';widget.append(link);
    const mini=node('div');widget.append(mini);document.querySelector('aside')?.append(widget);
    function bar(r){const p=node('progress');p.className='storage-bar';p.max=r.disk_total;p.value=r.disk_unavailable;
        p.setAttribute('aria-label','Quota del disco occupata o riservata');
        if(r.disk_available/r.disk_total<.1)p.classList.add('storage-low');return p;}
    function render(r){
        state.report=r;const used=100*r.disk_unavailable/r.disk_total;
        mini.replaceChildren(bar(r),node('p',`${used.toFixed(1)}% ${r.wsl?'del filesystem WSL':'del disco'} occupato o riservato`),
            node('p',`${bytes(r.disk_available)} disponibili ${r.wsl?'nel filesystem Linux':''}`),node('p',`Database e scritture: ${bytes(r.database_files_bytes)}`));
        if(r.wsl)mini.append(node('p','Spazio libero reale su Windows: non misurato.'));
        if(r.guard){
            const limit=node('progress');limit.className='storage-bar';limit.max=r.guard.limit_bytes;
            limit.value=Math.min(r.database_files_bytes,r.guard.limit_bytes);limit.setAttribute('aria-label','Database rispetto alla soglia di rotazione');
            mini.append(limit,node('p',`Arresto run a ${r.guard.limit_gb} GB decimali`));
            if(r.guard.exceeded)mini.append(node('p',r.guard.stop_requested?'Arresto per limite database richiesto.':'Limite database raggiunto: rotazione checkpoint.'));
            if(r.guard.error)mini.append(node('p',`Controllo spazio: ${r.guard.error}`));
        }
        mini.title=`Misurato alle ${new Date(r.measured_at*1000).toLocaleTimeString('it-IT')}`;
        if(!page)return;
        const cards=node('div');cards.className='storage-numbers';
        for(const [label,value] of [[r.wsl?'Disponibile nel filesystem WSL':'Disponibile sul disco',r.disk_available],[r.wsl?'Capacità virtuale WSL':'Capacità del disco',r.disk_total],
            ['File del database e scritture',r.database_files_bytes],['Riutilizzabile nel database',r.database_reusable_bytes]]){
            const c=node('div',label);c.append(node('strong',bytes(value)));cards.append(c);}
        $('storage-meter').replaceChildren(bar(r),cards,node('p',`${used.toFixed(1)}% occupato o riservato · ${r.run_count} run salvate · aggiornamento ogni 15 secondi`));
        $('storage-jobs').replaceChildren(...r.removals.map(job=>{
            const p=node('p',`${job.run}: ${{deleting:'rimozione in corso',deleted:'rimossa',error:'rimozione da completare'}[job.state]} · ${job.rows_deleted.toLocaleString('it-IT')} righe · ${job.files_deleted} snapshot gestiti${job.files_retained?` · ${job.files_retained} file conservati per verifica`:''}${job.error?` · ${job.error}`:''}`);
            if(job.state==='error'){const retry=node('button','Riprendi rimozione');retry.onclick=()=>remove(job.run);p.append(retry);}return p;}));select();
    }
    function message(text){if(closed)return;if(page)$('storage-message').textContent=text;else mini.textContent=text;}
    function select(){if(!page)return;const r=state.runs.find(r=>r.id===$('storage-run').value);
        const deleting=state.report?.removals.some(j=>j.state==='deleting');
        $('storage-delete').disabled=state.busy||!!deleting||!r||r.id===state.current||r.state!=='finished';
        $('storage-selection').textContent=r?`${r.id} · ${r.state==='finished'?'conclusa':'aperta'}${r.id===state.current?' · collegata al simulatore':''}`:'Nessuna run selezionata';}
    async function runs(more=false){
        const selected=$('storage-run').value,rows=await api(`/api/runs?offset=${more?state.runs.length:0}`);
        state.runs=more?[...state.runs,...rows]:rows;
        $('storage-run').replaceChildren(...state.runs.map(r=>{const o=node('option',`${new Date(r.created*1000).toLocaleString('it-IT')} · ${r.state==='finished'?'conclusa':'aperta'} · ${r.id}`);o.value=r.id;return o;}));
        if(state.runs.some(r=>r.id===selected))$('storage-run').value=selected;
        $('storage-more').disabled=rows.length<50;select();
    }
    async function refresh(){
        if(closed||state.refreshing)return;state.refreshing=true;
        try{if(page)state.current=(await api('/api/status')).run_id;render(await api('/api/storage'));}
        catch(e){message(e.message);}finally{state.refreshing=false;}
    }
    async function remove(run){
        if(state.busy)return;const r=state.runs.find(r=>r.id===run);
        if(!window.confirm(`Rimuovere definitivamente la run ${run}${r?` del ${new Date(r.created*1000).toLocaleString('it-IT')}`:''}?\nSaranno eliminati i dati e gli snapshot riconosciuti di questa run. Lo spazio interno verrà reso riutilizzabile.`))return;
        state.busy=true;select();
        try{await api(`/api/runs/${encodeURIComponent(run)}`,{method:'DELETE',body:JSON.stringify({confirm_run:run})});
            message('Rimozione avviata. L’avanzamento si aggiorna qui; puoi continuare a usare la dashboard.');
            await runs();await refresh();}
        catch(e){message(e.message);}finally{state.busy=false;select();}
    }
    if(page){$('storage-refresh').onclick=async()=>{await refresh();try{await runs();}catch(e){message(e.message);}};
        $('storage-more').onclick=()=>runs(true).catch(e=>message(e.message));$('storage-run').onchange=select;
        $('storage-delete').onclick=()=>remove($('storage-run').value);}
    refresh().then(()=>{if(page)return runs().catch(e=>message(e.message));});
    const timer=setInterval(()=>{if(!document.hidden)refresh();},15000);
    document.addEventListener('popx:exit',()=>{closed=true;clearInterval(timer);mini.replaceChildren(node('p','Aggiornamenti terminati.'));});
})();
