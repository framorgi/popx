'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const design=JSON.parse(fs.readFileSync('dashboard/static/design.json','utf8'));
const strokes=[];
const ctx={clearRect(){},fillText(){},fillRect(){},beginPath(){},moveTo(){},lineTo(){},stroke(){strokes.push(this.strokeStyle);},arc(){},fill(){}};
class Element{
 constructor(){this.children=[];this.style={setProperty(){}};this.type='';this.value='';this.width=1100;this.height=560;this.checked=false;}
 append(...nodes){this.children.push(...nodes);if(!this.value&&nodes[0]?.value)this.value=nodes[0].value;}
 replaceChildren(...nodes){this.children=nodes;this.value='';}setAttribute(){}reportValidity(){return true;}getContext(){return ctx;}
 getBoundingClientRect(){return{left:0,top:0,width:1100,height:560};}
}
const elements=new Map(),get=id=>{if(!elements.has(id))elements.set(id,new Element());return elements.get(id);};
get('threshold').value='.01';get('step-count').value='100';
const runtime={learning_rate:.06,...Object.fromEntries(design.learning.map((f,i)=>[`reward_${f.key}_weight`,[.1,.3,.35,.05,.2][i]]))};
const schema=Object.keys(runtime).map(name=>({name,type:'number',min:0,max:1,default:runtime[name],description:name}));
let status={run_id:'r',connected:true,state:'paused',tick:20,alive:2,runtime,appearance:design.defaults};
let snapshot={pop_id:'p',tick:20,sensor_model:'chemical_density_v2',brain:{sizeS:1,sizeN:1,sizeY:1,num_hidden_layers:1,activations:[],connections:[{from_layer:0,to_layer:2,entries:[{col:0,row:0,value:.4}]}]}};
const requests=[],timers=[];
const context={console,Math,Error,Number,JSON,Object,Array,setInterval:fn=>timers.push(fn),
 window:{POPX_DESIGN:design,applyPopXAppearance(){}},document:{hidden:false,getElementById:get,createElement:()=>new Element(),activeElement:null,querySelector:()=>null,querySelectorAll:()=>[]},
 fetch:async(path,options)=>{let data=status;
  if(path==='/api/commands'){const req=JSON.parse(options.body);requests.push(req);let result;
   if(req.command==='runtime_schema')result=schema;
   if(req.command==='runtime_apply')Object.assign(runtime,req.arguments);
   if(req.command==='inspect')result=snapshot;
   if(req.command==='agents')result={agents:[{pop_id:'p',age:20}]};
   data={ok:true,status,result};
  }else if(path.includes('/samples'))data=[{tick:20,learning:{reward:.2,signal:0,age_component:.1}}];
  return{ok:true,json:async()=>data};}};
vm.createContext(context);vm.runInContext(fs.readFileSync('dashboard/static/labels.js','utf8'),context);vm.runInContext(fs.readFileSync('dashboard/static/sandbox.js','utf8'),context);
(async()=>{
 for(let i=0;i<20;i++)await new Promise(r=>setImmediate(r));
 assert.equal(get('mixer').children.length,5);
 assert.equal(get('mixer').children[0].children[0].textContent,design.learning[0].label);
 const slider=get('mixer').children[1].children[1];slider.value='.7';await slider.onchange();
 assert.equal(requests.at(-1).arguments.reward_energy_weight,.7);
 await get('reset-mixer').onclick();assert.equal(runtime.reward_energy_weight,.3);
 await get('agents-load').onclick();get('agent').value='p';await get('inspect').onclick();
 assert.match(get('brain-stamp').textContent,/p.*tick 20/);assert(strokes.includes(design.themes.lagoon.accent));
 // A newborn can be inspected before its first forward pass: the first real activation must initialize aggregation.
 snapshot={...snapshot,tick:21,brain:{...snapshot.brain,activations:[[1],[.5],[.6]]}};
 await get('inspect').onclick();assert(!get('message').textContent?.includes('undefined'));
 get('aggregate').checked=true;get('aggregate').onchange();
 await get('step').onclick();assert.equal(requests.at(-1).command,'step');assert.equal(requests.at(-1).arguments.ticks,100);
 status={...status,connected:false};await timers[0]();assert.match(get('brain-stamp').textContent,/Seleziona/);
 console.log('PASS: shared mixer order, runtime apply/reset, exact-step wiring, newborn activity aggregation and disconnect cleanup');
})().catch(e=>{console.error(e);process.exitCode=1;});
