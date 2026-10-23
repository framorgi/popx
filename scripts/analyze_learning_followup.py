#!/usr/bin/env python3
"""Read-only learning audit: bounded historical windows and live inspector observations.

No pause, restart, config edit or statistical JSON export. Live inspections span
multiple ticks; agents dying before inspection are excluded and reported in n.
"""
import json,urllib.request,sqlite3,statistics as st,math,time
base='http://127.0.0.1:8000'
def api(path,data=None):
 req=urllib.request.Request(base+path,data=json.dumps(data).encode() if data else None,headers={'Content-Type':'application/json'})
 return json.load(urllib.request.urlopen(req,timeout=30))
s=api('/api/status');run=s['run_id'];c=sqlite3.connect('file:data_out/popx.sqlite?mode=ro',uri=True)
end=c.execute('select max(tick) from samples where run=?',(run,)).fetchone()[0]
print('run',run,'cutoff',end,'status',s['state'])
keys=['signal','mean_abs_signal','signal_std','mean_abs_plasticity','mean_abs_decay','mean_abs_trace','mean_abs_weight','zero_fraction']
for e in [20000,1000000,5000000,10000000,20000000,end]:
 rows=c.execute('select tick,data from samples where run=? and tick<=? order by tick desc limit 100',(run,e)).fetchall();ds=[json.loads(r[1]) for r in rows]
 avg={k:st.fmean(d['learning'].get(k,0) for d in ds) for k in keys}
 print('window',rows[-1][0],rows[0][0], 'learning',avg,'ratio',avg['mean_abs_plasticity']/avg['mean_abs_decay'] if avg['mean_abs_decay'] else None)
 print('resources',ds[0]['resource_ledger']['final'],'alive',st.fmean(d['alive'] for d in ds))
for span in [1000000,5000000]:
 rows=c.execute('select tick,data from samples where run=? and tick between ? and ? and tick%10000=0',(run,end-span,end)).fetchall()
 ds=[json.loads(r[1]) for r in rows];print('long_window',span,'n',len(ds),'learning',{k:st.fmean(d['learning'].get(k,0) for d in ds) for k in keys},'mean_alive',st.fmean(d['alive'] for d in ds))
 a=json.loads(c.execute('select data from samples where run=? and tick<=? order by tick desc limit 1',(run,end-span)).fetchone()[0]);b=json.loads(c.execute('select data from samples where run=? and tick=?',(run,end)).fetchone()[0])
 for key in ['counts','reproduction']: print('delta',span,key,{k:b[key][k]-a[key][k] for k in a[key]})
 print('delta_move',span,{k:b['totals'][k]-a['totals'][k] for k in ['movement_attempts','movement_successes','distance']})
 print('actions',span,[int(y-x) for x,y in zip(a['totals']['actions'],b['totals']['actions'])])
 print('resources_change',span,[y-x for x,y in zip(a['resource_ledger']['final'],b['resource_ledger']['final'])])
c.close()
r=api('/api/commands',{'command':'agents','run_id':run})['result'];agents=r['agents'];print('live_tick',r['tick'],'n',len(agents))
for lo,hi in [(0,10000),(10000,50000),(50000,100000),(100000,200001)]:
 aa=[a for a in agents if lo<=a['age']<hi]
 if aa:print('ageband',lo,hi,'n',len(aa),{k:st.fmean(a['learning'][k] for a in aa) for k in ['mean_abs_weight','mean_abs_trace','mean_abs_plasticity','mean_abs_decay']})
ins=[];ticks=[]
for a in agents:
 try:
  d=api('/api/commands',{'command':'inspect','run_id':run,'arguments':{'pop_id':a['pop_id']}})['result'];ins.append(d);ticks.append(d['tick'])
 except Exception:pass
print('inspect_n',len(ins),'ticks',min(ticks),max(ticks))
ww=[];hh=[];oo=[];connected_h=[];connected_o=[];by_layer={};weight_bands={};gap=[]
for a in ins:
 last=a['brain']['num_hidden_layers']+1
 targets=set()
 for m in a['brain']['connections']:
  key=str(m['from_layer'])+'->'+str(m['to_layer']);vals=[abs(e['value']) for e in m['entries']];ww+=vals;by_layer.setdefault(key,[]).extend(vals)
  for e in m['entries']:targets.add((m['to_layer'],e['row']))
 av=a['brain']['activations']
 for l in range(1,len(av)):
  for n,v in enumerate(av[l]):
   (hh if l<last else oo).append(abs(v-.5))
   if (l,n) in targets:(connected_h if l<last else connected_o).append(abs(v-.5))
 if av:
  out=sorted(av[-1],reverse=True);gap.append(out[0]-out[1])
 band='young' if a['age']<10000 else 'adult'
 weight_bands.setdefault(band,[]).extend(abs(e['value']) for m in a['brain']['connections'] for e in m['entries'])
print('weights_n',len(ww),'floor_fraction',sum(v<=1.01/8192 for v in ww)/len(ww),'zeros',sum(v==0 for v in ww)/len(ww),'median',st.median(ww))
for k,v in weight_bands.items():print('weights_age',k,'n',len(v),'floor',sum(x<=1.01/8192 for x in v)/len(v),'mean',st.fmean(v))
for k,v in [('hidden_all',hh),('output_all',oo),('hidden_with_input',connected_h),('output_with_input',connected_o)]: print(k,'n',len(v),'mean_abs_deviation',st.fmean(v),'near_half',sum(x<.001 for x in v)/len(v))
print('output_top_margin','median',st.median(gap),'near_zero',sum(x<1e-6 for x in gap)/len(gap),'exact_ties',sum(x==0 for x in gap)/len(gap),'idle_last',sum(a['last_action']==21 for a in ins))
for k,v in sorted(by_layer.items()):print('layer',k,len(v),'mean',st.fmean(v),'floor',sum(x<=1.01/8192 for x in v)/len(v))
