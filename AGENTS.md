# PopX — istruzioni operative

## Scopo e fonte unica

PopX studia comportamenti adattivi attraverso eredità, ecologia, sopravvivenza e
riproduzione. Non premiare arbitrariamente movimento o aggregazione. Separare
fixture diagnostiche e simulazioni evolutive; verificare meccaniche esistenti
prima di aggiungerne altre. Non confondere correlazione e causalità.

`docs/EVOLUTION_STATUS.md` è il riferimento unico: stato attuale in testa,
validazione e poi audit storici preservati. L'audit originale si riferisce al commit
`0e9fc465956eb967ad67f7352eabccc663c8bcf4`; ricontrollare il codice attuale.
Vecchie preferenze sull'eredità e descrizioni di termica/thread/NewGen sono
superate dagli interventi autorizzati descritti sotto.

## Architettura e vincoli attuali

- `main.cpp`: CLI headless/config/velocità/schema/validazione; INI predefinito relativo all'eseguibile.
- `popx_app.cpp`: scheduler100/200 tick/s con attese, ogni tick10 ms simulati; GUI opzionale
  nello stesso thread. Start/Restart valida e rilegge l'INI ricostruendo la run.
- `Stats`: raccolta centrale, campioni iniziale/finale e ogni200 tick, eventi individuali,
  bilanci e telemetria UDP loopback. Worker solo su copie, nessun accesso ai Pop vivi.
- Dashboard Python locale: unico scrittore SQLite, inspector manuale, snapshot neurali JSON
  solo su richiesta. Nessuna nuova esportazione JSON statistica, neppure diagnostica.
- Durata INI0 infinita; estinzione/Stop concludono la run, Esci termina il processo.
  Headless rilascia SFML/ImGui, Mostra GUI ricrea la finestra sulla stessa run.
- `simulator.cpp`: inizializza il mondo prima dello spawn; aggiorna agenti poi ambiente.
- `pops_manager.*`: ownership shared_ptr, bucket sense→think→act, FIFO nascite,
  morti e NewGen. Guard Pop impediscono fasi fuori ordine anche alla nascita.
- `pop.*`: sensori, azioni, fisiologia, learning e riproduzione asessuata; PHY
  ereditato separatamente dai geni. Direzione iniziale fra8 vicini, mai zero.
- `gene/genome`: endpoint uint32, mutazione sostituisce geni interi; copie preservano
  endpoint e domini. Usare `Genome::encoding_from_config` nei percorsi produttivi.
  Costruttori standalone senza encoding mantengono legacy32/32.
- `brain/neuron`: rete feedforward sparsa, hidden1…6, sigmoid, argmax singola azione;
  parità favorisce indice0. Geni fuori dimensioni attive silenti senza modulo.
- INI49 input: glucosio33–36, gradienti DANGER/MATE/HOME37–48;32 riservato.
  FOOD24–27 invariato. INI21 uscite;23 abilita KILL_FORWARD22,21 riservato.
  Dimensioni extra non implementano nuovi sensori/azioni. Rewire viventi non automatico.
- Eredità dei pesi appresi **true**, esplicitamente richiesta dall'utente: preservarla.
  Mutazione INI0,01, applicata anche ai tratti PHY. Distinguere dai default Config.
- Riproduzione: costo energetico dissipato, poi trasferimento1/8 di risorse/energia;
  nessuna dotazione gratuita ai figli. Distinguere nascite naturali e forzate.
- NewGen seleziona/abbatte ma NON riposiziona né riassegna i bucket dei superstiti;
  conserva identità, occupazioni, fasi e ordine. Non è profondità genealogica.
- Sense/think: `tbb::parallel_for` in task_arena limitata da num_threads (1 seriale).
  Le fasi terminano prima delle scritture. Azioni/fisiologia/mondo sequenziali.
  Non introdurre scritture condivise nelle fasi parallele; snapshot non autorizzano
  accesso concorrente alla simulazione senza sincronizzazione.
