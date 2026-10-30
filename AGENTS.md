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
- `popx_app.cpp`: scheduler100/200 tick/s oppure turbo headless (speed0), ogni tick10 ms simulati; GUI opzionale
  nello stesso thread. Turbo usa batch massimi di8 tick e attende se la coda telemetrica è >=128 messaggi;
  Mostra GUI torna a2× dopo apertura riuscita. Start/Restart valida e rilegge l'INI ricostruendo la run.
- `Stats`: raccolta centrale, campioni iniziale/finale e ogni200 tick, eventi individuali,
  bilanci e telemetria UDP loopback. Worker solo su copie, nessun accesso ai Pop vivi.
- Dashboard Python locale: unico scrittore SQLite, inspector manuale, snapshot neurali JSON
  solo su richiesta. Gli eventi SQLite nuovi codificano i genomi in forma binaria
  lossless versionata; API e lettori storici restituiscono gli array originali.
  Nessuna nuova esportazione JSON statistica, neppure diagnostica.
- Checkpoint CBOR versionati: cattura sul thread simulatore fra tick, ambiente completo,
  Pop/reti/tracce/scorer/fasi/code e INI; restore su nuova run tick0, età e tempo ambiente conservati.
  Config delle run da checkpoint bloccata UI/API; RNG non globalmente deterministiche.
  Cap database predefinito10 GB: checkpoint verificato, nuova run in pausa, sostituzione
  dello storico SQLite e ripresa. Errori conservano checkpoint/storico e lasciano in pausa.
  Ultimi2 checkpoint automatici, tutti i manuali; catalogo separato dallo storico.
- Durata INI0 infinita; l'estinzione attiva il recupero ereditario, Stop conclude la run,
  Esci termina il processo. Con durata finita l'estinzione conclude la run.
  Headless rilascia SFML/ImGui, Mostra GUI ricrea la finestra sulla stessa run.
- `simulator.cpp`: inizializza il mondo prima dello spawn; aggiorna agenti poi ambiente.
- `pops_manager.*`: ownership shared_ptr, bucket sense→think→act, FIFO nascite,
  morti e NewGen. Guard Pop impediscono fasi fuori ordine anche alla nascita.
- `pop.*`: sensori, azioni, fisiologia, learning e riproduzione asessuata; PHY
  ereditato separatamente dai geni. Direzione iniziale fra8 vicini, mai zero.
- `gene/genome`: endpoint uint32, mutazione completa1% e ritocco del solo peso5% condizionato
  alla mancata sostituzione, massimo256 quanti per ritocco; copie preservano
  endpoint e domini. La sostituzione di una connessione attiva resta cablabile
  e non nulla; i geni silenti mutano senza questo vincolo. Usare
  `Genome::encoding_from_config` nei percorsi produttivi.
  Costruttori standalone senza encoding mantengono legacy32/32.
- `brain/neuron`: rete feedforward sparsa, hidden1…6, sigmoid, massimo unico;
  uscite neutre o massimi pari producono DO_NOTHING21. Decadimento limitato al modulo
  1/8192; plasticità libera di attraversare zero. Eredità arrotonda al quanto più vicino,
  preservando il segno dei non nulli con minimo1 quanto; zeri esatti restano zero.
  Geni fuori dimensioni attive silenti senza modulo.
- INI57 input: densità FOOD/DANGER/MATE/HOME24–27; glucosio33–36,
  gradienti direzionali DANGER/MATE/HOME37–48;
  32 segnala l'ultimo movimento fallito e49–56 la percorribilità delle otto
  destinazioni al passo corrente. I checkpoint con49 input mantengono32 a zero.
  `SENSOR_COUNT` è un conteggio, non un input. Sensitività PHY imposta il raggio
  iniziale delle scansioni spaziali; l'azione20 aumenta raggio effettivo e reattività.
  FOOD24–27 invariato. INI22 uscite include DO_NOTHING21;23 abilita KILL_FORWARD22.
  Dimensioni extra non implementano nuovi sensori/azioni. Rewire viventi non automatico.
