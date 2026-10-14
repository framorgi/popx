# PopX — stato tecnico ed evolutivo

## Aggiornamento tecnico successivo all'audit — 14 ottobre 2026

Su richiesta del developer sono state corrette la rotazione delle fasi e la
direzione iniziale. Le sezioni **1–17 restano la fotografia dell'audit sul commit
indicato**, prima delle correzioni: i relativi riferimenti di riga e giudizi su
ordine invertito/direzione zero non descrivono più questi punti del working tree.
Le osservazioni e gli altri difetti dell'audit sono conservati, non cancellati.

| Aspetto | Comportamento dopo la correzione | Verifica |
|---|---|---|
| Rotazione | Prima swap(think, act), poi swap(sense, think): ciascun gruppo passa S→T→A→S. | Test del manager con24 fondatori, assegnazione casuale e8 tick; prima act nei tick3–5, poi ogni3 tick. |
| Ordine all'avvio/riposizionamento | `Pop::next_phase_` consente soltanto la fase successiva. Gruppi iniziali casuali possono attendere il primo sense, senza think/act anticipati. Dopo NewGen il ciclo pendente viene rispettato. | Chiamate fuori ordine e duplicate; breve controllo NewGen/ridistribuzione. |
| Direzione iniziale | `Pop::init` sorteggia uniformemente N/NE/E/SE/S/SW/W/NW, escludendo(0,0); vale anche per figli e reinit. |48 primi movimenti forward/left/right, da centro di griglia piatta libera, con i costruttori a genoma e a genoma+PhyT. |
| Reinit | Reset di fase attesa, `first_run_` e `last_action_`; nessuna decisione pendente riutilizzata. | Ciclo prima/dopo init; chiamate anticipate non spostano l'agente. |
| Swap/carico | Tre gruppi conservati, azione circa ogni3 tick a regime, swap dei vettori senza copia degli agenti; sense/think ancora paralleli, act sequenziale. | Non eseguito benchmark: beneficio quantitativo non misurato. |

Implementazione: `src/pops_manager.cpp`, `PopsManager::rotate_buckets`;
`src/pop.cpp`, `Pop::init/sense/think/act`; `src/pop.h`, `next_phase_`.
Test persistente: [tests/pop_cycle_regression.cpp](../tests/pop_cycle_regression.cpp).

Build Release con target esplicito `popx`: ricompilazione e link completati.
Test standalone: **PASS: phase guards, bucket rotation, initial relative movement**.
Riproduzione dei controlli, dalla root con Bash:

```sh
cmake --build build-release --target popx -j 2
clang++ -std=c++23 -O0 -g -Isrc tests/pop_cycle_regression.cpp build-release/src/CMakeFiles/popx.dir/{brain,cell,config,console_logger,gene,genome,grid_world,pop,pops_manager,random_utility}.cpp.o -ltbb -lstdc++fs -o /tmp/popx_cycle_regression
/tmp/popx_cycle_regression
```

Le fixture usano getter di configurazione dedicati e risorse/termica controllate,
senza cambiare `config/popx.ini`, senza rendering o simulazioni evolutive lunghe.
Non asseriscono frequenze casuali o fitness: gli invarianti devono valere per
qualsiasi direzione sorteggiata. CTest rimane privo di test registrati; il controllo
si esegue separatamente. Non è stata risolta la riproducibilità stocastica generale.

Il bias di argmax, l'inizializzazione delle occupazioni, i trasferimenti,
l'eredità Hebbiana e i limiti delle statistiche restano questioni separate.
Questa modifica non dimostra che il movimento emergerà per selezione naturale.

## 1. Sintesi esecutiva

PopX implementa agenti con genomi di connessioni neurali, rete feedforward sparsa,
tratti fisiologici ereditabili, mutazione, riproduzione locale, metabolismo,
risorse finite per cella, temperatura variabile, feromoni e mortalità. Contiene
anche plasticità Hebbiana modulata da un reward ed eredità dei pesi appresi,
attiva nell'INI corrente, e una selezione periodica esplicita `NewGen`.

La capacità di produrre differenze neurali ed ereditarie è presente a livello
meccanico. L'emergere di adattamento ecologico **non è dimostrato**. L'audit trova
problemi che possono confondere qualsiasi interpretazione evolutiva: cancellazione
delle occupazioni dopo lo spawn, ordine individuale delle fasi invertito,
immobilità iniziale delle azioni relative, sensori del glucosio non raggiungibili,
trasferimenti non conservativi e metriche incomplete o scorrette. Non è possibile
concludere che la selezione favorisca agenti immobili da queste sole evidenze.

La verifica con maggiore valore informativo è un controllo breve degli invarianti
di inizializzazione e della catena sensori–rete–movimento, prima di esperimenti
lunghi o cambiamenti biologici. Nessun sorgente, parametro o comportamento è stato
modificato durante questo audit.

## 2. Data, revisione, perimetro e verifiche

- Data: **14 ottobre 2026**, riferimento temporale utente Europe/Rome.
- HEAD: `0e9fc465956eb967ad67f7352eabccc663c8bcf4` (commit 24 giugno 2026).
- Analisi del codice corrente `src/`, INI, CMake, preset, script di visualizzazione,
  documentazione concettuale e definizioni del container. I riferimenti `file:riga`
  corrispondono ai file ispezionati in questa revisione.
- Nessun `AGENTS.md` applicabile o `docs/EVOLUTION_STATUS.md` preesistente nel
  repository. Creati entrambi; `README_concept.md` preservato.
- Stato iniziale: sorgenti tracciati senza modifiche; `old/` già non tracciato.
  `old/popw/` è un archivio separato, non incluso nel build corrente: non utilizzato
  per dedurre il comportamento attuale. Nessun risultato passato è stato validato.
- Dipendenze disponibili nel build Release esistente: SFML e TBB di sistema,
  sorgenti locali FetchContent di Eigen/JSON/ImGui/ImGui-SFML. Nessuna installazione.
- Ambiente: Clang 18.1.3, CMake 3.28.3. `ninja -C build-release -n popx` e
  `cmake --build build-release --target popx -j 2`: nessun lavoro da eseguire.
- Verifica aggiuntiva: tutti i **17** `.cpp` di `src/` controllati con
  `-fsyntax-only`, mantenendo compilatore, macro, include e C++23 del database
  `build-release/compile_commands.json`, rimuovendo `-c` e `-o`; esito senza errori.
  Non equivale a un nuovo link completo o a una verifica runtime.
- `ctest --test-dir build-release -N`: **Total Tests: 0**. Non esiste una suite
  corrente da dichiarare superata; nessun test nuovo implementato.
- **Nessun esperimento di simulazione condotto**, nessuna misura empirica di fitness,
  movimento, mutazione o prestazioni. Numeri derivati da formule sono esplicitamente
  identificati come calcoli statici, non risultati osservati.
- Il sandbox di shell non si avviava (`bwrap`, namespace non consentito dal kernel);
  le operazioni autorizzate sono state eseguite tramite escalation approvata.

## 3. Architettura corrente, analizzata indipendentemente

### 3.1 Componenti e responsabilità

| Sottosistema | Classi/simboli e riferimenti | Comportamento verificato |
|---|---|---|
| Composizione | `main`, `src/main.cpp:15` | Crea configurazione, logger, mondo, manager, simulatore, renderer, statistiche e GUI tramite interfacce. |
| Loop e tempo | `PopXApp::run`, `src/popx_app.cpp:25` | Tick fissi di 10 ms con accumulatore, limite grafico e GUI; nessun `dt` passato alla fisiologia. |
| Tick simulazione | `Simulator::update`, `src/simulator.cpp:16` | Aggiorna prima agenti, poi celle/ambiente. |
| Popolazione | `PopsManager::update_cycle`, `src/pops_manager.cpp:58` | Tre gruppi di fase, aggiornamento fisiologico di tutti, rimozione morti, riproduzione, rotazione e possibile selezione. |
| Agente | `Pop`, `src/pop.h:44`; `Pop::update`, `src/pop.cpp:187` | Stato individuale, età, energia, riserve, temperatura, sensori, rete e discendenti. |
| Genetica | `Gene`, `src/gene.h:5`; `Genome`, `src/genome.cpp:9` | Vettore di geni di estremi/peso. Il campo `genome_size_` non è la fonte affidabile dopo `add_gene`. |
| Rete | `Brain::wire`, `src/brain.cpp:49`; `feed_forward`, riga 131 | Matrici sparse per coppie di strati in ordine crescente, connessioni dirette/skip, sigmoide, una sola azione argmax. |
| Sensori/azioni | `Sensor`, `Action`, `src/neuron.h:5,61`; `Pop::sense/act`, `src/pop.cpp:215,433` | Sensori connessi soltanto; movimento, emissione, risorse, termoregolazione e modulazioni. |
| Fisiologia | `Pop::update_physiology`, `src/pop.cpp:705` | Assorbimento automatico, fotosintesi, respirazione, scambio termico, costi e cap energetico. |
| Mondo | `GridWorld`, `src/grid_world.cpp:7,15,127` | Griglia rettangolare finita, campi RBF gaussiani, una referenza debole per cella, temperatura periodica. |
| Celle | `Cell`, `src/cell.cpp:9,210` | Riserve, occupante, quattro feromoni, rigenerazione di glucosio/O2, decadimento. Nessuna diffusione. |
| Plasticità | `Pop::calculate_reward/learn`, `src/pop.cpp:116,175`; `Brain::hebbian_update`, `src/brain.cpp:368` | Aggiornamento locale dei pesi esistenti dopo fisiologia. |
| Statistiche | `PopsManager::get_pops_snapshot`, `src/pops_manager.cpp:385`; `Stats`, `src/stats.cpp:42` | Snapshot dei vivi, aggregati campionati, storico limitato e JSON. |
| Grafica | `Renderer::draw/update_world`, `src/renderer.cpp:33,61`; `SfmlGraphicEngine`, `src/sfml_graphic_engine.cpp:12` | Lettura del mondo, buffer triangoli/quadrilateri e SFML; GUI ImGui nello stesso loop. |
| Configurazione | `ConfigParams`, `src/config.h:7`; `Config::update_from_file`, `src/config.cpp:441` | Default più INI letto all'avvio. Parser `@N` presente, reload generazionale non chiamato. |
| Casualità | `RandomUtility`, `src/random_utility.cpp:5`; `rand()`, `src/pops_manager.cpp:22,242,341` | Molti generatori indipendenti da `random_device`, più generatore C per gruppi di fase. |

Il manager possiede i `Pop` mediante `shared_ptr`; il mondo li osserva mediante
`weak_ptr`; ciascun `Pop` osserva il mondo e possiede un `IBrain` via `unique_ptr`.
La proprietà evita cicli, ma non garantisce l'invariante spaziale: occorre distinguere
sicurezza della durata degli oggetti da correttezza delle loro registrazioni.

### 3.2 Timestep, fasi, sincronizzazione

Il tempo biologico è il numero di chiamate a `Simulator::update`, con target 100
tick per secondo simulato. Il loop recupera l'accumulatore senza limite di tick
per frame (`src/popx_app.cpp:43–59`): un carico superiore al tempo disponibile può
prolungare il recupero e ritardare rendering, input e rilevazione dell'estinzione.
SFML limita a 60 FPS (`src/sfml_graphic_engine.cpp:14`); il loop applica anche un
sleep a circa 16 ms (`src/popx_app.cpp:141`). Non c'è un eseguibile batch/headless.

