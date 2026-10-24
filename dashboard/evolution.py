"""Read-only, incremental lineage analysis of a fixed prefix of the event history.

Can also serve the new dashboard while the original receiver keeps running:
python -m dashboard.evolution --db data_out/popx.sqlite --port 8002
"""
import argparse
import collections
import concurrent.futures
import json
import math
import statistics
import threading
from dataclasses import dataclass
from contextlib import closing
from pathlib import Path
from urllib.parse import quote

from .genetics import canonical, color, fingerprint, similarity, MODEL
from .genome_codec import unpack_event


def distribution(values):
    values = sorted(values)
    if not values:
        return dict(n=0, mean=None, std=None, median=None, p90=None, maximum=None)
    return dict(n=len(values), mean=statistics.fmean(values), std=statistics.pstdev(values),
                median=statistics.median(values), p90=values[min(len(values)-1, int(.9*len(values)))],
                maximum=values[-1])


def survival(nodes, cutoff):
    """All-cause Kaplan–Meier: death precedes right censoring at a tied age."""
    counts = collections.defaultdict(lambda: [0, 0, 0])
    for n in nodes:
        counts[n.entry_age][2] += 1
        age = n.age if n.dead is not None else n.entry_age + max(0, cutoff-n.born)
        counts[age][0 if n.dead is not None else 1] += 1
    risk, probability = 0, 1.0
    result = [dict(age=0, survival=1.0, risk=counts[0][2], deaths=0, censored=0)]
    for age, (deaths, censored, entered) in sorted(counts.items()):
        risk += entered
        if deaths or censored:
            probability *= 1-deaths/risk
            result.append(dict(age=age, survival=probability, risk=risk, deaths=deaths, censored=censored))
        risk -= deaths+censored
    # Bound plot size, retaining endpoints. Statistics use the complete event table.
    step = max(1, math.ceil(len(result)/300))
    return result[::step] + ([result[-1]] if (len(result)-1) % step else [])


