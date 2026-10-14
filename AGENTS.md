# PopX — istruzioni per le sessioni di sviluppo

## Scopo e metodo

PopX studia l'emergere di comportamenti adattivi attraverso variazione ereditaria,
interazione ecologica, sopravvivenza e riproduzione. Non premiare arbitrariamente
movimento o altri comportamenti desiderati per ottenere risultati apparenti.
Le manipolazioni diagnostiche devono essere esplicite, controllate e separate
dagli esperimenti evolutivi. Non introdurre nuove meccaniche biologiche prima
di aver verificato quelle esistenti.

Consultare `docs/EVOLUTION_STATUS.md` per l'audit del 2026-10-14, riferito al commit
`0e9fc465956eb967ad67f7352eabccc663c8bcf4`. Ricontrollare il codice dopo modifiche:
le osservazioni storiche e le ipotesi non sono risultati sperimentali.

## Architettura verificata

- `src/main.cpp`: composizione dei componenti, configurazione relativa al percorso
  dell'eseguibile, avvio grafico; non offre CLI per seed o esecuzione headless.
- `src/popx_app.cpp`: accumulatore con tick di 10 ms, GUI/rendering nello stesso
  thread principale; statistiche campionate ogni 200 tick.
- `src/simulator.cpp`: popolazione e poi ambiente a ogni tick.
- `src/pops_manager.*`: possiede i `Pop` con `shared_ptr`, gestisce gruppi di fase,
  nascite/morti, coda FIFO riproduttiva e selezione periodica `NewGen`.
- `src/pop.*`: sensori, azioni, fisiologia, apprendimento, riproduzione asessuata
  e tratti `PhyT` ereditati separatamente dal genoma neurale.
- `src/gene.*`, `src/genome.*`: estremi delle connessioni e pesi; mutazione tramite
  sostituzione dell'intero gene, non del singolo bit.
- `src/brain.*`, `src/neuron.h`: rete feedforward sparsa Eigen con connessioni
  verso qualsiasi strato successivo, sigmoide e argmax di una sola azione.
- `src/grid_world.*`, `src/cell.*`: griglia finita, occupanti `weak_ptr`, risorse,
  campi gaussiani, rigenerazione locale e decadimento dei feromoni; nessuna diffusione.
- `src/stats.*`, `src/pop_snapshot.h`: snapshot dei soli vivi, aggregati e JSON.
- `src/renderer.*`, `src/sfml_graphic_engine.*`, `src/imgui_layer.*`: visualizzazione
  SFML/ImGui; il renderer consulta le occupazioni del mondo.
- `src/config.*`, `config/popx.ini`, `src/common.h`: default, valori INI e costanti.
  Distinguere i valori configurati dai parametri effettivamente usati.
- `old/popw/` è un archivio locale non tracciato al momento dell'audit e non entra
  nel target corrente. Non modificarlo o usarlo come prova dell'implementazione attuale.

## Avvertenze operative verificate

- `Simulator::init()` ora inizializza il mondo prima dello spawn. Verificare
  comunque identità e corrispondenza unica popolazione–occupazioni nei test.
- `rotate_buckets()` ora produce `sense → think → act → sense`. I tre gruppi
  restano distribuiti su tick differenti; `Pop::next_phase_` impedisce fasi fuori
  ordine anche alla nascita e dopo assegnazione casuale dei gruppi in NewGen.
- `Pop::init()` sceglie uniformemente una direzione tra gli otto vicini, mai (0,0),
  e resetta fase attesa, prima esecuzione e decisione pendente.
- Gli indici genetici sorgente/destinazione sono ora uint32 senza troncamento
  a5 bit. L'INI abilita49 input: glucosio33–36, gradienti DANGER/MATE/HOME37–48;
  32 resta riservato, FOOD direzionale mantiene24–27.
  `genome_source_index_count`/`genome_sink_index_count` limitano generazione e
  sostituzioni mutate;0 segue max larghezza input/hidden o output/hidden.
  `brain_size_s/y` limita invece gli endpoint attivi; geni fuori rete restano
  silenti senza modulo. Ridurre il dominio genetico non riscrive geni ereditati.
  L'INI mantiene21 uscite;23 include l'azione esistente22 KILL_FORWARD,21 riservato.
  Nuove dimensioni oltre le implementazioni non creano sensori/azioni nuovi.
  Le parità nell'argmax continuano a favorire indice0.