- `grid_world/cell`: occupanti weak_ptr, capacità finite, risorse/campi RBF,
  rigenerazione locale; feromoni additivi0…100, decadimento0,9995/tick, nessuna
  diffusione. Per cancellare un campo usare delta negativo del valore presente.
  Canali FOOD/DANGER/MATE/HOME non impongono significati o cooperazione.
- `Cell::give_*` restituisce la quantità accettata: sottrarre soltanto quella.
  Morte restituisce riserve; overflow è sink esplicito. Validare bilanci/cap/roundoff.
- Respirazione regolata: riserva target100, banda25, capacità frazionaria accumulata;
  flusso modulato dalla temperatura, inibizione calda continua fino a zero a47 C.
  Rendimento respiratorio gaussiano; reward termico ancora lineare. Calore/energia
  solo per reazioni completate. Formule, parametri e limiti nel documento unico.
  Credito non ereditario; reset a init e nessuna raffica dopo scarsità di substrati.
- Statistiche GUI dei vivi non descrivono vite completate. Registrare eventi dei morti.
- sexual_reproduction, inserzione/delezione, choose_parents_by_fitness,
  max_generations e reload @N non controllano i rispettivi comportamenti applicativi.
  steps_per_generation scala età/oscillatore; non scandisce le nascite.
- `old/popw/` è archivio locale estraneo: non modificarlo né usarlo come prova o
  includerlo nei commit. Conservare modifiche utente estranee come il layout imgui.ini.

## Build e verifiche

C++23, Clang/Linux x86_64; SFML, Eigen, json, TBB, ImGui/ImGui-SFML.
Non installare dipendenze senza autorizzazione. Usare il target esplicito `popx`:
`format ALL` modifica i sorgenti; EBABLE_CODE_FORMATTING non lo disabilita.
Nuove configurazioni CMake possono usare la rete con FetchContent.
ENABLE_UTESTS non coincide con UTESTS_ENABLED; `utests/` non è presente.

```sh
cmake --build build-release --target popx -j 2
bash scripts/run_regressions.sh
bash scripts/run_audit.sh /tmp/conservation.sqlite probes
bash scripts/run_audit.sh /tmp/pilot.sqlite 2000
bash scripts/run_thermal_comparison.sh /tmp/moderate.sqlite moderate
```

Regressioni standalone, non registrate in CTest: respirazione, ciclo, thread/spazio,
codifica genetica, feromoni, statistiche e runtime. Il diagnostico termico verifica il codice corrente:
non riproduce risultati della vecchia fisiologia dopo un cambio d'implementazione.
Non dichiarare una suite CTest o esperimenti mai eseguiti.

## Metodo scientifico e convenzioni

- Separare richieste/tentativi/movimenti riusciti, acquisizione, energia,
  sopravvivenza e discendenza. Controllare risorse totali, posizioni, età/esposizione,
  censura, cap, forzature e abbattimenti.
- Registrare commit+diff, INI, costanti, genomi/tratti/ambiente, RNG/backend/build.
  RNG produttive usano random_device e rand non centralizzati: singoli seed non
  bastano. Replay deterministici di fixture non provano riproducibilità globale.
- Preferire test brevi deterministici; pilot2000 tick/30s con stop anticipati.
  Politiche imposte/accesso friend sono diagnostici separati. Non trasferire
  override di policy o dotazioni dalle fixture alla produzione.
- Dati locali in data_out/audit ignorati da Git. Conservare protocollo e script;
  aggiornare il documento unico senza moltiplicare Markdown o cancellare evidenza storica.
- Stile snake_case, classi PascalCase, membri con suffisso _, smart pointer,
  override espliciti, commenti Doxygen,4 spazi/120 colonne. C++23 resta richiesto
  anche se .clang-format menziona C++17. Evitare rifattorizzazioni estranee.
