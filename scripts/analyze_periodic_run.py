"""Bounded read-only audit of environmental periods, inactivity and completed lives; text output."""
import json, sqlite3, urllib.request, statistics, math, collections, argparse
def corr(a,b):
 ma,mb=statistics.fmean(a),statistics.fmean(b)
 denom=math.sqrt(sum((v-ma)**2 for v in a)*sum((v-mb)**2 for v in b))
 return sum((x-ma)*(y-mb) for x,y in zip(a,b))/denom if denom else 0

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--run',help='Historical run: skips live API observations')
parser.add_argument('--cutoff',type=int)
parser.add_argument('--db',default='data_out/popx.sqlite')
args=parser.parse_args()
run=args.run or json.load(urllib.request.urlopen('http://127.0.0.1:8000/api/status'))['run_id']
c=sqlite3.connect('file:'+args.db+'?mode=ro',uri=True)
end=args.cutoff or c.execute('select max(tick) from samples where run=?',(run,)).fetchone()[0]
rows=c.execute('select tick,data from samples where run=? and tick between ? and ? and tick%2000=0 order by tick',(run,end-5000000,end)).fetchall()
ds=[json.loads(d) for _,d in rows];series={}
for group,key in [('learning','reward'),('learning','temperature_component'),('averages','abs_temp_delta'),('averages','body_temperature'),('learning','signal')]:series[key]=[d[group][key] for d in ds]
print('run',run,'cutoff',end,'n',len(ds),'step',2000,'expected_thermal_period',2*math.pi/.009*1000)
for key,x in series.items():
 centered=[v-statistics.fmean(x) for v in x]
 periods=range(50000,1000001,10000)
 power=[]
 for period in periods:
  angle=2*math.pi*2000/period
  re=sum(v*math.cos(i*angle) for i,v in enumerate(centered));im=sum(v*math.sin(i*angle) for i,v in enumerate(centered))
  power.append((re*re+im*im,period))
 print(key,'range',min(x),max(x),'periodogram_top',sorted(power,reverse=True)[:3], 'acf_698k',corr(x[:-349],x[349:]),'acf_50k',corr(x[:-25],x[25:]))
print('reward_thermal_corr',corr(series['reward'],series['temperature_component']))
# Direct probes across successive NewGen boundaries avoid phase-biased subsampling.
for boundary in range(end//50000*50000-100000,end//50000*50000+1,50000):
 near=c.execute('select tick,data from samples where run=? and tick between ? and ? order by tick',(run,boundary-200,boundary+400)).fetchall()
 print('newgen_boundary',[(t,json.loads(d)['alive'],json.loads(d)['counts']['culled']) for t,d in near])
deaths=[]
for typ,raw in c.execute('select type,data from events where run=? and tick>? and tick<=? and type in ("death","cull")',(run,end-1000000,end)):
 d=json.loads(raw);d['event_type']=typ;deaths.append(d)
print('completed',len(deaths),'age_median',statistics.median(d['age'] for d in deaths),'age_mean',statistics.fmean(d['age'] for d in deaths),'zero_lifetime_distance',sum(d['totals']['distance']==0 for d in deaths)/len(deaths),'zero_children',sum(d['totals']['offspring']==0 for d in deaths)/len(deaths),'offspring_hist',dict(collections.Counter(d['totals']['offspring'] for d in deaths)))
# Snapshot living cells and lifetime action counts, no neural-state export.
if not args.run:
 req=urllib.request.Request('http://127.0.0.1:8000/api/commands',data=json.dumps({'command':'agents','run_id':run}).encode(),headers={'Content-Type':'application/json'})
 a=json.load(urllib.request.urlopen(req))['result']['agents'];positions={tuple(p['position']) for p in a}
 print('live',len(a),'zero_distance',sum(p['totals']['distance']==0 for p in a),'zero_attempts',sum(p['totals']['movement_attempts']==0 for p in a),'photo_capable',sum(p['traits']['chloroplasts']>=10 for p in a))
 print('occupied_neighbors_mean',statistics.fmean(sum((p['position'][0]+dx,p['position'][1]+dy) in positions for dx in (-1,0,1) for dy in (-1,0,1) if dx or dy) for p in a))
print('saturated_calcium_cells',ds[-1]['calcium_saturated_cells'],'world_resources',ds[-1]['world_resources'])
roots={};alive={};cutoffs=[end-5000000,end-1000000,end];point=0
for tick,typ,pid,parent in c.execute("select tick,type,pop,json_extract(data,'$.parent_id') from events where run=? and tick<=? order by tick,case when type in ('birth','founder') then 0 else 1 end,id",(run,end)):
 while point<len(cutoffs) and tick>cutoffs[point]:
  print('ancestral_founders_at',cutoffs[point],collections.Counter(alive.values()).most_common());point+=1
 if typ in ('birth','founder','forced_birth','checkpoint_entry'):
  root=roots.get(parent,parent or pid);roots[pid]=root;alive[pid]=root
 elif typ in ('death','cull'):alive.pop(pid,None)
print('ancestral_founders_at',end,collections.Counter(alive.values()).most_common())
assert len(alive)==ds[-1]['alive'] or end%2000, 'Genealogy population mismatch'