- `hebbian_inheritance=true` nell'INI corrente: i figli possono ereditare pesi
  appresi. La preferenza progettuale resta escludere l'eredità lamarckiana: segnalare
  questa discrepanza; non correggere silenziosamente la configurazione.
- Il costo riproduttivo ora sottrae energia. Dopo il costo, il genitore dona1/8
  di tutte le riserve e dell'energia al figlio; niente dotazioni gratuite ai figli.
  La donazione è un trasferimento, distinto dal costo dissipato.
- Tutti i `Cell::give_*` restituiscono la quantità effettivamente accettata. Chi
  trasferisce deve sottrarre solo quella quantità. Death/NewGen restituiscono le
  riserve; eccedenze oltre capacità alla morte sono sink espliciti nel ledger.
- Cumulati energetici double, reazioni/cap/roundoff tracciati nei Pop per l'audit.
  Le statistiche GUI dei soli vivi continuano a non descrivere vite completate.
- `sexual_reproduction`, inserzione/delezione, `choose_parents_by_fitness`,
  `num_threads`, `max_generations` e reload `@N` non controllano i rispettivi
  comportamenti nel percorso applicativo attuale. `steps_per_generation` scala
  il sensore età e limita l'oscillatore; non scandisce le nascite.

## Build e validazione

C++23, toolchain Clang/Linux x86_64; dipendenze SFML, Eigen, nlohmann/json,
TBB, Dear ImGui e ImGui-SFML. Non installare dipendenze senza autorizzazione.

Comandi verificati nell'ambiente dell'audit:

```sh
cmake --build build-release --target popx -j 2
ctest --test-dir build-release -N
```

Il primo ha restituito `ninja: no work to do`; il secondo ha trovato zero test.
Le 17 unità di traduzione di `src/` sono state inoltre ricompilate con
`-fsyntax-only`, usando i comandi di `build-release/compile_commands.json`, senza
errori. Questo verifica la compilabilità sintattica, non la correttezza della simulazione.

Dopo le correzioni autorizzate di rotazione/direzione, il build Release ha
ricompilato e collegato il target `popx`. Esiste ora un controllo standalone
`tests/pop_cycle_regression.cpp`, senza dipendenze aggiuntive e senza rendering:

```sh
cmake --build build-release --target popx -j 2
clang++ -std=c++23 -O0 -g -Isrc tests/pop_cycle_regression.cpp build-release/src/CMakeFiles/popx.dir/{brain,cell,config,console_logger,gene,genome,grid_world,pop,pops_manager,random_utility}.cpp.o -ltbb -lstdc++fs -o /tmp/popx_cycle_regression
/tmp/popx_cycle_regression
```

Comandi eseguiti con esito PASS. La brace expansion richiede Bash; CTest non
registra questo controllo. Le fixture sovrascrivono i getter di configurazione
senza cambiare l'INI e usano una griglia piccola e piatta. Verificano invarianti
indipendenti dall'esito delle RNG; non sono esperimenti evolutivi o benchmark.

I preset di configurazione sono `Application Debug` e `Application Release`;
una nuova configurazione non è stata verificata e può usare la rete con FetchContent.
Il build predefinito e i preset di build richiedono cautela: `format ALL` esegue
`clang-format -i` sui sorgenti. Usare il target esplicito `popx` quando sono
proibite modifiche al codice. L'opzione `EBABLE_CODE_FORMATTING` non disabilita
quel target. `ENABLE_UTESTS` non coincide con la condizione `UTESTS_ENABLED`, e
`utests/` non è presente. Non dichiarare una suite di test superata.

## Vincoli scientifici e validazione delle modifiche

- Separare variazione neurale, azioni richieste, tentativi ed effettivi spostamenti,
  acquisizione di risorse, energia, sopravvivenza e discendenza.
