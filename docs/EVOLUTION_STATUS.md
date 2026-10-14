# PopX — stato corrente, validazione e archivio delle indagini

Documento unico di riferimento del progetto. `AGENTS.md` contiene le istruzioni
operative. Audit e indagini precedenti sono conservati sotto con risultati e
limiti originali; non descrivono automaticamente il modello attuale.

## Stato corrente — respirazione regolata, 14 ottobre 2026

Implementazione autorizzata dopo i confronti termici: consumo regolato dalla
riserva energetica, attività termica continua, materia/energia/calore prodotti
soltanto dalle reazioni realmente eseguite. Nessun premio al movimento, nessuna
modifica delle azioni o dell'ambiente produttivo. Adattamento evolutivo non dimostrato.

Indice:
- [Fisiologia implementata](#fisiologia-implementata)
- [Validazione corrente](#validazione-corrente)
- [Stato operativo e limiti](#stato-operativo-e-limiti)
- [Audit e indagini precedenti, sezioni1–22](#stato-storico)
- [Audit dinamico storico](#audit-dinamico-storico)
- [Indagini termiche precedenti alla regolazione](#indagini-termiche-storiche)
- [Pilot successivo alla segnalazione GUI](#pilot-storico)

### Fisiologia implementata

`Pop::run_mitochondrions()` interpreta i mitocondri come capacità massima di
reazione per tick. Parametri effettivi INI:

| Parametro | Valore | Significato |
|---|---:|---|
|`phy_respiration_energy_target`|100|Riserva bersaglio; non assegna energia gratuita|
|`phy_respiration_demand_band`|25|Richiesta piena a energia<=75, nulla a energia>=100|
|`phy_opt_temperature`|37 C|Centro della risposta termica|
|`phy_efficiency_half_range_c`|10 C|Semilarghezza del rendimento e offset dell'inibizione calda|
|`phy_energy_per_respiration`|0,3|Massima energia utile per reazione|
|`phy_heat_per_respiration`|0,3|Calore per reazione completata|
|`phy_alpha_min` / `phy_alpha_max`|0,01 /0,5|Scambio termico; valori produttivi precedenti conservati|

Con `z=(T-37)/10`, `Y=2^(-z²)`, `h=clamp(z,0,1)`:

```
domanda = clamp((bersaglio - energia) / banda, 0, 1)
inibizione_calda = 1 - 3*h² + 2*h³
attività = Y * inibizione_calda
flusso_richiesto = mitocondri * domanda * attività
```

Y è continuo e non nullo a25 C. Sul lato caldo l'attività diminuisce fino a zero
a47 C senza un salto al confine. Il reward termico conserva la banda lineare:
la sua formula NON è il rendimento respiratorio nuovo. Il parametro condiviso
è documentato per evitare confusione.

Il flusso frazionario si accumula in un credito <1; sono eseguite solo reazioni
intere, entro capacità e substrati. Non si accumulano reazioni intere arretrate.
Esaurimento substrati, riserva soddisfatta e completa inibizione azzerano il
credito. `init()` e i figli iniziano con credito/calori cumulati nulli. Ogni
reazione consuma1 glucosio e1 O2, produce1 CO2, energia0,3*Y e calore0,3.
La riserva può superare il bersaglio di meno di una reazione; il cap energetico
assoluto400 e il suo ledger rimangono. Il flusso si interrompe al bersaglio.
I coefficienti sono un modello astratto, non una calibrazione in unità SI.

`total_respiration_heat_` traccia il calore cumulativo; i ledger di energia
includono trasferimenti, cap e roundoff. Nessun calore o energia per capacità
non utilizzata. Vita, maturità, costo riproduttivo e donazione parentale restano
quelli precedenti. La regolazione è fisiologica, non un'azione o un reward nuovo.
Non garantisce sopravvivenza in ambienti letali o senza risorse.

Una prima curva simmetrica senza inibizione aggiuntiva sul lato caldo lasciava
M=25 troppo caldo e con scarsa energia; il controllo di recupero ha motivato
l'inibizione finale. Non è mantenuta una seconda implementazione produttiva
o un selettore nascosto della vecchia fisiologia.

### Validazione corrente

Comandi ripetibili (standalone, non registrati in CTest):

```sh
bash scripts/run_regressions.sh
bash scripts/run_audit.sh data_out/audit/regulated_conservation_2026-10-14.json probes
bash scripts/run_thermal_comparison.sh data_out/audit/thermal_regulated_2026-10-14.json moderate
```

- Build Release del target esplicito `popx` riuscita.
- Regressione respirazione: parametri/invalidi, consumo nullo a riserva soddisfatta,
  reazioni frazionarie, bilanci e calore, assenza di raffiche dopo scarsità,
  inibizione calda e reset. Recupero da25 C/energia25 in2000 tick con rifornimento
  controllato: M=1/5/25 raggiungono energia94,80/99,03/99,76 e corpo28,42/28,36/28,31 C.
- Un figlio nato con meno di13 energia donata raggiunge maturità mediante1500
  aggiornamenti reali e può riprodursi. Genitore iniziale maturo per costruzione,
  rifornimento esplicito: controllo della catena, non discendenza autosostenuta.
- Regressioni fasi, posizioni NewGen, parallelismo, codifica/eredità e feromoni PASS.
  Confronto controllato90 tick:1 e4 thread osservati, stati identici.
- Probe materia/energia/riproduzione/cap/morte/NewGen PASS.
-180 controlli isolati e144 confronti a risorse finite eseguiti sul modello
  regolato; bilanci e finitezza verificati. Il JSON identifica `reserve_regulated_v1`.
  Disegno e limiti delle fixture sono nell'appendice termica e nel codice.
  Replay esatto dei180 controlli e144 popolazioni sulla build finale: PASS.
- Pilot nativo2000 tick con INI produttivo:120 vivi finali,137 nascite naturali,
  0 forzate,97 morti (66 termiche,31 energetiche),5513 movimenti riusciti su18375
  tentativi. Bilanci PASS; durata diagnostica1,085s. Singola estrazione non appaiata,
  non confronto causale con i vecchi pilot né benchmark GUI. Clima ancora ostile
  e cap120 raggiunto: la nuova regolazione non elimina mortalità o censura.
  Dati in `data_out/audit/regulated_native_pilot_2026-10-14.json`.

Confronto descrittivo a25–32 C, coefficienti termici produttivi precedenti invariati:
12 condizioni per modello (3 tratti×2 politiche×2 layout),288 fondatori totali.

| Modello | Fondatori vivi | Nascite naturali | Figli vivi | Morti termiche | Morti energetiche |
|---|---:|---:|---:|---:|---:|
|Vecchia respirazione obbligatoria|96|192|192|192|0|
|Respirazione regolata|288|1152|1152|0|0|

A18–32 C il modello nuovo conserva288 fondatori,1157 nascite,1152 figli vivi,
0 morti termiche e5 energetiche. Il controllo estremo0–50 C conserva177/288
fondatori: non rende ogni nicchia abitabile. I conteggi aggregano condizioni
costruite, non sono probabilità stimate su popolazioni casuali.

Nel controllo25–32 C/pattuglia/layout0/M=5, nei tick1500–2000 il modello nuovo
produce1394,96 energia a fronte di1399,83 costi (riserva vicino al bersaglio),
contro zero nella vecchia variante0,1/0,02. Il cap120 e la censura al tick2000
limitano i conteggi riproduttivi. Politiche imposte, genoma vuoto, mutazione0
solo nella fixture, eredità true ma senza pesi attivi: nessuna prova qui di
apprendimento efficace, adattamento, cooperazione o stabilità indefinita.
I dati locali restano ignorati da Git; script, fixture e protocollo sono versionati.

### Stato operativo e limiti

- Mutazione produttiva INI0,01 (prima0,05); vale anche per i tratti PHY.
- Eredità dei pesi true per esplicita richiesta attuale dell'utente, superando la
  preferenza contraria degli audit storici; non modificata dalla regolazione.
- NewGen mantiene selezione/culling ma conserva celle, ordine e fasi dei superstiti.
- `num_threads` controlla l'arena TBB di sense/think;1=seriale,4=fino a4 thread.
  Azioni, fisiologia e mondo sono sequenziali. Nessuna accelerazione garantita.
- GUI a tick10 ms; nessuna modalità infinita/acceleratore nuova in questo intervento.
- RNG produttive non centralizzate: un solo seed non rende riproducibili i pilot nativi.
- `old/` archivio locale escluso da codice, prove e commit; layout UI locali esclusi.
- Il remoto aveva `master` e nessun `main`: la pubblicazione richiesta crea `main`
  dalla storia corrente, senza riscrivere `master` o cambiare il default remoto.

Le sezioni successive sono fotografie storiche. "Nessuna modifica fisiologica",
num_threads ignorato, par_unseq, teletrasporto NewGen o rendimento lineare
descrivono versioni precedenti, non lo stato attuale.

<a id="stato-storico"></a>
## Archivio: stato tecnico e indagini precedenti


## Feromoni completati e nuove run brevi — 14 ottobre 2026

Completati gradienti direzionali DANGER/MATE/HOME, aggiunti agli input37…48;
INI49 input,21 uscite. Emissione con RNG individuale del Pop, campo protetto da
valori non finiti, nessun nuovo reward. Test dei quattro canali e regressioni PASS.
Run nativa2000 tick:284 movimenti riusciti su6423 richieste,47 nascite naturali,
forte mortalità termica. Run controllata con risorse vicine2200 tick:9032 movimenti,
48 nascite naturali, prelievo netto18250 glucosio, nessuna morte termica.
Fondatori mossi45 figli diretti, immobili0: associazione senza prova causale.
Feromoni cambiano alcune scelte nei controlli appaiati, senza vantaggio coerente
di esplorazione. Gruppo diagnosticamente possibile, coordinamento evoluto non
dimostrato. Dettagli e rettifica del vecchio washout in sezione21.

## Codifica genetica configurabile — aggiornamento del 14 ottobre 2026

Su richiesta del developer rimossa la barriera a5 bit per gli indici genetici:
ora uint32 per sorgenti e destinazioni, domini di generazione/mutazione da INI,
37 input correnti comprendenti i sensori glucosio33…36. Uscite correnti21,
configurabili; indici storici preservati. Fixture genetica controllata verifica
glucosio est→MOVE_EAST e glucosio ovest→MOVE_WEST con spostamenti reali.
È raggiungibilità funzionale dimostrata, non adattamento spontaneo. Le prove
delle sezioni18–19 si riferiscono alla codifica precedente. Dettagli in sezione20.

## Risposte al contesto e figli mutati — nuova verifica del 14 ottobre 2026

Eseguite2400 esposizioni brevi e replay esatto dello stesso dataset:80 genomi
originali,80 copie senza mutazione,240 figli genetici mutati,6 contesti ciascuno.
Le risorse a est/ovest cambiano l'acquisizione ma non azioni o percorsi di alcun
genotipo nel controllo senza apprendimento. Bordo/occupazione sono percepibili,
ma cambiano le sequenze di pochi genomi.195/240 figli mutati mantengono l'intera
sequenza delle azioni parentali in tutti i contesti. Evidenza di scarsa variazione
comportamentale contestuale utile, non assenza di variazione né dimostrazione
di inefficacia evolutiva generale. Metodo, misure e limiti in sezione19.

## Confronto controllato di strategie — 14 ottobre 2026

**Nuova evidenza:** a tratti uguali e temperatura ambientale costante, esplorare
può aumentare acquisizione e discendenza. L'immobilità non è inevitabilmente la
strategia favorita. Nel controllo senza fotosintesi gli immobili sopravvivono
3000 tick ma non si riproducono; la camminata casuale lascia mediamente109
discendenti vivi con glucosio uniforme e94,5 con glucosio localizzato.
Con fotosintesi anche gli immobili si riproducono. Le strategie sono interventi
diagnostici prescritti, **non comportamenti evoluti spontaneamente**.

Le reti genetiche originali mostrano spesso una singola azione ripetuta e molti
tentativi bloccati al bordo. Questo sposta l'attenzione, dopo aver controllato la
mortalità termica, verso variazione comportamentale utile e capacità di cambiare
azione, senza stabilire definitivamente il collo evolutivo. Protocollo completo,
misure, limiti e riproduzione in sezione18. I risultati termici delle run precedenti
restano validi per le loro condizioni: non vengono smentiti dal controllo a37°C.

## Analisi dinamica e bilanci — aggiornamento del 14 ottobre 2026

Eseguite due run headless di2000 tick e una validazione contabile di200 tick,
oltre a fixture di trasferimento/NewGen e18 controlli termici brevi. Variazione
neurale presente e clone senza mutazione fedele nel pannello comune; efficacia
adattiva ancora non dimostrata. Mortalità termica prima della maturità è il
vincolo osservato più forte, senza prova di causalità evolutiva generale.

Corrette inizializzazione griglia, creazione/duplicazione di risorse, dotazioni
gratuitamente create nei figli (donazione1/8 autorizzata), costo riproduttivo reale,
cap lipidico, restituzioni Death/NewGen e fallimenti di riposizionamento. Residui
materiali0; ledger energetico finale0 dopo arrotondamenti esplicitamente misurati.
Persistono sink alla morte per capacità finite, documentati separatamente.

Dettagli, risultati, protocollo, limiti e comandi in
[audit dinamico storico](#audit-dinamico-storico).
Le sezioni1–17 sottostanti restano l'audit storico precedente alle correzioni;
le frasi “nessun esperimento condotto” e i problemi risolti appartengono a quella
fotografia, non allo stato successivo riportato nel nuovo documento.

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

## 18. Strategie con tratti uguali e ambiente termico controllato

### 18.1 Domanda e protocollo effettivamente eseguito

Data14 ottobre2026; base Git `015d3f4da0e2c19628bb6b2afbb9d7d095e408f7`
con le correzioni già documentate, nel working tree. Nessuna modifica aggiuntiva
alla fisiologia, alla selezione o a `config/popx.ini` per questo confronto.
Harness standalone `tools/popx_strategy_comparison.cpp`, classe `SimulationAudit`:
usa le implementazioni reali `Pop::sense/think/act/update`, `PopsManager::try_reproduce`,
`GridWorld`, `Cell`, `Brain`; controlli applicati solo nella fixture.

64 prove:4 strategie ×2 distribuzioni ×2 fisiologie ×4 seed(17,29,43,61).
Durata3000 tick ciascuna; tempo totale misurato7,252s. Fermate al termine del
confronto: prolungarle, con cap demografico e politiche prescritte, non avrebbe
dimostrato evoluzione. Sono stati eseguiti anche due replay neurali di120 tick
e un controllo del bordo3000 tick, non una simulazione evolutiva lunga.

| Controllo | Valore e motivazione |
|---|---|
| Ambiente |16×16, temperatura delle celle37°C, quota31 uniforme; niente aggiornamento climatico/rigenerazione. |
| Fisiologia comune in ogni blocco |1 mitocondrio, sensibilità1, capacità adiposa80;0 oppure14 cloroplasti. I due blocchi fotosintetici sono confronti separati. |
| Fondatore |Posizione(3,8), energia100, lipidi20, temperatura corporea37°C, direzione est, lazyness0; stessi valori per tutte le strategie. |
| Glucosio totale iniziale |32768 in entrambi i layout. Uniforme128/cella; localizzato nelle72 celle x8…15,y4…12,456 ciascuna eccetto l'ultima392; fondatore fuori dalla zona. |
| Altre risorse |Acqua1000/cella e CO₂1000/cella, finite; calcio e lipidi ambientali0. O₂ riportato a1000/cella ogni tick: serbatoio esterno dichiarato, uguale per tutte le strategie. |
| Genetica/plasticità |Genoma vuoto nelle politiche prescritte, mutazione0, apprendimento congelato; nessuna eredità dei pesi appresi. Tratti copiati senza mutazione. |
| Ciclo |sense→think→act, una fase/tick per agente; fisiologia ogni tick. Fasi determinate dall'età, senza bucket paralleli del manager. |
| Riproduzione |Condizione reale E>50, età>1000, glucosio>20; costo e donazione1/8 reali; collocazione adiacente reale. Massimo3 tentativi/tick, cap120. No NewGen/riproduzione forzata. |
| Ordine riproduttivo |Scansione stabile dei genitori, non FIFO produttiva; stesso criterio tra strategie. |
| Discendenti |Ricevono est, lazyness0 e la politica familiare per intervento diagnostico; questa trasmissione della politica non dimostra ereditabilità genetica. |

Politiche: **immobile** impone nessuna azione, mantenendo fisiologia e assorbimento;
**sempre est** impone MOVE_EAST; **pattuglia** inverte est/ovest ai bordi della riga;
**casuale** sceglie uniformemente le quattro direzioni cardinali a ogni act.
Quest'ultima usa mt19937 distinto per agente, seed base+104729×seriale di nascita.
È diversa da MOVE_RANDOM produttivo, che ammette anche diagonali e bersaglio nullo.
L'immobile è un controllo metabolico, non l'output naturale del genoma vuoto.

I controlli immobile/est/pattuglia sono deterministici nelle condizioni scelte:
le quattro ripetizioni coincidono e **non sono quattro repliche indipendenti**.
Per casuale si riportano medie e intervalli osservati dei quattro seed, non
intervalli di confidenza. ID e RNG produttive non sono globalmente seedabili;
nei confronti prescritti le decisioni stocastiche rilevanti sono controllate
(foto con probabilità1, mutazione0, azioni cardinali esterne). I replay neurali
restano esplorativi per RANDOM e altre RNG produttive non controllate.

### 18.2 Movimento → acquisizione → sopravvivenza → discendenti

Glucosio indica acquisizione **ambientale cumulativa**; non comprende quello
prodotto internamente dalla fotosintesi. Movimenti/acquisizione totali includono
fondatore e discendenti: sono conseguenze dell'espansione familiare, non misure
di efficienza per individuo. Per evitare questa confusione si riporta anche
l'acquisizione del solo fondatore. Conteggi casuali sono medie, quindi frazionari.

| Foto | Risorse | Strategia | Movimenti riusciti famiglia | Glucosio fondatore | Glucosio famiglia | Figli diretti fondatore | Discendenti nati | Discendenti vivi a3000 | Morti energetiche |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|
|No|Uniformi|Immobile|0|128|128|0|0|0|0|
|No|Uniformi|Sempre est|12|303|303|0|0|0|0|
|No|Uniformi|Pattuglia|427|2048|4112|19|19|11|8|
|No|Uniformi|Casuale|27160,25|5663,5|32768|34|149,5|109|40,5|
|No|Localizzate|Immobile|0|0|0|0|0|0|0|
|No|Localizzate|Sempre est|12|561|561|0|0|0|0|
|No|Localizzate|Pattuglia|423|2556|10121|20|20|18|2|
|No|Localizzate|Casuale|18048,5|1463,5|32762,75|31,75|117,5|94,5|23|
|Sì|Uniformi|Immobile|0|128|3200|8|24|24|0|
|Sì|Uniformi|Sempre est|12|303|2065|5|14|14|0|
|Sì|Uniformi|Pattuglia|428|2048|5498|20|32|32|0|
|Sì|Uniformi|Casuale|28660|5684,5|32768|53,25|119,25|119|0,25|
|Sì|Localizzate|Immobile|0|0|0|8|8|8|0|
|Sì|Localizzate|Sempre est|12|561|6915|5|14|14|0|
|Sì|Localizzate|Pattuglia|428|2556|15108|20|32|32|0|
|Sì|Localizzate|Casuale|26976,25|534,75|32768|34,75|119|119|0|

Tutti i64 fondatori vivi al tick3000; nessuna morte termica nella famiglia.
Temperature corporee osservate nell'intero confronto37…37,794702°C: ambiente
controllato non significa temperatura corporea bloccata artificialmente.
La sopravvivenza del fondatore è quindi censurata: **non è misurata la longevità
completa né dimostrato un vantaggio di sopravvivenza del fondatore**.
Discendenti vivi casuali senza foto:98…118(uniforme),42…119(localizzato).
Con foto:119 in tutti i seed/layout; insieme al fondatore raggiungono cap120.
Discendenti che hanno raggiunto età>1000, anche se poi morti: senza foto
0/0/16/29,25(uniforme),0/0/19/16(localizzato); con foto
8/5/20/50,5(uniforme),8/5/20/31,25(localizzato), nell'ordine delle strategie.

**Interpretazione causale limitata alla fixture:**

- Senza foto, sopravvivere stazionari è possibile nell'orizzonte, ma il glucosio
  manca alla maturità e impedisce figli. Movimento esplorativo può collegare
  acquisizione e discendenza; muoversi soltanto inizialmente non basta.
- Sempre est accumula più energia del controllo immobile (fondatore141,35 contro
  103,02 uniforme;212,60 contro67,69 localizzato), ma senza foto resta senza figli:
  blocco al bordo e soglia riproduttiva sul glucosio separano energia e fitness.
- La pattuglia acquisisce risorse, poi si blocca sui propri figli:10636 tentativi
  falliti per occupazione senza foto/uniforme e13379 senza foto/localizzato.
  La camminata casuale favorisce anche disponibilità di celle adiacenti per
  nuove nascite. Non attribuire tutto il vantaggio alla sola energia.
- Con foto, gli immobili si riproducono persino fuori dalla zona con glucosio:
  CO₂/acqua e quota favorevole permettono produzione interna. Questo riduce la
  necessità del movimento; non rende l'immobilità la strategia vincente qui.
- Il cap120 limita soprattutto casuale: medie787/421 tick al cap senza foto
  (uniforme/localizzato),958/949 con foto. Non stimare crescita libera o optimum
  dalle famiglie saturate. Un confronto in arene separate non misura invasione
  competitiva, selezione di frequenze genetiche o cooperazione.

### 18.3 Il costo disincentiva il movimento?

Costo attuale verificato `src/pop.cpp:16`:0,01 per **tentativo**, compresi falliti;
basale0,02 per tick. Con un act ogni3 tick,3000 tick comportano1000 richieste:
costo10, contro60 basali, incremento16,7% rispetto al basale. Il precedente valore
storico0,1 non descrive il codice attuale. Respirazione configurata0,3 per
mitocondrio/reazione (`config/popx.ini:57`), moltiplicata per efficienza termica.
Con risorse sufficienti e temperature corporee osservate, un mitocondrio può
produrre circa0,276…0,3/tick, contro0,02333/tick di basale+movimento medio.
È un confronto condizionale della formula, non il reddito medio misurato di
ogni famiglia: fame, donazioni, riproduzione e cap energia cambiano i bilanci.

Il costo non elimina il vantaggio osservato dell'esplorazione. Disincentiva
tentativi inutili, che persistono al bordo. Ridurre arbitrariamente il costo
non è giustificato da questi risultati. L'assorbimento automatico a ogni tick
(`Pop::update_physiology`, `src/pop.cpp:750`) permette reddito senza GET_GLUCOSE;
foto e risorse diffuse possono rendere superfluo esplorare per un certo tempo.
Niente bilancio globale chiuso in questo confronto: O₂ è esterno e foto/respirazione
trasformano risorse. I residui conservativi della precedente validazione restano
documentati separatamente; glucosio iniziale uguale non implica produzione totale
uguale quando cambiano cloroplasti o popolazione.

### 18.4 Reti reali: richieste, spostamenti ed esplorazione

Replay degli80 genomi iniziali della run precedente, ciascuno isolato in un mondo
uniforme con stessa fisiologia(1,0,1,80), temperatura37°C, direzione est e lazyness0.
120 tick/40 decisioni, nessuna override dell'azione, nessuna riproduzione, learning
congelato. I genomi sono quelli alla nascita, non reti modificate di adulti.

Due letture esplorative con RNG produttive non seedate: prima69/80 genomi con
un'unica azione,45 senza richiesta di movimento,1290 richieste/618 riuscite;
seconda69/80 con un'unica azione,43 senza movimento,1343 richieste/626 riuscite.
La seconda distingue703 fallimenti al bordo e14 richieste con bersaglio uguale
alla posizione attuale.10 genomi che muovono con una sola azione visitano al
massimo4 celle: rotazioni ripetute possono fare piccoli circuiti. Distanza
percorsa, varietà di azioni ed esplorazione spaziale non sono equivalenti.

Controllo aggiuntivo120×120,3000 tick, fisiologia uguale, un gene reale
`(source_type=1,source_num=0,sink_type=7,sink_num=4,weight=8192)`:
produce MOVE_EAST con esecuzione neurale reale, senza controller prescritto.
Parte(60,60), termina(119,60):1000 richieste,59 spostamenti,941 fallimenti;
primo fallimento tick180, costo movimento10, basale60. La trappola del bordo
esiste anche alla dimensione ordinaria, non solo nel mondo piccolo del confronto.
Questo dimostra raggiungibilità della singola azione, non ereditabilità di
un'esplorazione utile o adattamento spontaneo. Nessuna riproduzione in questo probe.

Riscontri statici pertinenti: argmax seleziona un solo vincitore e preferisce
l'indice più basso nei pareggi (`src/brain.cpp:154`); sensori glucosio33…36 oltre
gli indici0…31 codificabili (`src/neuron.h`, `src/gene.h:38`); AGE satura alla
scala configurata300, prima della maturità1000; mancano sensori diretti di fame,
energia interna e temperatura corporea. Sono limiti confermati, ma non è
dimostrato che uno specifico limite causi i69 output costanti. Altri sensori
possono comunque influenzare decisioni.120 tick non coprono sviluppo/maturità
né plasticità attiva; il freeze isola la componente genetica iniziale.

### 18.5 Collo evolutivo e osservazione sui gruppi

| Livello | Evidenza aggiornata | Cosa resta aperto |
|---|---|---|
|Variazione|Genomi diversi producono azioni diverse e movimento è raggiungibile. Molti però ripetono un'unica azione, rimangono al bordo o girano in poche celle.|Frequenza di strategie esplorative utili e mutazioni che le generano; risposta effettiva a contesti diversi. Candidato principale sotto controllo termico, non verdetto definitivo.|
|Ereditabilità|Copia senza mutazione fedele nel pannello precedente; nella fixture i tratti sono identici e le politiche imposte ai figli.|La politica prescritta non è eredità genetica. Non misurata persistenza di esplorazione funzionale con mutazione, plasticità e INI corrente (`hebbian_inheritance=true`).|
|Selezione|Ambiente/fisiologia consentono differenze riproduttive legate alla strategia; casuale supera immobile nei controlli.|Non dimostrata selezione evolutiva nella popolazione normale, né competizione tra strategie. Foto, rigenerazione, cap e mortalità termica possono cambiare il vantaggio.|

L'osservazione del developer (poco movimento, poche scelte individuali visibili)
è compatibile con output costanti e richieste bloccate, non prova assenza totale
di variazione. I controlli mostrano che muoversi può servire: non è necessario
premiarlo artificialmente. Assorbimento passivo e foto spiegano una convenienza
condizionale dello stare fermi, mentre costi di fallimento e occupazione possono
penalizzare controller poco flessibili.

Gruppi visivi possono formarsi semplicemente perché i figli nascono adiacenti;
anche la famiglia immobile fotosintetica cresce. Non sono prove di cooperazione,
comunicazione o scelta sociale. Né movimento scarso né movimento abbondante
permettono da soli di concludere sul comportamento di gruppo. Questo confronto
non testa segnali/feromoni o interazioni sociali selezionate.

**Prossima verifica con massimo valore informativo:** confrontare le risposte
dei medesimi genomi a più contesti sensoriali controllati (interno, bordo,
occupazione, risorse localizzate), distinguendo output neurali, bersagli,
spostamenti e celle nuove visitate. Campionare anche mutazioni/figli con seed
espliciti, senza reward al movimento: determinare se la variazione genetica
produce e conserva controller che cambiano azione quando la corrente fallisce.
Solo dopo, confronto competitivo di controller geneticamente codificati con
termica uguale, mortalità censurata e cap non saturato. Nessuna implementazione
di questi esperimenti futuri è inclusa nelle prove descritte.

### 18.6 Artefatti, verifica e limiti

Artefatti locali in `data_out/audit` (ignorata da Git):
`strategy_comparison.json`(64 prove e primo replay), `strategy_network_trace.json`
(secondo replay), `strategy_boundary_probe.json`, `strategy_summary.json`,
`strategy_comparison.png/.svg`. Il JSON delle nascite della prima raccolta aveva
depth0 come placeholder: profondità ricostruita deterministicamente da
parent_serial; le misure primarie non sono cambiate. Il codice attuale calcola
depth alla nascita. Non interpretare la ricostruzione come nuova simulazione.

```sh
bash scripts/run_strategy_comparison.sh /tmp/strategy_comparison.json data_out/audit/corrected_pilot.json
python3 scripts/analyze_strategy_comparison.py data_out/audit
python3 scripts/plot_strategy_comparison.py data_out/audit
# Dopo compilazione dello harness: modalità brevi senza ripetere le64 prove
/tmp/popx_strategy_comparison data_out/audit/corrected_pilot.json /tmp/replay.json replay
/tmp/popx_strategy_comparison data_out/audit/corrected_pilot.json /tmp/boundary.json boundary
```

Lo script costruisce solo target `popx`, poi collega lo harness standalone.
Non usare build globale: target format ALL modifica sorgenti. Dipendenze già
presenti, nessuna installazione. Analizzatore verifica64 prove, stessi budget,
assenza morti termiche, sopravvivenza fondatori, identità tentativi=riusciti+
fallimenti, conteggi nascite/vivi e ledger delle richieste nel replay.
Probe bordo contiene assert su1000 richieste/59 successi. Grafico ispezionato.
Ripetizione completa di verifica in7,540s: tutte le64 prove coincidono con la
prima raccolta in ogni campo, ordinando gli individui per seriale, comprese le
profondità genealogiche ricostruite. Il replay neurale non è incluso in questa
affermazione di riproducibilità. Controlli sintattici Bash/Python superati.

Limiti: mappa piccola, condizioni favorevoli/risorse finite, O₂ esterno, niente
rigenerazione, ordine riproduttivo semplificato, no plasticità/mutazione/forzature,
controller esterni, quattro seed e orizzonte censurato. Cap120 e disponibilità
di spazio confondono stima della sola convenienza energetica. La foto rende
diverso il budget di glucosio prodotto, pur a stesso glucosio ambientale iniziale.
Non estrapolare rapporti numerici alla GUI/ecosistema120×120. Nessuna dimostrazione
di adattamento spontaneo, selezione genetica nel tempo o comportamento sociale.

## 19. Esposizioni degli stessi genomi a contesti differenti e figli mutati

### 19.1 Protocollo effettivo

Data14 ottobre2026, commit base invariato
`015d3f4da0e2c19628bb6b2afbb9d7d095e408f7`, working tree già corretto.
Nessuna modifica produttiva né all'INI. Harness temporaneo
`/tmp/popx_context_probe.cpp`, analizzatore `/tmp/analyze_context_probe.py`:
usano l'accesso friend diagnostico esistente. Build target `popx` verificato:
`ninja: no work to do`; harness C++23 compilato contro gli oggetti reali.

80 genomi originali di `data_out/audit/corrected_pilot.json`, sezione
`run.initial_genomes`. Per ogni genoma: genitore, copia tramite `Genome::mutated(0)`,
3 figli genetici tramite **`Pop::make_offspring_genome()` reale**, rate INI0,05.
Nella fixture `hebbian_inheritance=false` e learning congelato per escludere
plasticità e pesi acquisiti. È una verifica di trasmissione/mutazione genetica:
**non nascite ecologiche**, nessun costo/donazione/maturità/fertilità imposto ai
figli di questo pannello. Non sono nuovi individui selezionati da una run lunga.

400 genotipi ×6 contesti =2400 esposizioni indipendenti,240 tick/80 decisioni
ognuna. Non sono2400 repliche genetiche indipendenti: genitori/figli sono appaiati
e copie/contesti costituiscono controlli. Tre figli per genitore non bastano a
stimare con precisione probabilità di innovazione evolutiva.

Condizioni comuni:16×16, quota31, ambiente e corpo iniziale37°C,
PhyT(1 mitocondrio,0 cloroplasti,sensibilità1,capacità adiposa80), energia100,
lipidi20, lazyness0, direzione est, acqua/O₂1000 per cella iniziali, CO₂/calcio/
lipidi ambientali0 salvo barriera dichiarata. Niente rigenerazione o world update,
nessuna riproduzione, fisiologia reale ogni tick, sense→think→act a tick separati.
Le cellule occupate sono Pop statici che non consumano risorse o agiscono;
riserve azzerate, mantenuti vivi senza update, rimossi dal mondo se uccisi.
Nessuna uccisione osservata in questo dataset.

| Contesto | Posizione iniziale e manipolazione |
|---|---|
|Interno|Posizione(8,8), glucosio128/cella, totale32768.|
|Bordo|Posizione(15,8), stessa disponibilità uniforme: davanti a est fuori mappa.|
|Barriera fisica|Interno, cinque celle(9,y),y6…10 con calcio=MaxCaco3, bloccate da `GridWorld::is_free`.|
|Celle occupate|Interno, cinque Pop statici nelle stesse celle della barriera; stimolo di densità rilevabile dai sensori esistenti.|
|Risorse est|Interno, glucosio256/cella per x≥8,0 altrove; stesso totale32768.|
|Risorse ovest|Interno, glucosio256/cella per x<8,0 altrove; stesso totale32768.|

La posizione iniziale diversa al bordo comporta un percorso assoluto diverso:
non usare questo semplice fatto come prova di risposta comportamentale.
Indicatori contestuali sono prima azione e sequenza delle azioni, confrontate
con lo stesso genotipo nell'interno; input della prima sense salvati nel JSON.

RNG di Pop sostituita **solo nella fixture** da una sottoclasse mt19937 con seed
20261014+indice famiglia, identico tra contesti/genitore/figli. Predictor copia
la stessa RNG prima di MOVE_RANDOM, senza consumare lo stato reale. Mutazione
produttiva crea RNG autonome da random_device: il primo campionamento dei figli
non è seedabile. Tutti i loro geni sono salvati integralmente, poi riutilizzati
nel replay: riproducibilità condizionata al pannello estratto, non della sua
rigenerazione. Divergenze successive di azioni possono consumare RNG in modo
diverso: la prima decisione appaiata ha controllo più stretto della traiettoria.

**Rettifica successiva, verificata nella sezione21:** in questa versione le
emissioni usavano `RandomUtility()` separata (`src/pop.cpp:525`). Il tentativo
di lavare i segnali con `set_feromone(type,0)` era inefficace: il setter somma
un delta, quindi0 non cancella nulla. Non erano controlli privi di signaling;
retroazione da emissioni non seedate poteva restare. I conteggi osservati e
il replay effettivamente uguale rimangono registrati, ma non costituiscono
garanzia generale di riproducibilità o isolamento dalla comunicazione.
La prima decisione, prima delle emissioni, mantiene il controllo appaiato.

### 19.2 Risultati dei genitori

Nuove celle = posizioni distinte visitate **escludendo la cella iniziale**;
totali sommano80 esposizioni, media nuova esplorazione per genoma.
"Azioni diverse" confronta sequenze complete con interno, non numero di tipi di
azione. "Cambio dopo blocco" conta un'azione differente alla decisione seguente
un tentativo fallito; associazione temporale, non prova di evitamento causale.

| Contesto | Genomi con prima azione diversa /80 | Genomi con sequenza diversa /80 | Richieste movimento | Spostamenti riusciti | Fallimenti bordo | Fallimenti ostacolo | Nuove celle medie | Cambi totali di azione |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
|Interno|0|0|2509|981|1498|0|3,675|265|
|Bordo|6|8|2508|883|1595|0|2,550|241|
|Barriera fisica|0|1|2509|873|742|864|2,525|252|
|Celle occupate|3|4|2592|882|734|945|2,625|254|
|Risorse est|0|0|2509|981|1498|0|3,675|265|
|Risorse ovest|0|0|2509|981|1498|0|3,675|265|

Nel contesto interno69/80 usano una sola azione per80 decisioni e46/80 non
richiedono mai movimento.70/80 hanno sequenze identiche in tutti e6 i contesti;
72/80 non cambiano la prima azione tra contesti. Input iniziali diversi dal
controllo per72 genomi al bordo,37 con occupanti,0 per barriera fisica/risorse.
La rete dunque riceve alcuni cambiamenti percepibili ma spesso il vincitore
dell'argmax non cambia. Non è stata misurata l'ampiezza delle variazioni dei
singoli valori di output: stabilità dell'azione non implica rete numericamente
identica o geni inutili.

Interno:69 cambi di azione su1507 occasioni successive a fallimento(4,58%),
distribuiti su5 genomi. Bordo:78/1604(4,86%),6 genomi. Gli altri fallimenti
sono seguiti dalla stessa azione. Una nuova azione può comunque fallire o
derivare da RANDOM/oscillatore/età; questi rapporti non sono successi di
evitamento. Richieste con bersaglio nullo:30 interno/bordo/barriera,31 occupati;
altri fallimenti0. Le identità dei ledger tornano in ogni esposizione.

### 19.3 Figli mutati e confronto appaiato

240 figli,95811 geni ereditati totali,4839 sostituzioni effettivamente diverse
(5,051%); media20,1625 cambi/figlio, intervallo1…51. La mutazione reale
sostituisce un gene con nuova connessione/peso casuali; non modifica un singolo
bit né aggiunge/rimuove geni (`src/genome.cpp:29`, `src/gene.cpp:41`).

Copie senza mutazione: **identità esatta in tutti i480 confronti contestuali**,
comprese azioni, percorsi, input iniziali e misure fisiologiche.
Figli mutati:195/240(81,25%) mantengono le sequenze parentali intere in **tutti
i6 contesti**;45/240 cambiano almeno una sequenza.205/240 mantengono la prima
azione parentale in ogni contesto.210/240 hanno la stessa sequenza tra tutti
i contesti della propria prova,216/240 la stessa prima azione.

| Contesto | Figli con prima azione diversa da interno /240 | Figli con sequenza diversa da interno /240 | Nuove celle medie | Figli con più / meno / stesse nuove celle del genitore |
|---|---:|---:|---:|---|
|Interno|0|0|3,579|9 /15 /216|
|Bordo|19|24|2,725|8 /7 /225|
|Barriera fisica|0|5|2,492|6 /9 /225|
|Celle occupate|6|11|2,596|7 /8 /225|
|Risorse est|0|0|3,579|9 /15 /216|
|Risorse ovest|0|0|3,579|9 /15 /216|

Interno206/240 figli usano una sola azione,142/240 non richiedono movimento.
147 cambi dopo blocco su4200 occasioni(3,50%). Le mutazioni possono alterare
comportamento ed esplorazione, ma in questo pannello dominano azioni conservate
e bassa sensibilità al contesto. I miglioramenti di copertura non sono ancora
miglioramenti di fitness; nessuna selezione dei figli è stata eseguita.
Nel contesto interno6 figli iniziano a richiedere movimento rispetto a un
genitore che non lo richiede;10 perdono il movimento.26/80 genitori e73/240 figli
visitano più di4 nuove celle. Sono conteggi del pannello, non probabilità generali
di innovazione o una stima di selezione direzionale.

### 19.4 Risorse: differenza ecologica senza risposta neurale osservata

Est contro ovest: **0/80 genitori e0/240 figli cambiano input iniziali, sequenza
di azioni o percorso**, pur acquisendo quantità diverse di glucosio. Media di
assorbimento automatico cumulato per genitore366,75 est contro89,9125 ovest;
figli361,079 est contro94,371 ovest. Questa misura conta solo auto-take prima di
update, non i GET_GLUCOSE aggiuntivi eventualmente eseguiti come azione.
Tutti gli agenti sono vivi a240 tick: niente selezione o censura differenziale
per morte in questo pannello breve. Diverse riserve e fisiologia possono
tradursi in fitness più tardi senza essere percepite direttamente dalla rete.

Evidenza statica concordante: sensori glucosio33…36 fuori dall'indirizzamento
a5 bit (`src/neuron.h`, `src/gene.h:38`); input diretti di energia/fame assenti.
Non è possibile con la sola mutazione attuale raggiungere quegli indici.
Questa verifica conferma **insensibilità diretta alla distribuzione di glucosio
nelle condizioni provate**, non incapacità di evolvere esplorazione aperta,
seguire un segnale FOOD correlato o beneficiare di selezione energetica.
Learning attivo, termica diversa e signaling non sono valutati qui.

Barriera di carbonato: movimento impedito ma nessun input iniziale cambia,
perché la densità misura occupanti, non quel materiale. Occupanti:37 genitori
ricevono input iniziali diversi,3 cambiano prima azione. Un ostacolo può quindi
essere reale per l'esecuzione e invisibile ai sensori disponibili. Il singolo
cambio di sequenza successivo nella barriera non dimostra percezione del calcio:
posizione/direzione e storia delle azioni possono cambiare dopo il blocco.

### 19.5 Valutazione aggiornata e prossima discriminante

- **Variazione utile:** presente ma limitata nel pannello; molte sostituzioni
  genetiche non cambiano argmax/azioni. Insensibilità al glucosio è ora verificata
  dinamicamente in questo controllo. Evitamento flessibile di bordo/ostacoli
  resta raro; non attribuire automaticamente ogni cambio dopo blocco al contesto.
- **Ereditabilità:** copia genetica fedele dimostrata e molte azioni persistono
  anche con5% di sostituzioni. Non è sostenuta l'ipotesi che ogni riproduzione
  distrugga necessariamente il comportamento. Stabilità di un comportamento
  poco utile non equivale a ereditabilità di una strategia adattiva complessa.
- **Selezione:** il confronto precedente mostra un vantaggio possibile
  dell'esplorazione. Questa prova non misura figli ecologici o crescita di
  frequenze: non permette di escludere selezione debole nell'ecosistema normale.

Il candidato principale nelle condizioni controllate è **accesso all'informazione
ecologica e generazione di controller contestualmente utili**, più che perdita
universale dei comportamenti all'eredità. Il collo generale resta condizionale:
la termica delle run normali può eliminare individui prima della maturità.
Nessuna modifica a sensori/rete/costi è stata introdotta.

Prossima discriminante: selezionare dal pannello esistente genomi con maggiore
copertura e copie mutate che la conservano, confrontarli con genomi immobili
in condizioni termiche uguali fino a maturità, con nascite reali e cap non
saturato. Separare adattamento tramite esplorazione non guidata da vera risposta
a risorse. Qualsiasi esperimento di espansione dei sensori sarebbe una modifica
architetturale distinta, da autorizzare e confrontare con controllo invariato.

### 19.6 Riproducibilità, artefatti e limiti

Prima raccolta21,361s; replay20,994s. **2400/2400 trial identici integralmente**
al replay con geni estratti congelati. Fermati dopo questo controllo: ulteriori
tick non renderebbero i sensori del glucosio raggiungibili e non dimostrerebbero
selezione. Dataset in `/tmp/popx_context_results.json`, summary in
`/tmp/popx_context_summary.json`, replay in `/tmp/popx_context_replay.json`.
Sono artefatti temporanei, non file versionati o archivio permanente: conservarli
prima della pulizia di `/tmp` per rieseguire lo stesso pannello mutazionale.
Persistenza in repository limitata ai documenti autorizzati.
Archivio temporaneo `/tmp/popx_context_verification_2026-10-14.tar.gz`(332KiB):
harness, analizzatore, dataset congelato, summary e manifest con INI completo,
hash dei sorgenti/configurazione, commit, compilatore, backend e tempi.
Non contiene un nuovo eseguibile o dipendenze e resta soggetto alla pulizia di `/tmp`.

SHA256 dataset: `878fdf5b67a24c2a42b67dbcfc1cb5d0bc74f4bb1217c4c25fd4b599f3b1da32`.
Harness: `adb47c0441b01d8176cd58ea48859e0d7fa1f92a84b483e3e0d2b182bef748a3`.
Analizzatore: `9ab420c2345eb70cd845843b4d5766af50d7bd3f851842dfe33812c7afe2815d`.

```sh
cmake --build build-release --target popx -j 2
clang++ -std=c++23 -O2 -Isrc -Ibuild-release/_deps/eigen3-src -Ibuild-release/_deps/nlohmann_json-src/include /tmp/popx_context_probe.cpp build-release/src/CMakeFiles/popx.dir/{brain,cell,config,console_logger,gene,genome,grid_world,pop,pops_manager,random_utility}.cpp.o -ltbb -lstdc++fs -o /tmp/popx_context_probe
/tmp/popx_context_probe data_out/audit/corrected_pilot.json /tmp/popx_context_results.json
python3 /tmp/analyze_context_probe.py
# Replay congelato: usare un output distinto, non sovrascrivere il dataset letto
/tmp/popx_context_probe /tmp/popx_context_results.json /tmp/popx_context_replay.json replay
```

La prima raccolta genera nuovi figli non identici a quelli documentati se
rieseguita; soltanto replay del dataset congelato mantiene il pannello originale.
Analizzatore verifica ledger dei tentativi e identità parenti/copie; replay
verificato mediante uguaglianza di tutti i genotipi e tutti i trial.

Limiti: un solo seed di comportamento per famiglia, tre figli per genitore,
esposizione240 tick prima della maturità, sola componente genetica congelata,
assenza di segnali, tratti fisiologici artificialmente uguali tra genotipi,
nessuna donazione/riproduzione ecologica, ostacoli statici, nessuna competizione
o selezione di frequenze. Non sono stime della diversità di tutta la popolazione
possibile né della probabilità di adattamento a lungo termine.

## 20. Codifica genetica e domini configurabili

Fotografia prima del completamento dei feromoni: i37 input qui riportati sono
diventati49 nella sezione21, con domini automatici49/21. I test a37 input restano
controlli di compatibilità della configurazione ridotta.

### Implementazione corrente

Modifica esplicitamente autorizzata dopo la verifica contestuale; base Git
`015d3f4da0e2c19628bb6b2afbb9d7d095e408f7` più correzioni e modifiche nel working
tree. `Gene::source_num_` e `sink_num_` sono uint32 completi, senza bitfield a5 bit
né troncamento implicito. Pesi restano int16/scala8192, tipi layer a3 bit,
output type7, hidden1…6 e feedforward invariati.

`GeneEncoding` definisce due conteggi di dominio. `Genome` conserva l'encoding,
lo usa alla creazione e nella mutazione; `mutated(rate, encoding)` permette di
applicare i limiti correnti senza riscrivere i geni non mutati. La mutazione
resta sostituzione casuale del gene intero al tasso configurato, non bit mutation.
`Pop` deriva limiti da IConfig per fondatori e figli, anche nella via già
esistente dei pesi appresi. Non cambiata la politica di eredità lamarckiana:
`hebbian_inheritance=true` nell'INI continua a richiedere valutazione separata.

| Parametro INI | Valore corrente | Significato |
|---|---:|---|
|`brain_size_s`|37|Dimensione input attivi, indici0…36; glucosio33…36 e32 riservato.|
|`brain_size_n`|16|Dimensione di ogni hidden layer.|
|`brain_size_y`|21|Dimensione output attivi, azioni0…20. Con23 è raggiungibile KILL_FORWARD22;21 riservato.|
|`genome_source_index_count`|0|0 risolve max(input,hidden); valore esplicito N genera sorgenti0…N−1.|
|`genome_sink_index_count`|0|0 risolve max(output,hidden); valore esplicito N genera destinazioni0…N−1.|

Dominio risolto corrente37/21. I conteggi espliciti possono essere minori o
maggiori della rete: **limitano i nuovi geni e le sostituzioni**, mentre le
dimensioni della rete limitano i geni eseguibili. Endpoint fuori layer vengono
conservati silenti da `Brain::wire`, senza modulo o rimappatura. Ridurre un
conteggio genetico non altera gli endpoint già ereditati; ridurre `brain_size`
li rende silenti nella nuova rete. Valori di conteggio risolti1…INT_MAX,
configurazione0 automatica; valori invalidi rifiutati. Input/attuatori non
implementati non nascono aumentando una dimensione: input sconosciuti0,
output oltre l'ultima azione esistente non eseguono un'azione nuova.

I costruttori standalone senza encoding conservano il dominio storico32/32
come compatibilità API, **non come limite della rappresentazione**. La via
produttiva usa esplicitamente `Genome::encoding_from_config`.
La configurazione si modifica nel file e si applica ricreando simulazione/reti
al riavvio: il loop corrente non fa reload/rewire automatico degli agenti vivi.

### Compatibilità e strumenti

Preservati gli indici0…31 dei sensori e tutti gli indici delle azioni, inclusi
glucosio33…36 e KILL_FORWARD22. Vecchi slot sentinella32(input) e21(output)
ora nominati RESERVED_INPUT/RESERVED_ACTION; NUM_SENSES37 e NUM_ACTIONS23 sono
conteggi corretti, non numeri da usare come connessioni funzionali.
I vecchi geni con endpoint0…31 mantengono identico significato. Pannelli storici
che non contengono connessioni al glucosio non acquisiscono da soli quelle
connessioni: generazione/mutazione ora possono produrle.

Export dei pesi senza cast a16 bit degli indici; snapshot Brain JSON contiene
`gene_encoding.version=2`, `index_bits=32` e conteggi risolti. Il formato delle
connessioni row/col resta leggibile dagli strumenti esistenti; nessun caricatore
binario di Gene è stato identificato. Il layout C++ del gene cambia: non assumere
compatibilità di dump binari esterni non documentati. `scripts/plot_net.py`
aggiornato con etichette glucosio, slot riservati e azioni12…22 allineate all'enum.

`Genome::add_gene` aggiorna il colore genetico; eliminato contatore di lunghezza
ridondante non aggiornato dall'inserimento, usa dimensione del vector; riserva
memoria per il numero iniziale di geni. Colore resta una visualizzazione compressa,
non una misura completa di distanza genetica.

### Validazione eseguita

- Build Release target `popx`: compilazione e link riusciti.
- `tests/genome_encoding_regression.cpp`: PASS. Campo senza troncamento anche
  sopra65535; domini automatici37/21 e70/65; limiti espliciti e invalidi INI;
  sostituzioni mutate limitate; copie senza mutazione e figli nel percorso reale;
  gene input64→output60 con esecuzione feedforward; export dei pesi/snapshot
  conserva indici64/60 e metadati; layer piccoli rendono gene silente e ampliati
  lo riattivano; gene legacy0→4 mantiene significato.
- Fixture Pop con input glucosio35→est4 e34→ovest5: cambiando distribuzione
  delle risorse cambia decisione e movimento eseguito, senza policy override.
  È rete costruita diagnosticamente, non una rete emersa per evoluzione.
- Attuatore esistente22 verificato fino all'esecuzione con output23 nella fixture;
  configurazione produttiva rimane21, senza nuova attivazione della predazione.
- Regressione precedente `tests/pop_cycle_regression.cpp`: PASS, ordine delle
  fasi e direzione iniziale conservati. Sintassi Python plotter verificata.

Comandi riproducibili in `AGENTS.md`, sezione codifica genetica configurabile.
CTest non registra questi controlli standalone. Nessuna lunga run evolutiva
eseguita dopo la modifica, nessuna prova di miglioramento della fitness.

La nuova distribuzione casuale degli endpoint (37/21 invece di32/32 nella via
produttiva corrente) modifica probabilità di geni silenti/connessi. Per un
controllo con distribuzione legacy impostare input32 e domini32/32, preservando
uscite21/hidden16; non confrontare run non seedate come effetto causale della
sola accessibilità al glucosio. Priorità successiva: misura del tasso con cui
mutazioni producono connessioni risorsa→azione utili e della loro ereditabilità,
poi confronto di fitness con termica controllata, senza reward artificiale.

## 21. Feromoni, comunicazione e nuova verifica dinamica

### 21.1 Stato dei meccanismi e modifiche autorizzate

Data14 ottobre2026, base Git invariata più modifiche del working tree. Il developer
ha richiesto completamento delle logiche mancanti prima della prova. Le quattro
emissioni/local sensing e FOOD direzionale erano già funzionali e normalizzati;
**non** è stato trovato il presunto errore raw/normalizzato tra le due quantità.
Asimmetria completata: DANGER/MATE/HOME mancavano dei quattro gradienti cardinali.
Nuovi sensori37…40 DANGER,41…44 MATE,45…48 HOME; indici esistenti invariati,
NUM_SENSES49, `brain_size_s=49`, domini genetici automatici49/21. Uscite21.

| Parte | Comportamento corrente verificato |
|---|---|
|Emissione|Azioni8…11 depositano70…100 nella cella corrente; ora `random_util_` del Pop invece di una nuova RNG non controllabile a ogni emissione. Distribuzione nominale invariata.|
|Campo|`Cell::set_feromone` somma e clampa0…100; rifiuta tipo sconosciuto e valori non finiti. Nome storico non implica assegnazione assoluta.|
|Decadimento|Moltiplicazione0,9995 a ogni update; soglia0,01 per cancellazione. Emissioni ripetute possono saturare il campo.|
|Trasporto|Nessuna diffusione/avvezione aggiunta. Il deposito è locale; la scansione sensoriale può rilevarlo a distanza.|
|Locale|Sensori20…23, concentrazione/100 in0…1.|
|Direzionale|FOOD24…27 e nuovi37…48; media di quattro celle lungo ciascun asse, gradiente clamp(0,5+0,5×(media−locale),0,1). Campo uniforme→0,5.|
|Semantica|Nomi dei canali non codificano comportamenti: nessuna regola automatica "trova glucosio→emetti FOOD" o "DANGER→fuggi". Sono associazioni che il sistema genetico/neuronale può produrre.|

Completare il canale non equivale a insegnare un protocollo comunicativo utile.
Hebbian learning modula i pesi già presenti con il reward esistente; non è un
addestratore supervisionato di codici condivisi. Nessun premio a movimento,
esplorazione, emissione o aggregazione introdotto. Nessuna diffusione o attrazione
sociale prescritta. Finitezza del campo è protezione numerica, non la spiegazione
stabilita dell'immobilità precedente.

### 21.2 Test funzionali prima dell'interpretazione biologica

`tests/pheromone_regression.cpp`: PASS. Per tutti i4 tipi emissione reale mediante
rete genetica→campo→sensore locale normalizzato e isolato dagli altri canali;
16 combinazioni tipo/direzione→argmax corrispondente; avvicinamento reale a
emittente; campo uniforme senza falso gradiente; saturazione, delta negativo,
NaN ignorato, decadimento. Nessuna action override nelle verifiche neurali.
Rieseguiti PASS anche `genome_encoding_regression` e `pop_cycle_regression`;
build Release compilato e linkato. CTest non registra i tre controlli standalone.

Rettifica della sezione19: `set_feromone(...,0)` lasciava i segnali presenti.
Nuove fixture cancellano sottraendo il valore effettivo di ogni tipo/cella.
La rettifica limita l'interpretazione del vecchio isolamento dichiarato;
non cancella i risultati osservati né inventa un errore di normalizzazione.

### 21.3 Run ordinaria con configurazione produttiva

Eseguito `bash scripts/run_audit.sh /tmp/popx_signal_native_pilot.json 2000`:
mondo120×120 inizializzato dalle RBF reali,80 agenti casuali, INI corrente,
learning/plasticità e politica di eredità effettive, rigenerazione e loop reali.
Stop al budget2000, tempo misurato0,992s, no confronto causale con vecchie run
non seedate. Tratti/ambiente termico variabili; nessuna forzatura delle azioni.

| Misura | Risultato |
|---|---:|
|Richieste movimento / riuscite|6423 /284(4,42%)|
|Emissioni FOOD /DANGER /MATE /HOME|1050 /2921 /209 /2105|
|Nascite naturali /forzate|47 /0|
|Morti termiche /energetiche|76 /14|
|Vivi finali|37|
|Residui sei bilanci materiali|0 /0 /0 /0 /0 /0|
|Ledger energia e controlli risorse|Validazione PASS|

Forte mortalità termica e molti movimenti non eseguiti persistono. Molte emissioni
non provano ricezione, significato o coordinamento. Nell'assetto iniziale solo
8/80 agenti hanno un altro agente lungo un raggio cardinale entro4 celle;
media0,1 vicini per agente. È una misura geometrica della visibilità potenziale
iniziale, non conteggio di messaggi ricevuti nell'intera run. Densità/raggio e
fase iniziale possono limitare il contatto; non giustificano inventare gruppo.

### 21.4 Controllo positivo di gruppo, distinto dall'emergenza

Harness `/tmp/popx_signal_probe.cpp`. Quattro esperimenti, uno per tipo, con
emittente centrale e4 riceventi inizialmente a distanza2 sui quattro assi.
Genomi **costruiti diagnosticamente**: emittente con uscita costante del proprio
tipo, riceventi con gradienti di quel tipo→movimento nelle quattro direzioni.
Emittente effettua una emissione reale prima della prima sense dei riceventi;
poi ciclo normale30 tick, learning congelato, niente nascite, nessuna override
dell'azione. Deposito emittente occupato: riceventi si fermano nelle celle adiacenti.

| Canale | Riceventi adiacenti con segnali /4 | Con campo cancellato /4 | Distanza media finale con /senza |
|---|---:|---:|---|
|FOOD|4|1|1 /4,912|
|DANGER|4|1|1 /4,912|
|MATE|4|1|1 /4,912|
|HOME|4|1|1 /4,912|

Risposta collettiva causale possibile con un protocollo genetico coerente.
**Non appresa né evoluta spontaneamente**: la relazione segnale→movimento è
stata codificata per testare la catena. Senza segnali i riceventi scelgono una
direzione per argmax/tie e uno resta adiacente per blocco: baseline non è zero.

### 21.5 Popolazioni casuali a termica controllata e ablation dei segnali

Stessi80 genomi iniziali nuovi della run nativa, PHY uguale(1,0,1,80), corpo/ambiente
37°C, quota31 piatta, lazyness0, direzione est, energia100 e riserve dei fondatori
reali. Posizioni sparse `(1+2×(serial%8),2+2×floor(serial/8))`, nessun vicino entro
una cella all'inizio. Griglia24×24. Acqua/O₂1000/cella iniziali, finiti; niente
rigenerazione di glucosio, cloroplasti0; totale glucosio19200 su64 celle da300.
Campo segue decadimento0,9995, senza world update climatico/rigenerazione.

`PopsManager::update_cycle` reale, inclusi bucket/fisiologia/FIFO/cap e nascite;
NewGen disabilitato nella fixture, plasticità **attiva**, eredità Hebbian **false**
solo nella fixture per separare trasmissione genetica da pesi acquisiti. L'INI
produttivo resta Hebbian inheritance true: confronto non equiparabile alla GUI.
RNG Pop seed1729/4361+serial, RNG emissioni ora condivisa con Pop; C RNG rand()
resettata per blocco. Geni mutati/fisiologia riproduttiva usano RNG autonome:
run con nascite non garantite riproducibili globalmente.

Quattro prove600 tick, due seed×segnali presenti/cancellati. Età inferiore a1000:
nessuna nascita/morte/forzatura,80 vivi in tutti i controlli. Mutazione estratta
prima e genomi identici tra condizioni, nessuna estrazione dopo nascita qui.
Replay con stessa configurazione e corpus: identità esatta di tutti i campi
(eccetto wall_seconds) nei4 confronti e negli8 controlli di gruppo.

**Risorse lontane**, patch x16…23,y8…15:0 ingressi/0 prelievo netto/0 nascite anche
nel pilot2200,80 vivi. Con segnali cambiano sequenze di3/80 e4/80 genomi ai due seed,
ma celle visitate236/255 identiche ai controlli senza segnali. Non prolungata:
mancata acquisizione non consente di valutare fitness del resource-seeking.

**Risorse vicine**, stessa quantità e stessa coorte, patch x8…15,y8…15;16 fondatori
partono nella patch e altri sono nel raggio sensoriale. Non è confronto a uguale
accesso iniziale tra lontano/vicino: è controllo distinto della visibilità/accesso.

| Seed | Segnali | Movimenti riusciti | Cambi di azione | Celle distinte famiglia | Prelievo netto glucosio | Ingressi patch | Frazione con vicino adiacente finale |
|---|---|---:|---:|---:|---:|---:|---:|
|1729|Cancellati|2091|153|236|8385|8|0,5875|
|1729|Presenti|2099|156|236|8385|8|0,5875|
|4361|Cancellati|1952|147|266|8025|10|0,5125|
|4361|Presenti|1924|155|259|8040|8|0,5500|

Sequenze diverse con segnali per4/80 e5/80 genomi. Effetto causale sulla scelta
di alcuni individui nel controllo appaiato, **senza vantaggio coerente di
esplorazione/acquisizione tra seed**. La geometria e le interazioni possono
propagare cambiamenti dopo una prima risposta; non tutte le differenze sono
un singolo agente che segue un messaggio. Clustering aumenta anche senza
segnali: aggregazione spaziale da sola non prova comunicazione/cooperazione.

### 21.6 Run breve con maturità e nascite, risorse vicine

Un pilot2200 tick con segnali e seed1729, configurazione controllata appena
descritta,0,881s misurati; mutazione reale0,05, nascite normali. Fermato al budget
anche perché cap satura: non testare efficacia a lungo termine su un plateau.
Tratti identici per i fondatori; i figli seguono la mutazione fisiologica reale
di `make_offspring_phy`, oltre a quella neurale, e la propria inizializzazione.
Il controllo a tratti rigorosamente uguali riguarda dunque i fondatori e le
prove600 tick senza nascite, non tutti i discendenti della prova2200.

| Misura | Risultato |
|---|---:|
|Richieste movimento / riuscite|31176 /9032(28,97%)|
|Passi che riducono distanza geometrica dalla patch|3012|
|Ingressi nella patch|101|
|Celle visitate dall'insieme|569/576|
|Prelievo netto glucosio ambientale|18250/19200|
|Nascite naturali /forzate|48 /0|
|Vivi finali /morti termiche /energetiche|120 /0 /8|
|Tick al cap120|967|
|Individui con cambi di azione, inclusi figli|45/128 osservati|
|Frazione con vicino entro1 cella, iniziale→finale|0→0,8167|

Tutti80 fondatori vivi e osservati per2200 tick, quindi confronto dell'associazione
non confuso da durate diverse dei fondatori.36 hanno avuto almeno uno spostamento
riuscito:45 figli diretti,6 genitori riproduttivi, media1,25 figli/fondatore mosso.
44 senza spostamenti:0 figli. Le3 nascite restanti sono discendenti di generazioni
successive.8 fondatori visitano più di4 nuove celle: grandi distanze possono
ancora derivare da piccoli circuiti ripetuti.

**Associazione**, non causalità dimostrata: posizione iniziale rispetto alla
patch, genoma, GET_GLUCOSE/altre azioni e comportamento appreso possono influire
su movimento e riproduzione. La misura di prelievo è netta (glucosio iniziale
meno finale nel mondo), non assorbimento lordo per individuo; restituzioni e
LEAVE_GLUCOSE sono comprese. Non attribuire tutti i3012 passi al glucosio percepito:
una direzione casuale può avvicinare geometricamente la patch. Il pilot lungo
non ha ablation appaiata2200 per fitness: mutazioni non seedate e cap renderebbero
ambiguo un singolo confronto tra famiglie divergenti.

### 21.7 Conclusione e priorità

Ora sono verificate catene risorsa→sensore→azione e, per tutti i tipi,
emissione→campo→gradiente→decisione→movimento. Produzione non impone un codice
sociale: la coorte casuale produce alcune scelte sensibili ai segnali ma non
dimostra un protocollo comunicativo vantaggioso o gruppi cooperativi evoluti.
La selezione può associare movimento/accesso a riproduzione nel controllo
termico; ambiente normale continua a mostrare forte mortalità e pochi movimenti
eseguiti. Informazione disponibile non garantisce che l'argmax usi quella
informazione in modo utile, né che Hebbian learning insegni un linguaggio.

Prossima discriminante: misurare l'associazione tra emissione e presenza reale
di risorse e la risposta dei riceventi al segnale, poi confrontare discendenza
di linee emittenti/riceventi con segnale ablated usando un pannello mutazionale
pre-estratto, controllando posizione iniziale e cap. Definire prima "gruppo"
come coordinamento causale persistente, distinto da vicinanza o nascita adiacente.
Non aggiungere attrazione sociale/premi per ottenere un gruppo visivamente bello.

### 21.8 Artefatti e riproduzione

Harness `/tmp/popx_signal_probe.cpp`, dati `/tmp/popx_signal_native_pilot.json`,
`/tmp/popx_signal_results_final.json`(patch lontana),
`/tmp/popx_signal_results_near.json`(vicina),
`/tmp/popx_signal_paired_replay.json`(replay600), archivio temporaneo
`/tmp/popx_signal_verification_2026-10-14.tar.gz`. Conservare prima della pulizia
di `/tmp`. Corpus integrale degli80 genomi fondatori incluso nei dati; snapshot
iniziale ambiente e fisiologia incluso nel pilot nativo. Il pilot controllato
registra misure individuali e nascite, ma non tutti i geni/tratti dei figli:
la ricostruzione genetica completa della sua traiettoria non è disponibile.
Le condizioni nuove non sono misure di
miglioramento causale rispetto a run storiche non appaiate.

```sh
cmake --build build-release --target popx -j 2
bash scripts/run_audit.sh /tmp/popx_signal_native_pilot.json 2000
clang++ -std=c++23 -O2 -Isrc -Ibuild-release/_deps/eigen3-src -Ibuild-release/_deps/nlohmann_json-src/include /tmp/popx_signal_probe.cpp build-release/src/CMakeFiles/popx.dir/{brain,cell,config,console_logger,gene,genome,grid_world,pop,pops_manager,random_utility}.cpp.o -ltbb -lstdc++fs -o /tmp/popx_signal_probe
/tmp/popx_signal_probe /tmp/popx_signal_native_pilot.json /tmp/far.json
/tmp/popx_signal_probe /tmp/popx_signal_native_pilot.json /tmp/near.json near
/tmp/popx_signal_probe /tmp/popx_signal_native_pilot.json /tmp/paired.json near paired
```

Rigenerare il pilot nativo cambia genomi/ambiente e non riproduce questi numeri.
Replay600 richiede il corpus salvato, stessa build/config e seed. Il completamento
espande la distribuzione genetica automatica da37 a49 sorgenti: isolare questa
modifica da clima, densità, geometria e learning nelle comparazioni future.
Nessuna lunga evoluzione eseguita, nessun risultato sociale inventato.


## 22. Termica e consumo; continuità spaziale e thread — 14 ottobre 2026

Vedere [indagini termiche storiche](#indagini-termiche-storiche):
360 controlli deterministici con bilanci e replay. Isolamento lipidico e calore
respiratorio possono causare morte prima della maturità; risorse abbondanti non
eliminano questo ostacolo e possono alimentarlo. Nessuna modifica termica globale.
Su richiesta: mutazione INI0,01, NewGen senza riposizionamento/riassegnazione fasi,
num_threads effettivo nell'arena TBB di sense/think. Eredità dei pesi mantenuta true
per esplicita preferenza attuale dell'utente, che aggiorna quella precedente.
Build, regressioni fasi/posizioni/thread e probe di conservazione PASS. Le descrizioni
precedenti di par_unseq, num_threads ignorato e teletrasporto sono ora storiche.


<a id="audit-dinamico-storico"></a>
## Appendice: Audit dinamico storico

### PopX — analisi dinamica breve e correzioni dei bilanci

Data: 14 ottobre 2026. Revisione di partenza:
`015d3f4da0e2c19628bb6b2afbb9d7d095e408f7`, più le modifiche descritte qui.
Configurazione `config/popx.ini` invariata. Il developer ha autorizzato la run,
la correzione degli scompensi e la donazione di **1/8 anche di energia, O2 e lipidi**.

## Conclusione

La variazione delle scelte neurali è osservabile e la copia genetica senza
mutazione preserva le scelte nel controllo comune. Non è dimostrata efficacia
adattiva dell'ecosistema. Il problema più evidente in queste prove è la
**sopravvivenza termica prima della maturità**, che restringe fortemente i candidati
alla selezione riproduttiva. Ereditabilità sotto mutazione/plasticità e traduzione
del movimento in fitness richiedono ancora confronti controllati.

Sono state eliminate creazioni gratuite di risorse/energia e incongruenze di
trasferimento e di contabilizzazione. Il ledger finale distingue reazioni,
rigenerazione, trasferimenti, capacità delle celle, morti e arrotondamenti.

Le run non sono state prolungate oltre il budget breve: continuare senza controllo
delle RNG e senza un confronto causale di strategie non avrebbe dimostrato adattamento.

## Protocollo ed esecuzioni effettive

Harness [tools/popx_audit.cpp](../tools/popx_audit.cpp), collegato agli oggetti
Release del simulatore corrente. Nessuna GUI, modifica di genomi durante la run,
ricompensa di movimento o variazione dell'INI. Usa `Simulator::init/update` con
PopsManager/Pop/Brain/Cell di produzione. `AuditWorld::update_cycle` misura le
rigenerazioni attraverso somme prima/dopo la chiamata alla versione di produzione.

L'harness conserva riferimenti agli individui morti per registrare età, prole e
contatori finali. Non li reintroduce nel mondo; serializzazione reti disattivata
nell'INI. Traccia richieste di movimento prima della fase act e distanza dopo il
tick, genealogia e nascite normali/forzate. Campioni ogni100 tick, tutti gli eventi
di nascita, tutti gli individui incontrati, genomi e ambiente iniziale dove previsto.

RNG di produzione ancora da `random_device`: **non sono run a seed riproducibile**.
Il seed C 20261014 controlla soltanto `rand()`. Ambienti/genomi differenti fra i
pilot: **nessuna differenza prima/dopo è una stima causale dell'effetto delle fix**.
Il JSON include INI, variante, revisione e questa limitazione. Anche l'ordine dei
probe prima della run fa parte del protocollo; le fixture sono separate dall'ecosistema.

| Esecuzione | Tick | Condizione e risultato |
|---|---:|---|
| Preflight iniziale | 0 |80 vivi,0 occupanti corrispondenti: arresto immediato. Corretto init mondo prima dello spawn. |
| Pilot con vecchi trasferimenti |2000 | Init corretto, risorse ancora difettose; termine al budget.76 nascite,74 normali e2 forzate. |
| Pilot dopo correzioni risorse |2000 | Termine al budget;73 nascite,71 normali e2 forzate. Bilancio materiale senza residui non spiegati. |
| Validazione contabile finale |200 | Solo controllo della precisione dopo accumulatori double/roundoff; non prova evolutiva. Ledger materiale ed energetico senza residui non spiegati. |
| Fixture trasferimenti e NewGen | nessun ciclo ecosistema | Test prima/dopo, compreso riposizionamento impossibile; controlli finali PASS. |
| Fixture termiche | massimo 150 tick/individuo |18 combinazioni ambiente/mitocondri/lipidi, senza azioni o learning; diagnostica fisiologica. |

Il timer dei pilot misura loop e assay finali; esclude init/compilazione/rendering
e serializzazione del JSON. Le durate di 2,81 e 1,06 secondi dei due pilot non sono un benchmark
comparabile. Budget max 2000 tick,30 secondi wall-time; arresto anticipato per
estinzione, invariante griglia violato, probe risorse fallito o assenza di nascite
normali oltre la finestra di maturazione (da 1200 tick). Non è stata eseguita una
nuova run lunga dopo la validazione contabile finale.

## Risultati osservati

| Misura | Vecchi trasferimenti, init corretto | Trasferimenti corretti |
|---|---:|---:|
| Fondatori |80 |80 |
| Firme di azione distinte sul pannello comune |68 |69 |
| Richieste di movimento eseguibili nel gruppo act |5495 |3671 |
| Richieste con distanza effettiva>0 |849 (15.45%) |656 (17.87%) |
| Morti termiche entro100 tick |53 |53 |
| Fondatori arrivati ad età > 1000 |18 |18 |
| Fondatori con almeno un figlio, inclusa forzatura |7 |12 |
| Nascite totali, esclusi fondatori |76 |73 |
| Vivi al termine |66 |42 |
| Morti termiche totali |90 |103 |
| Morti per energia totali |0 |8 |

La mortalità di 53/80 entro100 tick precede di molto la soglia riproduttiva
età > 1000. Il movimento è richiesto ed eseguito, ma molto spesso non produce
displacement. Non sono registrate tutte le cause del fallimento: non si può
ripartire il restante82.13% fra bordi, pendenza, occupazione, direzione o richiesta
random nulla. Il campione mostra casi1/2 del problema movimento distinguibili;
non dimostra un vantaggio di acquisizione o riproduzione (casi3/4).

I due53/80 sono osservazioni separate, non repliche equivalenti a seed abbinato.
Il cap120 non è la spiegazione esclusiva del risultato: nel pilot corretto la
popolazione resta sotto120 nei campioni. Non si esclude un blocco spaziale locale.
NewGen periodico50000 e refresh termico 5000 non sono raggiunti nei pilot; NewGen
è stato verificato separatamente nelle fixture.

### Variazione ed ereditabilità

Firma: 37 decisioni argmax per rete su un vettore zero, quattro vettori uniformi
0.25/0.5/0.75/1 e 32 vettori one-hot. Assay senza learning, identico per tutte le
reti. È una misura di differenza nelle **scelte neurali**, non fitness, output
completi o strategie efficaci; alcuni input sintetici non rappresentano uno stato
ecologico realmente incontrabile.

Nel pilot corretto:

- 69 firme diverse per80 genomi fondatori: assenza totale di variazione esclusa
  per questo controllo.
- Copia del genoma originale con mutazione 0: concordanza100% sui 37 input nei16
  individui testati. Questo verifica la fedeltà della codifica/copertura dell'assay.
- Figli realmente nati: concordanza media85.60% con il pannello del genoma originale
  del genitore,73 coppie. Include eredità dei pesi appresi, mutazione e possibili
  rate aumentati nel regime di bassa popolazione; non isola le loro cause.
- Assay supplementare16 individui: genoma appreso/originale99.32% di concordanza
  media; figlio diagnostico/originale87.84%, rate 0.05. Campione scelto per ordine
  lessicografico degli ID, non rappresentativo o casuale; comprende stati di vita diversi.
- 3 nascite normali sono da nonfondatori: trasmissione e riproduzione di discendenti
  sono avvenute, ma sono troppo poche per stabilire un trend evolutivo.

Resta `hebbian_inheritance=true`; nessuna politica lamarckiana è stata modificata.
Copia fedele del genoma originale non dimostra identico comportamento nella vita,
che dipende da stato, tratti, ambiente, random, plasticità e opportunità.

### Selezione e correlazioni

Nei80 fondatori del pilot corretto, Pearson distanza cumulativa–figli=0.0462;
con distanza/età diventa−0.1763; fra soli 18 fondatori maturi, distanza/età–figli=0.1063.
Sono statistiche descrittive, non test di causalità: censura dei vivi, habitat,
tratti e durata delle opportunità riproduttive non sono pienamente controllati.
Il cambiamento con la normalizzazione mostra perché il solo indicatore grezzo
non identifica un vantaggio del movimento.

Un limite di credito dell'apprendimento resta nel codice: reward e learning sono
calcolati in `Pop::update` prima della riproduzione del manager. Il delta energetico
del reward non include quindi la donazione e il costo riproduttivo successivi;
il livello energetico li riflette nel tick seguente. È un limite architetturale
non misurato come causa del risultato e non è stato modificato arbitrariamente.

Non si può attribuire l'andamento demografico alle correzioni da due run non
abbinate. Si può invece dimostrare causalmente nelle fixture che, prima delle
correzioni, un singolo figlio aumentava energia globale di 100, O2 e lipidi di20:
fitness e crescita erano sostenute anche da una fonte artificiale non contabilizzata.

Grafico del singolo pilot e dei controlli termici separati:
[dynamic_summary.png](../data_out/audit/dynamic_summary.png).
I vivi al tick150 nelle fixture sono censurati;150 non è la loro durata di vita.

## Scompensi identificati e correzioni verificate

| Problema | Evidenza prima | Correzione/verifica finale |
|---|---|---|
| Deposito glucosio su cella vuota | Richiesta10,ritorno0,stock0. | Deposito10,ritorno10,stock10. |
| Depositi e cap | Acqua con2 posti liberi: aumenta2 ma ritorna10; somme unsigned potevano overflow. | Tutti i give_* restituiscono quantità accettata usando capacità residua; test cap e UINT_MAX su tutte le6 risorse. |
| LEAVE_* crea risorse | Glucosio/acqua/calcio: totale agente+cella aumenta10 senza consumo. | Si trasferisce min(10,riserva,capacità); totale invariato e quantità non accettata resta nell'agente. |
| CO2 respiratorio duplicato |1 glucosio e1 O2 consumati,2 CO2 creati. |1 CO2 prodotto; quanto accettato dalla cella viene rimosso dall'agente, resto trattenuto. |
| O2 fotosintetico perso a cella piena | Riscontro statico: give clampava e il resto non era conservato. | Prodotto1 O2 totale anche con cella piena, resto interno; fixture PASS. |
| Costo riproduttivo fittizio | Contatore+0.3,energia invariata. | Costo reale detratto e registrato; nessun costo su spawn fallito, nessuna nascita se non finanziabile. |
| Dotazione gratuita neonato |+100 energia,+20 O2,+20 lipidi per nascita. | Donazione1/8 di tutte le6 riserve e dell'energia residua dopo costo; materia globale invariata, energia totale−0.3. |
| Stock lipidico ignora tratto | Riscontro statico: uptake oltre adipose_stock_max. | Init/uptake rispettano capacità; test80 lipidi e30 in cella lascia entrambi invariati. |
| Morte/rilascio ripetibile | Riserve non decrementate, nessun guard contro die ripetuto. | Solo una restituzione, sottrazione degli importi accettati e resto disponibile al ledger come overflow terminale. |
| NewGen elimina senza restituzione | Riscontro statico del percorso di culling. | Usa die prima della rimozione; test10→9 senza perdita materiale. |
| NewGen fallisce riposizionamento | Fixture tutta bloccata:−900 G,−900 acqua,−180 O2,−180 lipidi;0 vivi ma1 morte contata. | Posizione precedente preservata su spawn fallito, restituzione riserve e conteggio di tutte10 morti; residui0. |
| Minimo superstiti superiore ai vivi | Precondizione clamp violata con3 vivi,minimo8. | Minimo limitato alla popolazione; fixture3→3 PASS. |
| Statistiche energetiche numeriche | Residuo0.06069 nel pilot lungo con cumulati float. | Cumulati double; change_energy registra arrotondamento reale del float; validazione 200 tick: differenza0.007195 interamente spiegata dal roundoff, residuo0. |
| Popolazione non registrata | Preflight80 vivi,0 occupanti. | Init ambiente prima dello spawn;80 corrispondenze nei pilot successivi. |

La donazione energetica è un trasferimento, **non dissipazione**: non si aggiunge
al contatore “reproduction loss”. La dissipazione è il costo 0.3 già previsto dal
codice/INI. Le riserve intere donate sono arrotondate per difetto, con resto al
mittente; l'energia è divisa come float. Cambia quindi realmente la demografia
rispetto al modello precedente con dotazioni gratuite.

## Ledger e perdite esplicite

Ordine vettori: glucosio,acqua,calcio,CO2,O2,lipidi. Totali includono mondo e
agenti vivi; i morti sono mantenuti solo per misurare riserve terminali non accettate.
Equazioni nel modello semplificato attuale:

```text
Delta_G   = regen_G + fotosintesi - respirazioni - overflow_morti_G
Delta_H2O = -fotosintesi - overflow_morti_H2O
Delta_Ca  = -overflow_morti_Ca
Delta_CO2 = respirazioni - fotosintesi - overflow_morti_CO2
Delta_O2  = regen_O2 + fotosintesi - respirazioni - overflow_morti_O2
Delta_Lip = -overflow_morti_Lip
Delta_E = income_resp - basale - movimento - termoregolazione - riproduzione
          - surplus_cap - energia_dei_morti + arrotondamento_float
```

Sono conteggi delle unità del modello, non conservazione di atomi/massa fisica
con una stechiometria biologica completa. Non è stata introdotta una nuova chimica.

Pilot corretto 2000 tick: 55339 respirazioni,18072 fotosintesi,28769 G e28744 O2
rigenerati. Residui non spiegati delle6 risorse: **[0,0,0,0,0,0]**.
Costo riproduttivo realmente detratto21.9 (73×0.3); nessuna energia nascita gratuita.

Le capacità finite comportano ancora **uscite del sistema alla morte** quando la
cella non accetta tutte le riserve. Nel pilot corretto: [1229,2334,2066,7587,1500,333]
unità. Sono perdite quantificate separatamente, non trasferimenti conservativi
completi né discrepanze nascoste. Eliminarle richiederebbe decidere una regola
ulteriore per cadaveri, overflow o ridistribuzione; non è stata inventata durante
questa correzione. Le perdite vanno incluse nell'interpretazione ecologica della fitness.

Validazione contabile 200 tick sulla versione finale del ledger: residui materiali0,
energia iniziale 8000,energia finale vivi 1862.84810,roundoff+0.00719533,
**residuo energetico non spiegato0**. È una verifica contabile, non confronto
causale con il pilot2000 tick o prova che la popolazione abbia migliorato fitness.

## Diagnostica termica controllata

18 fixture con genoma vuoto, nessuna azione/learning, corpo iniziale25°C,
mitocondri 1/5/25,lipidi fissi 20/80,ambiente 0/25/37°C. Cloroplasti0 e nessun lipide
ambientale. Glucosio/O2 nella cella reintegrati per isolare la fisiologia termica;
l'assorbimento automatico resta5/tick, quindi25 mitocondri non implicano25
respirazioni sostenibili ogni tick. Nessuna riproduzione; orizzonte massimo 150 tick.

Esempi osservati:

| Ambiente | Mitocondri | Lipidi | Esito |
|---:|---:|---:|---|
|0°C |1 |20 | Morte per freddo al tick 3,corpo 6.63°C. |
|0°C |1 |80 | Vivo a150 tick,corpo 28.89°C. |
|25°C |5 |20 | Vivo a150 tick,corpo 28.97°C. |
|25°C |5 |80 | Morte per caldo al tick 41,corpo 75.66°C. |
|37°C |5 |20 | Vivo a150 tick,corpo 40.97°C. |
|37°C |5 |80 | Morte per caldo al tick 37,corpo 75.31°C. |

Questo isola un effetto **non neurale** forte: l'isolamento può proteggere dal
freddo e impedire di smaltire calore respiratorio. Non è automaticamente un bug,
né prova che una rete possa evolvere la strategia per gestirlo. Non modificati
heat,alpha,soglie di morte o parametri per “far funzionare” l'ecosistema.

## Artefatti e ripetizione dei controlli

Dati locali in `data_out/audit/` (directory ignorata da Git):

- `baseline.json`: fallimento iniziale e prime fixture dei difetti.
- `legacy_resources_pilot.json`: primo pilot valido con trasferimenti vecchi.
- `corrected_pilot.json`: pilot2000 tick con trasferimenti corretti.
- `accounting_validation.json`: verifica200 tick con contabilità finale.
- `newgen_failure_before.json`: regressione fallita prima della fix del riposizionamento.
- `conservation_regression.json`: fixture finali PASS e 18 prove termiche.
- `summary.json`: analisi descrittiva riproducibile dei JSON salvati.
- `manifest.json`: dimensioni e SHA256; `final_source_diff.patch`: modifiche di produzione.
- `dynamic_summary.png/svg`: figura statica generata da `scripts/plot_audit.py`.

Comandi, dalla root (Bash, toolchain/dependency cache già disponibili):

```sh
bash scripts/run_audit.sh /tmp/conservation_checks.json probes
bash scripts/run_audit.sh /tmp/short_pilot.json 2000
python3 scripts/analyze_audit.py data_out/audit
python3 scripts/plot_audit.py data_out/audit
```

Il primo ricontrolla fixture e termica senza run ecosistemica. Il secondo esegue
un **nuovo** pilot casuale: non riproduce i numeri di questa sessione. Exit1 se
falliscono fixture, invariante iniziale o ledger; JSON comunque conservato per diagnosi.
Lo script usa target `popx`, evitando `format ALL`; nessuna nuova dipendenza.
Sono passati anche build/link Release e `tests/pop_cycle_regression.cpp` già esistente.
CTest rimane senza test registrati: questi controlli sono standalone.

## Prossimo esperimento con maggior valore informativo

Common garden a temperatura/risorse controllate e stessi tratti, confrontando
strategie/genomi già osservati, con seed/replay completo e ledger validato.
Separare: richiesta→successo movimento, uptake→energia utilizzabile,
sopravvivenza→tempo idoneo, figli→discendenti sopravvissuti. Annotare forzatura,
mutazione aumentata e plasticità; effettuare repliche e controlli per habitat/età.

La priorità è verificare **quale strategia sia selezionabile in condizioni in cui
può arrivare alla maturità**, poi quanto fedelmente venga trasmessa. Non aggiungere
premi di movimento, nuovi sensori o learning prima di questa discriminazione.


<a id="indagini-termiche-storiche"></a>
## Appendice: Indagini termiche prima della regolazione

### Termica, consumo, continuità spaziale e parallelismo — 14 ottobre 2026

Analisi e interventi richiesti dopo la segnalazione della run GUI di140000 tick.
Quella traiettoria non è stata acquisita. Le prove sotto sono controlli causali
di fisiologia, non una replica della run e non esperimenti di adattamento.

## Dinamica implementata

Ogni tick, prima della morte e dell'apprendimento:

1. Prelievo automatico dalla cella: fino a5 glucosio,5 acqua,2 calcio,5 CO2,
   5 ossigeno;1 lipide se sotto la capacità adiposa. Nessuna scelta neurale richiesta.
2. Fotosintesi, se permessa da cloroplasti, luce e riserve: al massimo una reazione.
3. Fino a M reazioni respiratorie, limitate da glucosio e ossigeno disponibili.
   Per reazione: -1 glucosio,-1 O2,+1 CO2,+0,3 calore; energia +0,3×efficienza.
4. Aggiornamento temperatura, costo basale0,02 e costi delle azioni; cap energia400.
5. Morte per energia<=0, età>200000 o temperatura<=10 / >=75 C.

L'efficienza è `max(0, 1 - abs(T_corpo - 37)/10)`:
zero a27 C o meno e47 C o più. È calcolata prima dell'aggiornamento termico.
La quantità di reazioni e il loro calore NON diminuiscono quando l'efficienza
scende a zero, né quando l'energia raggiunge il cap. Le reazioni possono quindi
esaurire risorse senza guadagno energetico, o dissipare energia per saturazione.
Questo è comportamento effettivo del modello; non dimostra un bug contabile.

`T_next = T + alpha*(T_ambiente-T) + 0,3*reazioni + calore_azione`

`alpha = clamp(0,5 - 0,49*min(lipidi/80,1), 0,01, 0,5)`.
Con20 lipidi alpha=0,3775; con80 alpha=0,01. A produzione di calore costante,
l'equilibrio matematico sarebbe `T_ambiente + 0,3*reazioni/alpha`:
con5 reazioni e80 lipidi è150 C sopra l'ambiente; la morte interviene prima.
Con1 reazione e80 lipidi è30 C sopra l'ambiente. Il numero di reazioni effettivo
non coincide sempre con i mitocondri: l'assorbimento passivo di O2 è5/tick,
quindi25 mitocondri esauriscono presto la riserva iniziale20 e diventano limitati
anche dall'ossigeno. Le azioni possono cambiare il glucosio disponibile.

L'azione termoregolatrice aggiunge0,5 calore al costo0,05 energia; non raffredda.
Il raffreddamento deriva dallo scambio ambientale. Non esiste qui un'azione
per espellere lipidi o regolare direttamente la respirazione. I lipidi vengono
assorbiti automaticamente e possono spostare rapidamente il corpo da una
condizione dissipativa a una molto isolata. L'ambiente genera lipidi casuali
nelle celle; il test ne isola l'effetto, non ne stima la frequenza nella run GUI.

La maturità riproduttiva richiede età>1000, energia>50 e glucosio>20.
I fondatori partono con energia100: senza guadagni né azioni il solo costo
basale impiega circa5000 tick a consumarla. Sopravvivere2000 tick o raggiungere
l'idoneità una volta non dimostra quindi autosostentamento riproduttivo.
I figli ricevono risorse donate e ripartono dalla temperatura iniziale configurata25 C;
non ereditano la temperatura del genitore. Vanno misurati separatamente dai fondatori.
La rigenerazione locale produttiva di glucosio/O2 è1 ogni1000 tick per cella,
ben inferiore a una respirazione per tick per un individuo fermo senza altre fonti.

## Protocollo eseguito

`bash scripts/run_thermal_comparison.sh data_out/audit/thermal_comparison_2026-10-14.json`

360 condizioni deterministiche, fino a2000 tick ciascuna, stop alla morte:
5 temperature ambientali costanti (0,15,25,37,50 C),3 valori di mitocondri (1,5,25),
2 riserve lipidiche iniziali (20,80),2 modalità risorse,6 varianti.

Ogni controllo usa un individuo isolato, genoma configurato vuoto, cloroplasti0,
sensibilità1, capacità adiposa80, energia100, glucosio100, O2 20, acqua100;
cella con500 glucosio,1000 O2,1000 acqua, quota0, calcio/CO2/lipidi0.
Le modalità sono risorse finite e reintegro esplicito di glucosio/O2 fino al cap
prima di ogni tick. Il reintegro misura un limite favorevole, NON è produzione,
e gli apporti effettivi sono registrati; dopo una morte l'esposizione è più breve.
Nessun movimento, decisione, apprendimento, nascita o aggiornamento del mondo.
L'eredità Hebbian resta true nella configurazione, ma qui non avvengono nascite.
Non si usa RNG nella fisiologia misurata; i valori casuali residui dei costruttori
non influenzano la fixture. Lo stato completo di quelle RNG inutilizzate non è salvato.

Varianti, una alla volta rispetto alla baseline:
- temperatura iniziale corporea37 anziché25;
- calore respiratorio0,03 anziché0,3;
- semilarghezza del rendimento20 anziché10 C;
- scambio minimo alpha0,05 anziché0,01;
-80 lipidi ambientali iniziali anziché0, assorbiti con le API reali.

Bilanci glucosio/O2 ed energia (cap e roundoff inclusi) e finitezza verificati
in tutte360 le condizioni. Replay esatto di tutti i risultati. I dati locali
ignorati da Git includono INI, commit+stato working tree, hash dei sorgenti,
compilatore, comandi di compilazione e costanti; harness e script sono nel repository.
Questi sono casi deterministici, non360 repliche stocastiche indipendenti.

## Risultati principali

Baseline, risorse reintegrate, nessun lipide ambientale:

| Ambiente | Mitocondri | Lipidi iniziali | Esito entro2000 tick | Temperatura finale |
|---|---:|---:|---|---:|
|0 C|1|20|Morte termica al tick3|6,63 C|
|0 C|1|80|Vivo, energia226,42|30,00 C|
|25 C|1|20|Vivo, energia60,01; rendimento nullo tutti2000 tick|25,79 C|
|25 C|5|20|Vivo, energia400|28,97 C|
|25 C|5|80|Morte termica al tick41|75,66 C|
|37 C|5|20|Vivo, energia400|40,97 C|
|37 C|5|80|Morte termica al tick37|75,31 C|

A37 C,5 mitocondri,20 lipidi iniziali: aggiungere80 lipidi alla cella cambia
l'esito da vivo a2000 tick a morte termica al tick73, con energia124,58.
A25 C la stessa aggiunta porta a morte al tick85, energia140,42.
È un effetto causale del controllo lipidico nella fixture, non della selezione.

Su30 combinazioni per variante CON reintegro:

| Variante | Vivi a2000 | Almeno una idoneità riproduttiva |
|---|---:|---:|
|Baseline|16/30|16/30|
|Corpo iniziale37|16/30|16/30|
|Calore0,03|26/30|26/30|
|Semilarghezza20|16/30|16/30|
|Alpha minimo0,05|24/30|24/30|
|Lipidi nella cella80|7/30|7/30|

Le frazioni descrivono questo pannello, non probabilità di sopravvivenza nella
popolazione reale. Uguali conteggi non significano identiche traiettorie/energie.
SENZA reintegro nessuna delle180 condizioni raggiunge idoneità riproduttiva:
la singola cella non sostiene l'approvvigionamento fino alla maturità. Alcuni
individui rimangono vivi con riserve energetiche, ma senza glucosio sufficiente.
Nessuna discendenza è stata misurata: il manager riproduttivo è escluso.

Le modifiche non migliorano ogni nicchia. A0 C,1 mitocondrio,80 lipidi, la baseline
arriva viva a2000 tick; calore0,03 porta a morte fredda al tick114 e alpha minimo0,05
al tick31. Un mondo uniformemente37 C non elimina il surriscaldamento.
Allargare il rendimento aumenta l'energia in alcuni casi ma non elimina la
produzione di calore, dunque non basta a rimuovere le morti termiche del pannello.

## Valutazione e prossima discriminante

Il sistema impone una selezione termica molto rapida: morte in decine di tick
contro maturità oltre1000. Il comportamento neurale può avere poco tempo per
compensare tramite movimento; e non dispone di controllo diretto del consumo,
del raffreddamento attivo o dell'isolamento acquisito. Questo può privilegiare
fortemente posizioni e tratti fisiologici iniziali prima di strategie utili.

La scarsità e la temperatura sono due ostacoli distinti: dare risorse può
sostenere la riproduzione, ma anche alimentare il surriscaldamento. Migliorare
la temperatura non risolve da solo il rifornimento. Ridurre il calore o aumentare
lo scambio minimo sono candidati da confrontare con popolazioni e risorse finite,
in ambienti eterogenei, prima di adottarli globalmente. Nessun parametro termico
produttivo è stato modificato in questo intervento. Non sono introdotti nuovi
meccanismi, reward o attrazione sociale. Mancano ancora prove di adattamento.

## Modifiche produttive autorizzate

- INI: mutazione0,05→0,01. Mediamente1–7 sostituzioni su100–700 geni anziché5–35.
  Lo stesso parametro riduce anche la probabilità di mutazione di ciascun tratto
  fisiologico; può rallentare l'esplorazione fisiologica oltre a preservare circuiti.
  Default fallback Config resta0,001; il valore produttivo INI è0,01.
- Eredità dei pesi mantenuta true, come esplicitamente richiesto ora dall'utente:
  questa preferenza aggiorna quella contraria riportata negli audit precedenti.
- NewGen conserva la selezione periodica e il criterio, ma non teletrasporta né
  riassegna i bucket dei superstiti. Restano posizioni, identità, fasi pendenti e
  ordine dei sopravvissuti. I morti restituiscono riserve e liberano le celle.
  Il criterio di abbattimento rimane un intervento esplicito, non selezione puramente ecologica.
- Parallelismo: i precedenti oggetti contenevano già il backend TBB di par_unseq.
  num_threads era ignorato; ora un'arena TBB del manager applica il massimo
  configurato a parallel_for di sense e think.1=seriale,4=fino a4 thread inclusa
  la partecipazione del chiamante. Le fasi terminano prima delle scritture nel mondo.
  Azioni, fisiologia e ambiente restano sequenziali per preservare l'ordine degli scambi.
  Poche decine di individui per bucket, lavoro sequenziale e attesa GUI possono
  rendere poco visibile il parallelismo. Non è stato dimostrato un aumento di velocità.

## Validazione eseguita

- `cmake --build build-release --target popx -j 2`: PASS.
- `tests/pop_cycle_regression.cpp`: PASS, inclusa conservazione posizioni/occupazioni NewGen.
- `tests/parallel_phase_regression.cpp`: PASS,1 e4 thread osservati durante le
  fasi reali, identità esatta degli stati controllati dopo90 tick con120 individui;
  NewGen conserva108 superstiti e relativi bucket/fasi/celle, rimuove i12 eliminati.
- `bash scripts/run_audit.sh /tmp/popx_after_spatial_parallel_probes.json probes`:
  PASS trasferimenti, riproduzione, cap, death e NewGen. Nella fixture con tutte
  le celle bloccate da calcio, NewGen ora conserva9/10: non deve più respawnare.
-360 controlli termici e replay: PASS.

Regressione parallela compilata con:

```sh
clang++ -std=c++23 -O2 -Isrc tests/parallel_phase_regression.cpp build-release/src/CMakeFiles/popx.dir/{brain,cell,config,console_logger,gene,genome,grid_world,pop,pops_manager,random_utility}.cpp.o -ltbb -lstdc++fs -o /tmp/popx_parallel_regression
/tmp/popx_parallel_regression
```

Controlli standalone, non registrati in CTest. Nessuna suite generale o lunga
run evolutiva dichiarata. Le modifiche produttive richiedono riavvio del programma.

## Seconda indagine: ambiente moderato e sostenibilità — 14 ottobre 2026

Richiesta successiva: verificare un ambiente più moderato e valutare una revisione
fisiologica. Questo aggiornamento usa lo stesso documento; nessun nuovo Markdown.
Nessuna modifica alla fisiologia o all'ambiente produttivi in questa indagine.

### Confronti effettivamente eseguiti

```sh
bash scripts/run_thermal_comparison.sh data_out/audit/thermal_moderate_final_2026-10-14.json moderate
/tmp/popx_thermal_comparison config/popx.ini /tmp/popx_moderate_replay.json moderate
```

180 controlli isolati: ambiente18/22/25/28/32 C; mitocondri1/5/25;
20 lipidi iniziali e80 nella cella; risorse finite oppure reintegro esplicito;
6 varianti (baseline, calore0,1, calore0,2, alpha_min0,02, alpha_min0,05,
calore0,1+alpha_min0,02). Altri dettagli come nella prima indagine.

144 confronti finali di popolazione, tutti con risorse finite:
-24 fondatori su24×24 celle; genoma vuoto configurato, tratti M/0/1/80, energia100,
  glucosio100, ossigeno20, acqua100, lipidi20, temperatura iniziale25 C.
-Per cella:500 glucosio,1000 ossigeno e acqua,80 lipidi; calcio/CO2 iniziali0,
  quota0; niente fotosintesi, rigenerazione o aggiornamento climatico.
-Temperature con gradiente lineare lungo x:0–50 C (controllo estremo costruito),
  18–32 C e25–32 C. Non sono riproduzioni delle RBF native: si confrontano gli
  stessi profili spaziali riscalati, non nuove estrazioni ambientali casuali.
-Varianti baseline, calore0,1, alpha_min0,02, combinazione0,1/0,02;
  M=1/5/25; immobilità oppure pattuglia orizzontale;2 collocazioni speculari.
-Fondatore i: y=i, x=1+(7*i)%22; nel secondo layout x→23-x e direzione invertita.
  Un fondatore per riga evita ingorghi iniziali tra pattuglie. Nuove collisioni
  dovute alle nascite restano reali. Politiche imposte, non evolute né ottimali.
-Fasi sense/think/act ordinate in base all'età, fisiologia reale ogni tick.
  L'azione viene sovrascritta SOLO in questa fixture; sensori/reti non decidono
  la politica. Learning ed eredità restano abilitati, ma il genoma vuoto non ha
  pesi attivi: non si verifica qui l'effetto della plasticità o della sua eredità.
-FIFO e riproduzione reali, costi, donazioni, controllo di maturità e cap120;
  nascite forzate e NewGen esclusi. Mutazione0 solo nella fixture per confronti
  appaiati a fisiologia ereditaria uguale. INI produttivo resta0,01/eredità true.
-Direzione e lazyness dei figli normalizzate per la politica imposta. Nessuna
  estrazione casuale influenza i risultati; assegnazione casuale ai bucket non
  usata nel ciclo di questa fixture. Stato RNG inutilizzato non serializzato.
-Ogni confronto termina a2000 tick o all'estinzione, con limite30s per run.
  Nessun figlio può superare1000 tick di età: non dimostra riproduzione di seconda
  generazione, stabilità di lungo periodo o adattamento evolutivo.

Tutte le324 condizioni finali verificano finitezza e bilanci glucosio/O2/energia;
per popolazioni sono inclusi morti, eccedenze restituite/rimaste nei morti,
trasferimenti parentali, saturazione e roundoff. Replay esatto dei180 controlli e
144 confronti, inclusi eventi di nascita e campioni temporali, senza confrontare
metadata di build. Build del target popx PASS. Nessuna suite CTest dichiarata.

Dati finali nel JSON sopra, ignorato da Git, con provenienza di build e hash.
Un pannello esplorativo precedente collocava6 fondatori sulla stessa riga:
le pattuglie si bloccavano precocemente e non raggiungevano nuove risorse.
È conservato in `data_out/audit/thermal_moderate_2026-10-14.json`, ma NON viene
usato come prova di inefficacia del movimento. Il layout finale è esplicito
nell'harness e nei metadata; le2 collocazioni sono controlli spaziali, non
repliche stocastiche indipendenti o una stima statistica della popolazione reale.

### Risultati a25–32 C

Aggregazione descrittiva delle12 condizioni per variante (3 tratti×2 politiche×
2 layout,288 fondatori iniziali). I conteggi dei figli includono il loro diverso
tempo d'esposizione; non sono probabilità comparabili direttamente ai fondatori.

| Variante | Fondatori vivi /288 | Nascite naturali | Figli vivi a fine run | Morti termiche totali | Morti energetiche totali |
|---|---:|---:|---:|---:|---:|
|Attuale|96|192|192|192|0|
|Calore0,1|192|192|192|96|0|
|Alpha minimo0,02|96|192|192|192|0|
|Calore0,1 + alpha minimo0,02|288|576|519|0|57|

Con fisiologia attuale e ambienti moderati18–32 o25–32 C sopravvivono tutti i
fondatori M=1; muoiono tutti quelli M=5/25 nelle due politiche. Moderare soltanto
l'ambiente non basta. Nel primo layout25–32 C le pattuglie M=5/25 si estinguono
entro il tick82. Le pattuglie M=1 generano96 figli per run e raggiungono il cap.
Questa discendenza non dimostra sostenibilità: nel caso baseline M=1/layout0,
nei tick1500–2000 l'energia prodotta è69,32 contro costi1399,83; energia media
dei96 figli vivi finale10,50. Si stanno consumando riserve accumulate/donate.

Con solo calore0,1 gli immobili M=5/25 restano vivi; le pattuglie M=5/25 muoiono
termicamente (nel layout0 entro il tick319). Gli immobili esauriscono il glucosio
locale e cessano di scaldarsi. Il movimento continua a procurare combustibile e
alimenta il surriscaldamento. Sopravvivere consumando poco perché non si trova
cibo è diverso dall'acquisizione efficiente. È un effetto del controllo di politica,
non una dimostrazione di selezione spontanea contro il movimento.

La combinazione0,1/0,02 elimina le morti termiche in entrambi i pannelli moderati,
ma a18–32 C si osservano267 morti energetiche, a25–32 C57, tutte fra i figli
(i fondatori sopravvivono). Non si può scegliere un parametro dalla sola mortalità
termica. Il cap demografico viene raggiunto in varie run e censura nuove nascite.

### Perché la combinazione non risolve il modello

Caso25–32 C, calore0,1/alpha_min0,02, M=5, pattuglia, layout0:

| Tick | Vivi | Nascite cumulative | Reazioni cumulative | Energia prodotta cumulativa |
|---|---:|---:|---:|---:|
|500|24|0|60000|1891,80|
|1000|24|0|120000|1891,80|
|1500|120|96|162780|7870,82|
|2000|120|96|162780|7870,82|

Fra500 e1000:60000 reazioni e ZERO energia, con corpi fuori dalla banda utile.
Non è scarsità: i substrati vengono realmente bruciati senza ritorno energetico.
Fra1500 e2000:ZERO reazioni, perché il glucosio corporeo finale è zero e gli
individui non accedono più a un rifornimento sufficiente; costi1399,83.
Temperatura finale28,65–32 C, quindi la cessazione della produzione di calore
riporta il corpo verso l'ambiente. Restano risorse in altre celle, ma disponibilità
totale non equivale ad accessibilità sotto queste politiche e collisioni.

Con M=1 nella stessa combinazione/layout/politica, la finestra1500–2000 produce
9631,32 energia contro1399,83 costi. È sostentamento positivo in quella finestra,
non prova di permanenza indefinita, né fitness evolutiva dimostrata.

### Valutazione e soluzione proposta (non implementata)

La prova indica due limiti strutturali distinti:
1. I mitocondri sono trattati come numero di reazioni obbligatorie, non capacità
   massima. Il consumo non riceve feedback dal fabbisogno energetico o dal fatto
   che il corpo sia fuori dalla banda di rendimento utile.
2. L'efficienza termica riduce soltanto l'energia ricavata, lasciando intatti flusso
   e calore. Questo rende possibile consumo sostenuto completamente improduttivo;
   insieme all'assorbimento lipidico automatico può penalizzare acquisizione efficace.

Non è dimostrato un errore aritmetico: i ledger passano. È una scelta modellistica
che può imporre una pressione molto forte e poco controllabile dalla rete.
Calore0,1/alpha_min0,02 è un buon controllo per allargare la finestra di vita, ma
non una correzione fisiologica completa. Non è stato trasferito alla produzione.

Proposta concreta per un prossimo prototipo separato:
-Interpretare M come capacità; determinare il flusso effettivo con feedback negativo
  della riserva energetica e con disponibilità di substrati. Quando la riserva è
  sufficiente, ridurre la respirazione; quando viene consumata, aumentarla fino
  alla capacità. Nessun reward né modifica dei geni/azioni richieste.
-Far agire la temperatura anche sull'attività respiratoria, con risposta continua,
  evitando che un corpo già surriscaldato continui a bruciare a pieno regime.
  Non usare il rendimento attuale come interruttore secco: a25 C esso è zero e
  questo potrebbe impedire il recupero iniziale. La risposta al freddo va verificata.
-Per ogni reazione effettivamente eseguita, mantenere trasferimenti e costi
  espliciti e registrare energia utile e calore. Separare capacità metabolica,
  rendimento, produzione di calore e dispersione; non compensare con energia gratuita.
-Prima fissare criteri di accettazione: niente consumo al massimo su riserva satura;
  consumo proporzionato a richieste/costi; assenza di surriscaldamento autoalimentato
  a riserva sufficiente; possibilità di recupero da25 C in nicchie praticabili;
  bilanci chiusi; figli che raggiungono maturità e si riproducono in prove dedicate.

Scala utile: a rendimento1 una reazione dà0,3 energia e il costo basale è0,02/tick.
Senza altri costi basta mediamente1 reazione ogni15 tick per il solo mantenimento;
M=1 oggi può eseguirne1 ogni tick. Le frazioni di flusso richiedono un accumulatore
(o probabilità controllata) per non arrotondare sempre a zero. Riserva bersaglio,
ampiezza del feedback e curva termica sono parametri ancora da calibrare, non
valori ottimali già determinati. Riproduzione e movimento aggiungono domanda.

Preferenza: prototipo di regolazione metabolica prima di altre lunghe run native.
Non rendere ogni nicchia abitabile, non garantire la sopravvivenza: consentire che
l'acquisizione di risorse possa sostenere bisogni e discendenza senza imporre
automaticamente consumo massimale. Nessuna nuova fisiologia è stata implementata
in questa richiesta, che chiede test e una proposta.


<a id="pilot-storico"></a>
## Appendice: Pilot storico successivo alla segnalazione GUI

### Indagine successiva alla segnalazione di 140000 tick

La run GUI segnalata non è stata acquisita: nessuna conclusione sulla sua traiettoria.
Verifica del codice e pilot separato del 2026-10-14, configurazione produttiva invariata.

Comando: `bash scripts/run_audit.sh /tmp/popx_followup_audit.json 2000`.
Build del target popx riuscito; diagnostico exit 0, validation_passed=true.
80 fondatori, 2000 tick in 1,228 s, 22 vivi finali; 29 nascite naturali e 5 forzate;
92 morti: 72 termiche e 20 energetiche. 870 movimenti riusciti /3497 tentativi.
Bilanci e probe superati. RNG non riproducibili globalmente; singolo pilot,
non confronto causale, benchmark GUI o esperimento di adattamento.
JSON temporaneo con INI, commit, stato working tree, costanti, genomi iniziali,
ambiente e ledger; non contiene lo stato completo di tutti i generatori.

## Vincoli verificati e ipotesi da distinguere

- Respirazione: rendimento nullo a temperature corporee <=27 o >=47 C,
  ma consumo di glucosio/O2 e produzione di calore proseguono. Morte termica
  <=10 o >=75 C. Il costo ecologico va valutato, non corretto arbitrariamente.
- NewGen ogni50000 tick conserva circa90% e riposiziona casualmente i vivi:
  possibile interruzione di nicchie e strutture spaziali, non dimostrata qui.
- Cap120 e riproduzione forzata sotto20 influenzano il ricambio. La persistenza
  numerica non implica autosostentamento mediante sole nascite naturali.
- Mutazione0,05 sostituisce geni interi; 100–700 geni implicano in media5–35
  sostituzioni per figlio. Possibile distruzione di circuiti utili da misurare.
- Eredità Hebbian attiva: discrepanza con la preferenza per sola eredità genetica.
- Fotosintesi può sostenere strategie immobili; immobilità non prova fallimento.

## Durata e velocità

Il loop GUI non usa max_generations come arresto: continua fino a estinzione,
Stop o chiusura. MaxAge200000 limita il singolo individuo. Storico statistiche
limitato a50000 campioni; contatori cumulativi int richiedono revisione per run
molto lunghe e occorrono checkpoint con stato RNG per ripresa fedele.
La GUI usa tick di10 ms, circa100 tick/s. Il pilot senza rendering ha eseguito
circa1629 tick/s, su una coorte specifica e con strumentazione diagnostica.
Un acceleratore deve eseguire più tick invariati, con budget per frame;
non aumentare il passo biologico. num_threads non configura il backend attuale.

## Prossimi confronti discriminanti (non eseguiti)

Prima centralizzare/registrare RNG e corpus, poi repliche appaiate con stessa
coorte e risorse; variare un fattore alla volta: clima dinamico/fisso,
NewGen attivo/disattivo, mutazione0,05/ridotta, learning ed eredità separati.
Misurare cause di morte, nascite naturali/forzate, genealogia, tempo al cap,
acquisizione, azioni e movimenti riusciti, includendo morti e censura.
Clima favorevole e accesso sensoriale alle risorse sono controlli diagnostici,
non premi di movimento o prove di adattamento. Per segnali e cooperazione,
verificare prima un beneficio ecologico causale a risorse equivalenti.