- Eredità dei pesi appresi **true**, esplicitamente richiesta dall'utente: preservarla.
  Mutazione INI0,01, applicata anche ai tratti PHY. Distinguere dai default Config.
- GUI/web condividono token ed etichette in `dashboard/static/design.json`. Preferenze
  `appearance.json` accanto all’INI; comandi runtime whitelistati in `Config::runtime_schema`,
  atomici sul thread simulatore. Override inclusi nei checkpoint, audit in SQLite.
  Sandbox spawn/cella/peso solo in pausa; stepping 1–10000 tick in batch <=8.
- Scorer individuale: cinque pesi reward configurabili e normalizzati (default invariati),
  baseline mobile e segnale relativo; parametri learning in INI.
  Tracce registrate in act, uscita scelta e attività hidden; decadono ogni tick. Baseline/tracce
  non ereditarie; pesi appresi ereditati. Stats conserva cumulativi dei morti e medie istantanee dei vivi.
- Cap configurabile, 360 vivi nell'INI distribuito; telemetria separa idoneità agente×tick al cap, tentativi senza spazio e nascite.
  Nessun nuovo accodamento al cap; i candidati già in FIFO restano in attesa.
- Riproduzione: cella nella prima corona; se piena cerca fino al raggio configurato
  (2 nell'INI distribuito). Costo energetico dissipato, poi trasferimento1/8 di risorse/energia;
  nessuna dotazione gratuita ai figli. Distinguere nascite naturali e forzate.
- NewGen seleziona/abbatte e redistribuisce i superstiti in celle libere casuali,
  preferendo due celle di separazione. Conserva identità, tratti, riserve, pesi
  appresi e appartenenza ai bucket; azzera fase/decisione e credito temporale
  dopo il trasferimento. Non è una nascita né profondità genealogica.
- Sense/think: `tbb::parallel_for` in task_arena limitata da num_threads (1 seriale).
  Le fasi terminano prima delle scritture. Azioni/fisiologia/mondo sequenziali.
  Non introdurre scritture condivise nelle fasi parallele; snapshot non autorizzano
  accesso concorrente alla simulazione senza sincronizzazione.
- `grid_world/cell`: occupanti weak_ptr, capacità finite, risorse/campi RBF,
  ossigeno rigenerato localmente e due Source fisiche mobili per il glucosio.
  Un Reaper fisico predefinito cerca saltuariamente Pop maturi, segnala su
  DANGER e può uccidere nella cella davanti dopo un intervallo configurabile;
  feromoni additivi0…100, decadimento0,9995/tick, nessuna diffusione.
  Per cancellare un campo usare delta negativo del valore presente.
  Canali FOOD/DANGER/MATE/HOME non impongono significati o cooperazione.
- `Cell::give_*` restituisce la quantità accettata: sottrarre soltanto quella.
  Morte restituisce riserve; overflow è sink esplicito. Validare bilanci/cap/roundoff.
- Respirazione regolata: riserva target100, banda25, capacità frazionaria accumulata;
  flusso modulato dalla temperatura, inibizione calda continua fino a zero a47 C.
  Rendimento respiratorio gaussiano; reward termico gaussiano continuo. Calore/energia
  solo per reazioni completate. Formule, parametri e limiti nel documento unico.
  Credito non ereditario; reset a init e nessuna raffica dopo scarsità di substrati.
- Statistiche GUI dei vivi non descrivono vite completate. Registrare eventi dei morti.
- sexual_reproduction, inserzione/delezione, choose_parents_by_fitness,
  max_generations e reload @N non controllano i rispettivi comportamenti applicativi.
  steps_per_generation scala l'oscillatore; il sensore età usa
  log1p(age)/log1p(MaxAge). Non scandisce le nascite.
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