Ogni tick esegue `sense` su un gruppo e `think` su un altro con `par_unseq`/TBB;
`act`, fisiologia, morti, riproduzione e ambiente restano sequenziali. Le chiamate
agli algoritmi paralleli terminano prima della fase successiva. Il parametro
`num_threads` non configura TBB. Il logger ha un mutex, ma il livello impostato
in `main` è Error; questo nasconde warning di reward/coda nell'esecuzione standard.
Gli snapshot non rendono sicura un'eventuale futura GUI concorrente: nessun lock
protegge popolazione e mondo da accessi simultanei esterni.

La rotazione effettiva, per gruppi iniziali S/T/A, è:

| Momento | gruppo sense | gruppo think | gruppo act |
|---|---|---|---|
| prima della rotazione | S | T | A |
| dopo `swap(sense, think)` | T | S | A |
| dopo `swap(think, act)` | T | A | S |

Un agente passa quindi **sense → act → think → sense** e agisce una volta ogni
3 tick, usando l'azione calcolata in precedenza, non quella dei sensori appena
letti. `learn` gira ogni tick dopo il primo `think`, riutilizzando le ultime
attivazioni per tre aggiornamenti circa. Gli agenti inizialmente in `think`
leggono sensori ancora a zero; quelli in `act` hanno `last_action_=-1`.

### 3.3 Percorsi critici completi

| Operazione | Percorso verificato e dettagli |
|---|---|
| 1. Creazione | `main → PopXApp::init → Simulator::init → PopsManager::spawn_population → Pop` (`main.cpp:31`, `popx_app.cpp:14`, `simulator.cpp:12`, `pops_manager.cpp:16`, `pop.cpp:21`). Genoma lungo casualmente 100–700 nell'INI, geni casuali, energia 100, direzione zero. Costruttore chiama `init`; manager lo richiama dopo `try_spawn`, riassegnando fisiologia/lazyness. Coordinate sorteggiate con massimo inclusivo larghezza/altezza, poi ritentate se invalide/occupate. |
| 2. Decodifica | `Pop::init → Brain::wire → Gene` (`pop.cpp:82`, `brain.cpp:61`, `gene.cpp:15`). Validazione tipi, ordine e indici; duplicati sommati da `setFromTriplets`. `Simulator::init` chiama poi `GridWorld::init`, che cancella gli occupanti già inseriti (`simulator.cpp:13–14`, `grid_world.cpp:37`). |
| 3. Sensori/rete | Gruppo `sense → Pop::sense → get_connected_sensors → scansioni`; gruppo `think → feed_forward` (`pops_manager.cpp:61–72`, `pop.cpp:229,419`). Le letture sono separate nel tempo; tutte le attivazioni non-input sono sigmoidali, inclusi neuroni privi di ingressi. |
| 4. Azioni | `feed_forward → last_action_ → gruppo act → Pop::act → try_move/mondo` (`brain.cpp:154`, `pop.cpp:422,433,575`). Argmax deterministico, un'unica azione. Movimento paga anche se fallisce; direzioni relative iniziali producono richiesta a stessa posizione. |
| 5. Energia/risorse | Azione eventualmente prende 10 unità; ogni `update` prende automaticamente 5 glucosio/acqua/O2/CO2, 2 calcio, 1 lipide, poi reazioni, temperatura e costi (`pop.cpp:511,705`). Solo la respirazione aumenta direttamente energia; fotosintesi aumenta glucosio. |
| 6. Riproduzione | `wants_to_reproduce → collect_reproducer_candidates → FIFO → process_reproduction_backlog → try_reproduce` (`pop.cpp:631`, `pops_manager.cpp:127,159,216`). Figlio da un unico genitore, ricerca 8 celle adiacenti N/E/S/W poi diagonali, senza prova di pendenza. Risorse donate solo dopo successo; figlio aggiunto al gruppo di fase casuale e ruotato nello stesso tick. |
| 7. Mutazione | `make_offspring_genome → [get_learned_genome] → Genome::mutated` (`pop.cpp:635`, `brain.cpp:494`, `genome.cpp:29`). Ogni gene è sostituito integralmente con probabilità 0.05. `PhyT` copiato e ciascun tratto mutato ±1 con la stessa probabilità e clamp (`pop.cpp:646`). Sotto 20 vivi il manager moltiplica temporaneamente il rate per 10: attuale 0.5. |
| 8. Morte/rimozione | `Pop::update → die → Cell::give_* → alive=false`; manager rimuove dal mondo e da tutti i gruppi/vettori (`pop.cpp:190,87`, `pops_manager.cpp:92–110`). Nessun archivio di vite concluse. `KILL_FORWARD` esiste nel codice ma non è selezionabile con 21 output. `NewGen` elimina invece senza chiamare `die`. |
| 9. Statistiche | Ogni 200 tick dopo `sim_->update`, app misura quella sola chiamata, richiede snapshot dei vivi e totale risorse del mondo, poi `Stats::record_tick`; ogni frame chiama `record_frame` (`popx_app.cpp:46–56,134`, `stats.cpp:42,155`). JSON contiene aggregati, non record individuali. |

## 4. Meccanismi evolutivi correnti

Riproduzione asessuata continua: condizioni normali `alive && energy > 50 && age >
1000 && glucose > 20`, valutate dopo fisiologia. Non esiste azione riproduzione,
partner, crossover, scelta sessuale o fitness per selezionare i genitori delle
nascite; le opzioni che li suggeriscono sono inattive. Un adulto idoneo può
ritentare ogni tick: nessun cooldown. Cap hardcoded 120 vivi; la popolazione
iniziale non viene limitata da quel cap.

Sotto 20 vivi, i primi candidati nell'ordine del vettore possono essere accodati
e riprodotti anche senza età/energia/glucosio sufficienti. La coda conserva
referenze deboli e ID, massimo 64; tentativi normali 3/tick, più 1 nel regime
forzato. Il budget conta anche candidati scaduti/ispezionati. `repro_processed_this_tick`
conta chiamate a `try_reproduce`, inclusi fallimenti spaziali, non nascite. Nessuna
priorità separata effettiva: l'opzione aumenta il budget FIFO. Al cap la coda può
restare piena; ordine iniziale/anzianità e spazio possono influire sulle opportunità.
Con zero vivi non c'è genitore e non si recupera dall'estinzione.

Il genitore dona **1/8**, arrotondato per difetto, di glucosio/acqua/calcio/CO2;
non divide energia, O2 o lipidi (`pop.cpp:665`). Il figlio nasce con energia 100,
O2 20 e lipidi 20 predefiniti e riserve donate. Il costo riproduttivo nominale
0.3 incrementa un contatore, **non viene detratto** dall'energia (`pop.cpp:820`).
Questi sono fatti implementativi; l'effetto sulla selezione va misurato.

`NewGen` è attivo ogni 50.000 tick o manualmente. Ordina i vivi per
`offspring + 0.01 * energy`, conserva il 90% arrotondato con minimo 8, rimuove
tutti dal mondo e riposiziona i superstiti casualmente (`pops_manager.cpp:272–362`).
Non crea una nuova popolazione di figli, non resetta età, rete o discendenza e non
restituisce risorse degli eliminati. Il contatore generazione conta questi eventi,
non nascite o profondità genealogica. Questo aggiunge una pressione di selezione
esplicita e traslocazioni artificiali al ricambio continuo.

## 5. Implementazione genetica, neurale e plasticità

### 5.1 Codifica e raggiungibilità strutturale

Il gene codifica tipo sorgente/sink a 3 bit (0–7), indice a 5 bit (0–31), peso
`int16_t / 8192` in [-4, 3.9998779296875] (`gene.h:34–50`, `gene.cpp:15,41`).
Con H=4: tipo 0 input, 1–4 hidden, 7 output; 5–6 inattivi. Si accettano solo
connessioni verso strati più profondi, con indici validi; nessuna ricorrenza,
nessun bias esplicito. Lo scarto degli indici evita il bias da modulo; un commento
in `gene.h:36` descrive ancora modulo e non corrisponde a `Brain::wire`.

La rete attuale ha 32 input, quattro hidden da 16, 21 output. Le connessioni
sensor→output sono consentite: quattro strati hidden non sono obbligatori per
influenzare un'azione. I geni scartati/silenti possono divenire attivi se una
successiva sostituzione casuale genera una connessione valida. Non esiste mutazione
delle dimensioni degli strati nel percorso applicativo corrente.

**Calcolo statico**, assumendo campi casuali uniformi indipendenti come in
`Gene::genetic_lottery`: probabilità che un gene superi i filtri correnti
`[4*(1/2) + 21/32 + 6*(1/2)^2 + 4*(1/2)*(21/32)]/64 = 0.08544921875`.
Il conteggio considera input→hidden, input→output, hidden→hidden, hidden→output.
Per 100–700 geni, l'attesa teorica è 8.54–59.81 geni accettati, prima di
aggregare duplicati. **Non** misura connessioni causali, cammini completi, azioni
diverse o reti realmente presenti nella popolazione. I geni con sorgente sensore
accettati hanno probabilità teorica `(2+21/32)/64 = 0.04150390625` per gene.

Ogni neurone senza ingressi vale `sigmoid(0)=0.5`: può funzionare come sorgente
costante per uno strato successivo. Non occorre un cammino dall'input per generare
un output non neutro. Questo distingue autonomia costante da risposta ambientale.

Argmax sceglie il primo indice a pari valore, iniziando da soglia 0. Rete vuota:
output tutti 0.5, quindi indice 0 `MOVE_FORWARD`, **non** nessuna azione. Anche
output disconnessi possono prevalere su output con contributi negativi. Con pesi
che producono saturazione, ulteriori parità sono possibili. Il bias è verificato;
la sua frequenza nella popolazione resta ignota.

### 5.2 Sensori e azioni disponibili

| Indici | Sensori implementati | Limiti |
|---|---|---|
| 0–4 | posizione e distanza dai bordi | Distanza reale normalizzata 0 al bordo e circa 1 al centro; commento in `pop.cpp:273` incoerente. |
| 5 | similarità genetica frontale | Costante 0.5, TODO. |
| 6–7 | ultimo spostamento | Trasforma displacement con `x*0.5+0.5`; non clampato se salti >1. |
| 8–11 | occupazione N/W/E/S | Media sulle celle della direzione; dipende dalla registrazione spaziale. |
| 12–19 | temperatura media e gradienti | Temperatura/50 clampata: saturazione sopra 50; gradienti rispetto a cella locale. |
| 20–27 | quattro segnali locali e gradiente food | Valori /100 e gradiente in [0,1]; nessuna diffusione. |
| 28–31 | oscillatore, età, temperatura locale, random | AGE satura a 300 tick nell'INI; nessun sensore energia, fame, O2 o temperatura corporea. |
| 32 | `NUM_SENSES` | Sentinella, non sensore effettivo. |
| 33–36 | densità glucosio N/W/E/S | Fuori da input attuali e codifica a 5 bit: non calcolati con size_s=32, comunque non cablabili aumentando soltanto size_s. |

Le scansioni usano `probe_dist_=4` (`pop.h:319`); `PhyT::sensitiveness` ereditato
non modifica quel raggio. Si calcolano solo sensori con almeno un arco uscente,
anche se quell'arco non ha influenza sugli output. Il commento generale [0,1]
non è una garanzia per i sensori dell'ultimo spostamento.

