'use strict';
(() => {
    const $ = id => document.getElementById(id), design = window.POPX_DESIGN;
    let status = {}, schema = [], snapshot = null, selectedEdge = null, averages = [], peaks = [], lastAgent = '', lastTick = -1;
    let graphEdges = [], graphNodes = [], polling = false, inspecting = false, runtimeVersion = '';
    const message = (text, error = false) => { $('message').textContent=text;$('message').style.color=`var(--${error?'red':'accent'})`; };
    const safe = fn => async event => { try { await fn(event); } catch(e) { message(e.message,true); } };
    async function api(path, body) {
        const r=await fetch(path,body===undefined?{}:{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
        const data=await r.json();if(!r.ok)throw Error(typeof data.detail==='string'?data.detail:JSON.stringify(data.detail));return data;
    }
    async function command(name,args={}) {
        const reply=await api('/api/commands',{command:name,arguments:args,run_id:status.run_id});
        if(reply.ok===false)throw Error(reply.error);
        if(reply.status)status={...status,...reply.status};
        message('Applicato dal motore: '+name);await refresh();return reply.result;
    }
    function node(tag,text,props={}) {const e=document.createElement(tag);if(text)e.textContent=text;Object.assign(e,props);return e;}
    function renderRuntime() {
        const values=status.runtime||{};
        const mixer=$('mixer');mixer.replaceChildren();
        const total=design.learning.reduce((s,f)=>s+(values[`reward_${f.key}_weight`]||0),0);
        for(const factor of design.learning) {
            const key=`reward_${factor.key}_weight`,value=values[key]||0;
            const strip=node('div',null,{className:'mixer-strip'});strip.style.setProperty('--factor',factor.color);
            const label=node('label',factor.label),readout=node('output',`${value.toFixed(3)} · quota ${total?(100*value/total).toFixed(1):0}%`);
            const input=node('input',null,{type:'range',min:'0',max:'1',step:'0.005',value});input.setAttribute('aria-label',factor.label);label.append(readout);
            input.oninput=()=>{readout.textContent=Number(input.value).toFixed(3);};input.onchange=safe(()=>command('runtime_apply',{[key]:Number(input.value)}));strip.append(label,input);mixer.append(strip);
        }
        $('runtime').replaceChildren();
        for(const spec of schema.filter(s=>!s.name.startsWith('reward_'))) {
            const label=node('label',spec.name==='climate_rotation_speed'?'Velocità rotazione clima':spec.name,{title:spec.description});
            const input=node('input',null,{type:'number',min:spec.min,max:spec.max,step:spec.type==='integer'?'1':'any',value:values[spec.name]});input.setAttribute('aria-label',spec.description);
            const button=node('button','Applica',{type:'button'});button.onclick=safe(()=>{if(!input.reportValidity())return;return command('runtime_apply',{[spec.name]:Number(input.value)});});label.append(input,button);$('runtime').append(label);
        }
    }
    function renderStatus() {
        $('run-state').textContent=`${status.connected?'Collegato':'Disconnesso'} · ${status.state==='paused'?'In pausa':status.state==='running'?'In esecuzione':'Conclusa'} · tick ${status.tick||0} · ${((status.simulated_seconds||0)/60).toFixed(1)} min · ${status.alive||0} vivi · ${(status.effective_tps||0).toFixed(1)} tick/s · ${(status.render_fps||0).toFixed(1)} FPS`;
        $('play').textContent=status.state==='paused'?'Riprendi':'Pausa';
        const paused=status.connected&&status.state==='paused'&&!status.step_remaining;
        document.querySelectorAll('.requires-pause,[data-ticks],#step').forEach(b=>b.disabled=!paused);
        const version=JSON.stringify(status.runtime);
        if(version!==runtimeVersion&&!document.querySelector('#mixer input:focus,#runtime input:focus')){renderRuntime();runtimeVersion=version;}
        if(status.appearance) {
            window.applyPopXAppearance(status.appearance);
            for(const [id,key] of [['theme','theme'],['agent-scale','agent_scale'],['ui-scale','ui_scale'],['grid','grid'],['labels','labels']]) {
                if(document.activeElement===$(id))continue;
                if($(id).type==='checkbox')$(id).checked=status.appearance[key];else $(id).value=status.appearance[key];
            }
        }
    }
    async function refresh() {
        if(polling)return;polling=true;
        try {const oldRun=status.run_id;status=await api('/api/status');if(!status.connected||(oldRun&&oldRun!==status.run_id)){snapshot=null;selectedEdge=null;lastAgent='';$('live').checked=false;$('agent').replaceChildren();drawBrain();}renderStatus();}
        finally{polling=false;}
    }
    async function contributions() {
        if(!status.run_id)return;
        const result=await api(`/api/runs/${encodeURIComponent(status.run_id)}/samples?start=${Math.max(0,(status.tick||0)-1000)}&limit=10`);
        const list=Array.isArray(result)?result:result.samples||[];const sample=list.at(-1)?.data||list.at(-1)||{};const learning=sample.learning;
        const reaper=sample.reaper;
        if(reaper)$('reaper-summary').textContent=`Reaper · uccisioni cumulative ${reaper.kills||0} · esposizione ${reaper.exposure_ticks_alive||0} tick · tentativi di fuga ${reaper.escape_attempts_alive||0} · fughe riuscite ${reaper.escape_successes_alive||0}. Le ultime tre somme riguardano i Pop vivi e calano quando un Pop muore.`;
        if(!learning)return;
        $('contributions').textContent=`Campione tick ${sample.tick??'—'} · reward ${(learning.reward||0).toFixed(5)} · segnale ${(learning.signal||0).toFixed(5)} · |segnale| medio ${(learning.mean_abs_signal||0).toFixed(5)}`;
        const c=$('contribution-plot'),ctx=c.getContext('2d');ctx.clearRect(0,0,c.width,c.height);ctx.font='12px system-ui';
        design.learning.forEach((f,i)=>{const value=learning[f.key+'_component']||0;ctx.fillStyle=f.color;ctx.fillText(f.label,5,22+i*26);ctx.fillRect(300,10+i*26,Math.abs(value)*200,13);ctx.fillText(value.toFixed(5),430,22+i*26);});
    }
    async function inspect() {
        if(inspecting||!$('agent').value)return;inspecting=true;
        try {
            const r=await api('/api/commands',{command:'inspect',arguments:{pop_id:$('agent').value},run_id:status.run_id});if(r.ok===false)throw Error(r.error);snapshot=r.result;
            const a=snapshot.brain.activations;
            if(lastAgent!==snapshot.pop_id||averages.length!==a.length||a.some((l,i)=>l.length!==averages[i]?.length)){averages=a.map(l=>[...l]);peaks=a.map(l=>l.map(Math.abs));lastAgent=snapshot.pop_id;lastTick=-1;selectedEdge=null;}
            else if(lastTick!==snapshot.tick)a.forEach((layer,l)=>layer.forEach((v,n)=>{averages[l][n]=.8*averages[l][n]+.2*v;peaks[l][n]=Math.max(peaks[l][n],Math.abs(v));}));
            lastTick=snapshot.tick;drawBrain();
        } catch(e){$('live').checked=false;snapshot=null;selectedEdge=null;drawBrain();throw e;}finally{inspecting=false;}
    }
    function drawBrain() {
        const c=$('brain'),ctx=c.getContext('2d');graphEdges=[];graphNodes=[];
        ctx.setTransform?.(1,0,0,1,0,0);ctx.clearRect(0,0,c.width,c.height);
        if(!snapshot){$('brain-stamp').textContent='Seleziona e campiona un agente vivo.';return;}
        const b=snapshot.brain,layers=b.num_hidden_layers+2,sizes=Array(layers).fill(b.sizeN);sizes[0]=b.sizeS;sizes[layers-1]=b.sizeY;
        const width=1500,height=Math.max(700,45+21*sizes[0]),dpr=Math.min(3,window.devicePixelRatio||1);
        c.style.width=`${width}px`;c.style.height=`${height}px`;
        if(c.width!==Math.round(width*dpr)||c.height!==Math.round(height*dpr)){c.width=Math.round(width*dpr);c.height=Math.round(height*dpr);}
        ctx.setTransform?.(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,width,height);
        const threshold=Number($('threshold').value);if(!Number.isFinite(threshold)||threshold<0)return;
        const theme=design.themes[status.appearance?.theme||'lagoon'];
        const labels=window.sensorLabelsForSnapshot(snapshot);
        const labelFor=(l,n)=>l===0?labels[n]||`Ingresso ${n}`:l===layers-1?window.ACTION_LABELS?.[n]||`Uscita ${n}`:`Interno ${l}:${n}`;
        const point=(l,n)=>({x:190+l*(width-380)/(layers-1),y:27+(n+.5)*(height-44)/sizes[l]});
        ctx.font='bold 14px system-ui';ctx.fillStyle=theme.text;ctx.fillText('SENSORI',12,18);ctx.fillText('AZIONI',width-178,18);
        ctx.strokeStyle=theme.line;ctx.lineWidth=.5;for(let n=0;n<sizes[0];n+=2){const y=point(0,n).y+10;ctx.beginPath();ctx.moveTo(8,y);ctx.lineTo(width-8,y);ctx.stroke();}
        for(const block of b.connections)for(const edge of block.entries){if(Math.abs(edge.value)<threshold||edge.value===0)continue;if(graphEdges.length>=3000)continue;
            const a=point(block.from_layer,edge.col),z=point(block.to_layer,edge.row);ctx.beginPath();ctx.moveTo(a.x,a.y);ctx.lineTo(z.x,z.y);ctx.strokeStyle=edge.value>0?design.signals.positive:design.signals.negative;ctx.globalAlpha=.45;ctx.lineWidth=.5+2*Math.min(4,Math.abs(edge.value));ctx.stroke();graphEdges.push({...edge,from_layer:block.from_layer,to_layer:block.to_layer,a,z});}
        const activity=$('aggregate').checked?averages:b.activations;
        for(let l=0;l<layers;l++)for(let n=0;n<sizes[l];n++){const p=point(l,n),v=activity[l]?.[n]||0;ctx.fillStyle=v>=0?design.signals.positive:design.signals.negative;ctx.globalAlpha=.2+.8*Math.min(1,Math.abs(v));ctx.beginPath();ctx.arc(p.x,p.y,5,0,Math.PI*2);ctx.fill();ctx.globalAlpha=1;graphNodes.push({...p,layer:l,index:n,value:v,peak:peaks[l]?.[n]||0});ctx.fillStyle=theme.text;ctx.font='13px system-ui';if(l===0)ctx.fillText(labelFor(l,n),12,p.y+4);else if(l===layers-1)ctx.fillText(labelFor(l,n),p.x+12,p.y+4);else if(status.appearance?.labels){ctx.fillStyle=theme.muted;ctx.fillText(String(n),p.x+8,p.y+4);}}
        ctx.globalAlpha=1;const action=snapshot.last_executed_action;$('brain-stamp').textContent=`${snapshot.pop_id} · tick ${snapshot.tick} · ultima azione eseguita: ${Number.isInteger(action)&&action>=0?window.ACTION_LABELS?.[action]||`Uscita ${action}`:'nessuna'} · ${graphEdges.length} archi visibili (limite 3000) · aggiornamento ${$('live').checked?'1 Hz':'manuale'}`;
    }
    function pointer(e){const r=$('brain').getBoundingClientRect();return{x:(e.clientX-r.left)*1500/r.width,y:(e.clientY-r.top)*parseFloat($('brain').style.height)/r.height};}
    $('brain').onmousemove=e=>{const m=pointer(e),n=graphNodes.find(n=>Math.hypot(n.x-m.x,n.y-m.y)<7);if(n){const label=n.layer===0?window.sensorLabelsForSnapshot(snapshot)[n.index]:n.layer===snapshot.brain.num_hidden_layers+1?window.ACTION_LABELS?.[n.index]:`Hidden ${n.layer}:${n.index}`;$('brain-detail').textContent=`${label||`${n.layer}:${n.index}`} · attivazione ${n.value.toFixed(6)} · picco ${n.peak.toFixed(6)}`;}};
    $('brain').onclick=e=>{const m=pointer(e);let best=4,hit=null;for(const edge of graphEdges){const{a,z}=edge,dx=z.x-a.x,dy=z.y-a.y,t=Math.max(0,Math.min(1,((m.x-a.x)*dx+(m.y-a.y)*dy)/(dx*dx+dy*dy))),d=Math.hypot(m.x-a.x-t*dx,m.y-a.y-t*dy);if(d<best){hit=edge;best=d;}}if(hit){selectedEdge=hit;$('edge-weight').value=hit.value;const last=snapshot.brain.num_hidden_layers+1,labels=window.sensorLabelsForSnapshot(snapshot),name=(l,n)=>l===0?labels[n]||`Ingresso ${n}`:l===last?window.ACTION_LABELS?.[n]||`Uscita ${n}`:`Interno ${l}:${n}`;$('brain-detail').textContent=`${name(hit.from_layer,hit.col)} → ${name(hit.to_layer,hit.row)} · peso ${hit.value}`;}};
    $('edge-apply').onclick=safe(async()=>{if(!selectedEdge||!snapshot)throw Error('Seleziona un arco.');if(!$('edge-weight').reportValidity())return;const{from_layer,to_layer,col,row}=selectedEdge;await command('sandbox_weight',{pop_id:snapshot.pop_id,from_layer,to_layer,col,row,value:Number($('edge-weight').value)});await inspect();});
    $('play').onclick=safe(()=>command(status.state==='paused'?'resume':'pause'));
    document.querySelectorAll('[data-speed]').forEach(b=>b.onclick=safe(()=>command('speed',{value:Number(b.dataset.speed)})));
    document.querySelectorAll('[data-ticks]').forEach(b=>b.onclick=safe(()=>command('step',{ticks:Number(b.dataset.ticks)})));
    $('step').onclick=safe(()=>{if($('step-count').reportValidity())return command('step',{ticks:Number($('step-count').value)});});
    $('show-gui').onclick=safe(()=>command('show_gui'));
    $('reset-mixer').onclick=safe(()=>command('runtime_apply',Object.fromEntries(schema.filter(s=>s.name.startsWith('reward_')).map(s=>[s.name,s.default]))));
    $('energy-preset').onclick=safe(()=>command('runtime_apply',{reward_age_weight:0,reward_energy_weight:.3,reward_delta_weight:.35,reward_offspring_weight:0,reward_temperature_weight:.2}));
    for(const name of Object.keys(design.themes))$('theme').append(node('option',name,{value:name}));
    for(const[id,key]of[['theme','theme'],['agent-scale','agent_scale'],['ui-scale','ui_scale'],['grid','grid'],['labels','labels']])$(id).onchange=safe(()=>command('appearance',{[key]:$(id).type==='checkbox'?$(id).checked:$(id).type==='range'?Number($(id).value):$(id).value}));
    const coordinates=()=>({x:Number($('cell-x').value),y:Number($('cell-y').value)});
    const cellAction=erase=>{
        const field=$('cell-field').value,position=coordinates();
        if(!Number.isInteger(position.x)||!Number.isInteger(position.y)||position.x<0||position.y<0)throw Error('Coordinate intere non negative richieste.');
        if(field==='pop'||field==='source')return command(erase?'sandbox_remove':'sandbox_spawn',{...position,kind:field});
        const amount=Number($('cell-amount').value);
        if(!erase&&(!$('cell-amount').reportValidity()||!Number.isFinite(amount)))return;
        return command('sandbox_cell',{...position,field,value:amount,erase});
    };
    $('cell-add').onclick=safe(()=>cellAction(false));
    $('cell-remove').onclick=safe(()=>cellAction(true));
    $('cell-field').onchange=()=>{
        const field=$('cell-field').value,agent=field==='pop'||field==='source',temperature=field==='temperature';
        $('cell-amount-label').hidden=agent;
        $('cell-amount-label').firstChild.textContent=temperature?'Temperatura C ':'Quantità ';
        $('cell-amount').min=temperature?'-50':'0';
        $('cell-amount').max=temperature?'100':field==='nitrogen'?'2000':field==='water'||field==='oxygen'||field==='co2'?'1000':field==='lipids'?'300':field==='food'||field==='danger'||field==='mate'||field==='home'?'100':'500';
        $('cell-amount').step=temperature?'any':'1';
        $('cell-remove').textContent=temperature?'Annulla ultima modifica':'Elimina';
    };
    $('agents-load').onclick=safe(async()=>{const result=await command('agents');$('agent').replaceChildren();for(const p of result.agents)$('agent').append(node('option',`${p.pop_id} · età ${p.age}`,{value:p.pop_id}));});
    $('inspect').onclick=safe(inspect);$('aggregate').onchange=drawBrain;$('threshold').onchange=drawBrain;
    $('agent').onchange=()=>{$('live').checked=false;snapshot=null;selectedEdge=null;drawBrain();};
    async function init(){await refresh();schema=await command('runtime_schema');renderRuntime();await contributions();}
    safe(init)();setInterval(safe(async()=>{if(document.hidden)return;await refresh();if($('live').checked)await inspect();}),1000);
    setInterval(safe(async()=>{if(!document.hidden)await contributions();}),5000);
})();
