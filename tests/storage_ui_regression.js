'use strict';
const assert=require('assert'),fs=require('fs'),vm=require('vm');
class Element {
    constructor(tag='div'){this.tag=tag;this.value='';this.children=[];this.classList={add(){}};}
    append(...xs){this.children.push(...xs);}
    replaceChildren(...xs){this.children=xs;if(this.tag==='select')this.value=xs[0]?.value||'';}
    setAttribute(){}
}
const nodes=new Map(),get=id=>{if(!nodes.has(id))nodes.set(id,new Element(id==='storage-run'?'select':'div'));return nodes.get(id);};
const runs=[{id:'live',state:'running',created:1},{id:'old',state:'finished',created:0}];
let confirmed=false,deletes=0;
const report={disk_total:1000,disk_available:700,disk_unavailable:300,database_files_bytes:100,
    database_reusable_bytes:20,measured_at:1,run_count:2,removals:[]};
const context={document:{getElementById:get,createElement:tag=>new Element(tag),querySelector:()=>get('aside'),addEventListener(){}},
    window:{confirm:()=>confirmed},setInterval(){},fetch:async(url,options)=>{
        let data=url==='/api/storage'?report:url==='/api/status'?{run_id:'live'}:runs;
        if(options?.method==='DELETE'){
            assert.strictEqual(url,'/api/runs/old');assert.strictEqual(JSON.parse(options.body).confirm_run,'old');
            deletes++;runs.pop();report.removals=[{run:'old',state:'deleting',rows_deleted:0,files_deleted:0,files_retained:0}];data={state:'deleting'};}
        return {ok:true,json:async()=>data};}};
vm.runInNewContext(fs.readFileSync('dashboard/static/storage.js','utf8'),context);
(async()=>{
    for(let i=0;i<8;i++)await new Promise(r=>setImmediate(r));
    assert.strictEqual(get('storage-meter').children[0].value,300);
    assert.strictEqual(get('storage-delete').disabled,true);
    get('storage-run').value='old';get('storage-run').onchange();
    assert.strictEqual(get('storage-delete').disabled,false);
    await get('storage-delete').onclick();assert.strictEqual(deletes,0);
    confirmed=true;await get('storage-delete').onclick();assert.strictEqual(deletes,1);
    assert.strictEqual(get('storage-delete').disabled,true);
    assert(get('storage-jobs').children[0].textContent.includes('rimozione in corso'));
    console.log('PASS: disk meter, active-run protection, cancelled confirmation, selected-run deletion and progress');
})().catch(e=>{console.error(e);process.exitCode=1;});
