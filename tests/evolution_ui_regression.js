'use strict';
// Exercise page wiring, charts, filtering and selection with real backend-shaped data.
const assert=require('assert'),fs=require('fs'),vm=require('vm');
const realCytoscape=require('../dashboard/static/vendor/cytoscape.min.js');
class Element {
    constructor(){this.value='';this.children=[];this.style={};this.hidden=false;}
    append(...xs){this.children.push(...xs);} prepend(...xs){this.children.unshift(...xs);}
    replaceChildren(...xs){this.children=xs;}
    setAttribute(name,value){this[name]=value;}
}
const elements=new Map();
const get=id=>{if(!elements.has(id))elements.set(id,new Element());return elements.get(id);};
get('ev-threshold').value='.9';get('ev-filter').value='all';get('ev-sort').value='alive';
const dist={n:2,mean:1,std:1,median:1,p90:2,maximum:2};
const overview={alive:2,total:4,completed:2,children_completed:dist,children_alive:dist,age_completed:dist,
    natural_death_age:dist,culled_age:dist,max_depth:3,births_per_1000_agent_ticks:1};
const strain={...overview,id:'pop_1',root:'pop_1',representative:'pop_1',fingerprint:'abc',color:[106,130,89],born:0,last_tick:200};
const report={state:'ready',run:'r',cutoff:200,integrity:{consistent:true,births_events:4,deaths_events:2},
    source_model:'test',overview,strains:[strain,{...strain,id:'pop_2',alive:0}],roots:[strain],cap_fraction:.5,
    timeline:[{tick:100,counts:{pop_1:1,other:0},turnover:.2,effective_strains:1},
              {tick:200,counts:{pop_1:2,other:0},turnover:.5,effective_strains:1}],
    root_timeline:[],cap_timeline:[{tick:200,blocked_fraction:.5}],
    survival:[{age:0,survival:1,risk:4,deaths:0,censored:0}],age_histogram:[{start:0,end:100,deaths:2,culls:0,censored:2}]};
const detail={strain:'pop_1',summary:overview,survival:report.survival,age_histogram:report.age_histogram,
    individuals:[{id:'pop_1',parent:'',born:0,age:200,children:1,depth:0,censored:true}],total:1,
    genealogy:[strain,{...strain,id:'pop_2',parent:'pop_1',alive:0}]};
const charts=new Map(),requests=[];
const trees=[];let hold=false,release;
const context={document:{getElementById:get,createElement:()=>new Element()},location:{hostname:'127.0.0.1'},setTimeout,
    window:{},Chart:class {constructor(el,config){this.data=config.data;charts.set(el,config);}destroy(){}update(){}},
    cytoscape:config=>{const tree=realCytoscape({headless:true,elements:JSON.parse(JSON.stringify(config.elements)),layout:config.layout});trees.push(tree);return tree;},
    fetch:async url=>{requests.push(url);const data=url.includes('/extinctions')?{ticks:[100]}:
        url.startsWith('/api/runs')?[{id:'r',created:0,state:'running'}]:
        url==='/api/status'?{run_id:'r'}:url.includes('/strain/')?{...detail,strain:url.split('/strain/')[1].split('?')[0]}:report;
        if(hold&&url.includes('/strain/'))await new Promise(r=>release=r);
        return {ok:true,json:async()=>data};}};
vm.createContext(context);
vm.runInContext(fs.readFileSync('dashboard/static/chart_math.js','utf8'),context);
vm.runInContext(fs.readFileSync('dashboard/static/evolution.js','utf8'),context);
(async()=>{
    for(let i=0;i<5;i++)await new Promise(r=>setImmediate(r));
    assert.strictEqual(get('ev-error').hidden,true);
    assert.strictEqual(charts.size,6);
    assert.strictEqual(charts.get(get('ev-composition')).plugins[0].id,'popxExtinctions');
    get('ev-derivative').onclick();
    assert.strictEqual(charts.get(get('ev-composition')).data.datasets[0].data[1].y,.01);
    assert.strictEqual(charts.get(get('ev-composition')).data.datasets[0].data[0].y,null);
    assert.strictEqual(get('ev-strain-rows').children.length,2);
    get('ev-filter').value='alive';get('ev-filter').oninput();
    assert.strictEqual(get('ev-strain-rows').children.length,1);
    await get('ev-strain-rows').children[0].children[0].children[0].onclick();
    assert.strictEqual(get('ev-detail-title').textContent,'Ceppo pop_1');
    assert.strictEqual(get('ev-individuals').children.length,1);
    assert.strictEqual(charts.get(get('ev-survival')).data.datasets.length,2);
    const tree=trees.at(-1);assert.strictEqual(tree.edges().length,1);
    assert.strictEqual(tree.edges()[0].source().id(),'pop_1');assert.strictEqual(tree.edges()[0].target().id(),'pop_2');
    assert(tree.nodes().every(n=>Number.isFinite(n.position('x'))&&Number.isFinite(n.position('y'))));
    tree.getElementById('pop_2').emit('tap');
    for(let i=0;i<4;i++)await new Promise(r=>setImmediate(r));
    assert.strictEqual(get('ev-detail-title').textContent,'Ceppo pop_2');
    hold=true;trees.at(-1).getElementById('pop_1').emit('tap');await new Promise(r=>setImmediate(r));
    get('ev-clear').onclick();
    release();for(let i=0;i<4;i++)await new Promise(r=>setImmediate(r));
    assert.strictEqual(get('ev-detail-title').textContent,'Seleziona un ceppo');
    assert.strictEqual(get('ev-individuals').children.length,0);
    assert.strictEqual(charts.get(get('ev-survival')).data.datasets.length,1);
    assert(requests.every(url=>url.startsWith('/api/')));
    console.log('PASS: lineage page loads six charts, filters strains, opens detail, overlays survival and clears selection');
})().catch(e=>{console.error(e);process.exitCode=1;});