Gli output 0–7 sono movimento (relativo, random, cardinale); 8–11 emissione;
12–17 acquisizione/rilascio glucosio, acqua e calcio; 18 calore; 19 oscillatore;
20 responsiveness. Sentinella 21 e `KILL_FORWARD=22` sono fuori dai 21 output
correnti. Nessuna azione no-op esplicita. Le emissioni non pagano energia;
`LEAVE_*` non decrementa le riserve. I commenti di `neuron.h` parlano di 2 unità
e incremento responsiveness 0.1, ma il codice usa 10 e 0.001.

Movimento: pendenza di salita `< 6*(1-lazyness)`; discesa senza costo/limite
simmetrico, altitudine troncata a intero in `GridWorld::get_elevation`. Confini,
occupante e barriera calcio esattamente 500 bloccano la destinazione. Nessuna
verifica delle celle intermedie nei salti. `lazyness` è casuale a nascita, non
ereditata; non modula frequenza, contrariamente al commento `pop.h:330`.
Direzione iniziale (0,0): forward/left/right non avviano spostamenti; servono
random/cardinali. `MOVE_RANDOM` include anche (0,0). Il costo 0.01 è per tentativo,
indipendente da successo o distanza.

`responsiveness` float moltiplica displacement e viene convertito implicitamente
nelle coordinate intere. Esempio statico da x=10 con responsiveness=1.001:
EAST produce x=11, WEST x=8. Quindi può generare asimmetria direzionale; le
azioni relative riutilizzano l'intero displacement precedente, non un versore.

### 5.3 Plasticità, reward ed eredità

Dopo ogni fisiologia sopravvissuta, se c'è stato almeno un `think`, il codice
applica a ogni arco esistente:

```text
w' = clamp(w + 0.06 * reward * (pre-0.5)*(post-0.5) - 0.0001*w, -4, 4)
```

Non crea nuove connessioni. Il reward combina età/MaxAge, energia normalizzata,
delta di energia relativo alla capacità respiratoria, discendenza/25 e vicinanza
alla temperatura ottimale. Con le costanti attuali i pesi **effettivi** sono
0.10, 0.30, 0.35, 0.05, 0.20: `max/min` in `calculate_reward` cambiano quelli
apparenti di `common.h:12–15`. Il reward viene clampato [-1,1], con protezione
`isfinite`. Non esiste reward diretto di movimento, ma esistono obiettivi
handcrafted di sopravvivenza/energia/temperatura. L'efficacia del credito attribuito
alle azioni non è dimostrata; sono aggiornati tutti gli archi con attivazioni
potenzialmente precedenti, anche nei tick senza azione.

`hebbian_inheritance=false` disabilita l'esportazione dei pesi appresi, **non**
l'apprendimento durante la vita. Con `true`, `make_offspring_genome` usa il genoma
ricostruito dai pesi correnti quando disponibile. Questa è eredità di modifiche
acquisite rispetto al genoma parentale originario.

La ricostruzione è differita fino alla riproduzione (`brain.cpp:488–541`),
conserva geni silenti e scartati ma accorpa geni duplicati in un solo arco,
riordina i geni e quantizza/clampa il peso a int16. Somme originarie oltre ±4
possono essere limitate; il limite superiore genetico è leggermente inferiore
al clamp +4. Non conserva esattamente la lista originale o necessariamente la
lunghezza minima configurata. `Genome::add_gene` non aggiorna colore o
`genome_size_`; `mutated` ricalcola il colore prima di creare il figlio, quindi
il problema di colore non implica un colore finale necessariamente errato qui.

## 6. Ambiente, metabolismo e configurazione

### 6.1 Formule e frequenze

Per ogni tick, dopo acquisizioni e fotosintesi, con T corporea precedente:

```text
eff = clamp(1 - abs(T - 37)/10, 0, 1)
n = min(mitochondrions, glucosio_disponibile, O2_disponibile)
income = n * 0.3 * eff
E_dopo_fisiologia = min(400, E_prima + income - 0.02 - costi_azioni)
T_successiva = T + alpha*(T_ambiente - T) + 0.3*n + calore_volontario
alpha = clamp(0.5 - (0.5-0.01)*min(lipidi/80, 1), 0.01, 0.5)
```

Riferimenti: `pop.cpp:705–813`. `E_prima` è energia a ingresso nella fisiologia;
la riproduzione successiva non sottrae energia. Il contatore respirazione registra
il guadagno lordo anche se il cap elimina l'eccesso. Metabolismo opera **ogni tick**;
azione circa ogni 3. Termoregolazione volontaria costa 0.05 e produce 0.5 di calore
per esecuzione. Morte con E≤0, età>200.000, T≤10 o T≥75.

Temperatura iniziale 25: efficienza respiratoria iniziale esattamente zero, ma si
consumano comunque substrati e si produce calore. Con 1–25 mitocondri, il limite
teorico di guadagno per tick a efficienza 1 è 0.3–7.5, **prima** delle disponibilità
locali, consumo, costi e cap. Non è un tasso sostenibile misurato.

Un individuo che tenta movimento ad ogni fase `act` aggiunge circa 0.01/3 al
costo medio per tick, oltre 0.02 basale. Un'unità di substrato respirata produce
al massimo 0.3: un vantaggio di 0.01 richiederebbe più di `1/(30*eff)` unità
respirate in più per tentativo, a eff>0 e senza saturazione. Questo mostra che
il vantaggio energetico del movimento è **possibile** quando permette di consumare
substrati prima inaccessibili. Non dimostra un vantaggio atteso: importano pendenza,
termica, ossigeno, frequenza delle azioni, riserve e probabilità di arrivare a risorse.
A eff=0 o E al cap, più glucosio non garantisce guadagno utile.

Fotosintesi: cloroplasti≥10, acqua/CO2>0, elevazione≥30; probabilità
`min(cloroplasti/14,1)`; una unità di acqua+CO2 → una di glucosio, O2 rilasciato
nella cella. Respirazione produce CO2 interno **e** restituisce lo stesso CO2
alla cella, senza decrementare la riserva interna (`pop.cpp:781–788`).

Glucosio e acqua iniziali: 35 gaussiane ciascuno; elevazione: 20; temperatura: 10
(`grid_world.cpp:18–32`). Somma limitata alla massima ampiezza sorteggiata, poi
risorse clampate alla capacità. O2, CO2, lipidi e calcio sono inizializzati
casualmente nelle celle; azoto zero. RBF non equivale a patch nettamente isolate:
code positive, troncamento intero e consumo determinano la localizzazione reale.

Ogni cella riceve +1 glucosio e +1 O2 ogni 1000 tick, fasi casuali iniziali;
la rigenerazione non dipende dalla distribuzione iniziale. Senza consumo porta
anche celle inizialmente vuote verso il cap, ma il tempo e la distribuzione
in un ecosistema abitato non sono noti. Acqua non rigenera. Nessuna diffusione
risorse/feromoni; segnali locali capped 100 e moltiplicati per 0.9995/tick, poi
azzerati sotto 0.01. Sono quattro valori per cella, non oggetti autonomi.

Ogni 5000 tick si ricalcola temperatura ruotando centri RBF di 0.009 radianti
per aggiornamento dopo il primo, attorno al centro mappa. Cambiano anche deviazioni
standard (+cos(delta)*0.01 e +sin(delta)*0.01): quindi il campo non è soltanto
traslato/ruotato rigidamente (`random_utility.cpp:99`). Il controllo è già eseguito
al primo tick, con angolo zero. Termica influisce direttamente su energia e morte;
non è garantita una pressione ecologica moderata o superabile da ogni rete.

### 6.2 Parametri: default e INI corrente

Valori INI effettivi **se** `main` trova quel file. Il percorso è due directory
sopra l'eseguibile più `config/popx.ini` (`main.cpp:19–24`), coerente con
`build-release/src/popx`. Avvio via PATH o layout installato diverso non è
verificato; in assenza del file il programma continua con default e messaggio
stderr. Percorsi output configurati risolti rispetto alla directory dell'INI,
non rispetto al build come dice il commento dell'INI. Il JSON statistiche usa
invece `../data_out` rispetto al CWD (`popx_app.cpp:172`).

| Parametro/i | Default `ConfigParams` | INI corrente | Uso effettivo |
|---|---|---|---|
| brain_size_s / n / y | 5 / 5 / 5 | 32 / 16 / 21 | Dimensioni rete a costruzione. |
| brain_num_hidden_layers | 1 | 4 | Ammessi 1–6 dal parser. |
| population | 100 | 80 | Fondatori; non cap di popolazione. |
| steps_per_generation | 300 | 300 | Scala AGE e massimo oscillatore. |
| max_generations | 200000 | 200000 | Non usato per terminare. |
| num_threads | 4 | 4 | Non usato per dimensionare TBB. |
| genome_min_length / max_length | 24 / 24 | 100 / 700 | Lunghezza iniziale, non validata min≤max e non imposta sui figli. |
| point_mutation_rate | 0.001 | 0.05 | Sostituzione per gene e mutazione per tratto; ×10 sotto 20 vivi. |
| gene_insertion_deletion_rate / deletion_ratio | 0 / 0.5 | 0 / 0.5 | Letti ma non applicati. |
| sexual_reproduction / choose_parents_by_fitness | true / true | true / true | Letti ma non applicati alle nascite. |
| hebbian_inheritance | false | true | Esportazione dei pesi appresi; learning sempre attivo dopo primo think. |
| brain_serialization_enabled | false | false | Export/cleanup delle reti condizionato; file dei morti eliminati se attivo. |
| nnets_dir / log_dir / image_dir | ../data_out/nnets/, ../data_out/logs/, ../data_out/images/ | stessi | Solo nnets usato nel percorso corrente; logger console e salvataggio immagini TODO. |
| phy_init_mitochondrions_max / chloroplasts_max | 3 / 2 | 25 / 25 | Fondatori 1..max / 0..max; clamp mutazioni. |
| phy_init_sensitiveness_max | 6 | 6 | Tratto ereditato, non usato nelle scansioni. |
| phy_adipose_stock_max | 80 | 80 | Tratto ereditato; riserve lipidiche non limitate da esso. |
| phy_min_chloroplasts / max_chloroplasts_ref | 1 / 3 | 10 / 14 | Soglia e probabilità fotosintesi. |
| phy_energy_per_respiration | 0.2 | 0.3 | Per substrato/mitocondrio con efficienza; contatore riproduzione. |
| phy_heat_per_respiration | 0.3 | 0.3 | Per substrato consumato, anche a efficienza zero. |
| phy_alpha_min / max | 0.01 / 0.5 | 0.01 / 0.5 | Scambio termico; manca validazione congiunta min≤max e stabilità. |
| phy_max_lipids_ref | 80 | 80 | Normalizzazione isolamento, non limite di stoccaggio. |
| phy_init_temperature / opt_temperature | 25 / 37 | 25 / 37 | Corpo iniziale e optimum. |
| phy_photo_min_elevation | 30 | 30 | Soglia luce. |
| phy_efficiency_half_range_c | 10 | 10 | Efficienza e reward termico; parser clamp [0.1,100]. |
| newgen_enabled | false | true | Abbattimento/riposizionamento periodico. |
| newgen_interval_ticks | 5000 | 50000 | Intervallo, distinto da steps_per_generation. |
| newgen_survival_ratio / min_survivors | 0.35 / 8 | 0.9 / 8 | Quota e minimo superstiti. |
| max_reproducers_per_tick | 3 | 3 | Parser clamp [1,32]. |
| max_repro_backlog | 64 | 64 | Parser clamp [8,2048]. |
| forced_repro_priority_per_tick | 1 | 1 | Extra budget [0,32], senza coda prioritizzata. |
| param_change_generation | 0 | non specificato | Interno al parser; nessun effetto dinamico nel loop. |

