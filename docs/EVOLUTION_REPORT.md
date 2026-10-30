# Evoluzione della sandbox PopX — 28 ottobre 2026

Riferimento operativo: [stato corrente](EVOLUTION_STATUS.md). Baseline e priorità:
[BASELINE_ANALYSIS.md](BASELINE_ANALYSIS.md). Codice di partenza `4461095057c16401e8d8f1db072bc697c51a91ba`;
intervento presente nel working tree, senza commit automatico. Build release Clang 18,
C++23, SFML 2.6/ImGui, Eigen/TBB; nessuna dipendenza installata.

## Consegna e continuità

GUI C++ e dashboard condividono token, terminologia e controlli della sandbox.
La run iniziale è stata conservata in checkpoint completo verificato
`64aa37455fe14205b5ca4a4acfef6414` al tick 6.842.692, 460 Pop.
Dopo la ripresa intermedia, anche i successivi 28.320 tick sono stati conservati in
`0103f14ee9e04cc8b722a44e76b5ce6e`. La run finale
`1793211087031240027-2962789958` riparte da quest'ultimo checkpoint con nuova origine
temporale e tutte le informazioni biologiche. Verificata running al tick 13.475,
460 vivi, GUI aperta, speed 2. Lo storico precedente è conservato. Cap DB attivo
5 GB, misurati circa 2,835 GB; nessuna modifica al limite o alle regole ecologiche
della run ripresa.

Le prime verifiche del nuovo binario hanno creato anche una run breve e una al
tick zero; non sono state presentate come esperimenti evolutivi. I test hanno
processi e database separati. Non sono stati modificati `old/` o il layout utente
`imgui.ini`.

## Interfaccia e osservazione

- Sidebar C++ di massimo 390 pixel, pannelli Mondo / Pop / Sandbox. La mappa usa
  tutto il viewport disponibile e mantiene proporzioni corrette.
- Rotellina per zoom 1–24×, trascinamento centrale per pan (destro anche fuori
  dalla sandbox in pausa), Home per
  ricentramento; clic per selezione, follow del Pop. F1/F2/F3 per i pannelli,
  F4 per aprire il monitor, F per attivare/disattivare follow. Spazio pausa/riprende,
  punto avanza un tick. Le scorciatoie rispettano la cattura degli input ImGui.
- Layer esclusivo elevazione, terreno classico, temperatura, sette risorse o densità Pop 5×5;
  quattro canali feromonali sovrapponibili. Temperatura: scala fissa 0–30–60 °C;
  risorse: quantità/capacità. La densità usa somme prefisse e una scansione lineare,
  includendo solo Pop, con denominatore corretto ai bordi.
- Overlay dei valori: eliminati blob disallineati e loro vertici;
  corretti i centri degli agenti e la triplice emissione dello stesso triangolo.
  Source resta un diamante nero bordato. Triangolo = cloroplasti > mitocondri,
  come nel viewer web esistente: descrive questa convenzione morfologica, non
  certifica una reazione fotosintetica in corso.
- Inspector C++: avatar genetico, bussola/vettore, età, figli, energia, temperatura,
  distanza, tratti ereditari, genoma cliccabile e tutti i campi della telemetria.
  Gli ID non più vivi vengono deselezionati. Contatori delle vite concluse restano
  negli eventi/analisi web: i contatori del Pop selezionato descrivono la sua vita.
- Brain Monitor C++ e web: attività dell'ultimo campione oppure EMA con coefficiente
  0,2 **per campione osservato**, picchi dall'inizio dell'osservazione; peso firmato,
  spessore proporzionale al modulo, soglia, dettagli e modifica dell'arco esistente.
  Max 4 Hz C++ / 1 Hz web su richiesta, un solo Pop. Visualizzazione limitata ai
  primi 3000 archi sopra soglia, esplicitamente indicata. I campioni del monitor
  rimangono in RAM; gli snapshot neurali su disco restano manuali.
- Genoma: quanti ereditari /8192, distinto dai pesi correnti appresi. Il viewer web
  conserva ordinamenti, filtri e confronto con classificazione dei geni.