def age_histogram(nodes, cutoff):
    ages = [(n.age if n.dead is not None else n.entry_age+max(0,cutoff-n.born),
             'culls' if n.cause=='cull' else 'deaths' if n.dead is not None else 'censored') for n in nodes]
    width=max(1,math.ceil((max((a for a,_ in ages),default=0)+1)/20))
    bins=[dict(start=i*width,end=(i+1)*width,deaths=0,culls=0,censored=0) for i in range(20)]
    for age,kind in ages:
        bins[min(19,age//width)][kind]+=1
    return bins


@dataclass(slots=True)
class Individual:
    pid: str
    parent: str
    root: str
    strain: str
    born: int
    event: int
    depth: int
    forced: bool
    dead: int | None = None
    age: int = 0
    cause: str = ''
    children: int = 0
    entry_age: int = 0
    recovery: bool = False


class LineageIndex:
    def __init__(self, threshold=.9, cancel=None):
        self.threshold = threshold
        self.cancel = cancel
        self.nodes = {}
        self.strains = {}
        self.last_id = 0
        self.cutoff = 0
        self.processed = 0
        self.missing_parents = self.unknown_deaths = self.duplicate_births = 0
        self.sample = {}
        self.metadata = {}
        self.gaps = 0
        self.bins = []
        self.summary = None

    def check_cancel(self):
        if self.cancel is not None and self.cancel.is_set():
            raise concurrent.futures.CancelledError('Lettura genealogica interrotta alla chiusura.')

    def ingest(self, event, tick, kind, pid, data):
        if kind in ('founder', 'birth'):
            if pid in self.nodes:
                self.duplicate_births += 1
                return
            parent_id = data.get('parent_id', '')
            parent = self.nodes.get(parent_id)
            forced = bool(data.get('forced')) and kind == 'birth'
            recovery = forced and bool(data.get('recovery_from_archive'))
            inherited = parent is not None and (not forced or recovery)
            genes = canonical(data['genes']) if isinstance(data.get('genes'), list) else None
            root = parent.root if inherited else pid
            depth = parent.depth+1 if inherited else 0
            strain = parent.strain if inherited else pid
            if parent_id and parent is None and not (data.get('checkpoint_entry') or
                                                    recovery and data.get('recovery_parent_external')):
                self.missing_parents += 1
            reference = self.strains.get(strain)
            closeness = similarity(genes, reference['genes']) if genes is not None and reference and reference['genes'] is not None else None
            if reference is None or closeness is None or closeness < self.threshold:
                strain = pid
                self.strains[strain] = dict(id=strain, parent=parent.strain if inherited else None,
                    root=root, born=tick, genes=genes, representative=pid,
                    fingerprint=fingerprint(genes) if genes is not None else None,
                    color=color(genes) if genes is not None else [128, 128, 128],
                    traits=data.get('traits', {}), origin='recovery' if recovery else 'forced' if forced else 'founder' if kind == 'founder' else 'branch',
                    similarity_at_split=closeness)
            self.nodes[pid] = Individual(pid, parent_id, root, strain, tick, event, depth, forced,
                                         entry_age=int(data.get('entry_age', 0)), recovery=recovery)
            if parent and not forced and kind == 'birth':
                parent.children += 1
        elif kind in ('death', 'cull'):
            node = self.nodes.get(pid)
            if node is None:
                self.unknown_deaths += 1
            elif node.dead is None:
                node.dead = tick
                node.age = int(data.get('age', tick-node.born))
                node.cause = 'cull' if kind == 'cull' else data.get('death_cause', 'unknown')

    @staticmethod
    def open_database(path):
        import sqlite3
        db = sqlite3.connect('file:' + quote(str(Path(path).resolve())) + '?mode=ro', uri=True, timeout=5)
        db.execute('PRAGMA query_only=ON')
        return db

    def update(self, path, run):
        self.check_cancel()
        with closing(self.open_database(path)) as db:
            if self.cancel is not None:
                db.set_progress_handler(lambda: int(self.cancel.is_set()), 10000)
            # One WAL read snapshot across cutoff, event bounds and rows. Otherwise events arriving
            # between the bounds queries could advance last_id past events excluded by the cutoff.
            db.execute('BEGIN')
            latest = db.execute('SELECT tick,data FROM samples WHERE run=? ORDER BY tick DESC LIMIT 1', (run,)).fetchone()
            if latest is None:
                raise ValueError('Questa run non contiene ancora campioni.')
            cutoff, raw = latest
            # Restrict to a contiguous persisted event prefix: future events will be read on the next refresh.
            future = db.execute('SELECT min(id) FROM events WHERE run=? AND tick>?', (run, cutoff)).fetchone()[0]
            high = (future-1) if future is not None else db.execute('SELECT coalesce(max(id),0) FROM events WHERE run=?', (run,)).fetchone()[0]
            late = db.execute('SELECT 1 FROM events WHERE run=? AND id>? AND id<=? AND tick<=? LIMIT 1',
                              (run, self.last_id, high, self.cutoff)).fetchone()
            if late:
                self.__init__(self.threshold, self.cancel)
            query = 'SELECT id,tick,type,pop,data FROM events WHERE run=? AND id>? AND id<=? AND tick<=? ORDER BY tick,id'
            for event, tick, kind, pid, data in db.execute(query, (run, self.last_id, high, cutoff)):
                self.check_cancel()
                self.ingest(event, tick, kind, pid, unpack_event(json.loads(data)))
                self.processed += 1
                if len(self.nodes) > 1_000_000:
                    raise ValueError('Limite di un milione di individui raggiunto per questa vista.')
            self.last_id = high
            self.cutoff = cutoff
            self.sample = json.loads(raw)
            raw_meta = db.execute('SELECT metadata FROM runs WHERE id=?', (run,)).fetchone()[0]
            self.metadata = json.loads(raw_meta)
            self.gaps = db.execute('SELECT count(*) FROM gaps WHERE run=? AND tick<=?', (run, cutoff)).fetchone()[0]
            width = max(200, math.ceil(max(1, cutoff)/200/200)*200)
            self.bins = []
            # Read at most 201 samples, retaining the last sample of each interval.
            q = ('SELECT data FROM samples WHERE run=? AND tick IN '
                 '(SELECT max(tick) FROM samples WHERE run=? AND tick<=? GROUP BY tick/?) ORDER BY tick')
            for (data,) in db.execute(q, (run, run, cutoff, width)):
                self.bins.append(json.loads(data))
        self.summary = self.build_summary(run)
        return self.summary

    def summarize_group(self, nodes):
        self.check_cancel()
        complete = [n for n in nodes if n.dead is not None]
        alive = [n for n in nodes if n.dead is None]
        exposure = sum(max(0, (n.dead if n.dead is not None else self.cutoff)-n.born) for n in nodes)
        return dict(total=len(nodes), alive=len(alive), completed=len(complete),
            inherited_recoveries=sum(n.recovery for n in nodes),
            natural_children=sum(n.children for n in nodes), children_completed=distribution([n.children for n in complete]),
            children_alive=distribution([n.children for n in alive]),
            age_completed=distribution([n.age for n in complete]),
            age_alive=distribution([n.entry_age+max(0, self.cutoff-n.born) for n in alive]),
            natural_death_age=distribution([n.age for n in complete if n.cause != 'cull']),
            culled_age=distribution([n.age for n in complete if n.cause == 'cull']),
            births_per_1000_agent_ticks=sum(n.children for n in nodes)*1000/exposure if exposure else None,
            exposure_agent_ticks=exposure, max_depth=max((n.depth for n in nodes), default=0),
            causes=dict(collections.Counter(n.cause for n in complete)))

    def timeline(self, key, chosen, compact=False):
        ticks = [s['tick'] for s in self.bins]
        events = []
        for n in self.nodes.values():
            self.check_cancel()
            group = getattr(n, key)
            group = group if group in chosen else 'other'
            events.append((n.born, group, 1))
            if n.dead is not None:
                events.append((n.dead, group, -1))
        events.sort()
        counts, cursor, rows, previous = collections.Counter(), 0, [], {}
        for tick in ticks:
            self.check_cancel()
            while cursor < len(events) and events[cursor][0] <= tick:
                _, group, change = events[cursor]
                counts[group] += change
                cursor += 1
            total = sum(counts.values())
            shares = {g: counts[g]/total if total else 0 for g in [*chosen, 'other']}
            rows.append(dict(tick=tick, counts={} if compact else {g: counts[g] for g in [*chosen, 'other']},
                             effective_strains=total*total/sum(v*v for v in counts.values()) if total else 0,
                             turnover=.5*sum(abs(shares[g]-previous.get(g,0)) for g in shares) if previous and total else None))
            previous = shares
        return rows

    def build_summary(self, run):
        groups = collections.defaultdict(list)
        roots = collections.defaultdict(list)
        for n in self.nodes.values():
            self.check_cancel()
            groups[n.strain].append(n)
            roots[n.root].append(n)
        strain_rows = []
        for sid, s in self.strains.items():
            row = {k:v for k,v in s.items() if k != 'genes'}
            row['gene_count'] = len(s['genes']) if s['genes'] is not None else None
            row.update(self.summarize_group(groups[sid]))
            row['last_tick'] = self.cutoff if row['alive'] else max((n.dead or n.born for n in groups[sid]), default=s['born'])
            strain_rows.append(row)
        strain_rows.sort(key=lambda s: (-s['alive'], -s['total'], s['id']))
        root_rows = [dict(id=root, **self.summarize_group(nodes)) for root,nodes in roots.items()]
        root_rows.sort(key=lambda s: (-s['alive'], -s['total'], s['id']))
        # Full composition turnover before display aggregation into the top eight and "other".
        all_timeline = self.timeline('strain', set(groups), compact=True)
        timeline = self.timeline('strain', [s['id'] for s in strain_rows[:8]])
        for displayed, full in zip(timeline, all_timeline):
            displayed['turnover'] = full['turnover']
            displayed['effective_strains'] = full['effective_strains']
        cap = []
        prev = {}
        for sample in self.bins:
            reproduction = sample.get('reproduction', {})
            eligible = reproduction.get('eligible_agent_ticks')
            blocked = reproduction.get('cap_blocked_agent_ticks')
            old_e, old_b = prev.get('eligible_agent_ticks',0), prev.get('cap_blocked_agent_ticks',0)
            delta = eligible-old_e if eligible is not None else 0
            cap.append(dict(tick=sample['tick'], blocked_fraction=(blocked-old_b)/delta if delta>0 and blocked is not None else None))
            prev = reproduction
        counts = self.sample.get('counts', {})
        alive = sum(n.dead is None for n in self.nodes.values())
        complete = sum(n.dead is not None for n in self.nodes.values())
        integrity = dict(alive_events=alive, alive_sample=self.sample.get('alive'),
            births_events=len(self.nodes), births_sample=sum(counts.get(k,0) for k in ('founders','natural_births','forced_births')),
            deaths_events=complete, deaths_sample=counts.get('deaths'), missing_parents=self.missing_parents,
            unknown_deaths=self.unknown_deaths, duplicate_births=self.duplicate_births, telemetry_gaps=self.gaps)
        integrity['consistent'] = (alive == integrity['alive_sample'] and len(self.nodes) == integrity['births_sample']
                                   and complete == integrity['deaths_sample'] and not any((self.missing_parents,self.unknown_deaths,self.duplicate_births,self.gaps)))
        rep = self.sample.get('reproduction', {})
        eligible, blocked = rep.get('eligible_agent_ticks',0), rep.get('cap_blocked_agent_ticks',0)
        return dict(state='ready', run=run, cutoff=self.cutoff, threshold=self.threshold, model=MODEL,
            integrity=integrity, overview=self.summarize_group(list(self.nodes.values())),
            cap_fraction=blocked/eligible if eligible else None, strains=strain_rows, roots=root_rows,
            timeline=timeline, root_timeline=self.timeline('root',[r['id'] for r in root_rows[:8]]),
            cap_timeline=cap, survival=survival(list(self.nodes.values()),self.cutoff),
            age_histogram=age_histogram(list(self.nodes.values()),self.cutoff),
            checkpoint_origin=self.metadata.get('checkpoint'), source_model=self.metadata.get('model'), clustering='parent_reference',
            definition='Ogni fondatore o ripopolamento avvia una linea. Un figlio resta nel ceppo del genitore se '
                       'la somiglianza con il genoma del fondatore del ceppo raggiunge la soglia; altrimenti avvia '
                       'un ramo. I confronti usano genomi alla nascita, inclusi geni silenti e duplicati; PHY è mostrato separatamente.')

    def detail(self, strain, offset=0):
        if strain not in self.strains:
            raise ValueError('Ceppo sconosciuto.')
        nodes = [n for n in self.nodes.values() if n.strain == strain]
        nodes.sort(key=lambda n: (n.dead is not None, -n.born, n.pid))
        rows = [dict(id=n.pid, parent=n.parent, born=n.born, dead=n.dead, cause=n.cause,
                     age=n.age if n.dead is not None else self.cutoff-n.born, children=n.children,
                     depth=n.depth, censored=n.dead is None, forced=n.forced, recovery=n.recovery)
                     for n in nodes[offset:offset+100]]
        ancestors, current = [], strain
        while current and len(ancestors)<100:
            ancestors.append(current)
            current = self.strains[current]['parent']
        children = [s['id'] for s in self.summary['strains'] if s['parent']==strain]
        ids = set(ancestors + children[:100])
        return dict(strain=strain, cutoff=self.cutoff, individuals=rows, total=len(nodes),
                    summary=self.summarize_group(nodes), survival=survival(nodes,self.cutoff),
                    age_histogram=age_histogram(nodes,self.cutoff),
                    timeline=self.timeline('strain',[strain]),
                    genealogy=[{k:s[k] for k in ('id','parent','born','alive','total','color')} for s in self.summary['strains'] if s['id'] in ids],
                    genealogy_truncated=len(children)>100 or (len(ancestors)==100 and current is not None))

    def compare(self, path, run, first, second):
        def read(pid):
            if pid not in self.nodes:
                raise ValueError('Identificativo agente sconosciuto.')
            with closing(self.open_database(path)) as db:
                row=db.execute('SELECT data FROM events WHERE run=? AND id=?',(run,self.nodes[pid].event)).fetchone()
            data=unpack_event(json.loads(row[0]))
            if not isinstance(data.get('genes'),list):
                raise ValueError('Genoma alla nascita assente per questo agente storico.')
            return data,canonical(data['genes'])
        a,ga=read(first); b,gb=read(second)
        ea,eb=collections.Counter(g[:4] for g in ga),collections.Counter(g[:4] for g in gb)
        return dict(first=first,second=second,similarity=similarity(ga,gb),
                    topology_similarity=sum((ea & eb).values())/max(len(ga),len(gb)) if ga or gb else 1,
                    genes=[len(ga),len(gb)], fingerprints=[fingerprint(ga),fingerprint(gb)],
                    colors=[color(ga),color(gb)],traits=[a.get('traits',{}),b.get('traits',{})],
                    same_root=self.nodes[first].root==self.nodes[second].root,
                    same_strain=self.nodes[first].strain==self.nodes[second].strain, model=MODEL)


class EvolutionService:
    """Single background reader, bounded LRU cache; never touches live Pop objects or writes SQLite."""
    def __init__(self, path):
        self.path=Path(path)
        self.pool=concurrent.futures.ThreadPoolExecutor(max_workers=1,thread_name_prefix='lineages')
        self.lock=threading.RLock()
        self.cache=collections.OrderedDict()
        self.retired=set()
        self.closing=threading.Event()

    def retire(self, run):
        with self.lock:
            items=[key for key in self.cache if key[0]==run]
            if any(not self.cache[key][1].done() for key in items):
                raise ValueError('Attendi che termini l’analisi genealogica di questa run prima di rimuoverla.')
            self.retired.add(run)
            for key in items: del self.cache[key]

    def request(self, run, threshold=.9, refresh=False):
        if self.closing.is_set():
            raise ValueError('Lettore genealogico in chiusura.')
        if not .5 <= threshold <= 1:
            raise ValueError('La soglia deve essere compresa fra 0,5 e 1.')
        key=(run,round(threshold,4))
        with self.lock:
            if run in self.retired:
                raise ValueError('Run rimossa o in rimozione.')
            if key not in self.cache:
                if len(self.cache)>=1:
                    removable=next((k for k,(_,f) in self.cache.items() if f.done()),None)
                    if removable is None:
                        return dict(state='busy',processed=0)
                    del self.cache[removable]
                index=LineageIndex(key[1], self.closing)
                self.cache[key]=(index,self.pool.submit(index.update,self.path,run))
            index,future=self.cache[key]
            if future.done():
                future.result()
                if refresh:
                    future=self.pool.submit(index.update,self.path,run)
                    self.cache[key]=(index,future)
                else:
                    self.cache.move_to_end(key)
                    return index.summary
            return dict(state='loading',processed=index.processed,individuals=len(index.nodes),run=run)

    def query(self, run, threshold, operation, *args):
        with self.lock:
            item=self.cache.get((run,round(threshold,4)))
            if not item or not item[1].done():
                raise ValueError('Attendere il completamento della lettura della run.')
            index,future=item
            future.result()
            return getattr(index,operation)(*args)

    def reopen(self):
        with self.lock:
            self.cache.clear()
            self.retired.clear()
            self.closing = threading.Event()
            self.pool = concurrent.futures.ThreadPoolExecutor(max_workers=1, thread_name_prefix='lineages')

    def close(self):
        self.closing.set()
        self.pool.shutdown(wait=True, cancel_futures=True)


def install_routes(app, service):
    from fastapi import HTTPException
    def call(fn,*args):
        try:
            return fn(*args)
        except ValueError as e:
            raise HTTPException(422,str(e)) from e

    @app.get('/api/evolution/{run}')
    def summary(run:str,threshold:float=.9,refresh:bool=False):
        return call(service.request,run,threshold,refresh)

    @app.get('/api/evolution/{run}/strain/{strain}')
    def detail(run:str,strain:str,threshold:float=.9,offset:int=0):
        return call(service.query,run,threshold,'detail',strain,max(0,offset))

    @app.get('/api/evolution/{run}/compare')
    def compare(run:str,first:str,second:str,threshold:float=.9):
        return call(service.query,run,threshold,'compare',service.path,run,first,second)


def main():
    import os
    import uvicorn
    from contextlib import asynccontextmanager
    from fastapi import FastAPI
    from fastapi.middleware.cors import CORSMiddleware
    from starlette.middleware.trustedhost import TrustedHostMiddleware
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--db',required=True,type=Path)
    parser.add_argument('--port',type=int,default=8002)
    parser.add_argument('--dashboard-port',type=int,default=8000)
    args=parser.parse_args()
    os.nice(10)
    service=EvolutionService(args.db)
    @asynccontextmanager
    async def lifespan(app):
        yield
        service.close()
    app=FastAPI(lifespan=lifespan)
    app.add_middleware(TrustedHostMiddleware,allowed_hosts=['localhost','127.0.0.1'])
    app.add_middleware(CORSMiddleware,allow_origins=[f'http://127.0.0.1:{args.dashboard_port}',f'http://localhost:{args.dashboard_port}'],allow_methods=['GET'])
    install_routes(app,service)
    uvicorn.run(app,host='127.0.0.1',port=args.port,log_level='warning')


if __name__=='__main__':
    main()