Fonti: `config.h:7–68`, `config/popx.ini:8–73`, `config.cpp:237–482`.
Parser supporta commenti `#`, assegnazioni e `@N`: prevale l'ultima assegnazione
applicabile nell'ordine del file, non necessariamente il suffisso N più alto.
`update_from_file` è chiamato soltanto nel costruttore con generazione 0.
Non c'è applicazione di override futuri o aggiornamento di reti già costruite.

### 6.3 Costanti non configurabili

| Dove | Costanti che influenzano gli esperimenti |
|---|---|
| `common.h:5–16` | mappa 120², refresh temperatura 5000, MaxAge 200000, MaxEnergy 400, popolazione min/max 20/120, pesi reward nominali 0.05/0/0.35/0.60, eta Hebb 0.06. |
| `common.h:23–35,50,54` | acqua 1000, glucosio 500, lipidi 300, O2/CO2 1000, N2 2000, calcio 500, feromoni 100; regen G/O2 +1/1000; decay 0.9995; pendenza 6. WaterThreshold=100 è visuale. |
| `pop.cpp:16–19,25,54–65,190,511,631,665,713` | costi move 0.01, basale 0.02, calore 0.05/0.5; energia nascita 100, riserve; morte T10/75; azioni risorse 10; riproduzione E50/età1000/G20; donazione 1/8; assorbimento automatico. |
| `pop.h:319,323,327` | raggio 4, oscillatore 100, responsiveness 1. |
| `brain.cpp:386–396`; `pop.cpp:145–155` | decay 0.0001, clamp ±4; discendenza/25 e clamp dei pesi reward. |
| `grid_world.cpp:18–32`; `random_utility.cpp:109` | conteggi/ampiezze/dispersioni RBF, passo angolare e variazione dispersioni. |
| `popx_app.cpp:29–30`; `stats.h:34,61` | dt10 ms, campione200 tick, storico50000 record, finestra rate10 campioni. |

Sopravvivenza/riproduzione sono particolarmente sensibili a disponibilità O2/G,
mitocondri, termica, alpha/lipidi, età minima, energia iniziale e forzatura/cap.
Movimento dipende anche da direzione iniziale, pendenza, lazyness, interi e argmax.
Diversità dipende da sostituzione per gene, geni inattivi, ricostruzione dei pesi
e piccoli numeri di riproduttori; non da inserzione/delezione configurata.

## 7. Osservazioni storiche e confronto critico

Le osservazioni fornite dal developer descrivono esperienze passate, senza seed,
tracce individuali o revisione associata. Sono riportate come contesto storico,
non come esperimenti riprodotti. Le classificazioni distinguono un fatto nel codice
dall'effetto biologico di quel fatto; una riga può richiedere due giudizi diversi.

| Tema storico | Classificazione | Codice attuale ed evidenza | Incertezza e verifica risolutiva |
|---|---|---|---|
| Filosofia di selezione emergente, niente premio arbitrario al movimento | **CONSISTENT WITH CODE**, con discrepanze | Nessun reward diretto movimento; reward handcrafted fisiologico e `NewGen` esplicito (`pop.cpp:116`, `pops_manager.cpp:277`). Riproduzione forzata sotto 20. | Questi interventi alterano la selezione senza dimostrarne l'invalidità. Separare forzatura/selezione periodica nei dati e confronti controllati; E3/E6. |
| Immobilità dopo evoluzione prolungata | **REQUIRES EXPERIMENT** per l'osservazione; **CONFIRMED IN CODE** per impedimenti specifici | Direzione zero, argmax con parità→0, fase sense→act→think, occupazioni cancellate (`pop.cpp:25,443`; `brain.cpp:157`; `pops_manager.cpp:263`; `grid_world.cpp:37`). | Non noto quali azioni richiedono gli agenti reali o quante falliscono. E0/E1 distinguono richieste, esecuzione e resa. |
| Movimento energeticamente svantaggioso con 0.02/0.02/0.1 | **OUTDATED** per numeri; **REQUIRES EXPERIMENT** per vantaggio netto | Oggi respirazione0.3*eff per substrato, basale0.02/tick, movimento0.01/tentativo circa ogni3 tick (`pop.cpp:16,705,765`; INI:57). | Temperatura, riserve, O2, cap e successi cambiano il bilancio; E2. Non usare confronto dei tre coefficienti come prova. |
| Glucosio diventa diffuso per rigenerazione | **CONFIRMED IN CODE** per rigenerazione spazialmente uniforme; **CONSISTENT WITH CODE** per omogeneizzazione | +1/1000 per ogni cella, indipendente dal campo iniziale (`cell.cpp:196`). | Non conosciuta distribuzione a regime con consumo/fotosintesi. E5 misura varianza, copertura e disponibilità totale equivalente. |
| Movimento assente anche con risorse localizzate | **NOT EVALUABLE** come precedente esperimento; **CONSISTENT WITH CODE** come spiegazione architetturale | Sensori G fuori codifica; auto-assorbimento e direzioni relative ferme sono presenti anche in ambiente a patch (`neuron.h:52`, `pop.cpp:713`). | Mancano configurazione e traiettorie di quelle prove. E1/E3/E5 con stessi genomi e campi archiviati. |
| Temperatura varia ma pressione forse debole | **REQUIRES EXPERIMENT** per efficacia; **CONFIRMED IN CODE** per conseguenze fisiologiche | Efficienza lineare, heat e morte termica; campo cambia lentamente e si allarga (`pop.cpp:190,765,792`; `random_utility.cpp:99`). | Può favorire stazionarietà in siti buoni o migrazione, oppure causare morte prima della riproduzione. E2/E3 stratificati per T e mitocondri. |
| Condizioni di variazione/ereditabilità/selezione | **CONSISTENT WITH CODE**, efficacia **REQUIRES EXPERIMENT** | Genoma→pesi→azioni, clone mutato, soglie di risorse/età; con plasticità l'ereditarietà non è solo genetica (`brain.cpp:49`, `genome.cpp:29`, `pop.cpp:631,635`). | Misurare diversità delle azioni, somiglianza parentale e fitness in common garden; E1/E4/E3. |
| Distanza, prole e correlazione | **ALREADY ADDRESSED** per raccolta parziale; **CONFIRMED IN CODE** per limiti scientifici | Distanza euclidea dei successi e figli nati, Pearson contemporaneo sui vivi (`pop.cpp:590`, `stats.cpp:79–116`). | Nessuna vita completata, controllo età o Spearman. E6 con registri eventi e censura; §11. |
| Similarità genetica e distribuzione linee | **NOT EVALUABLE** per andamento storico; misura attuale **CONFIRMED IN CODE** incompleta | Sensore 0.5; colore da primi/ultimi geni, nessun parent ID (`pop.cpp:299`, `genome.cpp:41`, `pop_snapshot.h:16`). | Colore non identifica parentela. E4/E6 con genealogia e distanza genotipica esplicita. |
| Rete32/16×4/21, genoma100–700, popolazione80, passi300 | **CONSISTENT WITH CODE** per INI; **OUTDATED** se 300 inteso durata di generazione | I numeri corrispondono al file attuale, ma default sono diversi e300 scala AGE/oscillatore. NewGen50000 (`config/popx.ini:8–25,70`; `pop.cpp:400,547`). | Verificare percorso file e configurazione effettiva a runtime prima di confrontare run. |
| Random genes poco funzionali, innovazione rara | **CONSISTENT WITH CODE**, quantità biologicamente utile **REQUIRES EXPERIMENT** | Molti scarti, connessioni sparse ma skip diretti consentiti; sostituzione può riattivare geni (`brain.cpp:61–126`, `genome.cpp:29`). | Il tasso statico di accettazione non è quello di innovazione utile. E1/E4 misurano output, azioni e risposte ambientali. |
| Estinzione, cap e perdita linee | **ALREADY ADDRESSED** per cap e recupero parziale; **REQUIRES EXPERIMENT** per derive/bottleneck | Cap120, forzatura sotto20, mutazione×10, coda limitata; zero non recupera (`common.h:9`, `pops_manager.cpp:112–225`). | Effetti su fitness e deriva ignoti; E6 include nascite forzate, cap, perdite e discendenza. |
| Ricambio continuo e generazioni | **ALREADY ADDRESSED** per ricambio; **OUTDATED** per descrizione esclusivamente continua | Nascite/morti continue più NewGen attivo con eliminazione e riposizionamento (`pops_manager.cpp:119,295`). | Il contatore attuale non misura profondità ereditaria. E6 separa entrambe le scale. |
| Hebbian learning senza Lamarckismo | **CONFIRMED IN CODE** per conflitto con preferenza; **ALREADY ADDRESSED** per plasticità | Learning presente e sempre attivo dopo primo think; INI `hebbian_inheritance=true` esporta i pesi (`INI:35`, `pop.cpp:635`, `brain.cpp:502`). | Non è un difetto nascosto: esiste opzione intenzionale, ma contraddice il vincolo storico. E4 deve distinguere genoma originario e acquisito; discutere politica prima di cambiare. |
| Clamp dei pesi compatibile con geni | **ALREADY ADDRESSED** per scala/clamp; **CONSISTENT WITH CODE** per perdita di fedeltà | Scala8192 coerente, clamp int16 esplicito; duplicati sommati e poi accorpati, +4 quantizzato (`gene.cpp:15`, `brain.cpp:126,520`). | Misurare deviazione rete genitore→clone prima di mutazione; E4. |
| Prestazioni su mappe grandi, riproduzione e feromoni | **ALREADY ADDRESSED** parzialmente; colli reali **REQUIRES EXPERIMENT** | Matrici sparse, due fasi parallele, FIFO/budget, genoma appreso ricostruito lazily, rendering batch (`brain.cpp:488`, `pops_manager.cpp:159`, `renderer.cpp:33`). Quattro campi feromone/cella, non molti oggetti. | Nessun benchmark: E2/E6 possono cronometrare tick senza rendering; misure correnti da correggere, §12/16. |
| Risultati riproducibili a seed fisso | **CONFIRMED IN CODE** come capacità mancante | Nessun seed centrale, `random_device` per moltissime istanze e `rand()` separato (`random_utility.cpp:5`, `pops_manager.cpp:22`). | E0 deterministico può usare fixture; E3–E6 richiedono futura strumentazione autorizzata. |
| Cooperazione/comunicazione/linee complesse future | **NOT EVALUABLE** come efficacia emergente | Segnali food/danger/mate/home hanno solo nomi e valori; non c'è comportamento cooperativo prescritto. Kill/kin sensor non operativi nell'assetto corrente. | Non sono richieste di feature. Prima validare comportamento adattivo semplice; E1–E4. |

## 8. Problemi verificati e priorità

Le conseguenze dirette indicate qui derivano dalle istruzioni del codice. Non
sono misure della frequenza o della gravità durante run reali.

