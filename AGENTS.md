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

- `Simulator::init()` registra gli agenti prima di `GridWorld::init()`, che cancella
  gli occupanti. Non presumere coerenza tra popolazione, densità e rendering.
- `rotate_buckets()` produce `sense → act → think → sense` per ciascun agente.
- I sensori del glucosio 33–36 non sono codificabili con indici a 5 bit; le
  direzioni relative iniziali sono zero. Le parità nell'argmax favoriscono indice 0.
- `hebbian_inheritance=true` nell'INI corrente: i figli possono ereditare pesi
  appresi. La preferenza progettuale resta escludere l'eredità lamarckiana: segnalare
  questa discrepanza; non correggere silenziosamente la configurazione.
- Il contatore del costo riproduttivo non sottrae energia. Non usarlo come prova
  di una spesa reale. Le statistiche dei soli vivi non descrivono vite completate.
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
