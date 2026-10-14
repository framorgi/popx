# PopX — analisi dinamica breve e correzioni dei bilanci

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