### 8.1 Cinque priorità principali

| Priorità | Problema verificato | Evidenza e conseguenza diretta | Effetto evolutivo ancora incerto |
|---|---|---|---|
| 1 | Invariante popolazione–griglia violato all'avvio | `Simulator::init` spawn prima del reset occupanti (`simulator.cpp:13`, `grid_world.cpp:37`). A fine init i vivi del manager non sono registrati; renderer/densità non li vedono. Movimento può registrare l'agente in seguito. | Quanto sovrapposizioni, visualizzazione e riproduzione alterino i risultati storici. |
| 2 | Catena percezione–decisione–azione e bootstrap del movimento | Rotazione S→A→T; parità→MOVE_FORWARD; direzione iniziale0 (`pops_manager.cpp:263`, `brain.cpp:157`, `pop.cpp:25,443`). | Quanto prevalgano azioni stale/costanti o relative incapaci di partire rispetto a reti che richiedono movimento assoluto. |
| 3 | Bilancio risorse/energia e fitness non corrisponde ai contatori | Rilascio risorse senza prelievo interno; costo riproduttivo solo contabile; dotazione energetica del neonato creata, CO2 doppio (`pop.cpp:516,665,781,820`). | Se questi meccanismi favoriscano riproduzione senza ricerca, autotrofia o strategie di rilascio. |
| 4 | Ricerca direzionale del glucosio non esprimibile attraverso i sensori dedicati | Indici33–36, codifica≤31, input32 (`neuron.h:52`, `gene.h:38`, `config/popx.ini:8`). | È un vincolo a questa strategia, non prova che nessuna strategia adattiva possa esistere: restano temperatura, spazio, segnali e movimento non guidato. |
| 5 | Osservabilità evolutiva insufficiente e riepiloghi fuorvianti | Vivi soltanto; prole/distanza non normalizzate; denominatori tempi e cumulati errati (`stats.cpp:46,79,170`). | I risultati apparenti potrebbero mascherare o simulare adattamento. Servono eventi individuali, controlli e metriche validate. |

### 8.2 Altri difetti e discrepanze verificati

| ID | Evidenza | Valutazione tecnica |
|---|---|---|
| F1 | `grid_world.cpp:74–85` | `move_entity` cancella la cella di partenza senza verificare identità dell'occupante. Con l'invariante già rotto può cancellare un altro agente registrato; non può ricostruire coerenza globale. |
| F2 | `cell.cpp:103–109` | `give_glucose` aggiunge `min(amount, glucosio_attuale)`: a cella vuota aggiunge zero, a cella parzialmente piena meno del richiesto; ritorno può superare l'aumento realmente accettato al cap. Rende scorretti restituzione alla morte e trasferimenti. |
| F3 | `pop.cpp:516–540` | `LEAVE_GLUCOSE/H2O/CALCIUM` deposita10 senza decremento del mittente. Acqua e calcio possono essere creati; glucosio solo se la cella già ne contiene. |
| F4 | `pop.cpp:781–788` | CO2 respiratorio accreditato sia alla riserva interna sia alla cella. Non è un trasferimento conservativo. |
| F5 | `pop.cpp:820`, `pops_manager.cpp:238` | “Reproduction energy loss” registra0.3 ma non altera `energy_` o `energy_cost_`. Nessuna spesa energetica riproduttiva reale oltre donazione materiale. |
| F6 | `pop.cpp:705`; `pop.h:375` | Limite lipidico ereditario non limita `lipids_`; sensitiveness non controlla probe_dist. Questi tratti non hanno l'effetto suggerito dai commenti. |
| F7 | `pops_manager.cpp:316–318` | Se vivi<min_survivors (es.1–7 con minimo8), `std::clamp(survivors_n, 8, alive.size())` riceve limite inferiore maggiore del superiore: precondizione violata. Difetto condizionale verificato; nessun crash riprodotto. |
| F8 | `pops_manager.cpp:322–348` | NewGen elimina senza `die`: niente rilascio di risorse. Fallimento di riposizionamento di un superstite lo perde senza contarlo tra gli abbattuti. Richiede scenario specifico per manifestarsi. |
| F9 | `simulator.cpp:12`; `grid_world.cpp:15`; `cell.cpp:9` | Restart reimpiega celle esistenti: resetta G/acqua/T/elevazione/occupanti, non O2/CO2/lipidi/calcio, fasi regen, feromoni, contatori ambiente o contatore ID globale. Non è una nuova replica indipendente. |
| F10 | `config.cpp:269–277,377–384,198` | Nessuna validazione min_genome≤max_genome, alpha_min≤alpha_max o alpha≤1; conversioni intere possono lanciare su input enormi. INI corrente non viola tali intervalli. Rischio per configurazioni future. |
| F11 | `CMakeLists.txt:23`, `cmake/Options.cmake:4` | `UTESTS_ENABLED` diverso da `ENABLE_UTESTS`; directory utests assente. |
| F12 | `cmake/CodeFormatting.cmake:11–15`, `Options.cmake:6` | `format ALL` modifica sorgenti a ogni build predefinito se clang-format presente; opzione dichiarata non consultata. |
| F13 | `cell.cpp:178–181`, `renderer.cpp:228–239` | Payload feromoni a=DANGER/b=FOOD, ma renderer usa a con flag food e b con flag danger: visualizzazione scambiata; sensori usano i tipi corretti. |
| F14 | `imgui_layer.cpp:549–552,563–590` | GUI inferisce strato output dal massimo destinatario tra soli128 archi attivi; se non include output può etichettare hidden come output. Percorsi mostrati scelgono migliori predecessori, non tutti i cammini o causalità. |

### 8.3 Eredità acquisita: conflitto progettuale distinto dai bug

L'INI abilita esplicitamente il ramo lamarckiano implementato. È una discrepanza
**verificata rispetto alla preferenza fornita**, non evidenza di implementazione
accidentale: nome, commento e percorso concordano. Va scelta e documentata la
politica di ricerca; durante questo audit non è stato disabilitato nulla.
La conservazione dei geni scartati/silenti e il clamp sono soluzioni presenti,
ma non garantiscono ereditabilità del comportamento utile, §5.3/§13.

## 9. Ipotesi non verificate

| Ipotesi | Elementi a favore nel codice | Cosa manca |
|---|---|---|
| Reti costanti/argmax e bootstrap spiegano gran parte dell'immobilità | Sigmoide a0.5, tie→0, relative iniziali ferme. | Distribuzione degli output, azioni richieste/successi e assay comune dei genomi; E1. |
| Sosta favorita ecologicamente | Assorbimento automatico, riserve iniziali, rigenerazione globale e possibile fotosintesi locale; movimento paga tentativi. | Vantaggio di fitness su ambienti controllati, non solo energia; E2/E3/E5. |
| Vantaggio di movimento nascosto dalla saturazione | E capped400, soglia riproduttiva50, cap120 e vicinato bloccato. | Energia scartata, idoneità, coda, spazio e figli per strategia; E2/E3. |
| Tratti fisiologici dominano comportamento neurale | Mitocondri/cloroplasti ereditati, forte termica, sensing di glucosio assente. | Confronti a `PhyT` uguale e poi variabile; E3/E4. |
| Plasticità sovrascrive differenze genetiche | Aggiornamenti0.06 ogni tick con attivazioni riutilizzate e clamp. | Traiettorie pesi/azioni, saturazione e confronto al medesimo tempo di vita; E4. |
| Mutazione compromette architetture utili | Sostituzione integrale0.05/gene,0.5 nel regime forzato. | Distribuzione effetti e fedeltà comportamentale; E4. Tasso elevato non equivale a prova di fallimento. |
| Cap/FIFO causano deriva e perdita di linee | 120 vivi,64 candidati, ordine vettore, riproduzione forzata. | Genealogie, fitness per opportunità, repliche; E6. |
| Termica impedisce maturazione prima dei1001 tick | E iniziale100, eff iniziale0, limiti morte10/75. | Distribuzione mortalità per causa, posizione, genoma e tratti; E2/E6. |

## 10. Meccanismi già presenti o descrizioni non più applicabili

Sono già presenti ownership senza cicli e `shared_from_this`, matrici sparse,
connessioni skip, rigetto indici senza bias da modulo, conservazione dei geni
scartati nell'export appreso, sigmoid numericamente stabile, clamp dei pesi e
protezione del reward non finito, statistiche distanza/prole/Pearson, cap di
popolazione, throttling/coda e gruppi paralleli. Questo non certifica l'efficacia
scientifica del loro insieme.

Le cifre energetiche storiche non sono attuali. Non ci sono oggetti autonomi di
feromone o diffusione; la riproduzione effettiva non è sessuale benché l'opzione
dica true. La durata300 non definisce ricambio genetico. La plasticità non è
soltanto proposta: è implementata, con ereditarietà acquisita configurata true.

`README_concept.md` è in parte coerente sull'ownership, ma descrive `Slice`, mentre
il mondo usa `Cell`, e un flusso sense→think→act per ciascun agente che non è il
flusso corrente. Le interfacce citano reload a ogni generazione (`i_config.h:6`)
e possibile -1 se nessun output “fires” (`i_brain.h:40`), senza corrispondenza
al percorso applicativo e alla rete sigmoidale con output non vuoto. Nessun file
preesistente è stato riscritto per eliminare queste discrepanze.

## 11. Statistiche evolutive esistenti e significato

| Misura | Definizione attuale | Interpretazione e limite |
|---|---|---|
| Vivi/morti/nascite | Vivi=vettore manager; morti cumulativi rimossi; nascite includono fondatori; morti NewGen contati. | Non verificano occupazioni; nascite di fondazione non sono successo riproduttivo. |
| Età | +1 ogni update, snapshot vivi. | Età corrente, non lifespan completato. Cause di morte esistono nel Pop ma non sono archiviate/esportate. |
| Lifetime distance | Somma euclidea solo su `try_move` riusciti. | Nessun tentativo fallito; salti contribuiscono; traslocazioni NewGen non contribuiscono. Non misura automaticamente esplorazione utile. |
| Offspring | Incremento solo al successo di spawn del figlio. | Numero cumulativo di figli prodotti, non loro sopravvivenza/fertilità, né fitness normalizzata. |
| Contabilità E | Totali individuali di movimento, basale, termoregolazione, respirazione, riproduzione. | Movimento include fallimenti; respirazione lorda prima cap; “riproduzione” nominale non detratta. Assorbimenti e costi materiali non separati. |
| Media E/costi/distanza/prole | Media dei cumulati dei vivi nel campione; sessione media queste medie. | Non è flusso/tick, totale ecosistema o media su vite concluse. Cambia quando muoiono soggetti; survivor bias. Energia corrente disponibile nello snapshot, non come serie individuale JSON. |
| Pearson offspring-distance | Covarianza da momenti grezzi dei vivi /sqrt(varX*varY). Se denominatore≤1e-12 ritorna0. | Non distinguibile “indefinito” da correlazione zero; nessun controllo età o censura; cancellazione numerica possibile. Nome `corr_offspring_lifetime` significa distanza, non età. |
| Temperatura media / abs delta optimum | Media dei vivi nel campione. | Non incorpora morti per caldo/freddo e può mascherare estremi individuali. |
| Connessioni “useful” | `abs(w*pre*post) >= 0.05` sulle ultime attivazioni (`brain.cpp:274`). | Attività locale, non effetto su scelta o fitness; non garantisce cammino input→azione. |
| Colore genetico | Bit da lunghezza e primo/ultimo gene; ignora tutti i pesi e quasi tutti i geni (`genome.cpp:41`). | Indicatore visuale con collisioni; non diversità genetica, similarità o genealogia. |
| Risorse mondo | Somma riserve nelle celle (`grid_world.cpp:219`). | Esclude agenti e materia scomparsa/creata; non bilancio globale conservativo. |
| Tassi birth/death/sec | Differenze tra campioni divise per dt_tick/100, finestra10 campioni. | Secondi simulati, non throughput reale; mescola morti ecologiche e NewGen. |