Il monitor campiona l'ultimo forward pass disponibile; non ricostruisce ogni
attivazione intermedia fra campioni e non dimostra causalità comportamentale.

## Design system e persistenza

`dashboard/static/design.json` è la fonte comune di palette Lagoon/Slate, colori
semantici, risorse, fattori del mixer, strati neurali, etichette, spacing e radius.
C++ lo legge all'avvio; FastAPI serve gli stessi token in `/api/design.js` prima
dei moduli web. Le etichette legacy sono adattate alla dimensione sensoriale;
il conteggio dei sensori continua a non essere un ingresso.

Temi, scala Pop, griglia, etichette del monitor e scala UI sono modificabili da
entrambe le interfacce tramite il comando `appearance`. Le preferenze vengono
validate, scritte con sostituzione atomica in `appearance.json` accanto all'INI,
e pubblicate nello status. Il file locale è ignorato da Git. Camera e zoom restano
specifici della GUI; la griglia appare solo da zoom 4×. Non sono inclusi editor
liberi dei singoli colori o importazione di temi arbitrari.

Il significato positivo/negativo delle connessioni resta verde/corallo anche nel
tema Slate; i colori genetici sono dati derivati dal genoma, preservati nei due
frontend. Gli stessi fattori, ordine, colori, cursori orizzontali, quote effettive
e preset sono usati nei due Learning Mixer. I vecchi grafici analitici conservano
una palette fissa comune; il tema modifica superfici e controlli.

## Tempo e API runtime

GUI/UDP convergono su `PopXApp::dispatch`, nello stesso thread fra tick. Nessun
accesso concorrente ai Pop è introdotto. Sense/think restano paralleli con barriera;
azioni e mondo restano sequenziali. Il rendering resta una responsabilità separata,
eseguita nello stesso thread come prima.

`step` accetta 1–10.000 tick interi dalla pausa, esegue la normale pipeline in batch
massimi di 8, raccoglie le statistiche e torna in pausa. Non blocca il dispatcher
per l'intero avanzamento. Un checkpoint con richiesta di pausa cancella il residuo
di stepping. Durata/estinzione possono terminare prima del numero richiesto.
Play/pausa, 1×/2× e turbo hanno la stessa semantica nei frontend. Turbo chiude SFML;
Mostra GUI riapre la stessa run a 2×. Stop conclude la run; Chiudi simulatore termina
il processo e il launcher chiude il backend. Azioni distruttive C++ hanno conferma.

La GUI calcola un budget di rendering dal costo medio dei tick e dalla velocità
richiesta, lasciando margine alla simulazione. FPS configurati sono un tetto.
In pausa ridisegna per input e altrimenti circa una volta al secondo. Lo status
espone `effective_tps` e `render_fps` recenti; le medie cumulative restano nelle
telemetrie storiche.

| Categoria | Procedura effettiva |
|---|---|
| Learning, cinque pesi reward, cap vivi, raggio deposito figli, mutazioni di connessione/peso, temperatura ottimale, target respiratorio, FPS | `runtime_apply`, patch atomica validata dallo schema C++ |
| Spawn, temperatura locale, delta FOOD, peso di una connessione esistente | Sandbox in pausa, audit diagnostico con tick e argomenti |
| Mondo, numero/ritmi Source, dimensioni NN, popolazione iniziale, thread e altri INI non inclusi nella whitelist | Nuova run da INI; controlli ordinari di configurazione |
| Topologia di una rete viva, tratti fisici individuali, costanti compilate come MaxAge | Nessun editor runtime aggiunto; codice/configurazione e nuova costruzione appropriata |
| Configurazione da checkpoint | Editor INI resta bloccato; gli override sandbox espliciti sono ammessi e auditati |

Ridurre il cap non abbatte agenti; resta il vincolo di validazione cap >= popolazione
iniziale configurata. Le mutazioni riguardano la prole successiva. Le patch runtime
non riscrivono immediatamente l'INI sul disco: aggiornano configurazione viva e
INI incorporato nei prossimi checkpoint. Restore riproduce questi valori; i vecchi
checkpoint senza pesi reward ricevono i default equivalenti al modello precedente.