- Preservare esplicitamente strutture e pesi genetici. L'apprendimento individuale
  non deve essere incorporato nel genoma dei figli secondo la filosofia richiesta;
  qualsiasi cambio alla politica attuale richiede uno scope esplicito.
- Non confondere il contatore `NewGen` con profondità genealogica. Documentare
  cap, riproduzione forzata e abbattimenti quando si misura selezione.
- Per esperimenti registrare commit, INI completo, costanti, genomi/tratti, ambiente,
  seed e stato di tutti i generatori, ordine delle fasi, backend/thread e build.
  Il codice corrente usa molti `random_device` e anche `rand()` senza seed centrale:
  un solo seed aggiunto non basta a renderlo riproducibile.
- Confrontare strategie a risorse iniziali e disponibilità totale equivalenti;
  usare repliche, controlli per età/durata di esposizione, eventi dei morti e censura.
  Una correlazione movimento-discendenza non dimostra causalità.
- Verificare conservazione dei trasferimenti, bilancio energetico con saturazione,
  finitezza numerica, intervalli coerenti e statistiche con denominatori appropriati.
- `sense`/`think` usano `par_unseq`; azioni, fisiologia e mondo sono sequenziali.
  Non introdurre scritture condivise nelle fasi parallele; non assumere che gli
  snapshot autorizzino accesso concorrente alla simulazione senza sincronizzazione.
- Per modifiche future autorizzate usare verifiche mirate sugli invarianti coinvolti
  e poi compilazione. Preferire controlli brevi e deterministici alle simulazioni lunghe.
  Registrare comando, risultato e limiti; non dichiarare esperimenti mai eseguiti.

## Convenzioni

Seguire lo stile locale: file e metodi `snake_case`, classi `PascalCase`, membri con
suffisso `_`, interfacce `I*`, ownership tramite smart pointer, override espliciti,
commenti Doxygen. `.clang-format` usa 4 spazi e limite 120 colonne; il suo riferimento
C++17 non cambia il requisito C++23 del build. Evitare rifattorizzazioni estranee.
Non sovrascrivere documentazione incompatibile: registrare le discrepanze.

## Diagnostica dinamica breve

Vedere `docs/DYNAMIC_AUDIT_2026-10-14.md` per misure reali, varianti e limiti.
`tools/popx_audit.cpp` ha accesso friend ristretto per osservazione e fixture:
non usare questi accessi per cambiare strategie durante i pilot.

```sh
bash scripts/run_audit.sh /tmp/conservation.json probes
bash scripts/run_audit.sh /tmp/pilot.json 2000
python3 scripts/analyze_audit.py data_out/audit
```

I probe sono standalone, con output JSON e exit nonzero sui bilanci errati.
Le run hanno limiti2000 tick/30s e stop anticipati. Dataset locali in
`data_out/audit` ignorati da Git. RNG ancora non riproducibili globalmente:
non confrontare pilot diversi come effetto causale di una correzione.
Variazione osservata e clone fedele non equivalgono ad adattamento dimostrato.

### Confronto controllato delle strategie

Protocollo e risultati in `docs/EVOLUTION_STATUS.md`, sezione18.
`tools/popx_strategy_comparison.cpp` e `scripts/run_strategy_comparison.sh`
sono diagnostici standalone: politiche imposte, tratti uguali per blocco,
temperatura ambientale37°C, mutazione/plasticità disattivate nella fixture.
Non trasferire questi controlli alla produzione senza richiesta del developer.
Distinguere richieste, spostamenti riusciti, celle nuove, acquisizione, censura
di sopravvivenza e discendenti; non usare sola distanza o energia come fitness.
Queste prove mostrano un vantaggio possibile dell'esplorazione, non adattamento
evoluto. Cap demografico, spazio di nascita e fotosintesi influenzano il confronto.
I replay neurali usano RNG produttive non seedate: evidenza esplorativa.