Pearson è implementato; Spearman, correlazioni parziali, movimento/età,
figli/tempo riproduttivo, lignaggi e diversità genotipica non risultano implementati.
Il visualizzatore Python calcola raggiungibilità topologica da sensori a output
(`scripts/plot_net.py:220`), distinta dalla metrica di attività C++: nessuna
prova dell'influenza dell'arco sul comportamento. Richiede matplotlib/Qt5,
networkx, numpy e scipy; non è un test automatico e non è stato avviato.

## 12. Misure mancanti ed errori di osservabilità

Per distinguere i colli di bottiglia servono: logits/attivazioni di tutti gli
output, margine argmax/parità, ID azione e frequenze, tentativi e cause di fallimento,
displacement, posizione/termica prima e dopo, uptake per risorsa, substrati respirati,
fotosintesi, energia eliminata dal cap, idoneità riproduttiva, nascite normali/forzate,
attese/fallimenti/cap, parent ID, genoma originale/trasmesso, eventi di morte con
causa e statistiche lifetime. Nessuno di questi registri completi esiste oggi.

Gli snapshot ricchi vengono letti da GUI e Stats, ma il JSON conserva soltanto
aggregati. Manca anche metadata sperimentale (commit/config/seed, ambiente,
thread/build); un grafico o un JSON isolato non basta alla riproduzione di un run.

Problemi verificati nei riepiloghi (`stats.cpp:8,46,155–188`; `popx_app.cpp:40–56,134`):

- `total_frame_us_` somma **ogni frame**, ma `avg_frame_us` divide per numero
  di **campioni tick**. FPS medio conseguente scorretto anche prima del rollover.
- Quando storico≥50000, si elimina il quarto più vecchio ma i cumulati di sessione
  non vengono decrementati e si dividono per storico rimasto: medie tick, rate e
  cumulati dei vivi risultano gonfiate. Manca contatore campioni totali dedicato.
- “Tick medio” campiona solo ogni200-esimo tick. Refresh temperatura e NewGen
  hanno intervalli multipli di200 e possono essere sovracampionati; non rappresenta
  costo medio di tutti i tick. Tempo snapshot/Stats è escluso dal tick cronometrato.
- Il frame cronometrato include tick, GUI, display ed eventuale attesa interna
  SFML; esclude polling iniziale e sleep finale dell'app. Non è tempo rendering
  puro o intervallo completo tra presentazioni.
- `total_ticks` e conteggi finali vengono dall'ultimo campione: gli ultimi0–199
  tick o un'intera sessione breve possono mancare, anche all'estinzione.
- `StatsClock` è `steady_clock`, ma timestamp JSON sono etichettati `*_time_ms`
  con conversione `time_since_epoch`; non sono epoch UTC di calendario. La durata
  monotona resta appropriata; la rappresentazione temporale è ambigua.
- `begin_session` non resetta `last_frame_dur_us_`: possibile primo record dopo
  restart con durata frame precedente. Stato mondo riutilizzato, come F9.
- Media di Pearson nel tempo non è Pearson aggregato né prova di aumento di fitness;
  campioni degli stessi individui non sono repliche indipendenti.

Misure scientifiche consigliate: distanza/tick vivo (con zero-age gestito),
spostamenti riusciti/tentativi, area esplorata, uptake e delta netto/tick,
figli per tempo riproduttivamente idoneo, discendenti sopravvissuti a un orizzonte
fisso. Analisi in coorti a età comune o regressione/stratificazione per età,
`PhyT`, habitat ed esposizione alla riproduzione; completare vite e trattare i
vivi alla fine come censurati. Spearman gestisce ranghi ma non elimina da solo
confondimento, censura o causalità. Definire una misura genetica che usi davvero
struttura/pesi, separando tratti, plasticità e genealogia.

## 13. Valutazione del collo di bottiglia evolutivo

### A. Generazione della variazione comportamentale

**Capacità strutturale presente.** Un gene valido sensor→output può cambiare
l'argmax senza attraversare tutti gli hidden. Geni diversi possono quindi
produrre output e azioni diversi a input identici. Anche archi hidden→output da
neuroni costanti possono cambiare l'azione. La mutazione può cambiare estremi,
peso e stato attivo del gene, ma lo fa con sostituzione casuale completa.

**Vincoli verificati.** Molti geni non vengono decodificati; output differenti
non implicano argmax differente; argmax diverso non implica spostamento; le prime
azioni relative non iniziano movimento. La rete non legge i sensori dedicati al
glucosio, né fame/energia/temperatura corporea; risponde a proxy ecologici, quando
cablate. Fasi e occupazioni alterano l'accoppiamento con il mondo.

**Non stabilito:** proporzione di genomi con risposta ambientale, diversità delle
azioni effettive, frequenza di innovazioni utili. E1 distingue varianza degli
output, disaccordo delle azioni e successi per genome/fixture; E4 misura effetti
delle mutazioni. La formula8.54% non dimostra che il collo sia solo nella variazione.

### B. Ereditabilità dei caratteri comportamentali

**Capacità meccanica presente.** Con eredità Hebb disabilitata il genoma è copiato
prima della mutazione; con rate0 viene copiato esattamente, senza crossover.
`PhyT` è trasmesso separatamente. Con eredità attiva vengono trasmessi pesi
acquisiti quantizzati, più geni inattivi, non il genoma originario esatto.

**Calcolo statico**, nel ramo originale senza ricostruzione: per un insieme di k
geni rilevanti, probabilità di nessuna sostituzione `(1-p)^k` e sostituzioni attese
`L*p` per lunghezza L. Per p=0.05 e L100–700:5–35 sostituzioni attese;
in regime forzato p=0.5:50–350. Non si conosce k, ridondanza o tolleranza del
comportamento: questi conteggi non dimostrano perdita di adattamenti. Il genoma
appreso può avere L diverso a causa di accorpamento duplicati.

**Limiti:** stesso genoma non garantisce identico comportamento se differiscono
lazyness casuale, gruppi iniziali, tratti, contesto, input RANDOM, plasticità e
stato interno. Il neonato riparte da energia/T/direzione/oscillatore iniziali,
non dallo stato adulto. E4 deve distinguere fedeltà della rete istantanea da
fedeltà della risposta comportamentale in un contesto comune.

### C. Selezione dei caratteri vantaggiosi

**Mappatura possibile:** azioni possono cambiare posizione, uptake, temperatura,
energia, sopravvivenza oltre1000 tick e glucosio oltre20, quindi numero di figli.
La riproduzione produce costi materiali e richiede spazio; termica e O2 possono
rendere il movimento utile. Assorbimento automatico non elimina in generale il
vantaggio dell'esplorazione: una cella esaurita non fornisce5 unità per sempre.

**Pressioni reali intrecciate:** coda/cap, ordine, abbattimento periodico,
riposizionamento, nascite forzate e dotazione energetica del figlio. Saturazione
energetica e assenza di costo riproduttivo reale possono ridurre la relazione
tra guadagno extra e prole; spazio libero e sopravvivenza potrebbero comunque
creare differenze. I trasferimenti errati possono offrire percorsi energetici
non intesi. Non è stata misurata la forza di nessuna di queste pressioni.

**Conclusione limitata:** PopX è in grado di implementare variazione, trasmissione
e conseguenze ecologiche differenziali. Non è validato come sistema che seleziona
in modo affidabile adattamenti semplici. Il primo ostacolo verificato è la
correttezza/osservabilità della catena esecutiva; quale dei tre livelli costituisca
il collo biologico principale resta **REQUIRES EXPERIMENT**.

### Movimento: quattro casi distinti

| Caso | Condizione osservabile da misurare | Evidenza statica pertinente | Esperimento che lo distingue |
|---|---|---|---|
| 1. Rete non richiede movimento | Azioni0–7 rare/assenti a sensori controllati; output/margine argmax noti. | Argmax, archi scartati, plasticità; non si conoscono richieste reali. | E1: assay reti con stessi input e istogrammi prima/dopo learning. |
| 2. Rete richiede, ma esecuzione impedisce | Azioni0–7 frequenti, displacement0; motivi registrati. | Relative con direzione0, occupazione/confini/pendenza, conversione coordinate, ordine fasi. | E0/E1: cardinali/relative su griglia vuota piatta e ostacoli noti. |
| 3. Movimento avviene senza vantaggio di accesso | Successi>0, uptake/respirazione non aumentano a pari contesto. | Rigenerazione globale, risorse sotto Ecap, cella già fertile, sensing G assente. | E2/E5: uptake, esaurimento locale, esposizione termica e bilancio per strategia. |
| 4. Risorse migliorano senza vantaggio riproduttivo | Uptake/netto aumentano ma figli o discendenti equivalenti. | Soglie, età, cap, FIFO, spazio, NewGen e forzatura. | E3: idoneità/opportunità/esiti per evento; cap non vincolante e coorti comparabili. |

Un agente può attraversare più casi nel tempo. L'assenza visiva di spostamento
non identifica nessuno dei quattro, particolarmente con le occupazioni cancellate.

## 14. Strategia minima di validazione riproducibile

Tutti gli esperimenti qui sono **proposte non eseguite**. Non autorizzano modifiche
al simulatore, nuova strumentazione o cambi di INI in questa sessione. Prima di
implementarli concordare uno scope tecnico specifico. Le fixture controllate e le
strategie prescritte sono strumenti diagnostici, non reward o evoluzione artificiale.

### Protocollo comune

Registrare revisione, diff eventuale, build/compilatore/librerie, configurazione
risolta e costanti, ambiente completo, genomi/tratti iniziali, stato della
plasticità e dei generatori, ordine di aggiornamento e thread effettivi. Oggi
non è possibile imporre una ripetizione stocastica a un singolo seed. Per verifiche
pure usare genomi/input/mondo deterministici; per ecologia e mutazione è necessario
un futuro controllo di tutte le RNG o replay di eventi/stati, incluso `rand()`.

Una run riavviata dalla GUI non costituisce una replica indipendente: usare
oggetti nuovi e ambiente archiviato. Identificare ogni replica e trattarla come
unità statistica; gli agenti dello stesso ecosistema non sono repliche indipendenti.
Iniziare con pilot brevi, budget di tick/tempo esplicito e arresto deterministico,
poi dimensionare repliche alla precisione richiesta. Riportare campioni, dispersione,
intervalli d'incertezza e fallimenti, senza scegliere solo seed favorevoli.

### E0 — Controllo degli invarianti di inizializzazione e fase (prima priorità)

- **Ipotesi:** parte dell'apparente immobilità/assenza di interazioni deriva da
  registrazione spaziale e ordine di fase, indipendentemente dalla selezione.
