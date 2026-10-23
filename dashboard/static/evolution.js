'use strict';
(() => {
    const $ = id => document.getElementById(id);
    const state = {run:null, threshold:.9, report:null, detail:null, page:0, api:'', token:0, charts:{}, runs:[],
        extinctions:[], compositionDerivative:false};
    const extinctionLines = window.PopXChartMath.extinctionPlugin(() => state.extinctions);
    const fmt = v => v === null || v === undefined ? '—' : Number(v).toLocaleString('it-IT',{maximumFractionDigits:2});
    const pct = v => v === null || v === undefined ? '—' : `${fmt(100*v)}%`;
    const short = id => id.length > 18 ? '…'+id.slice(-16) : id;
    const rgb = (c,alpha=1) => `rgba(${c.join(',')},${alpha})`;
    const node = (tag,text) => {const e=document.createElement(tag);if(text!==undefined)e.textContent=text;return e;};
    function fail(e) {$('ev-error').hidden=false;$('ev-error').textContent=e.message;}
    const guard = fn => async (...args) => {try {await fn(...args);} catch(e){fail(e);}};
    async function json(url) {
        const r=await fetch(url,{cache:'no-store'});
        if(!r.ok){const error=new Error(`Lettura fallita (${r.status}).`);error.status=r.status;
            try{error.message=(await r.json()).detail||error.message;}catch{} throw error;}
        return r.json();
    }
    async function analysis(path) {
        if(state.api) return json(state.api+path);
        try{return await json(path);}catch(e){
            if(e.status!==404)throw e;
            const fallback=`http://${location.hostname}:8002`;
            try {const value=await json(fallback+path);state.api=fallback;return value;}
            catch {throw new Error('Il lettore genealogico è in attesa di avvio. La simulazione continua: il servizio dedicato usa la porta locale 8002; il nuovo backend integra questa funzione al prossimo avvio.');}
        }
    }
    const path = suffix => `/api/evolution/${encodeURIComponent(state.run)}${suffix||''}`;
    const query = () => `threshold=${state.threshold}`;
    async function runs(more=false) {
        const page=await json(`/api/runs?offset=${more?state.runs.length:0}`);
        state.runs=more?[...state.runs,...page]:page;
        $('ev-run').replaceChildren(...state.runs.map(r=>{const o=node('option',`${new Date(r.created*1000).toLocaleString('it-IT')} · ${r.state}`);o.value=r.id;return o;}));
        if(!state.run){const live=await json('/api/status');state.run=live.run_id||state.runs[0]?.id;}
        $('ev-run').value=state.run||'';$('ev-more-runs').disabled=page.length<50;
    }
    async function load(refresh=true) {
        const threshold=Number($('ev-threshold').value);
        if(!Number.isFinite(threshold)||threshold<.5||threshold>1)throw new Error('Scegli una soglia fra 0,5 e 1.');
        state.threshold=threshold;state.run=$('ev-run').value;
        if(!state.run){$('ev-content').hidden=true;$('ev-progress').textContent='Nessuna simulazione registrata. Avvia una run per raccogliere le genealogie.';return;}
        const token=++state.token;
        state.extinctions=[];
        $('ev-error').hidden=true;$('ev-content').hidden=true;$('ev-progress').textContent='Lettura dello storico…';
        let report=await analysis(path()+`?${query()}&refresh=${refresh}`);
        while(report.state!=='ready'){
            if(token!==state.token)return;
            $('ev-progress').textContent=`Ricostruzione in corso · ${fmt(report.processed||0)} eventi letti · ${fmt(report.individuals||0)} individui. La run prosegue.`;
            await new Promise(resolve=>setTimeout(resolve,2000));
            if(token!==state.token)return;
            report=await analysis(path()+`?${query()}`);
        }
        if(token!==state.token)return;
        state.report=report;state.detail=null;state.page=0;state.individualOffset=0;
        $('ev-comparison').textContent='';
        $('ev-content').hidden=false;$('ev-progress').textContent=`Analisi al tick ${fmt(report.cutoff)} · aggiorna su richiesta`;
        const q=report.integrity;
        $('ev-quality').textContent=q.consistent ? `Eventi e campione concordano: ${fmt(q.births_events)} individui registrati, ${fmt(q.deaths_events)} vite concluse. Modello della run: ${report.source_model}.` :
            `Copertura da verificare: vivi ricostruiti ${q.alive_events} / campione ${q.alive_sample}; nascite ${q.births_events} / ${q.births_sample}; genitori mancanti ${q.missing_parents}; lacune ${q.telemetry_gaps}. I risultati descrivono gli eventi disponibili.`;
        $('ev-alive').textContent=fmt(report.overview.alive);$('ev-cutoff').textContent=`Tick ${fmt(report.cutoff)}`;
        $('ev-strains').textContent=fmt(report.strains.filter(s=>s.alive>0).length);
        $('ev-strains-total').textContent=`${fmt(report.strains.length)} ceppi nell’intera storia`;
        $('ev-roots').textContent=fmt(report.roots.filter(s=>s.alive>0).length);$('ev-cap').textContent=pct(report.cap_fraction);
        $('ev-detail-title').textContent='Seleziona un ceppo';$('ev-detail-summary').textContent='';$('ev-individuals').replaceChildren();
        state.tree?.destroy();state.tree=null;
        renderTimeline();renderOutcomes();renderTable();
        guard(async()=>{
            const result=await json(`/api/runs/${encodeURIComponent(state.run)}/extinctions`);
            if(token!==state.token)return;
            state.extinctions=result.ticks;
            for(const id of ['ev-composition','ev-turnover','ev-cap-chart'])state.charts[id]?.update('none');
        })();
    }
    function chart(id,type,datasets,options={}) {
        state.charts[id]?.destroy();
        const x=options.x||{type:'linear',title:{display:true,text:'Tick'}};
        const y=options.y||{beginAtZero:true};
        const tickAxis=['ev-composition','ev-turnover','ev-cap-chart'].includes(id);
        state.charts[id]=new Chart($(id),{type,data:{datasets},plugins:tickAxis?[extinctionLines]:[],options:{responsive:true,maintainAspectRatio:false,animation:false,
            parsing:false,interaction:{mode:'nearest',intersect:false},plugins:{legend:{labels:{color:'#92a3b9',boxWidth:10}},...(options.plugins||{})},
            scales:{x:{...x,ticks:{color:'#92a3b9'},grid:{color:'#273246'}},y:{...y,ticks:{color:'#92a3b9'},grid:{color:'#273246'}},...(options.extraScales||{})},
            onClick:options.onClick||(()=>{})}});
        return state.charts[id];
    }
    function series(label,points,color,extra={}) {return {label,data:points,borderColor:color,backgroundColor:color,borderWidth:1.5,pointRadius:0,...extra};}
    function windowed(rows) {
        const lo=Number($('ev-from').value||0),hi=Number($('ev-to').value||state.report.cutoff);
        return rows.filter(r=>r.tick>=lo&&r.tick<=hi);
    }
    function renderTimeline() {
        if(!state.report)return;
        const r=state.report,roots=$('ev-group').value==='root';
        const rows=windowed(roots?r.root_timeline:(state.detail?.timeline||r.timeline));
        const ids=Object.keys(rows[0]?.counts||{});
        chart('ev-composition','line',ids.map(id=>{
            const strain=r.strains.find(s=>s.id===id);const c=strain?.color||[120,135,155];
            const values=rows.map((p,i)=>{
                const previous=rows[i-1],interval=previous?p.tick-previous.tick:0;
                return {x:p.tick,y:state.compositionDerivative?(interval>0?((p.counts[id]||0)-(previous.counts[id]||0))/interval:null):(p.counts[id]||0)};
            });
            return series(id==='other'?'Altri':short(id),values,rgb(c),
                {backgroundColor:rgb(c,.5),fill:!state.compositionDerivative,stack:'population',strain:id});
        }),{y:{stacked:!state.compositionDerivative,beginAtZero:!state.compositionDerivative,
               title:{display:true,text:state.compositionDerivative?'Variazione agenti / tick':'Agenti vivi'}},onClick:(e,items,c)=>{
            if(items.length&&!roots){const id=c.data.datasets[items[0].datasetIndex].strain;if(id!=='other')guard(select)(id);}
        }});
        const time=windowed(r.timeline);
        chart('ev-turnover','line',[
            series('Ceppi effettivi',time.map(p=>({x:p.tick,y:p.effective_strains})),'#88aaff'),
            series('Ricambio delle quote',time.map(p=>({x:p.tick,y:p.turnover})),'#ffc778',{yAxisID:'turnover'})
        ],{y:{beginAtZero:true,title:{display:true,text:'Numero equivalente'}},extraScales:{turnover:{position:'right',min:0,max:1,title:{display:true,text:'Ricambio 0…1'},grid:{drawOnChartArea:false}}}});
        chart('ev-cap-chart','line',[series('Idoneità bloccata (%)',windowed(r.cap_timeline).map(p=>({x:p.tick,y:p.blocked_fraction===null?null:100*p.blocked_fraction})),'#fb8b83')],{y:{min:0,max:100}});
    }
    function renderOutcomes() {
        const r=state.report,d=state.detail;
        const colors=new Map(r.strains.map(s=>[s.id,s.color]));
        const points=r.strains.filter(s=>s.children_completed.n>0).map(s=>({x:s.age_completed.mean,y:s.children_completed.mean,r:Math.min(15,3+Math.sqrt(s.completed)/3),strain:s.id,n:s.completed,std:s.children_completed.std}));
        chart('ev-offspring','bubble',[{label:'Ceppi con vite concluse',data:points,backgroundColor:points.map(p=>rgb(colors.get(p.strain),.7))}],{
            x:{type:'linear',title:{display:true,text:'Età media alla morte (tick)'}},y:{beginAtZero:true,title:{display:true,text:'Figli naturali per vita conclusa'}},
            plugins:{tooltip:{callbacks:{label:c=>`${short(c.raw.strain)} · ${fmt(c.raw.n)} vite · figli ${fmt(c.raw.y)} ± ${fmt(c.raw.std)}`}}},
            onClick:(e,items,c)=>{if(items.length)guard(select)(c.data.datasets[0].data[items[0].index].strain);}});
        const survival=[series('Tutta la popolazione',r.survival.map(p=>({x:p.age,y:p.survival,...p})),'#88aaff',{stepped:'after'})];
        if(d)survival.push(series(`Ceppo ${short(d.strain)}`,d.survival.map(p=>({x:p.age,y:p.survival,...p})),'#48d9b8',{stepped:'after'}));
        chart('ev-survival','line',survival,{x:{type:'linear',title:{display:true,text:'Età (tick)'}},y:{min:0,max:1,title:{display:true,text:'Sopravvivenza stimata'}},plugins:{tooltip:{callbacks:{label:c=>`${c.dataset.label}: ${pct(c.raw.y)} · a rischio ${c.raw.risk} · morti ${c.raw.deaths} · vivi censurati ${c.raw.censored}`}}}});
        const bins=(d||r).age_histogram;
        chart('ev-ages','bar',[
            series('Morti per cause ordinarie',bins.map(p=>({x:p.start,y:p.deaths,end:p.end})),'#88aaff'),
            series('Abbattimenti NewGen',bins.map(p=>({x:p.start,y:p.culls,end:p.end})),'#fb8b83'),
            series('Vivi: età raggiunta',bins.map(p=>({x:p.start,y:p.censored,end:p.end})),'#48d9b8')
        ],{x:{type:'linear',title:{display:true,text:'Inizio intervallo di età (tick)'}},y:{beginAtZero:true,title:{display:true,text:'Individui'}},plugins:{tooltip:{callbacks:{title:items=>items.length?`${fmt(items[0].raw.x)} ≤ età < ${fmt(items[0].raw.end)}`:''}}}});
    }
    function renderTable() {
        if(!state.report)return;
        const text=$('ev-search').value.toLowerCase(),filter=$('ev-filter').value,sort=$('ev-sort').value;
        const rows=state.report.strains.filter(s=>(filter==='all'||(filter==='alive'?s.alive>0:s.alive===0))&&`${s.id} ${s.root} ${s.fingerprint}`.toLowerCase().includes(text));
        const value=s=>sort==='offspring'?(s.children_completed.mean??-1):s[sort];
        rows.sort((a,b)=>value(b)-value(a)||a.id.localeCompare(b.id));
        state.page=Math.min(state.page,Math.max(0,Math.ceil(rows.length/50)-1));
        $('ev-strain-rows').replaceChildren(...rows.slice(state.page*50,(state.page+1)*50).map(s=>{
            const tr=node('tr'),id=node('td'),button=node('button',short(s.id)),chip=node('span');chip.className='ev-chip';chip.style.backgroundColor=rgb(s.color);
            button.prepend(chip);button.title=`Ceppo ${s.id} · firma ${s.fingerprint}`;button.onclick=guard(()=>select(s.id));id.append(button,node('br'),node('small',`Famiglia ${short(s.root)}`));tr.append(id);
            for(const text of [`${s.alive} / ${s.total}`,fmt(s.completed),`${fmt(s.children_completed.mean)} ± ${fmt(s.children_completed.std)}`,fmt(s.age_completed.median),`${fmt(s.born)} / ${fmt(s.last_tick)}`])tr.append(node('td',text));
            const compare=node('td');for(const which of ['a','b']){const b=node('button',which.toUpperCase());b.onclick=()=>{$(`ev-compare-${which}`).value=s.representative;};compare.append(b);}tr.append(compare);return tr;
        }));
        $('ev-pages').textContent=`${fmt(rows.length)} ceppi · pagina ${state.page+1} di ${Math.max(1,Math.ceil(rows.length/50))}`;
        $('ev-prev').disabled=state.page===0;$('ev-next').disabled=(state.page+1)*50>=rows.length;
    }
    async function select(id,more=false) {
        const token=state.token;
        const selection=state.selection=(state.selection||0)+1;
        if(!more)state.individualOffset=0;
        const detail=await analysis(path(`/strain/${encodeURIComponent(id)}`)+`?${query()}&offset=${state.individualOffset||0}`);
        if(token!==state.token||selection!==state.selection)return;
        state.detail=detail;const s=detail.summary;
        $('ev-detail-title').textContent=`Ceppo ${id}`;
        $('ev-detail-summary').textContent=`${s.alive} vivi; ${s.completed} vite concluse; profondità genealogica massima ${s.max_depth} (include i passaggi tramite riserva). Recuperi ereditari: ${s.inherited_recoveries??0}.
Figli delle vite concluse: media ${fmt(s.children_completed.mean)}, deviazione standard ${fmt(s.children_completed.std)}, mediana ${fmt(s.children_completed.median)}, 90° percentile ${fmt(s.children_completed.p90)}.
Figli dei vivi (conteggio ancora aperto): media ${fmt(s.children_alive.mean)}. Nascite naturali per 1.000 agenti × tick: ${fmt(s.births_per_1000_agent_ticks)}.
Età media delle morti ordinarie: ${fmt(s.natural_death_age.mean)} tick (${s.natural_death_age.n} vite); degli abbattimenti: ${fmt(s.culled_age.mean)} tick (${s.culled_age.n} vite).`;
        if(!more)$('ev-individuals').replaceChildren();
        for(const n of detail.individuals){const tr=node('tr');for(const text of [`${n.id} / ${n.parent||'origine'}${n.recovery?' · recupero ereditario':n.forced?' · nascita forzata':''}`,fmt(n.born),fmt(n.age),fmt(n.children),fmt(n.depth),n.censored?'Vivo · osservazione aperta':`Morto · ${n.cause}`])tr.append(node('td',text));$('ev-individuals').append(tr);}
        state.individualOffset+=detail.individuals.length;$('ev-more-individuals').disabled=state.individualOffset>=detail.total;
        if(!more){renderOutcomes();renderTree(detail);renderTimeline();}
    }
    function renderTree(detail) {
        state.tree?.destroy();
        const ids=new Set(detail.genealogy.map(s=>s.id)),elements=[];
        for(const s of detail.genealogy){elements.push({data:{id:s.id,label:`${short(s.id)}\n${s.alive} vivi · ${s.total} totali`,color:rgb(s.color),selected:s.id===detail.strain}});
            if(ids.has(s.parent))elements.push({data:{id:`${s.parent}>${s.id}`,source:s.parent,target:s.id}});}
        state.tree=cytoscape({container:$('ev-tree'),elements,layout:{name:'breadthfirst',directed:true,padding:30,spacingFactor:1.4},style:[
            {selector:'node',style:{label:'data(label)','text-wrap':'wrap','background-color':'data(color)',color:'#e7eef7','font-size':11,width:24,height:24,'text-valign':'bottom'}},
            {selector:'edge',style:{width:1.5,'line-color':'#60758c','target-arrow-shape':'triangle','target-arrow-color':'#60758c','curve-style':'bezier'}},
            {selector:'node:selected',style:{'border-width':3,'border-color':'#ffffff'}}]});
        state.tree.getElementById(detail.strain).select();state.tree.on('tap','node',e=>guard(select)(e.target.id()));
        $('ev-tree-note').textContent=`Rami genealogici fra ceppi: ogni freccia collega il ceppo del genitore al nuovo ceppo del figlio. Clicca per esplorare; trascina e ingrandisci per orientarti.${detail.genealogy_truncated?' Vista limitata a 100 antenati e 100 rami figli: esplora i nodi per continuare.':''}`;
    }
    $('ev-load').onclick=guard(()=>load(true));$('ev-run').onchange=guard(()=>load(false));$('ev-more-runs').onclick=guard(()=>runs(true));
    $('ev-window').onclick=renderTimeline;$('ev-group').onchange=renderTimeline;
    $('ev-derivative').onclick=()=>{
        state.compositionDerivative=!state.compositionDerivative;
        $('ev-derivative').textContent=state.compositionDerivative?'Mostra valori':'Mostra derivata /tick';
        $('ev-derivative').setAttribute('aria-pressed',String(state.compositionDerivative));
        renderTimeline();
    };
    for(const id of ['ev-search','ev-filter','ev-sort'])$(id).oninput=()=>{state.page=0;renderTable();};
    $('ev-prev').onclick=()=>{state.page--;renderTable();};$('ev-next').onclick=()=>{state.page++;renderTable();};
    $('ev-more-individuals').onclick=guard(()=>state.detail&&select(state.detail.strain,true));
    $('ev-clear').onclick=()=>{state.selection=(state.selection||0)+1;state.detail=null;state.tree?.destroy();state.tree=null;
        $('ev-detail-title').textContent='Seleziona un ceppo';$('ev-detail-summary').textContent='';
        $('ev-individuals').replaceChildren();$('ev-tree-note').textContent='';renderOutcomes();renderTimeline();};
    $('ev-compare').onclick=guard(async()=>{
        const token=state.token;
        const a=$('ev-compare-a').value.trim(),b=$('ev-compare-b').value.trim();
        if(!a||!b)throw new Error('Scegli due agenti con i pulsanti A e B, oppure inserisci i loro ID.');
        const c=await analysis(path('/compare')+`?${query()}&first=${encodeURIComponent(a)}&second=${encodeURIComponent(b)}`);
        if(token!==state.token)return;
        $('ev-comparison').textContent=`Somiglianza genetica: ${pct(c.similarity)} · strutturale: ${pct(c.topology_similarity)}.
Geni: ${c.genes.join(' / ')} · stessa famiglia: ${c.same_root?'sì':'no'} · stesso ceppo alla soglia scelta: ${c.same_strain?'sì':'no'}.
Firme: ${c.fingerprints.join(' / ')}.
Tratti A: ${JSON.stringify(c.traits[0])}\nTratti B: ${JSON.stringify(c.traits[1])}`;
    });
    guard(async()=>{await runs();await load(false);})();
})();