Lo spawn aggiunge un solo Pop casuale nella cella scelta, rispettando cap e occupazione;
è una nascita forzata con apporto esterno contabilizzato. Non usa i contatori di
recupero da estinzione. La temperatura locale può essere sovrascritta dal prossimo
refresh climatico; FOOD usa il deposito/ritiro esistente e il decadimento naturale.
Dal 29 ottobre 2026 la sandbox offre tutti i sette materiali, i quattro feromoni,
Pop, Source e temperatura da combobox. In pausa, il pennello C++ disegna segmenti
di celle con il sinistro e rimuove il tipo scelto con il destro; per temperatura
il destro ripristina l'ultimo valore precedente della singola cella nella sessione.
I Source aggiunti sono fisici e il loro numero viene letto dall'ambiente nel
checkpoint, così anche una modifica manuale del conteggio sopravvive al restore.
Le variazioni manuali dei sei materiali del bilancio entrano nella voce
`sandbox_input`; N2 resta visibile e salvato nel checkpoint ma non faceva già
parte del bilancio dei sei materiali. La velocità climatica è un moltiplicatore
runtime del passo angolare RBF (0 = fermo, 1 = .009 rad per refresh), senza
allargare le campane. Elevazione usa la scala neutra; Terreno interpola i colori
classici per quota. Le risorse sono quadrati centrali di lato mezza cella; i
feromoni sono marcatori a otto vertici con angoli smussati.
L'editor neurale modifica solo un arco già presente, con peso finito in [−4,4],
senza cambiare topologia; azzera decisione pendente e credito temporale. L'eredità
appresa resta abilitata e può trasmettere anche tale modifica esplicita.

## Formula del Learning Mixer

Con cinque pesi non negativi w in [0,1], α_i = w_i / Σw (tutti zero → α=0):

- Età: u_A = clamp(età / 200000, 0, 1).
- Energia: u_E = clamp((E/400 − 0,25)/0,75, −1, 1).
- Variazione: u_D = clamp((E − E_precedente) / max(1e−6, max(1,mitocondri) × energia_per_respirazione), −1, 1).
- Discendenza: u_F = clamp(figli/25, 0, 1).
- Termica: u_T = 2 exp(−ln(2) × ((T − T_ottimale)/semiampiezza)^2) − 1.

Reward r = clamp(Σα_i u_i, −1,1). Segnale s = clamp(r − baseline_precedente,−1,1),
poi baseline += β(r − baseline). La prima osservazione inizializza la baseline a r.
I default 0,10 / 0,30 / 0,35 / 0,05 / 0,20 conservano i pesi precedenti.

Aggiornamento w_neurale = clamp(w_neurale + η × s × traccia − decadimento,−4,4).
Le tracce degli output sono iniettate solo per l'azione scelta, usando pre × post ×
(1−post); hidden usa (pre−0,5)(post−0,5). L'iniezione è scalata con 1−λ³, le tracce
sono limitate a ±0,25 e decadono per tick con λ. Non è backpropagation o Q-learning.
Baseline e tracce non vengono ereditate; i pesi appresi sì. Il decadimento continua
anche con reward tutta zero. Cambiare parametri runtime azzera credito/baseline;
le barre mostrano l'ultimo campione raccolto, il cui tick è visibile.

Non sono stati aggiunti premi arbitrari per movimento, aggregazione o esplorazione.
Il punteggio NewGen (figli + 0,01 × energia) resta distinto da questa ricompensa.
L'analisi della baseline non dimostrava che la staticità fosse causata da una rete
spenta: occupazione e cap restavano vincoli dominanti, segnale e plasticità avevano
modulo e dispersione non nulli. Questi strumenti permettono nuove indagini, non
costituiscono da soli una soluzione all'adattamento direzionale.

## Prestazioni e validazione

Misure indicative su questa macchina/display WSL; RNG produttive non deterministiche.

| Osservazione | Tick medio | Frame medio | FPS |
|---|---:|---:|---:|
| Run iniziale, tick 6.647.600, 460 vivi (medie cumulative) | 2.839 µs | 31.006 µs | 4,04 |
| Pilot intermedio headless, 2000 tick, 142 vivi finali | 821 µs | — | — |
| Pilot intermedio GUI, 2000 tick, 114 vivi finali | 1.022 µs | 39.090 µs | 12,45 |
| Run finale, tick 13.475, 460 vivi (medie cumulative) | 3.215 µs | 17.579 µs | 10,57 |