- **File:** `simulator.cpp:12`, `grid_world.cpp:15,74,105`,
  `pops_manager.cpp:16,58,263`, `pop.cpp:107,419,433`.
- **Misure:** dopo init confrontare elenco vivi/posizioni/ID con occupanti;
  verificare una corrispondenza unica per vivo; tracciare fase, timestamp input,
  think e act di uno stesso agente per9 tick; posizione e occupante prima/dopo.
- **Controlli:** niente rendering, niente NewGen/riproduzione nel breve intervallo;
  prima verificare l'avvio reale, poi fixture di un agente/ambiente noto per
  isolare la fase. Non correggere automaticamente invarianti nel test.
- **Esiti concorrenti:** se occupanti mancano o sense→act usa decisioni precedenti,
  il percorso tecnico è compromesso prima della selezione; se invarianti e catena
  sono coerenti in una revisione futura, procedere a E1/E2.
- **Criterio:** asserzioni su identità e ordine, non valutazione qualitativa del video.
- **Riproducibilità:** l'asserzione di occupazione post-init non dipende dal seed;
  registrare genoma/input/fase esatta per la microtraccia.
- **Strumentazione:** nessuna suite attuale offre questi controlli; servirà un
  harness mirato o ispezione debugger dopo autorizzazione. Non implementato.

### E1 — Raggiungibilità neurale ed esecuzione delle azioni

- **Ipotesi:** i geni possono richiedere movimento, ma tie, bootstrap o gate
  esecutivi lo impediscono; alternativamente molte reti non lo richiedono.
- **File:** `gene.cpp:11`, `brain.cpp:49,131`, `neuron.h:61`,
  `pop.cpp:215,419,433,575`, `grid_world.cpp:57,74`.
- **Misure:** output completi, margine/parità, azione richiesta, destinazione,
  successo, displacement e motivo di rifiuto; numero di geni accettati/silenti/
  scartati e cammini input→output. Confrontare output di genomi diversi sugli
  stessi input, separando differenze numeriche da scelte argmax e azioni eseguite.
- **Controlli:** rete vuota; gene sensor→MOVE_EAST positivo con input noto>0;
  analogo verso FORWARD; rete verso azione risorsa; griglia piatta libera,
  direzione zero e nonzero; poi ostacolo, confine e salita controllati. Per
  campione di genomi iniziali usare stessi vettori sensoriali e stesso `PhyT`/
  stato; prima del learning e poi dopo esposizione identica.
- **Esiti concorrenti:** output diversi ma azione uguale identifica compressione
  argmax; richiesta cardinale corretta con successo dimostra raggiungibilità;
  richieste relative senza displacement isolano bootstrap; rifiuti cardinale
  nella fixture libera identificano esecuzione/griglia. Nessun confronto di fitness.
- **Criterio:** capacità di produrre azioni e spostamenti prescritti; distribuzione
  di azioni/input reali resta una misura separata. Non inferire che tutte le
  reti casuali siano funzionali dal successo del gene di controllo.
- **Riproducibilità:** genomi/input espliciti, niente RANDOM per controlli,
  stesso ordine/fase; archiviare ogni genoma del campione.
- **Strumentazione:** feed_forward già ritorna indice, ma output, rifiuti e
  `last_action_` non sono esposti nei log/snapshot; necessaria futura strumentazione.

### E2 — Bilancio energetico e termico

- **Ipotesi:** il movimento può migliorare energia quando consente consumo di
  substrati in più; alternativamente il costo o la termica annullano il vantaggio.
- **File:** `pop.cpp:16,433,705–813`, `cell.cpp:94–165`, `stats.cpp:42`.
- **Misure:** E/T all'inizio/fine tick, uptake automatico e da azioni, glucosio/O2
  consumati, fotosintesi, tutte le spese effettive, surplus scartato al cap,
  riserve/risorse mondo e distanza. Verificare residuo della formula energetica
  contro variazione osservata; confrontare contatore nominale riproduzione con E.
- **Controlli:** stesso genoma diagnostico/tratti/T/riserve, ambiente uniforme
  fertile e poi patch che esaurisce la cella iniziale; controlli efficienza0,
  optimum37 ed energia al cap; strategia ferma e cardinale con frequenza nota;
  prove con successo e tentativo bloccato. Prima senza riproduzione per isolare
  budget, poi un singolo evento riproduttivo controllato.
- **Esiti concorrenti:** movimento senza incremento uptake→caso3; più substrati
  ma cap/eff0→energia non utile; incremento netto oltre costo→possibilità di
  vantaggio, da testare in E3. Residuo nonzero→contabilità/trasferimento da chiarire.
- **Criterio:** guadagno netto per tick e per tentativo comparato con stazionario,
  con condizioni termiche esplicite; nessun vantaggio deciso dai soli coefficienti.
- **Riproducibilità:** ambiente/input archiviati; fotosintesi disabilitata nel
  controllo isolato o replay di RNG, poi variante fisiologica esplicita.
- **Strumentazione:** i cinque cumulati esistenti non bastano; servono eventi
  uptake, cap e reali cambiamenti delle riserve. Nessuna esecuzione in questo audit.

### E3 — Vantaggio selettivo di strategie diverse

- **Ipotesi:** un vantaggio di accesso/energia si traduce in discendenti; alternativa:
  età, spazio, cap/FIFO/forzatura o fisiologia interrompono quel legame.
- **File:** `pop.cpp:631–681,705`, `pops_manager.cpp:127–261,295`.
- **Misure:** sopravvivenza fino all'età riproduttiva, figli e loro sopravvivenza
  a orizzonte fisso, tempo idoneo, candidati, attese, fallimenti spaziali/cap,
  nascite forzate, energie/riserve e posizione di ogni evento.
- **Controlli:** strategie ferme, movimento casuale e percorso deterministico
  verso risorse in common garden, inizialmente in mondi separati equivalenti per
  evitare interazione; abbinare `PhyT`, riserve, età e temperatura. Variare posizioni
  bilanciate rispetto alle patch; disponibilità di spazio e cap non vincolanti
  nel controllo principale. Forzatura e NewGen esclusi dal controllo causale
  tramite futura fixture, poi riportati in variante ecologica originale.
  Nessun bonus di movimento, nessun training delle strategie a reward aggiuntivi.
- **Esiti concorrenti:** più E e più discendenti→mappatura selettiva dimostrata
  in quelle condizioni; più E senza figli→caso4 con gate identificabile; nessun
  accesso extra→tornare a E1/E2/E5. Successo di strategie prescritte non dimostra
  che evolvano spontaneamente, ma verifica che l'ambiente possa selezionarle.
- **Criterio:** differenza riproduttiva ripetibile a esposizione comparabile,
  controlli per lifespan, spazio e tratti; distinguere figli nati da fitness
  multigenerazionale. Dopo controlli, eventuale competizione nello stesso mondo.
- **Riproducibilità:** repliche a fixture/semi abbinati, durata uguale, ordine
  bilanciato; documentare ogni differenza rispetto all'ecosistema configurato.
- **Strumentazione:** registri individuali/eventi e controllo seed mancanti;
  nessun esperimento eseguibile fedelmente dai soli aggregati attuali.

### E4 — Mutazione, trasmissione e fedeltà comportamentale

- **Ipotesi:** caratteristiche utili sopravvivono alla trasmissione; alternative:
  sostituzione integrale, ricostruzione pesi o stato non genetico le disperdono.
- **File:** `genome.cpp:29`, `brain.cpp:368,494–541`, `pop.cpp:635–662`.
- **Misure:** lista geni e archi prima/dopo, duplicati, lunghezza, pesi, distanza
  genotipica, numero di sostituzioni, output e azioni sullo stesso pannello di
  input; concordanza parentale nei successi di movimento nella stessa fixture.
- **Controlli:** trasmissione senza mutazione con eredità acquisita esclusa;
  separatamente export appreso prima/dopo plasticità; rate0.05 e0.5; rete con
  duplicati e pesi oltre range, geni silenti/scartati, rete semplice funzionale.
  Stesso `PhyT`, lazyness, fase, direzione e storia sensoriale nell'assay dei
  cervelli; distinto assay con normale stato neonatale per misurare sviluppo.
- **Esiti concorrenti:** output diverso già a rate0 nell'export appreso→perdita
  da ricostruzione/quantizzazione; geni fedeli ma comportamento diverso→stato/
  plasticità; novità di azione con buona concordanza residua→variazione ereditaria
  utile possibile. La percentuale di geni cambiati non sostituisce il test di azione.
- **Criterio:** distribuzione della fedeltà/novità, non solo media; successi
  ecologici dei figli ancora da valutare in E3/E6.
- **Riproducibilità:** corpus esplicito di genomi/input e seed/stati per mutazioni;
  politiche ereditarie sempre registrate, nessuna modifica silenziosa dell'INI.
- **Strumentazione:** API Genome/Brain utili per harness, ma mancano esposizione
  genoma originario/genitori, seed e vettori output nel percorso app. Da autorizzare.

### E5 — Distribuzione spaziale delle risorse

- **Ipotesi:** rigenerazione uniforme elimina differenze di accesso; alternativa:
  patch persistono ma sensing o azioni impediscono di sfruttarle.
- **File:** `grid_world.cpp:15`, `cell.cpp:196–214`, `pop.cpp:376,705`, `common.h:26`.
- **Misure:** totale glucosio/O2, varianza spaziale, frazione celle con risorse,
  gradiente a raggio4, esaurimento per cella, uptake/energia per strategia;
  misurare anche T e disponibilità acqua/CO2.
- **Controlli:** distribuzione uniforme e patch con stesso totale iniziale,
  stessi O2, termica, tratti e genomi; stesso budget **effettivo** di rigenerazione
  dopo i cap, stessa densità. Prima confronto campi senza agenti per isolare
  rigenerazione, poi con medesime strategie/popolazioni. Variante senza regen
  distinta e documentata, non parametro modificato in questa sessione.
- **Esiti concorrenti:** perdita di contrasto nelle patch e riduzione del vantaggio
  di movimento→omogeneizzazione rilevante; patch persistenti con nessun movimento
  richiesto→architettura/controllo; patch sfruttate ma nessuna prole extra→E3.
- **Criterio:** confronto a totale e disponibilità reali comparabili, non soltanto
  stessa somma iniziale: cap e consumo possono alterare i totali durante il run.
- **Riproducibilità:** matrici ambiente o parametri RBF e fasi regen archiviati;
  stessi seed/eventi e allocazioni per le varianti.
- **Strumentazione:** statistiche attuali hanno solo totali; serve export dei
  campi/gradienti e override ambiente di fixture, entrambi non realizzati qui.

### E6 — Trend evolutivi e persistenza delle linee

- **Ipotesi:** aumentano caratteristiche adattive ereditarie e successo delle
  relative linee; alternativa: sola plasticità, survivor bias, drift o selezione
  artificiale periodica spiegano i trend.
- **File:** `pops_manager.cpp:385,295`, `stats.cpp`, `pop_snapshot.h`,
  `popx_app.cpp:51`, `genome.cpp`, `random_utility.cpp:5`.
- **Misure:** azioni/tentativi/successi e distanza per tempo vivo, uptake/netto,
  discendenti normalizzati per esposizione, lifespan/cause morte, diversità
  strutturale e funzionale, parent ID/profondità genealogica, estinzione linee,
  cap/coda/forzatura/NewGen. Assay common garden dei genomi a intervalli per
  distinguere adattamento genetico da learning e cambi ambientali.
