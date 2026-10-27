'use strict';
const assert=require('assert'),fs=require('fs'),vm=require('vm');
class Element {
    constructor(){this.children=[];this.value='';this.dataset={};this.style={};this.classList={toggle(){}};}
    append(...xs){this.children.push(...xs);}replaceChildren(...xs){this.children=xs;this.textContent='';}
    setAttribute(){} reportValidity(){return true;}
}
const nodes=new Map(),get=id=>{if(!nodes.has(id))nodes.set(id,new Element());return nodes.get(id);};
const exit=get('exit');exit.dataset.command='exit';
const calls=[],cleared=[],events=[];let stream,lateResolve,hold=false,heldSignal,confirmed=false;
const status={run_id:'r',state:'running',connected:true,speed:0,alive:10,tick:100};
const context={console,AbortController,DOMException,Event,clearInterval:id=>cleared.push(id),setInterval:()=>7,
    window:{POPX_METRICS:{},confirm:()=>confirmed},document:{getElementById:get,createElement:()=>new Element(),createTextNode:text=>({text}),
        addEventListener(){},dispatchEvent:e=>events.push(e.type),querySelector:()=>new Element(),
        querySelectorAll:selector=>selector.includes('data-command')?[exit]:[]},
    Chart:class{constructor(el,config){Object.assign(this,config);}update(){}},
    EventSource:class{constructor(){stream=this;}close(){this.closed=true;}},
    fetch:async(url,options)=>{calls.push(url);let data=url==='/api/status'?status:url.startsWith('/api/runs?')?[{id:'r',created:0,metadata:{}}]:[];
        if(url==='/api/commands'){assert.strictEqual(JSON.parse(options.body).command,'exit');data={status};}
        if(hold&&url.includes('/samples')){heldSignal=options.signal;await new Promise(r=>lateResolve=r);data=[{tick:999,alive:99}];}
        return {ok:true,json:async()=>data};}};
vm.createContext(context);
vm.runInContext(fs.readFileSync('dashboard/static/chart_math.js','utf8'),context);
vm.runInContext(fs.readFileSync('dashboard/static/app.js','utf8'),context);
(async()=>{
    for(let i=0;i<10;i++)await new Promise(r=>setImmediate(r));
    const state=vm.runInContext('state',context);assert(stream);
    state.samples=[{tick:0,alive:1},{tick:200,alive:3}];
    vm.runInContext('drawCharts()',context);
    const derivativeButton=get('charts').children[0].children[0].children[1];
    derivativeButton.onclick();
    assert.strictEqual(state.charts[0].data.datasets[0].data[1].y,.01);
    assert.strictEqual(state.charts[0].plugins[0].id,'popxExtinctions');
    get('extinction-toggle').onclick();
    assert.strictEqual(state.showExtinctions,false);
    assert.strictEqual(get('extinction-toggle').textContent,'Mostra linee rosse');
    state.samples=[{tick:100}];state.agents=[{pop_id:'p'}];state.snapshot={pop_id:'p'};
    let destroyed=false;state.cy={destroy(){destroyed=true;}};
    for(const chart of state.charts)chart.data.datasets[0].data=[{x:1,y:2}];
    hold=true;const late=vm.runInContext('loadHistory()',context).catch(e=>e.name);
    await new Promise(r=>setImmediate(r));
    const before=calls.filter(c=>c.startsWith('/api/runs?')).length;
    await exit.onclick();assert.strictEqual(state.closed,false);
    assert.strictEqual(calls.filter(c=>c==='/api/commands').length,0);
    confirmed=true;
    await exit.onclick();
    assert(state.closed&&stream.closed&&destroyed&&heldSignal.aborted);
    assert(cleared.includes(7));assert(events.includes('popx:exit'));
    assert.strictEqual(state.run,null);assert.strictEqual(state.snapshot,null);assert.strictEqual(state.agents.length,0);
    assert(state.charts.every(c=>c.data.datasets.every(d=>d.data.length===0)));
    assert.strictEqual(calls.filter(c=>c.startsWith('/api/runs?')).length,before);
    lateResolve();assert.strictEqual(await late,'AbortError');assert.strictEqual(state.samples.length,0);
    stream.onmessage({data:JSON.stringify({type:'sample',run_id:'r',data:{tick:1000}})});
    assert.strictEqual(state.samples.length,0);assert(get('notice').textContent.includes('azzerata'));
    console.log('PASS: Exit clears charts/inspector, closes SSE/polling, aborts requests and ignores late data');
})().catch(e=>{console.error(e);process.exitCode=1;});