Nell'ultimo intervallo della run finale: **201,8 tick/s, 12,0 FPS**, RSS 303.552 KiB
(simulatore), 52.052 KiB (backend). Sono campioni, non garanzie o percentili.
Le differenze di popolazione e di finestra impediscono di attribuire causalmente
il costo dei tick alla sola UI. Il rendering è più reattivo e la velocità 2× è
stata osservata effettivamente dopo il budget adattivo.

È stata anche provata una texture con un pixel per cella. Confronto seriale dallo
stesso checkpoint, 2000 tick, 460 vivi: renderer a quad 12,851 s / frame 22.576 µs;
texture 14,172 s / frame 22.710 µs. Nessun beneficio misurato: **ottimizzazione scartata**.
Il protocollo è conservato in `scripts/benchmark_gui.py`, dati SQLite in
`data_out/audit/ux/renderer-comparison/`. Un pilot della texture eseguito con contesa
fra processi ha prodotto 21,324 s e frame 63.037 µs; non è usato per un confronto
causale né attribuito alla versione consegnata.

Verifiche realmente eseguite:

- Build esplicita `popx` PASS; `scripts/run_regressions.sh` PASS per tutte le meccaniche
  preesistenti. Nuovo `sandbox_regression` PASS e aggiunto allo script: normalizzazione,
  reward zero, patch atomiche, topologia invariata, credito azzerato, bilanci spawn.
- `tests.sandbox_integration` PASS anche sull'ultimo binario: stepping esatto e limiti,
  validazione, spawn/occupazione, pesi, preferenze persistenti, checkpoint/restore.
- Otto test Python di checkpoint, storage limit e dashboard PASS; quattro test
  checkpoint/sandbox rieseguiti dopo le correzioni PASS.
- `tests.gui_smoke` PASS: GUI/headless sulla stessa run, tick fermo in pausa,
  apertura/chiusura/riapertura, uscita pulita. CPU in pausa 0,03 s headless e 0,05 s
  GUI su due secondi ciascuno (prova intermedia).
- `tests.gui_navigation_smoke` PASS sul renderer finale: eventi X11 reali per
  selezione, zoom, Home, F1–F4; catture del monitor e mixer, tick biologico invariato.
- Tutte le otto regressioni JavaScript PASS, inclusi nuovo wiring sandbox, genoma,
  nomi legacy, filtri rete, uscita, grafici evolutivi, pulizia storico e derivate.
- Sintassi Python/JS, diff whitespace e endpoint reali sandbox/token verificati.
- Checkpoint della produzione ripristinato e run finale lasciata running.

Screenshot locali: `data_out/audit/ux/before-gui.png`, `after-gui.png`,
`brain-monitor.png`, `learning-mixer.png` e `final-running.png`.
Il desktop root WSL non era catturabile; cattura della singola finestra X11 riuscita.
Automazione del mouse tramite warp/XTest non affidabile su WSL; il test ripetibile
usa eventi diretti alla finestra isolata. Non è disponibile un browser/Playwright:
**nessuno screenshot web o collaudo visuale browser dichiarato**. Token e logica UI
sono verificati con test, ma questo non sostituisce un controllo visivo dal telefono.

## Limiti e seguito

La dashboard è pronta nella nuova pagina `/static/sandbox.html`. Resta utile verificare
nel browser reale responsività, contrasto, scaling e identità visiva del mixer.
La GUI conserva alcuni dettagli tecnici in viste espandibili; il genoma C++ ha
selezione/tooltip, mentre ordinamento e confronto approfondito rimangono nel web.
Non sono introdotti replay deterministici globali, editing libero della topologia,
nuovi premi comportamentali o tratti fisici individuali modificabili.

Per indagare adattamento direzionale serviranno ancora prove ecologiche appaiate
con acquisizione, costi, sopravvivenza e discendenza: un arco attivo o una reward
positiva non dimostrano da soli una politica utile.