La verifica della sezione19 usa80 genomi e240 figli geneticamente mutati con
learning congelato: risorse est/ovest non cambiano azioni/percorsi, bordo e
occupazione cambiano alcune decisioni. Copie senza mutazione identiche;
molti mutanti conservano azioni, senza dimostrare fitness adattiva.
Distinguere ostacoli fisici da occupanti percepibili e cambi dopo fallimento da
evitamento causale. Dataset/harness temporanei in `/tmp/popx_context_*`:
Il replay osservato riproduceva il pannello salvato, non l'estrazione originaria
dei figli, che usa `random_device`. Rettifica sezione21: il vecchio tentativo
di washout con `set_feromone(type,0)` non cancellava segnali. Essendo additive,
le API Cell/World richiedono un delta negativo pari al valore presente per
cancellare il campo. Non dichiarare quella fixture priva di segnali.

### Codifica genetica configurabile — aggiornamento successivo alle prove

Vedere sezione20 di `docs/EVOLUTION_STATUS.md`; i risultati delle sezioni18–19
sono precedenti alla nuova codifica e non provano inefficacia della versione attuale.
`Genome::encoding_from_config` deve essere usato per fondatori, sostituzioni mutate
e genomi ricostruiti da pesi. Le copie preservano gli endpoint e i limiti memorizzati;
`mutated(rate, encoding)` applica i limiti correnti alle sole sostituzioni.
I costruttori standalone senza encoding mantengono il dominio legacy32/32 per
compatibilità: non usarli per generare genomi produttivi configurati.
Gli indici storici restano stabili; snapshot includono metadati encoding versione2.
Modifiche dimensionali dell'INI richiedono riavvio: non è implementato un rewire
automatico delle reti viventi. Profondità hidden ancora configurabile1…6.

```sh
cmake --build build-release --target popx -j 2
clang++ -std=c++23 -O0 -g -Isrc -Ibuild-release/_deps/eigen3-src -Ibuild-release/_deps/nlohmann_json-src/include tests/genome_encoding_regression.cpp build-release/src/CMakeFiles/popx.dir/{brain,cell,config,console_logger,gene,genome,grid_world,pop,pops_manager,random_utility}.cpp.o -ltbb -lstdc++fs -o /tmp/popx_encoding_regression
/tmp/popx_encoding_regression
```

Controllo standalone, non registrato in CTest: domini configurati, invalidi,
mutazione/eredità, endpoint oltre31/65535, export e rewire, sensori glucosio→
azioni→movimento, attuatore22 con23 uscite e compatibilità degli indici legacy.

### Feromoni e prove brevi successive

Vedere sezione21 di `docs/EVOLUTION_STATUS.md`. Tutti i quattro canali hanno
sensore locale0…1 e gradienti cardinali centrati in0,5. Campo additivo limitato
0…100, decadimento0,9995/tick, niente diffusione; scansione lungo quattro raggi
di4 celle. Le etichette FOOD/DANGER/MATE/HOME non impongono significati o risposte.
Emissione usa ora la RNG del Pop; RNG genetiche/ambientali restano non centralizzate.
Non confondere emissioni, esposizioni, reazioni causali a segnali, aggregazione
per spazio/nascite e coordinamento collettivo evoluto. La run controllata mostra
associazione movimento-discendenza, non selezione evolutiva dimostrata.

```sh
clang++ -std=c++23 -O0 -g -Isrc tests/pheromone_regression.cpp build-release/src/CMakeFiles/popx.dir/{brain,cell,config,console_logger,gene,genome,grid_world,pop,pops_manager,random_utility}.cpp.o -ltbb -lstdc++fs -o /tmp/popx_pheromone_regression
/tmp/popx_pheromone_regression
```

PASS su4 emissioni/sensori locali,16 gradienti/decisioni, avvicinamento reale,
campo uniforme senza gradienti spurii, cap/finitezza/decadimento e washout.
Harness `/tmp/popx_signal_probe.cpp` e dataset temporanei `/tmp/popx_signal_*`:
confronti600 tick appaiati riprodotti esattamente; run2200 con mutazioni/nascite
non garantita riproducibile. Gruppi nelle fixture con reti costruite non sono
gruppi spontaneamente evoluti. L'INI corrente resta21 azioni e reward invariati.