- **Controlli:** configurazione congelata, ambiente comparabile, coorti e seed
  indipendenti; controllo demografico neutrale per stimare deriva separato dalle
  run evolutive; confronto politiche ereditarie solo dopo decisione esplicita.
  NewGen e traslocazioni annotate, non interpretate come movimento spontaneo.
- **Esiti concorrenti:** miglioramento nei genomi trasferiti a condizioni comuni
  e maggiore fitness dei discendenti sostiene adattamento; miglioramento solo
  negli adulti suggerisce plasticità; aumento correlazione non controllata senza
  vantaggio comune può essere confondimento o selezione dei superstiti.
- **Criterio:** tendenze replicabili e fitness definita prima dell'esperimento;
  fitness e diversità possono cambiare indipendentemente. Nessuna soglia arbitraria
  di “intelligenza” o successo basata sul solo video.
- **Riproducibilità:** seed completo/replay, ambiente e metadata; dimensionare
  durata e numero repliche dopo i pilot. È la prova più costosa: rinviarla finché
  E0–E4 e metriche non sono affidabili; nessuna run lunga durante l'audit.
- **Strumentazione:** logger eventi duraturo, assay e analisi offline necessari;
  JSON attuale e file brain cancellati alla morte non bastano.

**Sequenza minima:** E0+controlli sintetici di E1, poi E2 e fedeltà a mutazione0
in E4; soltanto dopo, confronto E3 in due ambienti E5. E6 è validazione finale,
non primo passo. Il confronto storico non richiede introdurre nuovi sensori,
reward o meccaniche prima di questi controlli.

## 15. Roadmap prioritaria proposta (nessuna implementazione autorizzata qui)

| Ordine | Lavoro successivo da concordare | Condizione per procedere |
|---|---|---|
| P0 | Microverifica inizializzazione/fasi e catena movimento E0/E1. | Identificare deterministicamente se rete richiede e agente si sposta; popolazione/griglia coerenti o difetto dimostrato. |
| P1 | Correzioni mirate degli invarianti e trasferimenti confermati, con test brevi; chiarire costo riproduttivo reale e dotazioni di nascita. | Scope autorizzato, contabilità E/materiale verificata, nessun cambiamento biologico implicito. |
| P2 | Decidere politica di plasticità/eredità e criteri di NewGen/forzatura; rendere esplicita configurazione effettiva, RNG/replay e modalità di prova breve. | Rispetto della preferenza non lamarckiana oppure deviazione sperimentale documentata. |
| P3 | Logging individuale, eventi morti/genealogie, ledger e metriche corrette; validare denominatori/timestamp/censura. | Dati sufficienti per distinguere i quattro casi di movimento e i tre livelli evolutivi. |
| P4 | E2/E4 ed E3/E5 controllati, poi E6. | Dimostrare prima che una strategia utile sia esprimibile, trasmissibile e premiata ecologicamente. |
| P5 | Solo se misure individuano il collo: valutare sensing G, mutazione, ambiente o selezione; profilare costo che limita esperimenti. | Discussione dei tradeoff, prove prima/dopo e nessun reward gratuito di movimento. |
| P6 | Comunicazione complessa, kin recognition, cooperazione o altre direzioni biologiche. | Adattamento semplice validato; queste non sono richieste attuali. |

Questa roadmap non prescrive automaticamente modifiche: specie costi, nascite,
forzatura e learning sono regole ecologiche che richiedono una scelta scientifica,
oltre a correttezza tecnica. I problemi di registrazione e contabilità devono
essere separati dalle ipotesi su quale ecosistema sia desiderabile.

## 16. Limiti tecnici, prestazioni e incertezza

### 16.1 Scalabilità e riproducibilità

Costo ambiente O(celle) ogni tick, più refresh O(celle*RBF) periodico; rendering
scansiona tutte le celle per frame e genera triangoli per risorse e quattro
segnali visibili. Occupazione e accesso cella sono O(1); scansioni sensor direzionali
O(raggio) per sensore connesso. Nessuna ricerca globale di vicini. `get_cells`
restituisce copia del vettore (`grid_world.h:81`), ma il renderer usa accesso diretto
alle singole celle. Non presumere quel costo nel percorso grafico corrente.

Rete sparsa: esecuzione attraversa coppie di strati e archi, ma alloca vettori
Eigen/attivazioni a ogni `feed_forward`; learning scorre tutti gli archi ogni
tick. Snapshot calcola conteggi e ordina archi attivi per ciascun agente, anche
quando Stats usa solo pochi campi; GUI ne richiede altre copie. Riproduzione
costruisce genoma/brain/tratti/ID prima di sapere se c'è spazio; budget per tick
riduce picchi ma non elimina allocazioni di figli che non verranno inseriti.
Ogni gene casuale costruisce un nuovo RNG e chiama `random_device`, possibile
costo d'inizializzazione significativo. Nessun profiling ha quantificato questi costi.

Risorse/feromoni non sono oggetti dinamici separati per ogni emissione; le mappe
hanno quattro voci per cella, ma `get_feromone_map` ne restituisce una copia.
Render batch limita le draw call e conserva buffer, pur ricostruendo geometria.
Nessuna evidenza autorizza dare priorità all'ottimizzazione grafica rispetto agli
invarianti evolutivi. Si può misurare `Simulator::update` indipendentemente dalla
grafica tramite un futuro harness, ma il programma corrente non offre headless
né un benchmark dedicato. Frame time corrente non separa rendering da simulazione.

RNG per istanza, `rand()` separato, ordine dei candidati, fase casuale iniziale,
risoluzione sequenziale dei conflitti e precisione floating point influenzano i
risultati. `par_unseq` non dimostra da solo una race nei dati attuali, ma ordine
ed esecuzione dipendono dal backend; `random_device`, allocazioni e gestione dei
`weak_ptr` nelle callback richiedono verifica di compatibilità con esecuzione
non sequenziata prima di estendere il parallelismo. Nessuna race è stata riprodotta.
Scritture al mondo sono attualmente seriali; non aggiungere parallelismo di `act`
senza definire conflitti, sincronizzazione e ordine riproducibile.

### 16.2 Build, test e disponibilità

CMake usa C++23, Clang x86_64/Linux e richiede SFML≥2.5, TBB. FetchContent fissa
Eigen3.4.0, JSON3.11.3 con hash, ImGui1.89.9, ImGui-SFML2.6. Un configure fresco
può richiedere rete; non eseguito in questo audit. Preset presenti, ma target
predefinito esegue autoformatting. Il Dockerfile usa `ubuntu:latest` e pacchetti
non pinnati e non elenca `libtbb-dev`: non garantisce rebuild del container
riproducibile o sufficiente da solo, pur avendo TBB nell'ambiente attuale.

Non trovati test correnti/unit test di meccaniche, statistiche o determinismo.
Non eseguiti sanitizer, runtime debugger, rendering, script grafico o profili.
La compilabilità sintattica non verifica invarianti, undefined behavior condizionale,
fitness o prestazioni. Il build esistente non sostituisce una build pulita verificata.

### 16.3 Portata della conclusione

L'audit individua capacità e difetti del percorso attuale, non ricostruisce quale
revisione abbia prodotto le osservazioni passate. Non sono disponibili corpus di
genomi, seed, configurazioni e registri verificati delle prove storiche; non è
possibile stabilire causalità dell'immobilità, tempi di estinzione, fitness media,
frequenza dei cammini funzionali, velocità di evoluzione o regressioni prestazionali.
La presenza di meccanismi corretti non prova efficacia biologica; un difetto
verificato non prova da solo che abbia causato una specifica osservazione storica.

## 17. Indice dei riferimenti al codice

| File | Simboli/punti di ingresso principali |
|---|---|
| [src/main.cpp](../src/main.cpp) | main15; config19; componenti31; cleanup59. |
| [src/popx_app.cpp](../src/popx_app.cpp) | init14; loop25; dt29; campione51; restart69; fine101; tempi134; JSON162. |
| [src/simulator.cpp](../src/simulator.cpp) | init12; tick16. |
| [src/pops_manager.cpp](../src/pops_manager.cpp) | spawn16; ciclo58; candidati127; FIFO159; nascite216; rotazione263; NewGen295; reset364; snapshot385. |
| [src/pop.cpp](../src/pop.cpp) | costi16; costruttori21; init54; die87; reward116; update187; sense215; think419; act433; move575; condizioni631; genoma635; tratti646; donazione665; fisiologia705; foto739; respirazione765; termica792; costo nominale820. |
| [src/pop.h](../src/pop.h) | PhyT33; accessori163; stato241; raggio319; tratti375. |
| [src/gene.h](../src/gene.h), [src/gene.cpp](../src/gene.cpp) | campi34; scala15; lottery41. |
| [src/genome.cpp](../src/genome.cpp) | genesi9; add22; mutazione29; colore41. |
| [src/brain.cpp](../src/brain.cpp) | wire49; feed131; metriche263; sensori342; sigmoid361; Hebb368; export494/502. |
| [src/neuron.h](../src/neuron.h) | sensori5; extended52; azioni61. |
| [src/grid_world.cpp](../src/grid_world.cpp) | init15; is_free57; move74; remove89; add105; ciclo127; temperatura203; elevazione211; totali219. |
| [src/cell.cpp](../src/cell.cpp) | risorse9; trasferimenti94; payload167; feromoni187; regen196/203; update210. |
| [src/config.h](../src/config.h), [src/config.cpp](../src/config.cpp), [config/popx.ini](../config/popx.ini) | default7; parser237/441; INI8–73. |
| [src/random_utility.cpp](../src/random_utility.cpp) | seed5; interi7; campi48; valutazione88; rotazione99. |
| [src/stats.cpp](../src/stats.cpp), [src/stats.h](../src/stats.h), [src/i_stats.h](../src/i_stats.h) | session8; campioni42; frame155; summary160; JSON197; cap34; schema16. |
| [src/pop_snapshot.h](../src/pop_snapshot.h) | snapshot16, soli campi individuali correnti. |
| [src/renderer.cpp](../src/renderer.cpp), [src/sfml_graphic_engine.cpp](../src/sfml_graphic_engine.cpp) | buffer11/33; scan61; feromoni228; frame cap14; batch89/104. |
| [src/imgui_layer.cpp](../src/imgui_layer.cpp), [scripts/plot_net.py](../scripts/plot_net.py) | pulsanteNewGen268; snapshot409; percorsi532; grafici652; raggiungibilità Python220. |
| [src/common.h](../src/common.h) | costanti5–16; capacità23–35; segnali47–50; pendenza54. |
| [CMakeLists.txt](../CMakeLists.txt), [src/CMakeLists.txt](../src/CMakeLists.txt), [cmake/ThirdPartyLibs.cmake](../cmake/ThirdPartyLibs.cmake) | build, opzioni test23; link e dipendenze. |
| [cmake/CodeFormatting.cmake](../cmake/CodeFormatting.cmake), [CMakePresets.json](../CMakePresets.json) | autoformat11; preset configurazione/build. |
| [README_concept.md](../README_concept.md) | contesto architetturale preesistente, ownership e descrizioni parzialmente superate. |

Aggiornare questo documento dopo verifiche nuove, riportando revisione, protocollo,
risultati e incertezza; non convertire automaticamente le ipotesi in fatti.
