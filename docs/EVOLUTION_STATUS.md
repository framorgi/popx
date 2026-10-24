# PopX — stato corrente, validazione e archivio delle indagini

Documento unico di riferimento del progetto. `AGENTS.md` contiene le istruzioni
operative. Audit e indagini precedenti sono conservati sotto con risultati e
limiti originali; non descrivono automaticamente il modello attuale.

The [README](../README.md) covers the project, features, mathematical model, architecture,
and setup. Validation status and audit records remain in this document.

## Stato corrente — genomi compatti, mutazione valida e storico azzerato, 24 ottobre 2026

L'utente ha autorizzato la chiusura della run precedente e la cancellazione
completa dei dati operativi. Prima della pulizia, `data_out` occupava circa
4,67 GB, il database SQLite conteneva una sola run ed era ancora sotto la
soglia configurata di 10 GB: la crescita osservata in quel momento non era
una rotazione fallita. Due checkpoint automatici nel catalogo appartenevano
a rotazioni di run precedenti. Nel database esaminato la tabella `events`
occupava circa 3,25 GiB; i geni costituivano circa il 46% di un evento di
nascita recente. Su 500 eventi reali, la lista dei geni in JSON pesava in
media 1.728 byte; una codifica binaria lossless con tipi uniti in un byte,
indici `uint32` a varint, peso `int16` e Base64 ne usava 668. La codifica
versionata riguarda solo le righe SQLite nuove; protocollo, API e lettura
degli eventi storici conservano gli array di geni originali. Gli script che
escludono i geni dalle analisi ignorano anche il campo compatto.

La rotazione tramite checkpoint continua a costruire un database nuovo e a
sostituire il vecchio. Ora controlla esplicitamente che entrambi i WAL siano
stati troncati e che dopo la chiusura non rimangano file WAL/SHM trattenuti da
lettori esterni; in quel caso conserva lo storico, segnala errore e riprova.
Questa verifica ha intercettato un lettore lasciato aperto nel fixture di
integrazione: senza il controllo, il vecchio inode poteva restare occupato
anche dopo la sostituzione del nome del file. Il test corretto chiude il
lettore e conferma la rimozione del database grande e di tutti i file
temporanei della rotazione. Un retry nello stesso processo riutilizza la run
già ripristinata e in pausa, senza caricare ancora il checkpoint. Fixture
dedicata verifica errore con lettore aperto, conservazione dello storico e
successo dopo la chiusura del lettore.

`Genome::mutated` ora campiona un sostituto cablabile e di peso non nullo
quando il gene sostituito era una connessione effettiva per le dimensioni
della rete attiva. I geni silenti continuano a mutare normalmente e possono
riattivarsi. La mutazione del solo peso non trasforma un collegamento attivo
in uno zero esatto. Questo preserva la validità della singola connessione,
non garantisce un percorso sensore→attuatore né premia il movimento. Fixture
dedicate verificano mutazioni ripetute e domini degli indici.

Dopo build, 18 regressioni C++, test Python del codec, API/genealogia e
integrazione della rotazione passati, la run precedente è stata chiusa con
Esci. È stato eliminato ogni contenuto di `data_out` (database, WAL,
checkpoint, audit, snapshot, log e file temporanei); l'archivio di codice
`old/` e il layout utente `imgui.ini` non sono stati modificati. Nuova run
`1792865911637053038-3461282500` avviata headless turbo con dashboard dopo
un secondo azzeramento, necessario per attivare nel processo Python la
correzione finale del retry; al primo controllo tick 2.960, 183 vivi,
configurazione sbloccata e un solo storico SQLite. Gli 80 fondatori iniziali
sono codificati nel database e l'API restituisce ancora i genomi completi.
Il filesystem ext4 ha liberato
i blocchi Linux; `fstrim` è stato rifiutato dall'ambiente e la dimensione
fisica del VHDX Windows non è verificabile da questo container. Un VHD
dinamico può mantenere la dimensione fisica raggiunta dopo la cancellazione;
il recupero sul lato Windows richiede un'operazione di compattazione a WSL
fermo e non è stato eseguito, per lasciare attiva la nuova run.

## Stato corrente — degenerazione della mobilità nella run attiva, 24 ottobre 2026

Analisi in sola lettura della run `1792842595866512534-87985946`, lasciata in
esecuzione. È una ripresa da checkpoint completo della run
`1792793607130415743-126939361` al tick sorgente 40.610.513: il tick zero
locale contiene 313 Pop importati, non 80 fondatori casuali. INI della run:
57 ingressi, 5 strati hidden da 8, 22 uscite, cap 360, raggio di dispersione 2,
NewGen periodico disabilitato. Le osservazioni qui sotto non sono una prova
di causalità né un confronto controllato fra configurazioni.

Al tick locale 16.202.000 la popolazione è tornata a 360 vivi dopo **44
estinzioni complete** e 880 nascite forzate dalla riserva ereditaria. I recuperi
sono 29 nei primi 8 milioni di tick, 14 nei successivi 8 milioni e uno dopo:
la frequenza delle estinzioni non sta aumentando. La popolazione media ai
campioni da 200 tick passa da circa 304 a 328 fra le due metà. Resta però
instabile: nell'ultimo collasso osservato scende da 360 a 12 vivi fra i tick
16.000.000 e 16.160.000, mentre lo scarto termico medio dei vivi raggiunge
21,2 °C. In questa finestra ci sono 6.849 morti, 5.412 attribuite a energia e
1.437 a temperatura; glucosio e acqua totali del mondo non diminuiscono.
Questi totali non misurano la disponibilità nelle celle occupate.

La mobilità funzionale peggiora. Negli ultimi 500 nati campionati fra i tick
16.172.618 e 16.187.921, solo 3 genomi hanno un percorso di geni non nulli,
valido per le dimensioni attive, da un qualsiasi sensore a un output di
movimento; nessuno lo ha da temperatura, densità o percorribilità. Nei 500
nati campionati fra 957.049 e 999.974, 428 avevano un percorso da
percorribilità a movimento. La quota media di geni cablati nei due campioni
passa dal 6,7% al 2,3%; la lunghezza mediana del genoma resta 100. I nati
sono genealogicamente correlati, quindi questi rapporti descrivono quei
campioni, non individui indipendenti o tutta la popolazione viva. Inoltre
un percorso strutturale non garantisce che l'azione sia scelta.

L'osservazione delle azioni è coerente con questa perdita: nelle ultime 1.000
morti ispezionate, `DO_NOTHING` rappresenta il 95,7% delle circa 3,55 milioni
di azioni registrate; 996 individui non hanno mai tentato un movimento e
944 non hanno né tentato né percorso distanza. Nelle 34.212 morti esaminate
dal tick 15 milioni, l'età
mediana è 710 tick e 24.810 muoiono prima della soglia riproduttiva di 1.000
tick. Fra 8 e 16 milioni di tick, il 93,8% dei tick-agente idonei alla
riproduzione è bloccato dal cap, mentre il 93,7% dei tentativi processati non
trova spazio entro raggio 2. Questo coesiste con estinzioni ricorrenti: il
cap non indica da solo una popolazione ecologicamente stabile.

Interpretazione provvisoria: la rete sparsa e il genoma di 100 geni consentono
che linee con pochissimi collegamenti attivi continuino a riprodursi quando
la cella offre condizioni favorevoli. `DO_NOTHING` è gratuita e la fisiologia
continua automaticamente. Inserzione e delezione non sono operative nel codice
corrente e sono fissate a zero nell'INI: il massimo configurato di 900 geni
non allunga questi discendenti. L'ereditarietà dei morti recuperati conserva
anche questi genomi. L'argmax neutro o in parità sceglie `DO_NOTHING`, ma la quota
istantanea di pesi esattamente nulli è zero: il problema osservato è soprattutto
la **mancanza di percorsi funzionali**, non il decadimento di tutti i pesi a
zero. Il recupero spiega la continuità genealogica e il ritorno al cap, ma
non dimostra miglioramento della capacità di seguire un clima mobile.

Prima di cambiare reward o forzare movimento, verificare in fixture se
sensori di temperatura e percorribilità possono guidare un vantaggio ecologico
con questa topologia; poi confrontare run con identica ecologia ma diversa
capacità di produrre e preservare collegamenti neurali validi. Una misura
prioritaria è la quota di nati e vivi con percorsi sensore→movimento, insieme
a movimento riuscito, sopravvivenza e figli naturali per linea. La run attuale
può restare come controllo storico; prolungarla da sola non isola il motivo
della staticità.

Audit del meccanismo di perdita: ad ogni nascita `Genome::mutated` sostituisce
l'intero gene con probabilità 1%, riassegnando entrambi gli endpoint e il peso.
La mutazione del solo peso (5% se non c'è sostituzione) e la plasticità non
creano nuovi endpoint. `Brain::wire` accetta solo tipi di strato feedforward
attivi e indici entro le dimensioni 57/8/22; conserva nel genoma i geni
scartati o silenti, che però non sono sinapsi funzionanti. Nei 500 nati
campionati fra 15 e 16 milioni di tick, 330 non avevano alcun gene cablato con
peso non nullo: su 50.000 geni complessivi, 214 erano cablati, 13.032 silenti
per indici e 36.754 invalidi per tipo/ordine; nessun peso era zero. Negli
ultimi 500 nati campionati dopo il tick 16 milioni, i geni cablati erano
1.338 su 50.000 e solo 3 reti avevano un percorso sensore→movimento. La
sostituzione casuale può anche creare connessioni, ma i colli di bottiglia
demografici, la riproduzione di linee immobili e il riuso dei donatori possono
amplificare quelle che non le hanno. Senza un confronto parentela→mutazione→
discendenza non si può attribuire una quota esatta della perdita alla mutazione
rispetto alla selezione o alla deriva genetica.

## Stato corrente — affollamento e possibilità di fuga, 23 ottobre 2026

Analisi non invasiva della run `1792748686533986224-712313108`, lasciata in
esecuzione. Il codice non ha una morte per affollamento: la fisiologia registra
energia, temperatura ed età come cause ordinarie. Lo spazio può influire
indirettamente su movimento, accesso alle risorse e riproduzione; questi
contatori non identificano da soli la causa di una singola morte.

Al tick 1.400.000, su 37,78 milioni di tentativi di movimento cumulativi,
il 5,9% era riuscito, il 73,9% era fallito per occupazione e il 19,4% per
pendenza. L'86,3% dei 260.994 tentativi riproduttivi processati era senza
spazio entro raggio 2; il 96,5% dei tick-agente idonei alla riproduzione era
bloccato dal cap. Questi valori descrivono una forte pressione spaziale
locale, pur in una mappa globalmente poco occupata. Fra tick 300.000 e
360.000, durante il collasso da 185 a un vivo, il 56,2% dei tentativi di
movimento era bloccato dalla pendenza e il 42,6% dall'occupazione; le morti
per temperatura sono aumentate di 976 e quelle per energia di 739. La
distanza termica media dei vivi è salita da 12,2 a 23,0 °C. Questi dati
suggeriscono una componente climatica e topografica nel collasso, senza
dimostrare una causa unica o l'effetto causale dell'affollamento.

Al tick 1.553.961, uno snapshot manuale dei 360 vivi mostrava 319 agenti
senza alcun tentativo di movimento e 338 senza movimento riuscito. In un
secondo controllo iniziato al tick 1.625.057, 325 dei 360 vivi elencati
erano ancora presenti all'ispezione. Nessuna delle 325 reti ispezionate
aveva un percorso con pesi non nulli dai sensori di densità (8…11) o di
percorribilità (49…56) verso un output; nessuna collegava i sensori termici
(12…19) agli otto output di movimento. Otto reti avevano un percorso termico
verso un qualche output. Tutte le 325 avevano almeno un percorso da qualche
input verso il movimento: la difficoltà specifica è usare l'informazione
ambientale per scegliere *dove* muoversi. La presenza di un percorso resta
solo una condizione necessaria, non prova di decisione efficace. Il campione
di vivi esclude 35 agenti non più ispezionabili durante la lettura e descrive
un solo momento, non tutte le generazioni.

Al tick 362.643 c'è stata una sola estinzione completa nella run osservata:
20 semi recuperati da 20 donatori distinti della riserva. La popolazione è
poi tornata a 360 vivi. Il recupero preserva continuità ereditaria ma
introduce individui con riserve piene in posizioni casuali: può attenuare
temporaneamente la selezione per il movimento. Poiché la staticità e il
collasso erano già presenti *prima* di questo primo recupero, esso non ne è
la causa in questa run. La deposizione della prole nella seconda corona può
trasferire linee verso zone favorevoli senza movimento degli adulti.

Conclusione operativa: mantenere distinguibili sopravvivenza naturale e
recuperi; prima di cambiare reward o forzare la fuga, verificare con una
fixture a politica controllata se l'informazione di cella libera e temperatura
permette effettivamente di aumentare sopravvivenza e nascite in terreni
percorribili. Misurare poi nelle run evolutive la quota di reti con un percorso
sensore→movimento e i successi condizionati alle vie fisicamente libere.

## Stato corrente — continuità ereditaria dopo estinzione, 23 ottobre 2026

Il recupero delle run infinite usa una riserva di massimo 64 profili recenti.
Alla rimozione ordinaria di un morto il manager conserva il genoma ereditabile
(pesi appresi quantizzati con le stesse regole delle nascite naturali quando
`hebbian_inheritance=true`), PHY e ID del donatore. La chiave di deduplicazione
è la firma del genoma di nascita più i quattro tratti PHY: il donatore più
recente sostituisce il precedente con la stessa chiave. A riserva piena si
elimina il profilo aggiornato da più tempo. Gli abbattimenti NewGen non
alimentano la riserva. Nessun Pop morto o stato fisico resta trattenuto.

All'estinzione si estraggono i profili in ordine casuale senza ripetizioni
finché possibile, ripetendo l'estrazione se la riserva è più piccola del
numero di fondatori necessari. Il target resta 20, limitato dal cap e dalla
dimensione della mappa. Si copiano genoma ereditabile e PHY senza nuove
mutazioni; età, scorer, tracce neurali, stato fisico e direzione sono
inizializzati come per i fondatori. Le successive nascite naturali usano le
mutazioni INI ordinarie. Il posizionamento resta casuale sulle celle libere;
non cerca una temperatura favorevole. Soltanto una riserva vuota usa il
precedente fallback casuale. Il meccanismo conserva informazione ereditaria
fra estinzioni, senza garantire la persistenza di ogni ceppo né la stabilità
ecologica. È un intervento esterno, non sopravvivenza naturale.

I recuperi sono eventi `birth` forzati con `parent_id`, `recovery_seed` e
`recovery_from_archive`. Le loro dotazioni sono ingressi espliciti nei bilanci
di materia ed energia anche con donatore noto. `counts.inherited_recovery_births`
è un sottoinsieme delle nascite forzate; `reproduction.recovery_bank_size`
misura i profili disponibili. La pagina Linee evolutive mantiene la famiglia
del donatore e applica la normale soglia di somiglianza per i rami; la
profondità include i passaggi tramite riserva, mentre i figli conteggiati
nelle statistiche di fitness restano solo quelli naturali.

Il checkpoint include la riserva compatta; i vecchi checkpoint senza riserva
sono caricabili e la riempiono alle morti successive. Dopo una rotazione
dello storico, gli ID dei donatori archiviati restano registrati con origine
esterna alla nuova run: la dashboard segnala il recupero e non inventa lo
storico degli antenati assenti. Un nuovo Start/Restart azzera la riserva.

Validazione: fixture dedicate per morte→riserva→recupero, pesi appresi e geni
silenti, PHY, reset di scorer/tracce, campionamento bilanciato, limite di
memoria, deduplicazione e fallback vuoto; roundtrip del checkpoint con
riserva, compatibilità legacy e rifiuto di riserve sovradimensionate.
Il test dei bilanci di recupero verifica anche i fondatori con donatore noto.
Il test genealogico distingue recuperi, figli naturali e donatori di run
precedenti. Queste prove verificano la meccanica; non sono una misura di
miglioramento evolutivo a lungo termine.

Esiti: `bash scripts/run_regressions.sh` (18 regressioni C++), 7 test
genealogici Python, 4 test di integrazione checkpoint/fallimenti/marcatori
di estinzione e regressione UI genealogica passati; catalogo metriche,
sintassi JS/Python e `git diff --check` verificati. HEAD resta
`f6b6fbd042a3cc17eb8da59002ea15cab54e0bcd` con modifiche locali; INI e
diff conservati in `data_out/audit/hereditary_recovery_20261023/`.

Come richiesto, chiusi launcher, simulatore e dashboard precedenti e rimosso
lo storico operativo SQLite (4.560.650.240 byte); checkpoint separati
conservati. Nuova run `1792748686533986224-712313108`, headless turbo,
configurazione ordinaria sbloccata, cap 360 e limite database 10 GB.
Raggio di dispersione INI ancora 2, così la nuova run introduce il solo
meccanismo di continuità rispetto ai parametri precedenti. Al controllo
tick 53.000: 360 vivi, 1.003 nascite naturali, 723 morti, 64 profili nella
riserva, nessun recupero ancora necessario; residui dei bilanci di materia
ed energia tutti zero. Le estinzioni successive e la stabilità ecologica
restano da osservare nella run; il recupero è già verificato nelle fixture.

## Stato corrente — dispersione della prole, 23 ottobre 2026

`offspring_dispersal_radius` ora controlla effettivamente la ricerca della cella
di nascita fino al raggio configurato (schema INI: 1…8). Le corone di distanza
di Chebyshev sono provate in ordine e ciascuna è mescolata; le posizioni fuori
mappa sono escluse. `reproduction.dispersed_births` conta tutte le nascite
oltre la prima corona, inclusi i raggi 3…8. La regressione locale verifica
il fallimento con raggio insufficiente, le nascite nelle corone 2, 3 e 4 e
la conservazione delle risorse trasferite.

La run sospesa descritta sotto conserva la configurazione e il processo
precedenti (raggio 2). Il suo checkpoint resta invariato. Per valutare
l'effetto ecologico della correzione serve una nuova run con raggio 3 o 4,
confrontata con raggio 2 a parità degli altri parametri; il fix della ricerca
non dimostra da solo che la dispersione prevenga i collassi.

## Stato corrente — collassi della run lunga, 23 ottobre 2026

La run `1792735061314312259-1932886052`, nata dal checkpoint completo
`caeba8cdc3c24a6497add06ab80ebc2b` della run precedente al tick 15.486.384,
è stata sospesa reversibilmente al tick 7.711.341 e 360 vivi. Il checkpoint
manuale completo e verificato `be987ea5ba304650a3fc3406009f00b0` conserva
questo stato. Database e dashboard restano disponibili; nessun parametro è
stato cambiato. Il database corrente contiene soltanto questo segmento perché
le rotazioni automatiche hanno eliminato gli storici precedenti. Codice in
uso: HEAD `f6b6fbd042a3cc17eb8da59002ea15cab54e0bcd` con diff di lavoro
non committato; parametri effettivi inclusi nel checkpoint/metadata della run.

Al campione finale: 94 estinzioni complete seguite da 1.880 semi casuali,
281.452 nascite naturali, 283.332 morti (206.858 per energia, 76.462 per
temperatura, 12 per età). Dei 94 cicli conclusi, 59 non hanno mai raggiunto
50 vivi; 29 hanno raggiunto almeno 300, con durata mediana circa 212.000 tick
e massimo 695.228. Dopo ogni estinzione `recover_from_extinction()` crea nuovi
fondatori casuali senza ereditarietà dalle linee estinte. Non emerge quindi
accumulo genealogico fra i cicli. Il numero di recuperi nei tre blocchi
successivi di 2,5 milioni di tick è 26, 23, 44: nessun miglioramento
monotono della stabilità. Intervalli brevi fra recuperi spesso fanno parte
di una cascata di tentativi di ripopolamento falliti, non di popolazioni
consolidate che collassano ogni poche migliaia di tick.

I tentativi riproduttivi cumulativi sono 6.919.180; il 95,9% è fallito per
mancanza di spazio entro le due corone locali. Il 54,2% delle nascite riuscite
usa la seconda corona. L'84,3% dei tick-agente idonei è bloccato dal cap 360.
Solo il 9,5% dei 156,8 milioni di tentativi di movimento ha avuto successo;
il 63,5% è stato bloccato dall'occupazione e il 24,9% dalla pendenza. Su una
lettura non invasiva di 329 vivi al tick 7.584.397, il 94,5% non aveva mai
percorso una cella e la mediana aveva tutti gli otto vicini immediati occupati
(23 su 24 entro raggio due). Sono dati di un singolo snapshot, non una prova
che tale densità valga in ogni fase. Il mondo è 180×180: l'addensamento è
locale, non un riempimento della mappa.

Prima di collassi consolidati il glucosio e l'acqua globali restavano
nell'ordine dei milioni di unità. In un episodio i vivi sono scesi da 360
a due mentre la temperatura media dei superstiti scendeva fino a 13,3 °C;
in un altro i superstiti erano attorno a 50 °C. La fisiologia riduce il
rendimento respiratorio lontano da 36 °C e lo inibisce del tutto a 46 °C
con la semilarghezza effettiva di 10 °C. La morte per energia può quindi
essere mediata dal clima senza risultare classificata come morte termica
(soglie dirette 10 e 75 °C). La presenza globale di risorse non dimostra
accessibilità delle stesse nella cella occupata. Il nesso causale fra
addensamento, mobilità, clima e collasso va verificato con confronti
controllati; nessun singolo fattore è isolato in questa run.

Decisione operativa: sospendere la run e conservare il checkpoint, perché
altri tick con la stessa configurazione ripetono cicli di estinzione e
fondazione casuale. Prima di una nuova run lunga confrontare in prove brevi
la dispersione della prole a raggio 2 con 3–4 e monitorare spazio locale,
movimento, durata delle linee e bilanci. Al momento dell'indagine l'INI
esponeva `offspring_dispersal_radius`, ma il codice produttivo limitava il
raggio a 2 (`std::min(..., 2u)`): il limite è stato rimosso nella correzione
documentata sopra. Aumentare durata massima di vita, cap globale
o apporto di glucosio non è supportato come prima correzione da questi dati.

Metodo: campioni SQLite ogni 200 tick, differenze dei contatori cumulativi,
tick distinti degli eventi `recovery_seed`, cicli fra recuperi e un roster
live letto tramite `agents`; telemetria senza messaggi persi al momento della
pausa. Censura del ciclo corrente e delle vite ancora aperte. Il database
storico delle rotazioni precedenti non è disponibile per un confronto
longitudinale completo fra giorni.

## Stato corrente — filtro dei percorsi neurali, 21 ottobre 2026

Il grafo dell'inspector offre tre viste: tutte le sinapsi della rete attiva,
quelle da cui si può raggiungere un output e soltanto quelle appartenenti a
un percorso completo sensore→output. La raggiungibilità usa tutte le sinapsi
con peso non nullo dello snapshot, prima della soglia di visualizzazione;
questa soglia può quindi nascondere parte di un percorso. I geni silenti o
esclusi non compaiono nel grafo della rete attiva e restano nell'elenco del
genoma. I pesi esattamente zero sono visibili solo nella vista «tutte» quando
si abilita il relativo controllo.

Il filtro dei percorsi completi descrive il trasporto possibile di informazione
sensoriale, non tutte le operazioni eseguite da `Brain::feed_forward`. Ogni
strato nascosto applica `sigmoid` anche senza ingressi: un neurone isolato
produce 0,5 e può influire sull'output tramite una sinapsi in uscita. Gli
archi verso rami senza uscita sono calcolati, ma non influenzano la decisione.
La vista «verso output» conserva anche i rami nascosti senza ingresso che
possono contribuire all'attuatore.

Validazione: regressione JS con percorso diretto, percorso attraverso più
strati, ramo senza uscita, neurone nascosto senza ingresso, arco di peso zero,
soglia e Cytoscape; test dell'inspector e `git diff --check`. Su uno snapshot
letto dalla run attiva: 40 sinapsi nella rete, 23 potenzialmente influenti su
un output e 17 su percorsi completi sensore→output. Modifica solo degli asset
statici della dashboard: la run continua senza riavvio.

## Stato corrente — marcatori di estinzione e variazioni temporali, 21 ottobre 2026

La dashboard mostra una linea rossa al tick di ogni ripopolamento dopo
estinzione globale; per una run conclusa per estinzione usa il tick dell'ultimo
campione. I tick provengono dagli eventi di nascita con `recovery_seed=true`,
non dai campioni sottocampionati. L'indice del backend legge una volta lo
storico della run, poi soltanto i nuovi eventi. Le linee compaiono sui grafici
con asse temporale della panoramica e delle linee evolutive; grafici per età,
condizione diagnostica e distribuzioni non hanno un asse dei tick della run.

Per popolazione, stock ambientali, somme dei vivi e contatori/bilanci aggregati
si può commutare tra valore e variazione media per tick, calcolata come
`(valore corrente - valore precedente) / (tick corrente - tick precedente)`.
La prima osservazione dell'intervallo non ha derivata; intervalli di tick non
uniformi sono normalizzati. Nella pagina genealogica anche la composizione dei
ceppi può essere mostrata come variazione per tick, con valori negativi per
contrazione. Le curve derivate descrivono differenze tra campioni disponibili,
non gli eventi istantanei avvenuti fra loro.

Validazione: test JS per derivata, marcatori, panoramica e pagina genealogica;
test Python dell'indice con ripopolamenti duplicati, incrementali e terminali;
integrazioni dashboard/checkpoint e `git diff --check`.

## Stato corrente — costo dei tentativi attivi, 21 ottobre 2026

La run precedente ha mostrato una policy ripetitiva: in 87 reti campionate,
29 Pop hanno richiesto `GET_CALCIUM` 555.067 volte senza acquisire calcio.
Il codice precedente non addebitava energia a trasferimenti, emissioni,
regolazioni e attacchi. Ora ogni azione attiva costa 0,01 energia per tentativo,
anche se non produce effetto; i movimenti mantengono il costo già esistente,
`BURN_CALORIES` mantiene il proprio costo 0,05 e `DO_NOTHING` resta gratuito.
La nuova voce `other_action_cost` separa le azioni attive non motorie dalla
termoregolazione e chiude il bilancio energetico anche dopo una morte fra
azione e fisiologia. I checkpoint v4 precedenti caricano il nuovo contatore a
zero, lasciando invariato lo stato energetico salvato.

La stessa analisi sulle reti ha trovato due gruppi di genomi fra gli 80 Pop
selezionati casualmente (62 e 18, con soglia Jaccard 0,5 sugli endpoint). I
quattro nuovi sensori di densità dei feromoni hanno cambiato una sola scelta
su 87 reti quando portati singolarmente a 0 o 1; il glucosio non ne ha
cambiata nessuna. La convergenza genealogica e le azioni ripetute dimostrano
struttura ereditaria, ma non da sole un adattamento alle risorse mobili.

Validazione: build `popx`, 17 regressioni C++ standalone (incluso il nuovo
controllo di tutti gli attuatori), regressione checkpoint precedente privo del
nuovo contatore, sei integrazioni Python, controllo etichette JS e
`git diff --check` passati. Il checkpoint manuale completo
`0876e5710a2b443f97d668458865af76` della run precedente al tick
8.806.083 è stato verificato e caricato dalla nuova run
`1792587923217596324-2845636136`, che prosegue con lo storico SQLite
conservato. A tick 8.600 della nuova run `other_action_cost=8.319,71`,
bilancio energetico residuo zero; 360 Pop vivi.

Protocollo di decisione direzionale ripetibile:
`python3 scripts/analyze_nn_direction.py --sample 80`. Legge soltanto
`agents`/`inspect`, ricostruisce la rete sparsa, lascia costanti gli altri
sensori e confronta la scelta con tutte le otto direzioni bloccate e con
una sola direzione aperta alla volta. Fra 74 reti disponibili alla nuova
run, 592 aperture hanno prodotto 35 cambi di azione ma zero cambi verso la
direzione corretta; in un successivo campione di 39 reti, 312 aperture
hanno prodotto 24 cambi e ancora zero corretti. La ricostruzione della
scelta osservata coincideva in 69/74 e 39/39 reti rispettivamente. È un
test controfattuale della policy a sensori fissi, non una misura di
sopravvivenza o una simulazione della nuova configurazione fisica.

## Stato corrente — feromoni, raggio sensoriale e cap, 21 ottobre 2026

Gli ingressi 24…27 hanno ora ciascuno un feromone distinto: FOOD, DANGER,
MATE, HOME. Ognuno restituisce la media delle concentrazioni normalizzate
nelle celle della piazza di raggio sensoriale attorno al Pop, esclusa la
cella occupata. Un campo uniforme ha valore uniforme; non si sottrae il
valore locale. Gli ingressi locali 20…23 restano la concentrazione nella
cella del Pop; i gradienti direzionali DANGER/MATE/HOME 37…48 restano
direzionali. Gli indici non cambiano, ma il significato 24…27 cambia: per
questo i checkpoint vecchi, se caricati con il nuovo binario, proseguono con
una semantica sensoriale diversa. La nuova run parte da zero.

Il tratto ereditato `phy.sensitiveness` imposta il raggio iniziale effettivo
`probe_dist_` (1…12 celle). L'azione 20 mantiene l'aumento della reattività
al movimento e aumenta di una cella il raggio effettivo fino al minore fra
il massimo configurato e 12. Le scansioni direzionali di popolazione,
temperatura, glucosio e feromoni e le nuove densità usano questo raggio.
Le misure della cella corrente, gli ingressi interni e la percorribilità
della destinazione immediata mantengono il loro significato esatto, che non
dipende da un raggio di ricerca. Il tratto ereditato resta distinto dal
raggio modificato durante la vita; l'inspector mostra entrambi. Il cap
configurato per la nuova run è 360 Pop vivi.

Le etichette della dashboard seguono il modello sensoriale effettivo:
`SENSOR_COUNT` è soltanto il numero di ingressi, mai un sensore; l'indice 32
dei checkpoint a 49 ingressi rimane etichettato come slot riservato e vale
zero. Gli ingressi oltre 56 sono segnalati come non implementati.

Validazione: build `popx`, 16 regressioni C++ standalone, regressioni JS
etichette/rete/inspector e tre test di integrazione dashboard/checkpoint
passati; `git diff --check` pulito. La nuova run headless turbo
`1792575828674331184-2927656479` parte da un database SQLite vuoto,
espone `sensor_model=chemical_density_v2`, 57 ingressi e cap 360. I
checkpoint esterni sono stati conservati. La regressione dei feromoni verifica
separazione dei quattro canali, media spaziale normalizzata, raggio ereditato,
incremento tramite l'azione 20 e propagazione del raggio alle altre scansioni.

## Rotazione database e recupero della run — 21 ottobre 2026

La run `1792516222119803499-451470706` si è fermata in pausa a
21.741.452 tick oltre la soglia di 10 GB decimali. Il checkpoint automatico
falliva con `INI differs from active run`: l'INI sul disco era stato
modificato dopo l'avvio (49 ingressi, NewGen attivo, raggio prole 1) mentre
la run usava 57 ingressi, NewGen disattivato e raggio 2. Una copia dell'INI
successivo è conservata in
`data_out/config_backups/popx-before-storage-recovery-45fc2476139540de9eb5ddd42d5dbc18.ini`.
L'INI originale è stato recuperato dai metadati della run e verificato contro
la configurazione attiva; il monitor ha quindi salvato e verificato il
checkpoint automatico `1433928e34a6475199867bce2f94c3d6`, sostituito
lo storico e ripreso la simulazione nella nuova run
`1792538105513374180-2081800247`. Dopo la rotazione il database occupava
circa 15 MB e conteneva solo la nuova run con campioni da tick 0. Il
checkpoint conserva il tick sorgente e lo stato biologico; la nuova run
riparte con storico vuoto e configurazione bloccata.

La correzione in `PopXApp` conserva l'INI che ha costruito la run e lo usa
per i checkpoint, anche se il file sul disco cambia in seguito. La nuova
run caricata da checkpoint usa già un INI bloccato, quindi la rotazione
successiva non dipende dal file modificato. La compilazione aggiornata e la
regressione end-to-end con modifica dell'INI dopo Start verificano il caso.

## Stato corrente — accessibilità locale, dispersione e NewGen, 20 ottobre 2026

Per la prossima run evolutiva l'INI usa 57 ingressi, `newgen_enabled=false` e
`offspring_dispersal_radius=2`. Gli ingressi 49…56 valgono 1 se la destinazione
nelle otto direzioni è percorribile al passo corrente, 0 se bloccata da bordo,
occupante (Pop o Source), calcio saturo o salita oltre la soglia individuale.
Il sensore 32 indica se l'ultimo tentativo di movimento è fallito; torna a 0
dopo un successo. La decisione usa le normali connessioni del genoma: nessuna
azione o ricompensa è stata imposta. Le configurazioni storiche con 49 ingressi
mantengono il sensore 32 a 0 e i checkpoint precedenti restano caricabili.

La nascita cerca prima tutte le otto celle confinanti in ordine mescolato.
Solo se nessuna è disponibile e il raggio configurato è 2, cerca le sedici
posizioni della seconda corona. Il contatore `reproduction.dispersed_births`
registra le nascite in seconda corona. Costo energetico e trasferimento di un
ottavo delle riserve restano invariati; la seconda corona rappresenta una
dispersione breve della prole, non un movimento dell'adulto. Un raggio 1
preserva il precedente limite spaziale, salvo l'ordine di ricerca casuale.

Disabilitare NewGen rimuove gli abbattimenti periodici e lascia attive
selezione ecologica, mortalità e nascite naturali. Questa è una scelta di
esperimento, non una conclusione che la mobilità emergerà: il confronto
controllato precedente mostrava stasi anche senza NewGen. Valutare almeno
100.000 tick nuovi con successi/tentativi di movimento, ripartizione dei
fallimenti, nascite nella seconda corona, discendenza e permanenza al cap.

Validazione: build `popx`, 16 regressioni C++ standalone (inclusa una policy
che legge la percorribilità e sceglie una mossa riuscita), 2 test di
integrazione dashboard, integrazione checkpoint e catalogo metriche superati.
Il test dashboard verifica che la telemetria di apprendimento sia presente
senza presupporre che un Pop scelto casualmente abbia già aggiornato i pesi.
Il checkpoint storico del tick 16.757.754 è stato caricato con una porta
diagnostica separata e lasciato in pausa per cinque secondi senza errore;
la nuova run produttiva non è stata interrotta.

Nuova run `1792516222119803499-451470706`, avviata da zero in headless turbo
con dashboard, 57 ingressi, NewGen disattivato e raggio di dispersione 2.
A 100.000 tick: 138 vivi, 833.391 tentativi di movimento, 512.542 successi,
133.234 blocchi per occupazione e 108.470 per salita; 7.349 nascite naturali,
di cui 2.589 nella seconda corona, e 153.162 tentativi senza spazio. Fra
80.000 e 100.000 tick i successi sono 100.044 su 147.494 tentativi (67,8%),
ma nello stesso intervallo la popolazione cala da 300 a 138: la minore
occupazione è un confondente evidente. La run resta in corso. Questi dati
congiunti non isolano l'effetto dei sensori, di NewGen o della dispersione;
serve osservare stabilità demografica, distanze e discendenza su più cicli.
Il controllo successivo a 250.000 tick trova 58 vivi, 13.842 nascite naturali,
240 nascite forzate e 14.104 morti. Fra 150.000 e 200.000 tick la popolazione
scende da 34 a 23 vivi nei campioni disponibili e il contatore delle nascite
forzate passa da 40 a 120: si sono verificati recuperi da estinzione. NewGen
è disattivato, quindi i crolli non derivano dai suoi abbattimenti periodici.
I successi di movimento cumulativi salgono a 878.993 su 2.309.458 tentativi,
ma l'elevata mobilità iniziale e la dispersione non garantiscono ancora una
linea demograficamente stabile. Il test checkpoint è stato ripetuto dopo
l'ultima correzione della serializzazione usando una mappa temporanea 24×24.
Al controllo successivo intorno a 396.000 tick i vivi sono tornati a 300,
ma le nascite forzate cumulative sono 440: il recupero della numerosità non
dimostra stabilità autonoma delle linee. La run rimane attiva per osservazione
più lunga.

## Indagine controllata — fallimenti di movimento e NewGen, 20 ottobre 2026

La run `1792490142244376844-3311247595` è stata fermata a 16.809.931 tick.
Il checkpoint completo e verificato `data_out/checkpoints/4b3e4f8dc2d14aa592219ab488422938.popx`
proviene dal tick 16.757.754. Dopo la chiusura del simulatore e della dashboard,
lo storico principale SQLite (~8,3 GiB) è stato eliminato su richiesta; il
checkpoint esterno è conservato. I due database diagnostici temporanei sono
stati letti per questa analisi e poi eliminati. Nessuna run produttiva resta
in esecuzione.

`Pop::try_move` ora conta ogni tentativo in una delle categorie esclusive:
successo; destinazione occupata da Pop/Source; barriera di calcio saturo;
salita oltre la soglia individuale; bordo; destinazione uguale alla partenza;
altro. La verifica della salita precede quella dell'occupazione, quindi una
cella sia ripida sia occupata è classificata come pendenza. Questi contatori
sono osservativi, sommati per vivi e morti da `Stats`, descritti nella dashboard
e salvati opzionalmente nei checkpoint v4; le snapshot precedenti restano
caricabili. La regressione dedicata verifica che ogni tentativo abbia esattamente
un esito.

Confronto: `dashboard.diagnostics movement` ricarica due volte lo stesso
checkpoint, con identiche impostazioni eccetto `newgen_enabled` (true/false),
ed esegue 100.000 tick per ramo. Un seed **solo diagnostico** resetta i nuovi
`RandomUtility` e `rand`; la Source riprende il proprio RNG dal checkpoint.
I campioni dei due rami a 10.000, 20.000, 30.000 e 40.000 tick coincidono
esattamente per vivi, nascite, morti, tentativi, successi e stock mondiali.
Il primo NewGen interviene fra 40.000 e 50.000 tick; il secondo entro 100.000.
Questo è un controfattuale controllato da un checkpoint, non una stima della
variabilità fra molti mondi o una garanzia di replay deterministico produttivo.

| Esito nei 100.000 tick | NewGen attivo | NewGen disattivo |
|---|---:|---:|
| tentativi di movimento | 573.897 | 260.844 |
| successi | 48.441 (8,44%) | 47.711 (18,29%) |
| occupazione | 315.839 (55,03%) | 142.675 (54,70%) |
| pendenza | 197.736 (34,45%) | 60.447 (23,17%) |
| bordo / destinazione invariata | 4.447 / 7.434 | 2.553 / 7.458 |
| calcio / altro | 0 / 0 | 0 / 0 |
| vivi mai mossi alla fine | 276/300 | 283/298 |
| nascite naturali / forzate | 2.307 / 20 | 2.041 / 20 |
| morti, di cui abbattimenti NewGen | 2.082, di cui 43 | 1.818, di cui 0 |

Nella fase comune dei primi 40.000 tick si osservano 46.116 successi su
71.254 tentativi. Nei 60.000 tick successivi: NewGen attivo 2.325 successi
su 502.643 tentativi (0,46%); NewGen disattivo 1.595 su 189.590 (0,84%).
Quindi NewGen in questo fork induce molto più movimento *richiesto* ma quasi
nessuno spostamento aggiuntivo. Le due popolazioni restano al cap circa alla
fine; entrambi i rami rimangono largamente immobili. Il primo pilot senza
seed diagnostico divergeva prima del primo NewGen ed è stato escluso dalle
inferenze causali.

Priorità di intervento: (1) rendere riconoscibili agli agenti le direzioni
fisicamente impraticabili, verificando prima se i sensori di densità attuali
discriminano davvero la cella di destinazione e il dislivello; l'occupazione
e la pendenza spiegano quasi tutti i fallimenti. Evitare di premiare il
movimento in sé. (2) Valutare NewGen disattivato o una selezione meno ancorata
al numero assoluto di figli: `Scorer::selection` usa figli + 0,01×energia e
mantiene i superstiti nelle stesse celle; questo fork mostra che NewGen non
risolve la stasi. (3) Solo dopo, indagare dispersione locale della prole e
accesso alle risorse nelle nicchie affollate. Aumentare il cap o modificare
il calcio non è sostenuto da questi dati.

Validazione: build `popx`, 15 regressioni C++ standalone, integrazione
dashboard (2 test) e catalogo metriche. La fixture RNG produce due sequenze
identiche quando viene resettata e resta disabilitata in produzione. Protocollo
ripetibile: `python -m dashboard.diagnostics movement DB CHECKPOINT`, con
`DB` SQLite temporaneo e `CHECKPOINT` percorso del file sopra; il report è
salvato nella tabella `diagnostics`, non in JSON.

## Analisi run corrente — 13,8 milioni di tick, 20 ottobre 2026

Run `1792490142244376844-3311247595`, osservata in sola lettura mentre
continuava. INI della run: mappa 180×180, 2 Source, cap 300 Pop, NewGen ogni
50.000 tick con rapporto superstiti 0,9. A 13.817.200 tick: 300 vivi,
405.955 nascite naturali, 980 nascite forzate da recuperi dopo estinzione,
406.715 morti (242.195 per energia, 156.778 per temperatura, 7.121
abbattimenti NewGen, 621 per età). Esiste forte ricambio, ma anche collassi:
la numerosità non è una popolazione stabilmente vicina al cap. L'andamento
ricorrente 300→270 è imposto in parte da NewGen, non un equilibrio spontaneo.

Negli ultimi 20.000 tick misurati della fase precedente (13.550.000–
13.570.000), 4.052 movimenti riusciti su 510.850 tentativi (0,8%); negli
ultimi 50.000 prima di quel punto, 32.323/1.825.601 (1,8%). Un censimento
read-only dei 300 vivi a circa 13,64 milioni di tick ne trovava 246 senza
alcuno spostamento nella propria vita, pur con 185 che avevano richiesto
almeno un movimento. Occupavano x=42…95 e y=46…73, una piccola parte della
mappa. In un secondo censimento 102 dei 250 individui mai mossi non avevano
alcuna delle otto celle adiacenti libera; la mediana di celle adiacenti libere
era una. L'occupazione locale spiega una parte dei blocchi; pendenza e policy
che scelgono una direzione sfavorevole contribuiscono plausibilmente, senza
una ripartizione causale completa dei fallimenti. Il calcio saturo rilevato
dal campione era zero, quindi non spiega questa fase.

La pressione selettiva non è assente: su tutta la run 89,5% degli agent-tick
idonei alla riproduzione era bloccato dal cap e 95,4% dei tentativi processati
falliva per mancanza di spazio adiacente. Questo favorisce la ritenzione degli
insediamenti che già riproducono. NewGen ordina per numero di figli più
0,01×energia e lascia i superstiti nella stessa cella: è un meccanismo
compatibile con la persistenza di una nicchia immobile. La selezione può
restare forte pur premiando uno stile di vita sedentario; questa è
un'interpretazione dei meccanismi e dei dati, non una prova causale isolata.

Le risorse globali a 13,8 milioni non sono esaurite (glucosio ~2,77 milioni,
acqua ~6,83 milioni di unità nelle celle), ma il glucosio era ~4,98 milioni
all'inizio e l'accesso locale può essere scarso. Il clima ruota di 0,009
radianti ogni 1.000 tick: ciclo teorico ~698.000 tick. Fra 9 e 13,7 milioni,
l'autocorrelazione a lag 698.000 di reward e distanza termica media è circa
0,30 e 0,26; l'oscillazione climatica contribuisce ai pattern senza spiegarli
interamente. Il segnale medio di apprendimento vicino a zero riflette anche
la baseline adattiva: a 13,8 milioni media −0,0021, media assoluta 0,0045,
deviazione standard 0,0058. Non indica rete spenta, ma la plasticità media
proposta per connessione resta piccola (~9,2e−7) rispetto al decadimento
(~2,1e−6). Aspettare altri cicli da solo non garantisce una strategia mobile:
la run ha già attraversato circa venti cicli termici completi.

## Stato corrente — cap dei Pop configurabile, 20 ottobre 2026

`max_population_allowed` nell'INI determina il numero massimo di Pop vivi
contemporaneamente (default 240; valore iniziale `population` non superiore al
cap). La dashboard lo espone nella configurazione e riporta il valore effettivo
nella telemetria `reproduction.capacity`; le modifiche si applicano al prossimo
Start/Restart. Ammissione delle nascite, recupero da estinzione e caricamento
dei checkpoint leggono lo stesso parametro. I checkpoint v4 preesistenti senza
questa chiave usano il default 240; la firma storica v4 conserva il vecchio
valore per compatibilità. La run già avviata prima della build resta legata al
proprio binario e al suo cap finché non viene riavviata.
Validazione: build `popx`, 14 regressioni C++ standalone e integrazione
dashboard (2 test) PASS; lo schema e la validazione CLI espongono il default 240.

## Stato corrente — Source fisiche e fronti di glucosio, 20 ottobre 2026

Le Source sono ora occupanti reali della griglia, collidono con Pop, altre
Source, barriere di calcio e pendenze non scalabili. Due istanze per default
nascono in celle libere casuali; il cap dei Pop rimane indipendente. Il simbolo
SFML è un rombo nero con bordo chiaro, di diametro leggermente maggiore dei Pop.
Il movimento segue un heading continuo con curvatura casuale che cambia ogni
6–18 passi; quando il passo fallisce, prova direzioni alternative casuali
aggiornando anche la curvatura. L'intervallo di movimento passa da10 a40 tick.

L'emissione di glucosio è indipendente dal movimento: default ogni40 tick,
8 unità offerte a ciascuna delle otto celle adiacenti. Gli avanzi per capacità
piena si propagano in larghezza nelle celle vicine finché trovano capacità;
il deposito sotto la Source è escluso. Solo le unità effettivamente accettate
entrano nel ledger. Il limite teorico interno resta 2×8×8/40=3.2 unità/tick;
bordi e saturazione completa possono ridurlo. source_count,
source_move_interval_ticks, source_glucose_interval_ticks e
source_glucose_per_neighbor sono validati dall'INI. Il checkpoint
popx_state_v4 conserva posizioni, heading, curvatura, fasi dei due orologi
e stato RNG delle Source, oltre alle risorse e al ledger. Il modello v3 resta
incompatibile e richiede il binario precedente.

Validazione: build popx, 14 regressioni standalone C++, integrazione reale
checkpoint/caricamento/rotazione e audit conservazione2000 tick PASS.
La regressione Source verifica collisioni, movimento alternativo dopo blocco,
identità grafica, flood al secondo anello e bilancio; il checkpoint verifica
la continuazione delle fasi e dello stato RNG. Nella nuova run
1792486016214411264-1794519221, campione13200 tick:41424 unità di glucosio
accettate e residuo di tutti i sei materiali zero. Un checkpoint v4 della run
reale contiene due Source in posizioni distinte, senza sovrapposizione ai Pop.
La dashboard locale e il proxy LAN sono nuovamente attivi; lo storico della
run v3 è conservato, così come un checkpoint manuale v3 e il suo binario in
data_out/checkpoints/popx-model-v3-before-physical-source-backup. Le misure
della run precedente appartengono al modello non occupante descritto nella
sezione storica successiva.

Osservazione preliminare della nuova run: a20000 tick i vivi erano4 dopo
aver raggiunto il cap240; decessi cumulativi5406 per temperatura e963 per
energia, glucosio globale ancora2.60 milioni e apporto Source62472 unità.
A40000 tick i vivi erano1, con5471 decessi termici contro1027 energetici;
l'apporto Source era124816 unità e il glucosio globale2.66 milioni. La
finestra non sostiene una carenza globale di glucosio; un campo termico
iniziale diverso e l'adattamento delle linee sono confondenti. Il paragone
con la vecchia run non è un esperimento a condizioni iniziali identiche,
quindi non attribuire il crollo alla nuova traiettoria delle Source. A76000
tick i vivi erano2, l'apporto cumulativo238720 (3.14 unità/tick), il residuo
materiale ancora zero, i decessi termici5593 contro1138 energetici. Per
valutare causalmente il nuovo percorso andrà confrontato su uno stato iniziale
condiviso; questa run da fondatori nuovi non lo consente.

### Diagnosi del crollo e del recupero (lettura live fino a168000 tick)

La nuova run è tornata al cap240 verso110000 tick e vi rimaneva al taglio
168968. Ha accumulato260 nascite forzate:13 episodi di estinzione completa,
ciascuno seguito da20 nuovi fondatori del recupero endless. Dopo il recupero
intorno a104000 tick non risultano ulteriori fondatori forzati fino a168000;
la fotosintesi cumulativa sale da2561 a100000 tick a2.70 milioni a165000.
Anche la precedente run v3 era scesa fino a1 vivo verso110000 tick, dopo
160 fondatori forzati, prima di tornare al cap240 verso130000: non era una
popolazione continuamente stabile.

Nella nuova run fra10000 e20000 tick:4667 nascite naturali,4903 morti,
di cui4372 per temperatura;4279 di questi per freddo,93 per caldo. L'età
mediana dei morti termici è4 tick;3115 sono morti entro10 tick. I figli
nascono a25°C, ricevono1/8 dei lipidi/energia del genitore e, con pochi
lipidi, scambiano calore rapidamente con l'ambiente. Quindi il dato è
compatibile con un collo di bottiglia termico dei neonati, non con
l'impossibilità generale di muoversi: nello stesso intervallo i movimenti
riusciti aumentano di403825 su501438 tentativi (80.5%). Non c'erano
abbattimenti NewGen in quella finestra.

La nuova mappa casuale ha solo93/32400 celle con quota>=50 e temperatura
iniziale20–47°C; la vecchia ne aveva568. È una proxy di habitat che
consente fotosintesi senza caldo/freddo ambientale estremo, non una misura
diretta di sopravvivenza. Nella nuova run solo12/1626 nascite dei primi10000
tick erano a quota>=50, contro375/625 nella vecchia; solo4/1626 avevano
anche almeno5 cloroplasti, contro367/625 nella vecchia. Fra10000 e20000
le nascite a quota>=50 erano178/4667. La fotosintesi cumulativa a20000 tick
era2313 nella nuova run contro912398 nella vecchia, mentre il glucosio
globale della nuova mappa restava2.60 milioni e l'apporto delle Source era
62472 unità. Fra100000 e130000 tick, dopo il recupero,1311/2480 nascite
sono a quota>=50 e quasi tutti i figli hanno almeno5 cloroplasti: coincide
con il ritorno della fotosintesi e della popolazione alta.

La causa più sostenuta dai dati è quindi l'interazione fra configurazione
spaziale casuale di quota/temperatura, espansione iniziale in zone basse,
forte mortalità a freddo dei neonati e scarsa fotosintesi. La nuova
distribuzione locale del glucosio delle due Source potrebbe influire sulla
scelta degli habitat, ma non è isolata da questi dati: l'apporto totale è
vicino a3.2 unità/tick e una comparazione causale richiede lo stesso mondo
e la stessa popolazione iniziale. Il cap240 e i due occupanti Source non
spiegano il crollo: le nascite e i movimenti proseguivano durante la crisi.

### Rotazione termica: secondo collasso dopo un ciclo

La temperatura è ricalcolata ogni1000 tick; ad ogni refresh i centri RBF
ruotano di0.009 rad senza cambiare ampiezza o larghezza. Il periodo completo
è circa698132 tick; a90 celle dal centro il centro di una campana si sposta
circa0.81 celle per refresh. Quindi non è un salto spaziale grande a ogni
tick, ma il clima attraversa in modo periodico configurazioni ecologiche
molto diverse.

Usando le RBF termiche del checkpoint v4 a22591 tick, ruotandone i centri
secondo il codice e rivalutando le32400 celle con quota statica, le celle
con quota>=50 e temperatura ambientale20–47°C sono93 a fase0 (conferma
l'ambiente iniziale salvato),1417 a375000 tick e90 a700000 tick. Questa
misura è una proxy del possibile habitat fotosintetico, non una previsione
deterministica della sopravvivenza: risorse, corpi, movimento e geni contano.
Il minimo ricorre quasi esattamente dopo un giro.

La telemetria mostra240 vivi a660000 tick,50 a680000,54 a700000,1 a710000
e750000, quindi240 a770000 e ancora240 oltre un milione. Fra690000 e710000
si registrano4655 morti, di cui3808 per freddo e194 per caldo, età mediana
dei morti5 tick; gli abbattimenti NewGen sono6. La fotosintesi cumulativa
cresce di sole9158 reazioni fra690000 e710000, contro milioni nelle fasi
favorevoli. Le nascite forzate cumulative passano da260 a320 entro760000:
tre nuovi recuperi completi dopo l'estinzione. Il primo collasso era intorno
a10000–20000 tick; la distanza fra i due è compatibile con il periodo
climatico, pur senza attribuire a una sola variabile la causalità completa.

Interpretazione: il clima mobile crea selezione durante le fasi favorevoli,
ma i colli di bottiglia quasi totali e le estinzioni azzerano la continuità
genealogica; molta mortalità neonatale non equivale ad apprendimento
evolutivo. Ridurre la velocità da sola prolunga sia le fasi favorevoli sia
quelle inospitali; per una selezione continua serve soprattutto che
l'intersezione fra temperatura tollerabile, quota fotosintetica e accesso
alle risorse non collassi a quasi zero. Un confronto sperimentale dovrebbe
partire da uno stesso checkpoint, variare velocità e ampiezza del clima,
ripetere più run (RNG produttivo non centralizzato) e misurare estinzioni,
discendenza naturale e occupazione delle celle rifugio.

## Stato corrente — fronte mobile di glucosio, 20 ottobre 2026

Analisi rapida in sola lettura della run `1792468236105594011-4060155386` al
taglio12253400, con INI modificato dall’utente: rete5 hidden×8, mappa180×180,
`GlucoseRegenInterval=10000` nel binario precedente, ottimo termico36°C,
soglia fotosintesi5 e cap240. Gli ultimi100000 tick:3097 nascite naturali,
3117 morti,42 abbattimenti;858788 tentativi di movimento,394858 successi
(45.97%). Nell’ultimo milione:18831 nascite,19031 morti;22419388 tentativi,
975791 successi (4.35%);93.28% dei tick individuali di idoneità riproduttiva
bloccati dal cap. Tutti i240 vivi ispezionati non avevano percorso distanza,
235 non avevano tentato il movimento,240 erano fotosinteticamente capaci.
Fra i morti nell’ultimo milione,48.13% aveva percorso almeno una cella e76.81%
non aveva figli. Il contrasto vivi/morti è selezione del campione: osservazione
compatibile con una strategia stazionaria che sopravvive e si riproduce vicino
alla temperatura mite, non prova causale che muoversi peggiori la fitness.
Nella finestra100000 tick:2151 morti per energia e952 per temperatura su3145,
con percentuale di movimento riuscito più alta che nel milione intero; medie
lunghe nascondono periodi ecologici molto diversi. Plasticità proposta media
4.63e-6, decadimento medio1.43e-5 negli ultimi100000 campioni ogni200 tick;
rapporto0.324. La termica e la disponibilità locale restano fattori plausibili;
non si è cambiato il reward o imposto un bonus al movimento.

### Meccanica Source

`Source` implementa `IAgent` come agente ambientale senza metabolismo,
riproduzione o occupazione di una cella. Quattro istanze percorrono quattro
quadranti con scansione a serpentina, un passo ogni10 tick, e depositano una
unità di glucosio nelle otto celle adiacenti. Il deposito è limitato dalla
capacità della cella e nel ledger compare solo la quantità accettata. I Pop
possono occupare la posizione di una Source; la sua presenza non li blocca né
viene contata nel cap. L’overlay SFML la disegna come cerchio giallo.
La rigenerazione autonoma del glucosio nelle celle è rimossa; quella
dell’ossigeno rimane. La dotazione iniziale di glucosio RBF resta la stessa.
Su180×180, tetto di apporto4×8/10=3.2 unità/tick rispetto al valore atteso
precedente180²/10000=3.24; il flusso effettivo può essere inferiore per celle
piene o confini. Un quadrante90×90 viene percorso ogni81000 tick. La stessa
quantità media, distribuita in impulsi spaziali, introduce occasioni di
inseguire il fronte; la fotosintesi può comunque mantenere vantaggiosa
l’immobilità. È una modifica ecologica sperimentale, non una garanzia di moto.

Checkpoint modello `popx_state_v3`: salva posizione e fase delle quattro Source,
insieme ai cumulativi di apporto. La baseline delle statistiche sottrae i
cumulativi presenti all’avvio di una nuova run da checkpoint, mantenendo il
residuo di materia pari a zero. Il binario precedente è conservato in
`data_out/checkpoints/popx-model-v2-before-source-backup`, per poter leggere
l’ultimo checkpoint della run precedente; v3 rifiuta v2 esplicitamente.
La dashboard descrive il nuovo significato dell’apporto di glucosio.

## Stato corrente — staticità, cicli termici e cap240, 19 ottobre 2026

Analisi della run `1792425238763647807-3452666255`, taglio27312600 per
apprendimento e27330600 per periodicità/genealogia. Script ripetibili:
`analyze_learning_followup.py` e `analyze_periodic_run.py` in `scripts/`;
risultati testuali in `data_out/audit/learning-periodic-current.txt`,
`periodicity-current.txt` e `periodicity-historical.txt`. Letture vive su più
tick: non costituiscono una fotografia simultanea. Lo script periodico supporta
`--run`, `--cutoff`, `--db` per verifiche storiche senza API. Eventi ordinati
per tick prima di ricostruire genealogie, perché l’ordine di inserimento può
riflettere trasporto asincrono. La modalità storica conserva160 vivi ricostruiti.

### Risultati e interpretazione

Ultimo milione:2610 nascite naturali e morti,320 abbattimenti,1819 morti
per energia,450 per età,21 per temperatura, nessun ripopolamento forzato.
Cap pieno997010 tick su un milione;152207757/152649154 tick idonei bloccati
(99.71%). Solo2610 dei5298 tentativi effettivi riescono;2688 senza spazio.
Azioni idle48035675 su circa53.3 milioni (90%);1450423 tentativi di moto,
43288 successi (2.98%). Ultimi5 milioni:13138 nascite;99.70% idoneità bloccata.
I campioni decimati su multipli di10000 sovrarappresentano i cali NewGen:
la media156.8 vivi di quel sottocampione non è la media temporale esatta.

Campione vivo successivo:160/160 capaci di fotosintesi,160/160 con distanza
cumulativa nulla,155 senza tentativi;6.55 vicini occupati in media su8,
nessuna cella satura di calcio. Un campione poco precedente dava6.71 vicini.
Quindi affollamento locale e scelta/fallback idle prevalgono; una densità globale
bassa (160 su14400 celle) non rappresenta l’accessibilità dei vicini.
Su2672 vite terminate nell’ultimo milione al secondo taglio,91.58% senza
spostamenti,61.86% senza figli, età mediana18484.5 e media61183.5 tick.
Tutti i vivi discendono da `pop_48` ai tagli22.33,26.33 e27.33 milioni.
È fissazione della linea fondatrice, non identità dei genomi o prova di assenza
di evoluzione. Mortalità e discendenza diseguale mantengono selezione attiva.
La strategia sedentaria fotosintetica è compatibile con risorse accessibili
senza locomozione, che invece costa energia anche se fallisce.

Plasticità/decadimento medio3.08 nell’ultimo milione e2.18 negli ultimi5 milioni:
la precedente prevalenza del decadimento non spiega da sola la staticità.
Segnale medio−0.00000751 nell’ultimo milione, modulo individuale medio0.002868.
Il primo resta vicino a zero per baseline e compensazione fra segni. Ispezione
159 cervelli:62.45% pesi <=1.01/8192 (prima88.44%), zeri esatti0;96.30% delle
hidden con ingressi ancora entro0.001 da0.5,75.77% delle uscite con ingressi.
Pareggi esatti30.82%;146/159 ultima azione idle. Il fallback e le hidden deboli
restano contributi possibili; non tutti gli idle sono fallback. Nessuna modifica
alla policy o bonus al movimento introdotta in questo intervento.

Termica: rotazione deterministica0.009 radianti ogni1000 tick, periodo teorico
698131.7 tick (6981.3 secondi simulati). Su2500 campioni a passo2000 negli ultimi
5 milioni: autocorrelazione a698000 tick0.925 per reward,0.945 per componente
termica,0.948 per scarto termico; correlazione reward/componente termica0.989.
Periodogramma grossolano dominante intorno350000 tick, oltre alla fondamentale
intorno700000: armoniche compatibili con il campo a più campane e le trasformazioni
simmetriche dello scarto termico. Non è prova di un ritmo neurale appreso.
NewGen produce un secondo ritmo ogni50000 tick:160→144 al confine, ritorno160
entro il campione successivo200 tick dopo. Il sottocampionamento della dashboard
può modificare l’apparenza dei picchi; la periodicità termica è anche nei dati grezzi.
Acqua totale5.75M al primo taglio,−431538 in5M tick, ma+51416 nell’ultimo milione:
non dimostra un esaurimento imminente. Glucosio totale7.19M; i totali non provano
che le risorse siano raggiungibili da ogni individuo.

### Intervento autorizzato

Cap aumentato160→240 in `src/common.h`, mantenendo invariati fondatori80,
ambiente, costi, apprendimento e NewGen. Scelta sperimentale motivata dal blocco
riproduttivo99.7%; non garantisce maggiore movimento né risolve l’affollamento
locale. NewGen eliminerà24 individui al cap, sempre il10%, ogni50000 tick.
Nuova run da fondatori indipendenti: confronto descrittivo, non esperimento
causale appaiato; per attribuire effetti al cap servono repliche controllate.
Storico precedente conservato. Checkpoint prima dell’arresto
`2bf11c30d30f4322a0d810b18c4f046d`; binario cap160 conservato accanto ai checkpoint
come `popx-model-v2-cap160-backup`, perché il cap entra nella firma di compatibilità.
Limite dello storico10GB e rotazione automatica invariati.

Verifica finale:14 regressioni standalone C++ (inclusa Source) PASS.
Dopo la correzione della baseline degli apporti, rieseguite le regressioni di
statistiche/checkpoint/Source e l’integrazione reale checkpoint: PASS.
Audit di conservazione in `/tmp/popx-source-pilot-2.sqlite`:2000 tick,
183 vivi, residuo dei sei materiali tutto zero,6006 unità di glucosio accettate.
Primo tentativo di audit fallito al tick0 per l’ottimo termico37°C codificato
nella fixture mentre l’INI usa36°C; fixture corretta a leggere l’INI.
Prima integrazione checkpoint fallita per il cumulativo di apporto restaurato
sottratto senza baseline di nuova run; corretta e verificata a residuo zero.
Lo snapshot `c83c28afb7424692adceed2a125c1b57` conserva lo stato completo
prima dell’arresto. Storico SQLite precedente (~4.45GB) eliminato dopo la
chiusura dei processi; catalogo e file checkpoint preservati.
Nuova run `1792484602640242177-3973666402` da fondatori, mappa180, rete5×8,
GUI attiva, velocità2, dashboard8000 e proxy LAN8080. Al tick5200:240 vivi,
16034 unità di glucosio Source accettate, residuo di tutti i materiali zero,
receiver_errors0 e nessun messaggio perso. Limite DB10GB invariato.

Build Release esplicita. Pilot isolato2000 tick:240 vivi e conservazione PASS,
SQLite `/tmp/popx-cap240-pilot.sqlite`. La fixture riproduttiva è stata ampliata
a40×40: con240 agenti la vecchia disposizione20×20 collideva con la cella del
parent diagnostico. Nessuna dotazione o regola della fixture trasferita in produzione.

Verifica finale: tutte le13 regressioni standalone e integrazione checkpoint PASS.
Nuova run `1792440609541561133-2405984528`, tick20048, vivi240, cap240,
turbo headless, dashboard8000 e LAN8080 operative, errori ricevitore0.
Storico della run precedente presente; nessuna cancellazione effettuata.

## Stato corrente — correzioni di apprendimento e ammissione, 19 ottobre 2026

Interventi autorizzati dall’utente, con arresto, cancellazione dello storico e nuova
run da fondatori. Prima dell’arresto salvato il checkpoint completo
`63c77da24d9645debbf3dacbda156584` della run precedente; catalogo e file preservati.

- Ammissione FIFO con cursore persistente dopo l’ultimo ammesso, riferito all’ID
  del prossimo individuo; indice circolare di ripiego se quell’individuo muore.
  Cap, idoneità e budget restano invariati. Cursore conservato nei checkpoint.
  Ogni Pop espone tick idonei, tick bloccati dal cap, esclusioni dalla coda,
  ammissioni e attesa idonea corrente/massima, anche negli eventi di morte.
  L’attesa accumula solo tick idonei fuori coda fra ammissioni, include il cap
  e può attraversare periodi di non idoneità: non è durata cronologica continua.
- `learning_weight_decay` configurabile e validato [0, 0.01], default/INI0.00003.
  Eredità dei pesi appresi preservata; nessuna nuova connessione creata dal learning.
  Minimo asintotico del decadimento1/8192 invariato. La plasticità può attraversarlo
  o invertire il segno. Learning rate0.06 e baseline0.01 invariati.
- Trace decay0.97. Credito locale dell’uscita scelta = attività presinaptica ×
  uscita × (1−uscita), con iniezione1−trace_decay³ e clamp della traccia±0.25.
  La derivata sigmoid rimane sensibile intorno a0.5; attività presinaptica nulla
  e saturazione possono ancora ridurre il credito. Le hidden mantengono il
  prodotto centrato precedente: è un’approssimazione locale, non backpropagation
  né una stima non distorta del gradiente della policy argmax. Segnale nullo
  produce plasticità nulla; segni opposti producono cambiamenti opposti.
- Reward termico0.2 × (2 × 2^−((T−ottimo)/intervallo)² −1), stessa curva del
  rendimento respiratorio. Continuo oltre la vecchia soglia; pesi dei componenti
  e baseline invariati. Descrizioni dashboard aggiornate. Nessun premio al moto.
- Cap mantenuto160: il confronto160/240 resta un esperimento successivo alla
  verifica della nuova ammissione nella run naturale. Questo intervento non
  dimostra ancora un vantaggio adattivo né ottimalità dei parametri scelti.
- Checkpoint modello `popx_state_v2`, metadati run `learning_v9`. Salvataggi del
  vecchio modello rifiutati esplicitamente; i file conservati richiedono il
  precedente modello. Nuovi checkpoint preservano anche i contatori individuali.
  Il limite dello storico resta10GB con checkpoint e rotazione automatica.

Nuova run `1792425238763647807-3452666255`, avviata da80 fondatori, turbo headless.
Verifica al tick21904:160 vivi, storico con una sola run, receiver_errors0,
nessun messaggio perso/in coda. Dashboard8000 e proxy LAN8080 rispondono.
SQLite ricreato (circa7.8MB alla verifica); catalogo checkpoint conservato.

### Verifiche della modifica

Build Release target esplicito `popx`. Tutte le13 regressioni standalone passate
(non CTest); dopo l’aggiornamento delle asserzioni neurali rieseguite le ultime5.
Dashboard2, genealogia6, servizio checkpoint2 e integrazione checkpoint1: PASS.
La fixture dashboard ora sovrascrive le directory temporanee in fondo all’INI,
per isolare anche configurazioni contenenti percorsi assoluti da checkpoint.
Confronto deterministico sulla stessa rete senza reward,20000 aggiornamenti:
1e-4 → peso0.135427;3e-5 →0.548862;1e-5 →0.818752, da peso iniziale1.
Scelto il valore intermedio come primo esperimento conservativo. Tracce dopo30
aggiornamenti:0.9 →4.2391%;0.97 →40.1007%. Fixture di ammissione persistente:
159 candidati, FIFO64, servizio3,1000 cicli: tutti159 serviti (prima64).
La fixture isola l’ammissione: non simula successo riproduttivo o selezione.
Test coprono credito positivo/negativo vicino a0.5, neutralità a reward0,
uscite non scelte, termica oltre banda, eredità e reset. Vecchie asserzioni
sul decadimento e sul credito sono state aggiornate ai parametri configurati.
Pilot isolato `scripts/run_audit.sh /tmp/popx-learning-pilot-20261019.sqlite 2000`:
PASS conservazione,2000 tick,160 vivi, stop per budget diagnostico.
Integrazione checkpoint reale: PASS salvataggio/caricamento, configurazione
bloccata, rifiuto corruzione e rotazione automatica. Nessun replay globale
riproducibile promesso: RNG produttive restano non centralizzate.

## Valutazione corrente — apprendimento e ammissione riproduttiva, 19 ottobre 2026

Analisi in sola lettura della run `1792397944484202031-3575920632` durante
l'esecuzione: prima lettura al tick23481200, seconda al tick41941400. Nessuna
modifica al modello, INI o processo; nessuna pausa o ripartenza. Riferimento
Git `f6b6fbd042a3cc17eb8da59002ea15cab54e0bcd` con modifiche locali già presenti.
Protocollo ripetibile: `scripts/analyze_learning_followup.py`; risultati testuali
in `data_out/audit/learning-second-20261019.txt` e
`data_out/audit/learning-followup-latest-20261019.txt`. Campioni storici di
100 punti nell'ultimo milione e500 negli ultimi5 milioni, a passo10000 tick;
finestre brevi di100 campioni consecutivi per confronto. Le differenze dei
contatori usano invece gli estremi esatti delle finestre, senza sottocampionamento.

### Segnale medio e plasticità

Al taglio41941400, ultimi5 milioni: segnale medio−0,000445; modulo individuale
medio0,009080; deviazione standard tra individui media0,021041.
In una lettura quasi contemporanea, taglio41938200 sulla stessa durata:
quota media con |segnale|<1e-4 pari0,803%, quota positiva26,27%.
Media temporale del modulo della media della popolazione0,001489.
Ricompensa media−0,07055, baseline media−0,07012; la lieve differenza rispetto
alla media dei segnali deriva anche dal fatto che la baseline telemetrica è
quella già aggiornata, mentre il segnale usa quella precedente.

`Scorer::observe` usa s=r−b e b'=b+0,01(r−b), con clamp del segnale.
In regime stabile la baseline insegue la ricompensa e il segnale medio tende
naturalmente a zero. In assenza di clamp, su una vita individuale la somma dei
segnali è proporzionale alla variazione della baseline: non misura il beneficio
adattivo cumulativo. La media dei vivi include anche ricambio e selezione di
sopravvivenza. Non va forzata positiva per ottenere apprendimento.
Esempio puntuale al tick41938383:51 Pop con segnale positivo medio+0,01190,
109 negativo medio−0,003539; media complessiva+0,001380 e modulo medio0,006203.

Plasticità/decadimento, rapporto dei moduli medi:10,57% negli ultimi5 milioni,
14,00% nell'ultimo milione. Al precedente taglio23,48 milioni era circa7–8%.
Non è una diminuzione monotona della plasticità a zero, ma permane la prevalenza
del decadimento sulle modifiche proposte. La variabilità delle coorti e
l'eredità dei pesi impediscono di interpretare il confronto come effetto causale
della ricostruzione dell'ambiente.

Ispezione154 dei160 Pop, tick41941461…41942228 (6 morti prima dell'ispezione;
possibile selezione verso i sopravvissuti):7342 connessioni,88,44% al livello
<=1,01/8192,1,17% esattamente nulle, mediana1/8192. Fra i soli neuroni con
almeno una connessione entrante,98,32% delle hidden e97,90% delle uscite sono
entro0,001 da0,5. La neutralità non dipende soltanto dalle unità scollegate.
Margine mediano fra le due uscite maggiori1,45e-5; pareggio esatto nel2,6%
dei cervelli ispezionati. Una rete debole può quindi scegliere azioni ripetute
con argmax, senza ricadere sempre in DO_NOTHING.

Campione completo dei vivi al tick41941443: modulo medio del peso0,01774
sotto10000 tick d'età (n46),0,002215 fra10000 e50000 (n59),0,0001791
fra50000 e100000 (n25),0,0001177 oltre100000 (n30). È una fotografia tra
coorti, non una misura longitudinale del decadimento dello stesso individuo.
Il codice dà però un meccanismo coerente: decadimento per tick1e-4,
emivita dell'eccesso sul minimo circa6931 tick; traccia0,9, emivita6,58 tick.
Tracce formate dal prodotto di attività centrate a0,5 diventano piccole quando
le attività tendono a quel valore. Aumentare solo learning_rate moltiplicherebbe
anche i rari picchi, lasciando intatto il problema della traccia quasi nulla.

Ulteriore criticità del reward: nel campione al tick41938383 il53,1% dei Pop
ha il termine termico fermo a−0,2. Il reward termico lineare si appiattisce a
|T−37|>=10 °C: lì miglioramenti termici locali non cambiano quel componente,
sebbene possano ancora cambiare indirettamente energia e altri componenti.

### Ecologia, cap e nuova criticità della coda

Ultimi5 milioni al taglio41941400:55073 nascite naturali e55073 morti,
nessuna nascita forzata;44615 morti per energia,8488 per temperatura,
1598 abbattimenti e372 per età. Cap:96,45% dell'idoneità agente×tick bloccata;
88,08% dei461862 tentativi effettivi senza spazio. Successi di movimento
43121418/197199458=21,87%. Il69,55% delle richieste di movimento è MOVE_RIGHT;
la distanza percorsa non dimostra esplorazione utile (possibili cicli locali).
Non c'è evidenza di arresto del ricambio né motivo di equiparare segnale medio
nullo a pressione evolutiva nulla. Il cap impone un collo di bottiglia forte.

L'acqua totale, circa1,74 milioni ora, è molto inferiore ai7,72 milioni iniziali
ricostruiti, ma negli ultimi5 milioni varia di−42214, circa−2,4% della quantità
all'inizio della finestra. Si osserva un regime meno ricco rispetto alla prima
ripartenza, non un esaurimento rapido dimostrato. Il totale non misura la
raggiungibilità locale per i Pop. Il confronto con la run precedente conserva
il limite della migrazione parziale dell'ambiente.

`collect_reproducer_candidates()` scansiona sempre `pops_` dall'inizio;
con coda piena respinge gli ulteriori candidati. Il budget consuma pochi posti,
che il giro successivo può riassegnare ai primi candidati già serviti.
Fixture isolata `tools/popx_queue_admission_audit.cpp`, compilata contro gli
oggetti Release attuali:159 candidati persistenti, coda64, consumo3 per ciclo,
1000 cicli ->64 candidati distinti serviti,95 mai serviti. La fixture chiama
l'ammissione produttiva ma sostituisce il servizio con estrazione senza nascite:
dimostra la possibilità di starvation, non la sua quota nella run naturale.
Un modello di ammissione con cursore rotante serve tutti159 nelle stesse
condizioni. I contatori di scarto ripetuti non contano individui distinti.

### Interventi da sperimentare, non applicati

1. Ammissione riproduttiva equa: scansione con cursore persistente che riprenda
   dopo l'ultimo ammesso; mantenere FIFO, idoneità, costi e cap. Registrare attesa
   ed esclusioni per individuo. Conservare anche il cursore nei checkpoint.
   Priorità alta perché elimina una selezione dovuta all'ordine dei contenitori.
2. Rendere configurabile il decadimento e confrontare1e-4,3e-5,1e-5, lasciando
   invariata l'eredità richiesta. Emivite6931,23105,69314 tick. Successivamente
   valutare consolidamento di una componente ereditaria e decadimento della
   sola componente plastica; modifica più invasiva, da distinguere nel protocollo.
3. Confrontare trace_decay0,9 e0,97: emivite6,58 e22,76 tick; residuo dopo30 tick
   4,24% e40,10%. Poi, separatamente, valutare credito all'azione selezionata
   che non si annulli solo perché la sua uscita sigmoid è0,5. Non basta usare
   indiscriminatamente attivazioni grezze, che introdurrebbero deriva positiva.
4. Valutare un termine termico continuo, coerente con il rendimento fisiologico,
   e misurare separatamente i componenti del segnale. Mantenere la baseline
   adattiva; nessun bonus arbitrario a movimento o aggregazione.
5. Solo dopo la correzione dell'ammissione, confrontare cap160 e240. Un cap
   maggiore può spostare la limitazione verso risorse/spazio e ridurre tick/s;
   non garantisce di eliminare il blocco o accelerare l'evoluzione per ora reale.

Esperimenti separati dalla produzione, stessa distribuzione iniziale derivata
da checkpoint con compatibilità esplicita dei parametri, una modifica alla
volta, più repliche. Misurare sopravvivenza, discendenza a esposizione comparabile,
fallimenti di movimento, pesi per fascia d'età, contributi plasticità/decadimento,
attese riproduttive e costo CPU. Confronti iniziali della stessa rete con
plasticità attiva/disattiva distinguerebbero cambiamenti dei pesi da beneficio
adattivo; RNG non centralizzate impediscono di promettere traiettorie identiche.

## Stato corrente — checkpoint, migrazione e rotazione a 10 GB, 19 ottobre 2026

Implementati salvataggio e caricamento dalla dashboard, con nuova run senza
recupero dello storico. Produzione riavviata in turbo headless: run
`1792397944484202031-3575920632`, 160 Pop iniziali, configurazione bloccata,
`database_limit_gb = 10`. Dashboard localhost8000 e accesso LAN8080 attivi.
Nessun parametro di apprendimento modificato durante questa migrazione.

### Analisi precedente alle modifiche

Run originale `1792347273710687193-3678064490`, trovata già in pausa al
tick55457267. Ultimi100 campioni, tick55437400…55457200:

- Segnale medio +0,00006959; modulo medio0,009798; deviazione standard0,019421.
  La media prossima a zero è compatibile con la baseline adattiva e la
  compensazione dei segnali; non dimostra l'assenza di apprendimento.
- Plasticità proposta media1,6446e-8, decadimento2,2023e-7: rapporto7,47%.
  Il decadimento domina in questo intervallo. Non è un rapporto tra guadagno
  adattivo e perdita: le quantità sono moduli medi delle variazioni proposte.
- Ispezione dei160 Pop:7849 connessioni,87,62% con modulo <=1,01/8192;
  mediana esattamente1/8192. Zeri esatti medi per Pop circa0,054%.
  Fra le attivazioni ispezionate,99,64% delle hidden e99,26% delle uscite
  distano meno di0,001 da0,5; la mediana della distanza è zero. Sono incluse
  unità scollegate, e alcune attivazioni risalgono all'ultima fase think.
  Poiché le tracce usano il prodotto delle attività centrate a0,5,
  questa neutralità contribuisce meccanicamente a tracce e plasticità piccole.
  È un punto critico da sperimentare, non prova di inefficacia adattiva globale.
- Ultimo milione di tick:13079 nascite naturali e13079 morti, di cui318
  abbattimenti NewGen,9650 per energia,3078 per temperatura,33 per età.
  Il ricambio continua. Il cap blocca121321359/130290991=93,12%
  dell'idoneità agente×tick;193554/206633=93,67% dei tentativi effettivi
  fallisce per spazio locale. Successi di movimento5991714/28589599=20,96%.
  Cap e spazio sono vincoli distinti; questi aggregati non quantificano da soli
  la selezione tra ceppi o il beneficio causale del movimento.

### Prima migrazione, parziale e autorizzata

Il vecchio eseguibile esportava soltanto l'inspector individuale e l'ambiente
iniziale: non aveva un checkpoint completo. Ptrace e lettura di `/proc/PID/mem`
non erano consentiti dall'ambiente. L'utente ha autorizzato esplicitamente la
migrazione parziale e la ricostruzione dei dati mancanti prima della sostituzione.

Cattura coerente dei160 Pop allo stesso tick, in
`data_out/checkpoints/legacy-1792347273710687193-3678064490-55457267.sqlite`
(6086656 byte). Il convertitore `scripts/import_legacy_checkpoint.py` produce
`migration-portable-55457267.popx`: conserva genomi di nascita, pesi e attività
neurali, sensori, stato fisico esportabile, posizione, direzione, età,
discendenza cumulativa e baseline; conserva il terreno originale. Risorse per
cella, feromoni, campi termici, tracce di eleggibilità, credito respiratorio e
FIFO non esportabili vengono ricostruiti. La config originale è incorporata,
con limite10 GB richiesto e percorsi risolti. L'origine ricostruita resta nei
metadati anche dei checkpoint completi successivi.

### Checkpoint ordinari

`src/checkpoint.h` codifica lo stato in CBOR, con versione/modello, costanti,
controllo d'integrità e scrittura temporanea, fsync e rename. Il salvataggio
avviene sul thread della simulazione fra tick completi. Si conservano:

- tutte le risorse delle celle, compreso azoto; temperatura, elevazione,
  feromoni e fasi di rigenerazione; RBF e contatori ambientali;
- genoma originale, matrici neurali sparse con pesi float attuali, attivazioni,
  tracce di eleggibilità, geni silenti/scartati, baseline dello scorer;
- stato fisiologico e costi pendenti, credito respiratorio, posizione e
  direzione, età, fasi, sensori, caratteristiche individuali e RNG dei Pop;
- ordine dei Pop e dei bucket, FIFO riproduttiva, contatore NewGen,
  identificativi e configurazione INI incorporata.

La ripartenza crea un nuovo run_id e tick0, conserva l'età biologica e il tempo
ambientale. I bilanci sottraggono i flussi già presenti al caricamento. I contatori
individuali conservati sono stato di dimensione fissa; campioni/eventi delle run
precedenti non sono necessari. Le genealogie marcano l'ingresso da checkpoint,
considerano l'età iniziale e l'ingresso ritardato nella curva di sopravvivenza.
Gli antenati fuori dalla snapshot non costituiscono lacune telemetriche della
nuova run. RNG locali conservate; `rand` e generatori transitori delle nuove
nascite restano non centralizzati: non si promette replay futuro deterministico.

Dashboard: «Salva checkpoint», selezione e «Carica checkpoint». Caricamento
verificato prima di sostituire la run; INI sovrascritto e configurazione bloccata
via UI/API, Start/Restart ordinari bloccati per le run da checkpoint. Per
riavviare da CLI: `scripts/run_popx.sh --headless --checkpoint PATH`;
parte in pausa. Il caricamento manuale dalla dashboard riprende automaticamente.
Il caricamento manuale conserva gli storici di altre run, salvo raggiungimento
del cap; la nuova run inizia con il solo campione iniziale e i Pop importati.

### Rotazione automatica e limiti operativi

Soglia predefinita10 GB decimali, SQLite+WAL+SHM, verifica ogni0,5 s.
Al superamento: salva e verifica un checkpoint, sospende i tick, ricostruisce una
nuova run in pausa, attende la telemetria iniziale, chiude i lettori genealogici,
sostituisce il database con uno nuovo contenente solo la run ripristinata,
quindi riprende. Tombstone minime impediscono ai pacchetti tardivi di ricreare
la run rimossa. Il catalogo checkpoint è separato dallo storico e conserva un
marcatore dell'operazione per ritentare dopo interruzioni.

Se salvataggio/caricamento falliscono lo storico rimane; il simulatore viene
lasciato in pausa e l'errore è visibile. La rotazione viene ritentata dopo30 s.
Se il nuovo database iniziale supera già la soglia, rimane in pausa anziché
ripartire in un ciclo incontrollato. Conservati gli ultimi due checkpoint
automatici e tutti quelli manuali. I checkpoint esterni sono esclusi dal cap;
la soglia campionata non è una quota filesystem e può essere superata durante
il completamento dell'operazione. Nessuna compattazione del VHDX Windows.

La prima rotazione reale ha recuperato lo storico da circa10,7 GB, lasciando
circa10 MB subito dopo la ripresa; poi il database ricomincia a crescere.
Checkpoint completo iniziale automatico: `431cb01365b04d148246dbb42ef9e942`;
salvataggio manuale verificato sulla nuova run: `e408213059264191bc455eac351bff79`.
Dimensione del primo file completo4731322 byte.

### Validazione eseguita

- Build Release target esplicito `popx`;12 regressioni C++ preesistenti passate
  (`/tmp/popx-checkpoint-regressions.log`). Nuova regressione C++ checkpoint:
  confronto dei dati ripristinati in30 fasi successive, continuazione ambiente,
  round-trip su file e rifiuto della corruzione; passata separatamente e aggiunta
  a `scripts/run_regressions.sh`.
- `checkpoint_integration.py`: simulatore e backend reali su DB temporaneo;
  salvataggio/caricamento, file alterato rifiutato, config bloccata, rotazione
  causata da volume storico di prova, un'unica nuova run e recupero spazio.
- `checkpoint_service_regression.py`:2 casi di errore, nessuna cancellazione
  prima del caricamento verificato, pausa e marcatore persistente di recupero.
- Passate regressioni Python dashboard(2), storage(3), limite/fallback(3),
  evoluzione(6); JavaScript uscita, pagina genealogie e gestione spazio.
- Prova separata del checkpoint parziale con tutti160 Pop: confronto esatto
  dei campi esportabili e dei pesi, poi salvataggio completo e continuazione
  per almeno1600 tick; residui materiali ed energetici zero.
- Produzione: tick77000, residui materiali tutti zero ed energia0;
  genealogie589 ingressi,429 morti,160 vivi, nessun genitore mancante inatteso,
  duplicato o gap; integrità coerente. Config bloccata e LAN8080 HTTP200.

## Stato precedente — accesso LAN senza riavvio, 18 ottobre 2026

La produzione è nuovamente attiva per avvio dell'utente. Hotfix LAN applicata
senza interrompere backend o simulatore: `dashboard.lan_proxy` ascolta su
`0.0.0.0:8080` e inoltra HTTP/SSE al backend esistente `127.0.0.1:8000`.
Nessun accesso al database o modifica al trasporto UDP. Avvio esplicito:
`.venv/bin/python -m dashboard.lan_proxy --port 8080` (dopo il launcher).
Il launcher ordinario rimane locale; riavviare anche questo comando nelle run
future per riattivare l'accesso LAN. Il proxy termina quando cambia il PID del
backend o dopo tre controlli di salute falliti; arrestarlo lascia intatta la run.
Sono accettati Host IP privati/loopback e localhost; Origin deve corrispondere
all'indirizzo richiesto. Header di inoltro del client eliminati. Nessuna
autenticazione: i dispositivi ammessi alla porta hanno i controlli della dashboard.

Validazione: due regressioni Python con upstream simulato (inoltro percorso,
query, corpo e origine, rifiuto cross-origin/Host arbitrari, cambio PID).
Verificati realmente HTTP200 per overview, pagine evoluzione/storage, JS,
status e stream SSE via `172.18.0.2:8080`. Run invariata
`1792347273710687193-3678064490`, backend PID836599, tick avanzati fino a8321272.
Nessun comando mutante inviato alla produzione durante queste verifiche.

Accesso dal telefono non verificabile da questo ambiente: l'indirizzo osservato
è privato e può appartenere a una rete container dentro WSL. Windows deve poter
raggiungere la porta prima di inoltrarla alla LAN. Lo script
`scripts/enable_dashboard_lan.ps1 -TargetAddress <IPv4-raggiungibile-da-Windows>`
verifica tale raggiungibilità, aggiunge un portproxy TCP8080 e una regola firewall
per il solo profilo Private/LocalSubnet. Eseguire da PowerShell amministratore;
script preparato ma non eseguito qui, perché Windows non è accessibile.
In presenza di container serve anche la pubblicazione/inoltro della sua porta.
Riferimento: https://learn.microsoft.com/en-us/windows/wsl/networking

## Stato precedente — limite database, Esci e verifica genealogie, 18 ottobre 2026

Produzione ferma su richiesta dell'utente. Il database
`data_out/popx.sqlite` è stato eliminato integralmente dopo verifica dell'assenza
dei processi:17649504256 byte rimossi; nessun WAL/SHM presente al momento della
cancellazione. I test successivi usano database temporanei e non ricreano quello
di produzione. Nessuna nuova run produttiva avviata.

### Soglia di spazio

Nuovo parametro INI/schema/configurazione dashboard `database_limit_gb = 50`,
in GB decimali (1 GB =1000000000 byte), intervallo valido0,001…1000000.
Il backend misura ogni0,5 secondi la somma delle dimensioni dei file
SQLite+WAL+SHM, includendo tutte le run conservate e le pagine interne libere.
Al raggiungimento della soglia richiede Stop al simulatore, che registra
`storage_limit` come causa e salva il riepilogo finale. La dashboard mostra
un avviso esplicito e la barra rispetto al cap. Start/Restart/Resume via API
sono rifiutati se i file sono già al limite; Start/Restart leggono il nuovo
valore dalla config, mentre la run in corso usa la propria configurazione.
L'arresto viene ritentato in caso di mancata conferma.

È una soglia campionata con arresto ordinato: coda già prodotta, latenza del
comando e dati finali possono superare il valore. Non è una quota filesystem.
Il controllo richiede il backend attivo, anche con browser chiuso; il launcher
termina il simulatore se il backend termina. Errori di misura sono segnalati e
ritentati, senza arresti automatici non associati a un superamento misurato.
Snapshot esterni, database diagnostici e altri file non rientrano in questo cap.

Verifica WSL: kernel `6.6.87.2-microsoft-standard-WSL2`, progetto su
`/dev/sdd[/home/framorgi/dev/popx]`, ext4. Il filesystem risiede nel disco
virtuale WSL2, che può crescere nel VHDX Windows. La capacità/libero riportata
da Linux è virtuale e non certifica il libero sul volume Windows; interop
Windows e percorso VHDX non sono esposti in questo ambiente. Il meter ora
distingue questi valori esplicitamente. L'utente ha chiarito che è sufficiente
il cap dei dati del database: nessun monitor del volume Windows implementato.
Cancellare file Linux non garantisce il restringimento immediato del VHDX.
Riferimenti ufficiali:
[WSL disk space](https://learn.microsoft.com/en-us/windows/wsl/disk-space),
[compact vdisk](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/compact-vdisk).
Nessuna compattazione del VHDX o arresto dell'intera distribuzione effettuati.

### Esci e genealogie

Corretto il ciclo C++: dopo Esci interrompe l'elaborazione dei comandi e passa
alla chiusura, senza eseguire ulteriori tick. Il riepilogo e la telemetria finale
vengono scaricati prima dell'uscita. Il launcher chiude anche il backend.
La pagina, alla conferma di Esci, azzera run selezionata, grafici, agenti,
inspector, rete, eventi, diagnostici e configurazione mostrata; chiude SSE,
ferma polling e widget spazio, annulla richieste pendenti e ignora risposte
tardive. Lo storico SQLite resta salvato: pulizia dell'interfaccia e cancellazione
dei dati sono azioni distinte.

Verificato il grafo genealogico con la libreria Cytoscape effettiva in modalità
headless: archi genitore→figlio, coordinate valide, clic sui nodi, dettaglio del
ceppo, sovrapposizione della sopravvivenza e rimozione della selezione.
Corrette risposte tardive dopo “Tutti” e confronti provenienti dalla run
precedente; database vuoto mostra un messaggio esplicito. La chiusura del
backend segnala la cancellazione al lettore genealogico e alle query SQLite,
così un'analisi lunga non trattiene il processo durante Esci.

Validazione:12 regressioni C++ standalone passate;4 test del limite, incluso
simulatore reale con soglia1 MB e verifica del riepilogo `storage_limit`;
2 integrazioni dashboard reali;3 regressioni di rimozione;6 genealogiche;
test JS Esci con richieste/SSE tardivi, grafo con Cytoscape reale, meter,
inspector e rete neurale. Smoke launcher con run attiva e SSE aperto: processi
chiusi, causa finale `exit`, tick finale uguale al tick della conferma Esci.
Tutti i processi delle fixture vengono chiusi e la produzione resta ferma.

## Intervento v7 — genealogie, somiglianza e diagnosi dell'apprendimento, 18 ottobre 2026

Modello `lineages_genetic_sensing_v7`, sulla base e sul working tree v6 descritti
sotto. Eredità dei pesi appresi, ricompense, decadimento, mutazioni e fisiologia
conservati. Il sensore GENETIC_SIM_FWD ora influenza realmente le decisioni:
questo intervento cambia la dinamica e richiede una nuova run per il confronto.

### Spazio e rimozione delle run — aggiornamento dashboard

Pagina `/static/storage.html` e misuratore condiviso in panoramica e genealogie,
aggiornato ogni15 secondi. Mostra capacità/spazio disponibile del filesystem,
quota occupata o riservata, dimensioni database+WAL+SHM e pagine interamente
riutilizzabili. Le pagine occupate includono indici e spazio parzialmente libero.
La quota disco comprende gli altri programmi; i file esterni degli snapshot
non sono compresi nella dimensione dei file SQLite. Lettura tramite stat,
disk_usage e PRAGMA, senza scansioni periodiche dei payload.

Rimozione selettiva tramite conferma della run: solo run concluse, sempre
protetta quella collegata al simulatore. `DELETE /api/runs/{run}` richiede
`confirm_run`, risponde202 e rende disponibile l'avanzamento; si rimuovono
campioni, eventi, ambiente, diagnostici, lacune e riferimenti agli snapshot.
Gli snapshot vengono cancellati solo se il percorso corrisponde esattamente
al file generato nella directory della run e non attraversa symlink; file
non riconosciuti restano sul disco e vengono conteggiati nella segnalazione.
Directory vuote e registri tecnici di deduplicazione rimangono.

Unico writer mantenuto: operazioni in blocchi di400 righe, commit e yield fra
blocchi; nessun worker aggiuntivo scrive nel database. Intenzione e avanzamento
persistono in `removed_runs`; un'interruzione riprende al riavvio. Il tombstone
impedisce a telemetria tardiva di ricreare la run. Un'analisi genealogica ancora
in corso blocca la rimozione; le cache completate vengono invalidate. Le run
in rimozione spariscono dagli elenchi e le API storiche restituiscono410.

La cancellazione libera pagine per nuove scritture; il file fisico SQLite può
restare della stessa dimensione. Nessun VACUUM automatico durante la run.
Per ridurre il file fisico serve una compattazione pianificata, con spazio
temporaneo e sospensione delle scritture. Riferimento:
[documentazione SQLite VACUUM](https://www.sqlite.org/lang_vacuum.html).
Il misuratore rende visibile la distinzione tra spazio riutilizzabile interno
e spazio disponibile nel filesystem.

Priorità proposte per la riduzione dei dati (non ancora applicate):
compressione lossless degli eventi (campione precedente43,85% dei byte
originali), codifica compatta e deduplicazione dei genomi identici preservando
ordine, endpoint uint32, encoding e pesi, aggregati dei campioni vecchi con
grezzo archiviato dove necessario, archivi separati per run per poter restituire
spazio eliminando un archivio completo. Nessuna deduplicazione basata sul solo
colore o hash senza verifica del contenuto. Sottocampionare i campioni incide su
circa17% del payload osservato, mentre la compressione degli eventi agisce
sull'83%; risparmi reali complessivi da verificare includendo indici e costi CPU.

Verifiche:3 test di rimozione su database temporanei (protezione, isolamento,
conferma, origine HTTP, percorsi snapshot, eventi tardivi, ripresa e conflitto
con lettore genealogico); test JS del meter, conferma annullata, selezione e
avanzamento;5 regressioni genealogiche e2 integrazioni reali C++/UDP/HTTP passate.
Nessun dato storico reale cancellato durante le verifiche. Riavvio con nuova
run autorizzato esplicitamente dall'utente per attivare il backend aggiornato.

Attivazione verificata: run precedente `1792328116532112618-1764166378`
conclusa al tick485153, riepilogo salvato e coda telemetrica vuota. Nuova run
`1792328943496482749-875090835`, turbo headless come al momento dello stop.
Al tick36168: zero errori e messaggi persi; API spazio e tre asset della pagina
rispondono correttamente. Conservate18 run, zero richieste di rimozione reali.
File SQLite e scritture circa17,62 GB, spazio disponibile circa808,57 GB.


### Genealogie e rappresentazione genetica

Nuova pagina `/static/evolution.html`: composizione dei ceppi e delle famiglie,
diversità effettiva, ricambio delle quote, esposizione idonea bloccata dal cap,
dispersione della discendenza, età delle vite concluse e dei vivi, sopravvivenza
Kaplan–Meier con censura a destra. Tabella ricercabile, ordinabile e paginata di
ceppi presenti/estinti, albero esplorabile di antenati e rami figli, individui e
confronto di due genomi alla nascita con tratti PHY separati. Selezionare un ceppo
mostra anche la sua traiettoria temporale. Ogni grafico spiega calcolo,
significato e interpretazione; le curve temporali conservano al massimo circa
201 punti, la curva di sopravvivenza circa300 punti. Intervalli diversi producono
valori di ricambio diversi; la curva di sopravvivenza combina coorti e ambienti.

Il lettore usa solo SQLite in lettura, un worker, una vista in cache e un limite
di un milione di individui per vista. La prima ricostruzione di una run lunga
richiede tempo e RAM; l'aggiornamento è manuale e incrementale. Il limite produce
un messaggio esplicito. Per storici oltre questo limite servirà un indice
genealogico persistente, mantenuto dall'unico scrittore, oppure un'analisi per
finestre con stato genealogico iniziale. Eventi e campione sono confrontati per
vivi/nascite/morti, genitori mancanti, duplicati e lacune. Tutte le query di un
aggiornamento usano una transazione di lettura WAL coerente: una prova con
scritture concorrenti copre il salto di eventi scoperto durante la verifica.

Definizione operativa: ogni fondatore/ripopolamento avvia una famiglia. Un figlio
naturale mantiene il ceppo del genitore se la somiglianza rispetto al fondatore
di quel ceppo raggiunge la soglia (default0,90), altrimenti apre un ramo. I ceppi
dipendono dalla soglia e dalla genealogia; famiglie distinte possono essere
geneticamente simili. L'abbattimento NewGen resta una causa distinta; le nascite
forzate aprono famiglie e non incrementano la discendenza naturale del genitore.

Somiglianza `endpoint_weight_v1`, condivisa da C++ e Python: multinsieme di geni
ordinati per tipo/indice sorgente, tipo/indice destinazione e peso. Geni con gli
stessi estremi si abbinano per ordine dei pesi; contributo
`1 − abs(w1−w2)/65535`, spaiati0, normalizzazione per la lunghezza maggiore.
Due genomi vuoti valgono1. Include duplicati, geni silenti, pesi zero e tutti i
32 bit degli indici. È una misura del genotipo; PHY e stato neurale acquisito
dell'adulto restano osservabili separatamente. Il sensore5 legge il vicino vivo
nella cella adiacente lungo il segno dell'ultima direzione; vuoto/bordo vale0.
Nessuna scrittura condivisa nelle fasi sense/think.

Colore `projection_v1`: proiezione dell'intero multinsieme, con contributi degli
estremi e dei pesi, normalizzata con sqrt(numero geni), limitata in RGB64…224.
Invariante all'ordine e tendenzialmente stabile a piccole mutazioni. Collisioni
cromatiche possibili: il confronto numerico resta la misura da usare. Firma
FNV1a64 canonica con collisioni teoricamente possibili. Inspector e SFML usano lo
stesso colore; snapshot v6 conservano il vecchio colore. Vettore condiviso C++/
Python: firma `f390b98f0fc55cfc`, RGB `[106,130,89]`.

### Diagnosi della run v6 e nuove misure

Run osservata `1792312991763973395-2439725205`. Al taglio completo7514200:
147272 individui,147112 vite concluse,160 vivi, contatori concordi e nessuna
lacuna. Una famiglia fondatrice superstite,3 ceppi presenti alla soglia0,90 su
13499 complessivi. Figli naturali delle vite concluse: media0,999, mediana0,
deviazione standard4,037, massimo124. Età mediana627 tick; profondità344.
Idoneità bloccata al cap79,27%. Il ricambio delle quote nei quattro intervalli
finali vale circa0,397/0,600/0,469/0,037. La discendenza è diseguale e i rami
continuano a sostituirsi anche dentro una famiglia; questi risultati non
supportano l'ipotesi di molte linee persistenti con discendenza simile. Deriva,
selezione ecologica e NewGen contribuiscono: i dati descrittivi non ne isolano
le cause. Una media dei figli prossima a1 è compatibile col vincolo demografico.

Ricalcolo completo con snapshot coerente al tick19315800:425037 individui,
424877 vite concluse,160 vivi, contatori perfettamente concordi, nessun genitore
mancante o lacuna. Dieci ceppi presenti su40075 alla soglia0,90, una famiglia
superstite. Figli delle vite concluse: media0,9998, deviazione standard4,9765,
mediana0, p90=1, massimo156; età mediana637 tick, profondità1079. Blocco al cap
64,86% delle esposizioni idonee. Ricambio finale0,894/0,769/0,256 su intervalli
di circa96600 tick (ultimo89200). La ricostruzione completa conferma forte
disuguaglianza della discendenza e ricambio dei rami; l'effetto causale della
selezione resta da separare dalla deriva e dagli abbattimenti.

Snapshot individuali ai tick19280328/19282342/19284354 (160 vivi): segnale medio
0,000840/0,002224/0,000644; modulo medio0,005763/0,007019/0,004842;
deviazione standard0,01898/0,03036/0,01298. Solo1,875%/3,125%/1,875% dei vivi
aveva |segnale|<0,0001. La media vicina a0 è quindi in parte compensazione;
la baseline è progettata per centrare l'esperienza recente. Il segno medio
non basta per diagnosticare efficacia dell'apprendimento o pressione evolutiva.

Plasticità proposta inferita per individuo da `0,06 × |signal| × mean_abs_trace`,
poi mediata sui vivi:5,77e-8/1,15e-8/1,27e-8 per connessione/aggiornamento.
Variazione effettiva media dei pesi:3,51e-7/3,73e-7/4,42e-7. La plasticità è
piccola anche in modulo; nel codice il decadimento riduce i pesi verso1/8192,
le sigmoid possono avvicinarsi a0,5 e le tracce moltiplicano gli scostamenti da0,5.
Questo è un meccanismo plausibile di indebolimento del credito, non una prova
che l'apprendimento sia riuscito o inutile. Le tracce campionate nella storia
oscillano: non c'è un azzeramento monotono universale. Il cumulativo della
plasticità cresce ancora (circa6182 al tick19200000).

Aggiunti `learning.mean_abs_signal`, `signal_std`, `positive_signal_fraction`,
`near_zero_signal_fraction` (soglia1e-4), `mean_abs_plasticity`, `mean_abs_decay`.
Plasticità e decadimento sono proposte prima dei limiti; `mean_abs_delta` misura
la modifica effettiva. Curve separate e descrizioni complete nel catalogo;
valori piccoli mostrati in notazione scientifica anziché arrotondati a0.
Nessuna modifica opportunistica ai coefficienti di apprendimento.

### Memoria e conservazione dello storico

Misura durante la run: database17522995200 byte (17,52 GB,16,32 GiB), circa
807 GB liberi. Sedici run conservate. La run corrente contiene circa2,634 GB
di payload eventi e0,557 GB di campioni: gli eventi sono circa83% dei due
payload. Si aggiungono indici, deduplicazione messaggi, ambienti e WAL. La
crescita continua senza una retention automatica. I processi ordinari osservati
usavano circa192 MiB (simulatore) e72 MiB (dashboard); la lettura genealogica
può richiedere centinaia di MiB o più e ha il limite esplicito sopra descritto.
Il lettore completo di425037 individui e40075 genomi di riferimento ha raggiunto
circa1,3 GiB RSS; memoria disponibile dell'host circa10,7 GiB in quel momento.

Finestra reale20,18 secondi con GUI a2×: allocazione circa1,62 GiB/giorno se
quel ritmo persistesse. È una misura breve sensibile ai blocchi SQLite e ai
riusi, non una previsione garantita. Il payload accumulato nei circa4 ore
precedenti, in gran parte turbo, corrisponde a circa19 GB/giorno senza indici.
Il turbo cambia molto l'orizzonte: con i ritmi misurati il rischio riguarda
settimane/mesi più che pochi giorni; altre scritture sul disco riducono il margine.
Script ripetibile: `scripts/analyze_learning_storage.py --interval 10`, sola
lettura e output testuale, incluse distribuzioni individuali e spazio libero.

Piano di retention, senza cancellazioni applicate allo storico:

1. Conservare integralmente nascite, genomi, parentela, morti, cause, contatori
   finali, configurazione, provenienza e lacune. Compressione senza perdita dei
   payload e archivi SQLite per run/periodo con lettura trasparente: sui1000
   eventi recenti, zlib6 per record produce il43,85% dei byte originali. È una
   misura del payload campionato, non del risparmio dell'intero database.
2. Conservare campioni200 tick recenti; per dati più vecchi creare aggregati a
   più risoluzioni (esempio24 ore/7 giorni/oltre7 giorni), con count/min/max/
   somma/somma dei quadrati, primo/ultimo tick e contatori cumulativi ai bordi.
   Età della retention riferita al tempo reale di acquisizione, da registrare;
   la risoluzione analitica resta espressa in tick. L'assegnazione temporale di
   singoli picchi e le correlazioni nuove richiedono il grezzo archiviato.
3. Verificare conteggi, integrità e lettura dell'archivio prima della rotazione;
   lasciare il solo writer responsabile di migrazioni e compattazioni. DELETE
   da solo libera pagine riutilizzabili, non riduce il file sul filesystem.
   Compattazioni e backup richiedono spazio temporaneo e vanno pianificati.
4. Per giorni/settimane di analisi genealogica, mantenere un indice persistente
   compatto (individui, genomi deduplicati, riferimenti agli archivi) e query per
   finestre. Un tetto di memoria del lettore e un sottocampionamento dei grafici
   risolvono problemi diversi dalla conservazione degli eventi.

Validazione v7:12 regressioni C++ standalone passate,5 test genealogici Python
(incluso writer concorrente),2 test d'integrazione dashboard, regressioni JS
inspector/rete e nuova pagina evolutiva. Pilot2000 tick in
`/tmp/popx_v7_pilot.sqlite`: bilanci passati,160 vivi, `short_tick_budget`.
I test deterministici verificano meccaniche e contabilità, non adattamento di
lungo periodo. Nessuna esportazione statistica JSON aggiunta.

Riavvio finale autorizzato: la run v6 è stata fermata al tick19367821, con
riepilogo finale salvato, coda telemetrica svuotata e zero messaggi persi; chiusi
simulatore, dashboard, launcher e lettore ausiliario precedenti. Nuova run
`1792328116532112618-1764166378`, v7, GUI a2× e dashboard sulla porta8000;
storico conservato (17 run). Verifica reale al tick3428: esecuzione attiva,
zero errori/perdite, nuove metriche presenti, pagina evolutiva HTTP200 e
conteggi genealogici concordi (306 individui,146 morti,160 vivi al taglio3400).
Smoke launcher con SSE aperto passato; nessun vecchio processo residuo.

## Intervento v6 — turbo, ricambio e apprendimento temporale, 18 ottobre 2026

Modello `temporal_learning_local_mutation_v6`, base `f6b6fbd042a3cc17eb8da59002ea15cab54e0bcd`
con working tree preesistente conservato. Implementati i quattro interventi
approvati dall'utente; run `1792276517115838265-2272337452` fermata al tick457089
e processi chiusi ordinatamente prima delle modifiche. Le verifiche successive
usano processi e database isolati; la produzione resta ferma come richiesto.

### Velocità e riproduzione

`--speed 0` abilita turbo headless, senza attese dello scheduler tra tick:
tempo biologico sempre10 ms/tick. CLI/launcher e comando dashboard accettano0/1/2.
La dashboard alterna1×,2×,turbo; il turbo chiude la GUI, mentre un'apertura GUI
riuscita torna a2×. Comandi controllati ogni batch di massimo8 tick. Il turbo
attende10 ms quando la coda telemetrica raggiunge128 messaggi; nessuna riduzione
del campionamento200 tick o degli eventi per nascondere carico/perdite.

Cap aumentato da120 a160. La raccolta dei candidati conta separatamente
`eligible_agent_ticks`, `cap_blocked_agent_ticks` e `cap_blocked_ticks`:
sono esposizioni ripetute, non figli perduti. Al cap non si tentano nuovi
accodamenti; i candidati già nella FIFO restano in attesa. Dopo i controlli
di idoneità, un tentativo effettivo è classificato come nascita o assenza di
spazio adiacente: `attempts = births + no_space`. Budget3 tentativi/tick,
coda64, costi e dotazioni riproduttive conservati. Valutazione NewGen ora in
`Scorer::selection`, formula invariata figli +0,01×energia. I fondatori non
incrementano `reproduction.births`, che coincide con `counts.natural_births`.

### Mutazione e scorer

Mutazione completa del gene ancora1%, inclusi endpoint e peso; PHY ancora1%.
Se un gene non è stato sostituito, probabilità configurabile5% di ritoccare
solo il suo peso, di1…256 quanti con segno casuale (256/8192 =0,03125), con
limiti int16. Anche i geni silenti restano presenti e possono mutare; nessun
modulo sugli indici. I costruttori e le vecchie chiamate standalone a `mutated`
mantengono la semantica precedente; la riproduzione produttiva passa esplicitamente
`weight_mutation_rate` e `weight_mutation_step` dall'INI.

`Scorer` è individuale, senza accesso al mondo. Centralizza ricompensa e
componenti, baseline recente e selezione. Coefficienti ecologici effettivi
espliciti: età0,10; energia0,30; variazione energetica0,35; discendenza0,05;
temperatura0,20. Rimossi i vecchi coefficienti nominali di `common.h`, che
erano limitati implicitamente nel calcolo. Primo segnale neutro; poi:

`signal = clamp(reward − baseline_precedente, −1, 1)`

`baseline += learning_baseline_rate × (reward − baseline)`

Baseline aggiornata ogni tick, default0,01. È una media della ricompensa, non
una funzione di valore appresa o una stima causale del contributo dell'azione.
Nessun premio specifico per movimento, aggregazione o esplorazione.

Il cervello registra le tracce solo in `act`, dopo la guardia di fase. Nuovo
credito alle sinapsi dell'uscita scelta e alle coattivazioni hidden; le altre
uscite conservano soltanto eventuali tracce precedenti. Per ogni azione:
`trace += (1 − lambda³) × (pre−0,5) × (post−0,5)`, limitata a±0,25.
Ogni aggiornamento applica `learning_rate × signal × trace`, il decadimento
precedente e i limiti±4; poi `trace *= lambda`. Default rate0,06, lambda0,9
(emivita circa6,6 tick). La normalizzazione considera un'azione ogni3 fasi.
Il primo aggiornamento dei pesi segue la prima azione, non il primo think.
Tracce, baseline e contatori sono azzerati alla nascita/ricablaggio appropriato;
ereditarietà dei pesi appresi ancora true. La traccia hidden è una regola locale
approssimata: nessuna garanzia di miglioramento comportamentale.

### Telemetria e validazione

Grafici dashboard per ricompensa, baseline, segnale e variazione assoluta dei
pesi; grafici separati per risultati riproduttivi e attese al cap. Le componenti
della ricompensa, tracce, moduli e frazione di zeri sono disponibili nel catalogo
misure e nell'inspector grezzo. Medie istantanee dei vivi in `learning.*`;
numero aggiornamenti e cambiamenti cumulativi anche dei morti in `totals.*`
e negli eventi individuali. Un cambiamento dei pesi non dimostra apprendimento
utile; un aumento delle nascite non dimostra maggiore adattamento.

Parametri INI validati e disponibili nella configurazione dashboard:
`weight_mutation_rate`, `weight_mutation_step`, `learning_rate`,
`learning_trace_decay`, `learning_baseline_rate`. Nuovi valori applicati a
Start/Restart. Intervallo termico1000 e correzione RBF della v5 conservati.

Validazione eseguita:11 regressioni standalone (le10 esistenti e la nuova
`learning_evolution`),2 test di integrazione dashboard, smoke del launcher
con avvio turbo e SSE aperto, regressioni JS inspector/rete. La nuova fixture
verifica baseline neutra a ricompensa costante, credito ritardato dopo azione,
assenza di nuovo credito all'uscita non scelta, reset/non ereditarietà delle
tracce, mutazione locale senza alterare endpoint uint32, contabilità cap/spazio/
nascite (escludendo i fondatori) e permanenza dei contatori di apprendimento dopo
la morte. Build esplicita e tutte le11 regressioni ripetute dopo l'ultima correzione
dei contatori. Eseguibile SHA256
`eb567002fbfe84d9cfd2335ed730607c8b87f78eb8e8bb946035eb73a52e8921`.
Pilot finale2000 tick in `data_out/audit/v6_final_pilot.sqlite`: conservazione passata,
160 vivi alla fine, stop `short_tick_budget`. Non è una prova di efficacia
evolutiva o di apprendimento a lungo termine.

Benchmark finito sulla stessa popolazione in evoluzione, in sequenza2×/turbo/2×,
finestre di circa1,5 secondi:198,0 /1990,0 /199,1 tick/s; circa10× rispetto a2×.
Nessuna perdita di messaggi o lacuna nella storia; coda vuota a ogni pausa.
Run diagnostica `1792309137393682685-778329342`, conclusa al tick3833:
80 fondatori,190 nascite naturali,110 morti,160 vivi;224 tentativi riproduttivi,
34 fallimenti per spazio. Residui dei bilanci delle risorse e dell'energia pari
a zero; contatori di apprendimento presenti e coorte istantanea pari ai160 vivi.
Storia/provenienza e tempi in `data_out/audit/v6_headless_final/`
(`history.sqlite`, `timings.csv`); protocollo `scripts/benchmark_headless.py`.
Campione breve, popolazione/ecologia cambiano tra finestre: la velocità non è
una garanzia per una run lunga o con altra popolazione. Tutti i processi di
verifica vengono terminati; nessun riavvio automatico della produzione.

Conservati gli artefatti preliminari `v6_pilot.sqlite` (109 vivi al tick2000)
e `v6_headless_benchmark_2/` (197,5 /1917,7 /199,4 tick/s). In quella versione
il nuovo contatore `reproduction.births` includeva erroneamente gli80 fondatori;
`counts.natural_births` era corretto. Corretto l'incremento nello spawn iniziale,
aggiunte regressione e verifica di uguaglianza nel benchmark; per la contabilità
riproduttiva v6 usare gli artefatti finali sopra.

### Analisi del margine turbo sulla run viva

Osservazione in sola lettura della run `1792312991763973395-2439725205`, senza
pausa né riavvio: tra i tick1260953 e1267713, speed0 headless,146→152 vivi,
6760 tick in circa6 s (1126 tick/s). Il costo recente ricostruito dai contatori
cumulativi è873 µs/tick: i tick occupano circa98,3% del tempo di parete della
finestra. CPU del processo circa1,62 s per secondo reale; coda telemetrica0→1,
messaggi persi0. Il dato dipende da popolazione, geni, ambiente e carico esterno;
il benchmark breve precedente raggiungeva1990 tick/s. La run era ancora attiva
alla fine dell'osservazione.

Conclusione limitata: ridurre batch/comandi/attese del turbo difficilmente
incide molto su questa run; serve diminuire il costo effettivo del tick. Candidati
dal codice, **non profili misurati**: ogni tick aggiorna14.400 celle, ognuna con
quattro feromoni in `std::map`, anche quando i valori sono zero; i sensori copiano
mappe dei feromoni e `Brain::get_connected_sensors()` ricostruisce un vettore e
scorre le matrici ad ogni sense; `Pop::learn` costruisce stringhe di debug a ogni
aggiornamento anche col logger impostato a Error. Queste allocazioni e traversate
sono priorità ragionevoli per prove isolate con verifica di equivalenza delle
meccaniche e benchmark controllato. Aumentare `num_threads` alla cieca non elimina
le fasi sequenziali di azione, fisiologia e mondo e può aggiungere overhead.

### Hotfix delle spiegazioni della dashboard sulla stessa run

Le234 definizioni in `dashboard/metrics.py` specificano calcolo, significato e
interpretazione, con esempi per segno, intervalli, medie, contatori e residui.
La pagina presenta tre paragrafi distinti nei riquadri «Significato delle misure»
e nell'esploratore. Aggiornate anche le indicazioni di lettura di eventi,
diagnostici, avatar, genoma e rete. Rimossi grafico, riepilogo e selezione
calcio–movimento dalla dashboard; telemetria e storico restano disponibili.

Per applicare i testi senza riavviare Python, `python -m dashboard.metrics`
genera `dashboard/static/metrics.js`, contenente soltanto definizioni. La pagina
usa questo catalogo prima dell'endpoint già caricato in memoria dal processo;
`python -m dashboard.metrics --check` verifica l'allineamento con la fonte Python.
Gli asset aggiornati hanno un identificatore di versione: ricaricare la pagina
li applica alla run attiva senza inviare comandi al simulatore.

Verificati sintassi JS, regressioni inspector/rete, copertura di tutti i campi
numerici del campione reale e inizializzazione della pagina in DOM simulato:
11 schede, tre parti per spiegazione, selezione della misura e rimozione della
correlazione. Il server attivo serve i nuovi asset. Run
`1792312991763973395-2439725205` ancora running al tick3784592, perdite
telemetriche0; verifiche tramite sole letture HTTP e SQLite.

## Intervento precedente — campane termiche senza deriva, 18 ottobre 2026

Modello `reserve_regulated_water_calcium_neural_rbf_v5`, base `f6b6fbd042a3cc17eb8da59002ea15cab54e0bcd`
con modifiche locali preesistenti conservate. Mantiene metabolismo, calcio,
DO_NOTHING ed eredità della v4. Conservato `TemperatureRefreshInterval = 1000`
scelto dall'utente:10 secondi simulati tra aggiornamenti, passo angolare0,009 radianti.

`RandomUtility::rotate_rbf_set` incrementava le deviazioni standard a ogni
aggiornamento: `sigma_x += 0,01*cos(delta)` e `sigma_y += 0,01*sin(delta)`.
Anche tick0 aumentava sigma_x. Con delta0,009, ogni milione di tick e intervallo1000
aggiungeva circa10 celle a sigma_x e0,09 a sigma_y, senza limite. Il refresh più
frequente accelerava quindi una dilatazione artificiale delle zone calde.
Rimossi questi incrementi: orbitano soltanto i centri, con larghezze e ampiezze
originali costanti. Le ellissi restano allineate agli assi, come prima; non è
una rotazione rigida dell'intero campo. Sovrapposizioni e bordi della mappa
possono ancora cambiare la temperatura media locale/globale. Non viene imposta
una distribuzione termica né una pressione selettiva artificiale.

Validazione: la nuova regressione deterministica fallisce sul codice precedente;
con la correzione100.000 rotazioni conservano larghezze, picchi, raggio delle
orbite e profili locali. Verificati anche tick0, passo angolare, raggio0 e insieme
vuoto. Nella fixture di10 campane su120×120 celle, dopo10.000 aggiornamenti,
a identici centri e ampiezze, quota >=47 °C:41,05% con larghezze fisse e96,21%
reintroducendo solo la vecchia dilatazione. È un confronto diagnostico, non una
misura della run utente. La GUI satura già il rosso sopra40 °C: lo stesso colore
non prova che le temperature siano uguali.

Build del target esplicito `popx` e tutte le10 regressioni di
`scripts/run_regressions.sh` passate. Pilot separato2000 tick: controlli di
conservazione passati,120 vivi, conclusione `short_tick_budget`, risultato SQLite
`data_out/audit/rbf_v5_pilot.sqlite`. Nessuna esportazione statistica JSON.
Eseguibile SHA256 `3d57cbd7430f358e81e12d277b77115484733c1ef54dd16d85c280f09a260768`.
La telemetria registra ora anche intervallo termico e variante RBF; il backend
conserva commit, diff, INI e provenienza nella sessione SQLite.

Riavvio richiesto completato: run precedente `1792242113105640881-731193789`
chiusa con Stop al tick6873549, campione finale e riepilogo confermati in SQLite;
34369 campioni storici conservati. Nuova run `1792276517115838265-2272337452`,
launcher PID638156, backend638163, simulatore638165; verificata attiva a tick3438
con120 vivi, GUI aperta e velocità2×, intervallo1000 e variante RBF corretti,
nessuna perdita/gap di telemetria segnalati. Il riavvio genera un nuovo mondo;
non riprende la popolazione precedente.

## Intervento precedente — attesa e conservazione dei pesi, 17 ottobre 2026

Modello `reserve_regulated_water_calcium_neural_v4`, intervento richiesto dopo
l'identificazione di molti pesi esattamente nulli nella run lunga. Conserva le
modifiche metaboliche e l'erosione del calcio della versione precedente.

### Inspector aggiornato a caldo — 17 ottobre 2026

Modifica dei soli asset web (`dashboard/static/inspector.js`, `inspector.css`,
`index.html`, `app.js`), su base `f6b6fbd` con working tree già modificato.
Nessun cambio a modello, INI, eseguibile o backend della run in corso.
L'avatar usa il colore di `Genome::compute_genetic_color_value`, il nero per i
morti e la geometria di `Renderer::update_entity`: triangolo verticale quando
cloroplasti > mitocondri, altrimenti cerchio a11 segmenti. È un ingrandimento
a scala fissa; non replica la pulsazione delle fasi sense/think. La bussola
usa x positivo a est, y positivo a sud. La freccia normalizzata visualizza
`last_direction`, segnalando la direzione iniziale casuale prima del primo
movimento riuscito; non misura velocità e non ruota artificialmente la sagoma.

Una barretta interattiva per ogni gene originale, nell'ordine del genoma,
inclusi duplicati, zeri, silenti ed esclusi secondo `Brain::wire`. Anteprima
al passaggio/focus e selezione persistente al clic/Invio. I dettagli separano
peso int16, peso normalizzato, somma iniziale dei geni sullo stesso arco e
peso appreso attuale. Il filtro del grafo neurale non filtra il genoma.
Ordinamento crescente/decrescente per posizione originale, sorgente o destinazione
(strato e indice numerici), peso genetico, modulo del peso e stato di cablaggio.
Numerazione e selezione restano associate al gene originale; le parità conservano
l'ordine originale. La preferenza resta attiva tra ispezioni nella stessa pagina.
L'estensione è stata applicata a caldo sulla stessa run a tick3433929, backend
PID522846 invariato: regressioni inspector/rete e verifica degli asset HTTP passate,
inclusa la selezione dopo il riordino; nessun messaggio perso segnalato.

Validazione: `node tests/inspector_regression.js`, `node tests/network_regression.js`
e controllo sintassi JS passati. Controllati via HTTP gli asset aggiornati e
un'ispezione manuale non persistita:413 geni,47 archi, liste silenti/escluse
e insieme degli archi identici a quelli C++. Nessun browser grafico disponibile:
verificati trasformazioni, handler DOM e servizio HTTP, non il layout renderizzato.
Run `1792242113105640881-731193789` osservata da tick3299522 a3344505, stesso
identificativo, backend PID522846, telemetria senza messaggi persi né gap segnalati.

Per applicare questi asset basta ricaricare il browser. Lo storico SQLite
sopravvive al riavvio del backend, ma `scripts/run_popx.sh` usa `wait -n` e
cleanup congiunto: terminare Python termina anche il simulatore e non consente
di proseguire lo stato vivo di quella run. Questo intervento non lo ha riavviato.

### Decisione e attuatore DO_NOTHING

L'indice21, prima riservato e già privo di effetti, diventa l'attuatore esplicito
`DO_NOTHING`. L'INI passa da21 a22 uscite; gli indici precedenti restano invariati
e `KILL_FORWARD22` richiede ancora23 uscite. Il dominio automatico dei geni segue
le nuove dimensioni mediante `Genome::encoding_from_config`. Eredità appresa
ancora true, mutazione0,01, nessuna modifica a metabolismo, cap o ricompense.

Il decisore sceglie il massimo **unico** delle attivazioni. Uscite tutte0,5
(rete vuota/nulla), tutte0 (saturazione), massimi esattamente pari o valori non
finiti producono attesa; non viene più scelto implicitamente MOVE_FORWARD0.
La parità è quella dei float effettivi, senza epsilon aggiuntivo che cancelli
preferenze piccole ma distinguibili. Un massimo unico successivo a due massimi
parziali pari viene scelto normalmente. Anche reti legacy con meno di22 uscite
possono attendere come fallback, ma non hanno il corrispondente neurone selezionabile.

L'attesa viene conteggiata in `actions[21]`, senza tentativi/costi di movimento;
fisiologia, invecchiamento e condizioni di riproduzione proseguono normalmente.
Il contatore aggrega scelta esplicita e fallback: da solo non prova una strategia
evoluta di attesa. Schema configurazione, etichette NN e catalogo Python aggiornati.

### Decadimento ed eredità

Un limite elevato e rigido a tutti i pesi potrebbe imporre segnali artificiali
e impedire alla plasticità di indebolire una connessione. È stato scelto invece
il quanto minimo del genoma, `q = 1/8192 ≈ 0,0001220703125`, come limite del solo
decadimento. La regola diventa:

`decay_term = sign(w) × 0,0001 × max(0, |w| − q)`

`w_nuovo = clamp(w + termine_hebbiano − decay_term, −4, 4)`

Senza termine hebbiano il modulo tende a q senza attraversarlo. Pesi già inferiori
a q non vengono gonfiati dal decadimento, zeri esatti non vengono attivati e il
segno è preservato. La plasticità può ancora ridurre il modulo sotto q, annullare
o invertire un peso: non è un divieto assoluto di dimenticare connessioni.

Alla ricodifica ereditaria si arrotonda `w×8192` all'intero più vicino, mantenendo
i limiti int16. Se un peso non nullo arrotonderebbe a0, si trasmette +1 o−1
secondo il suo segno. Lo zero esatto resta0. Questo evita sia la cancellazione dei
pesi sub-quanto sia l'erosione sistematica di un quanto a ogni nascita dovuta al
troncamento. Resta una quantizzazione: un peso sub-quanto viene portato al minimo
rappresentabile nel figlio, non conservato con precisione float. Geni silenti e
scartati mantengono il percorso di copia precedente. Non si reinizializzano
casualmente le connessioni né si riattivano automaticamente zeri già ereditati.

Il controllo aggiuntivo degli eventi della run v2 ha ricostruito le connessioni
cablate alla nascita, filtrando domini e vincoli feedforward e sommando geni
duplicati sullo stesso arco:0/3.351 zeri negli80 fondatori;6.619/6.719 zeri nelle
ultime120 nascite (tick18.577.321–18.704.944). Sono coorti diverse dal censimento
live97,9%, ma confermano che l'accumulo è già presente nei genomi dei neonati e
non proviene soltanto dalla visualizzazione dei pesi appresi.

### Verifica

Nuova regressione `neural_policy`: rete vuota e zeri espliciti, parità dei massimi,
preferenza di un solo quanto, attuatore di attesa dominante, compatibilità21/1
uscite, saturazione a0,200.000 aggiornamenti di decadimento su pesi positivi e
negativi, zeri invariati, plasticità sotto il minimo e inversione di segno,
100 ricodifiche ereditarie e attesa di Pop con metabolismo e guard di fase reali.
La fixture del parallelismo ora usa un gene esplicito per MOVE_FORWARD: continua
a verificare traiettorie con movimento invece di dipendere dal vecchio fallback.
Il test dei domini configurati e l'integrazione dashboard controllano22 uscite.

Esito: nove regressioni C++ passate; sonde di conservazione e pilot2.000 tick
passati (120 vivi al termine del budget); quattro test Python di protocollo e
integrazione passati. Verificati sintassi delle etichette/catalogo e diff.

```sh
bash scripts/run_regressions.sh
bash scripts/run_audit.sh /tmp/popx_neural_v4_pilot.sqlite 2000
.venv/bin/python -m unittest tests.dashboard_protocol tests.dashboard_integration
```

Il minimo protegge dalla cancellazione numerica, non garantisce sensibilità
funzionale di ogni percorso multilivello o comportamenti adattivi. L'attesa
elimina movimento artificiale: la sua introduzione può ridurre i tentativi
osservati e non va interpretata automaticamente come regressione evolutiva.

Run v3 `1792232706491281466-2902466650` archiviata con Stop al tick990.882,
poi Exit e riavvio del launcher conservando GUI attiva e velocità corrente1×.
Nuova run `1792238220943446053-2507469525`, binario SHA256
`cde496c0262205168c3bd86404e0ad625127ec03134468e5727ae9a9157b102e`.
Al controllo tick2.927: running,120 vivi, nessun errore del ricevitore. Campione
2.800:7.464 azioni di attesa registrate,22.994 tentativi di movimento, residui
di tutte le risorse e dell'energia nulli. Inspector live conferma22 uscite;
catalogo Python espone la descrizione di DO_NOTHING. È una verifica di operatività,
non una prova di maggiore adattamento a lungo termine.

## Calcio, run lunga e dashboard — 17 ottobre 2026 (versione v3)

Modello produttivo `reserve_regulated_water_calcium_v3`: conserva l'acqua metabolica
e introduce erosione conservativa dei soli depositi saturi di carbonato. Analisi
rapida della run v2 fino a18.680.000 tick, controllo live successivo e Stop/Exit
esplicitamente richiesti dall'utente; la run è archiviata al tick18.705.028.
La stabilità descritta negli audit precedenti vale per i rispettivi prefissi:
**non è persistenza ininterrotta sull'intera run lunga**.

### Evidenza della run precedente

Run `1792138698673088289-2625920900`, binario SHA256
`1c5fcdc06d464660d9aadf0e1e28da28bf4fa33deb77c8bd1abd4c99dd243bdc`, zero gap al
cutoff18.680.000.120 vivi,108.242 nascite naturali,4.880 forzate,113.082 morti:
87.130 energia,17.282 temperatura,4.734 età e3.936 esterne/NewGen. Primo recupero
dopo estinzione al tick12.053.952;244 tick distinti di recupero,20 semi ciascuno.
Tutte le nascite forzate hanno genitore vuoto. I120 vivi al cutoff discendono da
`pop_6876115`, introdotto dall'ultimo recupero al tick18.210.815, profondità9–20.
Questa è una linea recente, non la precedente linea `pop_36` ancora persistente.

Glucosio nelle celle6.986.448/7.200.000 (97,03%); ossigeno13.661.696/14.400.000
(94,87%); acqua mondo+vivi7.081.969/8.794.281 iniziali (80,53%). L'acqua era
6.293.793 al tick12.000.000 e poi è risalita: non c'è esaurimento globale imminente
dimostrato da questa serie. Restano possibili scarsità locali, costi termici e
dipendenza dal cap; le estinzioni non sono attribuibili causalmente al solo calcio
o alla sola acqua. Al tick18.000.000 si osservano2 vivi; la presenza successiva
di120 individui non dimostra autosostentamento senza interventi.

**Calcio confermato come problema strutturale.** Nel codice v2 una cella vuota
con500 unità è impraticabile, non ha erosione/diffusione e non può essere scavata
da un agente adiacente. Da20 celle sature iniziali si ricostruisce un minimo di
2.682 celle permanentemente bloccate (18,625% della mappa): un residuo di calcio
non restituito alla morte prova saturazione della cella. È un limite inferiore:
i riempimenti esatti senza overflow non sono identificabili dagli eventi.
Inferenza valida solo per v2, dove nessun processo svuota una cella satura vuota.
Il totale di calcio diminuisce per gli overflow, mentre i blocchi possono crescere.

Controllo live iniziato al tick18.698.578:120 agenti,80 senza distanza percorsa
dalla nascita. Le destinazioni delle ultime azioni osservate si classificano in
41 calcio confermato,47 occupante,26 pendenza,5 azione non deterministica/non di
movimento e1 reattività non ricostruita. La pendenza è valutata prima dell'accesso
alla cella come nel codice; solo azioni deterministiche senza incrementi di
reattività sono attribuite. Le ispezioni sono sequenziali su una run in movimento:
è un censimento delle destinazioni, non un conteggio storico delle cause dei
tentativi falliti né un campione perfettamente simultaneo. Il calcio non spiega
tutta l'immobilità. Il prelievo automatico di2 unità/tick dalla propria cella e
il deposito alla morte favoriscono accumuli terminali; non abbiamo evidenza di
costruzione strategica degli ostacoli. L'auto-prelievo contrasta anche il mantenimento
di una saturazione ottenuta con LEAVE_CALCIUM mentre l'agente resta sulla cella.

Reti ispezionate:6.571/6.712 connessioni con peso **esattamente zero** (97,90%);
minimo modulo non nullo8,035e−15, massimo4. Lo snapshot esporta direttamente
i float delle matrici: non è un arrotondamento Python. Nel codice il decadimento
hebbiano è0,0001; la ricodifica ereditaria tronca `peso×8192` a int16, azzerando
moduli sotto1/8192. Se le uscite sono tutte0,5, argmax favorisce l'azione0, avanti.
Sono meccanismi compatibili con la ripetizione osservata, non una misura causale
del contributo di ciascuno. Eredità appresa, apprendimento e scelta delle azioni
rimangono quelli autorizzati; nessun premio al movimento è stato introdotto.

### Intervento e nuove osservazioni

- Ogni1.000 aggiornamenti ambientali, una cella a500 trasferisce1 unità a ciascuno
  degli8 vicini validi con calcio inferiore a499: massimo8 unità per intervallo.
  I destinatari restano sotto500, quindi l'erosione non sposta semplicemente il
  blocco su un'altra cella. Il calcio totale è conservato, i cap rispettati;
  nessuna modifica alle altre risorse o ricompense. Una cella può riaprirsi entro
  il prossimo intervallo se c'è capacità locale; un'area interamente satura può
  restare bloccata. È un modello astratto di redistribuzione, non una reazione chimica.
- Stats registra `calcium_saturated_cells` e `calcium_distance_correlation`:
  Pearson tra calcio medio nei vicini alla fine della finestra e distanza percorsa
  nella finestra dagli stessi individui presenti ai due campioni. Sono esposti
  numerosità, durata e medie; n<2 o varianza nulla produce `null`, non0. Nati/morti
  nell'intervallo sono esclusi; età, pendenza, occupazione e censura restano possibili
  confondenti. I campioni duplicati allo stesso tick non azzerano la finestra.
- Corretta la contabilità dei semi di recupero: ingressi espliciti di materia ed
  energia, separati dalle nascite naturali. Nella vecchia run i residui
  `[488000,488000,0,0,97600,97600]` e488.000 di energia corrispondono esattamente
  alle dotazioni di4.880 semi, non a materia prodotta dal ciclo metabolico. La
  cronologia v2 è preservata; le descrizioni segnalano il limite storico.
- Dashboard: tutte le6 risorse nelle celle più azoto (attualmente0 senza ciclo),
  grafici separati per saturazione e correlazione, catalogo Python `/api/metrics`
  con etichette italiane e definizioni. Copertura di tutte le188 misure numeriche
  presenti nel campione v2 controllato. Dati mancanti/null diventano lacune, non
  zeri inventati; la misura selezionata mostra definizione e ultimo valore.
- Grafo NN: pesi piccoli in notazione scientifica, zeri esatti opzionali tratteggiati,
  filtro inclusivo sul modulo con soglia validata, conteggio archi visibili, opzione
  per nodi isolati. Spessore `max(0,4;6×|peso|/massimo_modulo)` px, scala stabile
  rispetto all'intero snapshot durante il filtraggio. Cambiare run cancella
  l'inspector precedente. Nessun peso della simulazione è modificato dal grafico.
- Chiusura backend: limite di3 secondi all'attesa delle connessioni HTTP/SSE
  durante lo shutdown. Il primo rilancio ha rilevato il vecchio backend ancora
  proprietario della porta UDP dopo Exit: la run era già persistita e conclusa.
  Il processo bloccato è stato terminato; una fixture con stream SSE aperto
  verifica ora che Exit rilasci entrambi i processi del launcher.

### Validazione e protocollo

Build target `popx`; otto regressioni standalone passate, inclusa nuova fixture
calcio (durata, conservazione, bordi, capacità, nessun nuovo blocco, Pearson inversa,
varianza nulla, sostituzione campione e reset). Estesa la fixture del recupero per
controllare ingressi e residui di materia/energia con campione iniziale esplicito.
Sonde di conservazione e pilot2.000 tick passati; pilot concluso al budget con120
vivi. Quattro test Python di protocollo/integrazione passati; controllo Node dei
pesi nulli/minuscoli, soglie inclusive, spessori e grafo con Cytoscape vendorizzato
passato. Sintassi JS/Python controllata. Nessuna suite CTest dichiarata, nessuna
nuova dipendenza installata e nessuna prova evolutiva lunga del nuovo equilibrio:
le fixture verificano meccanica e osservabilità, non garantiscono nuove strategie.

```sh
python3 scripts/analyze_calcium.py data_out/popx.sqlite \
  --run 1792138698673088289-2625920900 --tick 18680000 --live
bash scripts/run_regressions.sh
bash scripts/run_audit.sh /tmp/popx_calcium_conservation.sqlite probes
bash scripts/run_audit.sh /tmp/popx_calcium_pilot.sqlite 2000
.venv/bin/python -m unittest tests.dashboard_protocol tests.dashboard_integration
node tests/network_regression.js
.venv/bin/python -m tests.launcher_smoke
```

Il protocollo d'analisi è limitato al modello v2; `--live` usa solo `agents` e
`inspect`, senza salvare snapshot neurali. Rapporti testuali locali in
`data_out/audit/calcium_18680000/{report,final,restart}.txt`; le letture live originali
non sono ripetibili dopo Stop. Vecchia run archiviata con reason `stop`; rilancio
del launcher sulla stessa configurazione/database, GUI e velocità2×. La nuova
run registra modello, binario, commit+diff, sorgenti, INI e costanti attraverso
la provenienza esistente; RNG globali non deterministici come prima.

Rilancio verificato: run `1792232706491281466-2902466650`, binario SHA256
`60d5c9d776b35e7fd3e43f2f9b388686cc3f631c2f5623d58c3d14ab43e6deda`. Al controllo
tick3.833: running, GUI attiva, velocità2×,120 vivi, nessuna perdita di telemetria
o errore del ricevitore. Campione3.800:2 celle sature, correlazione0,07235 su114
individui nell'intervallo200 tick, residui delle6 risorse ed energia nulli.
Tutte le199 misure numeriche/null definite presenti sono coperte dal catalogo.
Questa verifica di avvio non è un confronto evolutivo causale con la mappa precedente.

## Follow-up della stabilità — tick4.690.000, 16 ottobre 2026

Seconda analisi della stessa run `1792138698673088289-2625920900`,
`reserve_regulated_water_v2`, al cutoff4.690.000. Stessa configurazione/provenienza
del precedente controllo227.800 (binario SHA256
`1c5fcdc06d464660d9aadf0e1e28da28bf4fa33deb77c8bd1abd4c99dd243bdc`). Nessuna
modifica produttiva o interruzione della run. Zero lacune registrate; conteggi
nascite/morti/vivi ed età completate ricostruiti dagli eventi concordano con Stats.

### Continuità, ricambio e perdita delle linee

120 vivi,8.997 nascite naturali,0 forzate e0 recuperi dopo estinzione.
8.957 decessi:4.786 energia,1.618 temperatura,1.437 vecchiaia,1.116 abbattimenti
NewGen. Vita media conclusa61.416 tick; tra i nati naturalmente mediana15.154 e
media61.763. Restano quindi mortalità e ricambio: la popolazione costante non è
una popolazione di individui immortali. Il mantenimento naturale è osservato per
oltre23 durate massime individuali, ma non è una garanzia di persistenza infinita.

Delle11 radici rappresentate al tick227.800 rimane solo `pop_36`, già fondatore
iniziale. L'ultima morte fuori da questa genealogia è al tick2.458.580: la linea
persiste da sola per oltre2,23 milioni di tick. Profondità genealogica massima43;
120 vivi alle profondità40:6,41:29,42:66,43:19. Al tick227.800 questa radice aveva
solo3 vivi; a1.000.000 ne aveva51, a2.000.000 ne aveva66. I120 vivi hanno120 genomi
di nascita distinti: radice comune non significa identità genetica o funzionale.
La perdita di79 radici è osservata; selezione, deriva, FIFO e NewGen non sono
isolati causalmente. Non si può concludere che la linea superstite sia l'ottimo.

Cap120 in23.340/23.451 campioni (99,53%); coda64 e275.915.225 rifiuti cumulativi
di accodamento (richieste ripetute, non figli distinti). Il cap limita le occasioni
di riproduzione e la dimensione della popolazione non misura la capacità ecologica
massima del mondo. NewGen contribuisce al ricambio e alla selezione artificiale.

### Staticità misurata

Prima lettura appaiata ai tick4.693.917–4.695.929:119 individui comuni,118 senza
successi,110 con tentativi tutti falliti,8 senza tentativi.74.445 tentativi e1
movimento riuscito. Tutti119 effettuano fotosintesi e sono idonei alla riproduzione
alla fine della finestra;117 hanno15 cloroplasti e2 ne hanno16. Energia mediana99,21.
Posizioni nel rettangolo x23–68,y14–31: nessuno è sul bordo del mondo120×120; il
vicinato è spesso occupato (7 dei119 hanno tutti gli8 vicini rappresentati nel
campione), ma non è sufficiente per attribuire tutti i fallimenti all'occupazione.

Seconda misura indipendente ai tick4.715.799–4.716.804,120 individui appaiati:
37.185 tentativi,0 successi;111 tentano senza riuscire,9 non tentano.35.845 tentativi
sono `MOVE_FORWARD` (96,40%),670 `MOVE_LEFT`,335 `MOVE_EAST`,335 `MOVE_SOUTH`.
Gli altri atti sono2.680 `LEAVE_GLUCOSE` e335 `GET_H2O`. Tutti120 fotosintetizzano,
119 sono riproduttivamente idonei, energia mediana99,31;119 hanno15 cloroplasti.
Il costo delle azioni fallite viene comunque pagato. Questi dati documentano
ripetizione di un'azione inefficace, non soltanto una strategia deliberatamente
sedentaria. Cause esatte dei fallimenti (occupante/pendenza/destinazione/altro)
non sono registrate e restano da isolare; nessun snapshot neurale è stato richiesto.

I movimenti riusciti per1000 agent-tick passano da6,12 nei primi227.800 tick a
2,19 fra227.800–1.000.000,0,85 fra1–2 milioni,0,15 fra2–3 milioni,0,10 fra3–4
milioni,0,64 fra4–4,69 milioni (quindi non monotoni). Nell'ultima finestra
4.650.000–4.690.000:170 successi su1.486.156 tentativi. Esposizione ricavata dal
costo basale/0,02, con l'approssimazione della costante float esplicitata nel
protocollo precedente. Nessun aumento del movimento è un obiettivo implicito.

### Risorse e interpretazione

Al cutoff, **nelle sole celle**:7.112.612 glucosio su capacità7.200.000 (98,79%),
14.155.728 ossigeno su14.400.000 (98,30%). Totali mondo+vivi: glucosio7.201.098,
ossigeno14.301.366. La rigenerazione effettiva del glucosio rallenta molto dopo la
saturazione iniziale:282.564 negli ultimi690.000 tick contro2.261.410 nei primi
227.800. È una misura di spazio residuo/consumo, non l'intensità nominale di regen.
Le concentrazioni globali non provano assenza di scarsità in ogni nicchia o figlio.

Acqua mondo+vivi7.436.310 contro8.794.281 iniziali (84,56% residuo): riciclo molto
più sostenibile ma **non acqua infinita o perfettamente stazionaria**. Bilancio:
`8.794.281 + 81.731.157 respirazioni - 82.707.319 fotosintesi - 381.809 overflow
= 7.436.310`. La fotosintesi netta trattiene equivalenti d'acqua nei prodotti del
modello; overflow e trasformazioni aperte impediscono di dedurre un equilibrio
indefinito. CO2 totale anch'essa in calo, da7.176.879 a5.554.130. Residui delle sei
risorse nulli; residuo energetico5,47e−9 compatibile con precisione numerica.

Conclusioni supportate: l'obiettivo di una linea che persiste e si rinnova senza
nascite forzate è raggiunto sull'orizzonte osservato. Il regime è prevalentemente
fotosintetico, residente e limitato dal cap, con ripetuti tentativi di movimento
falliti. Auto-acquisizione e riciclo consentono sostentamento locale; assenza di
costi specifici dei cloroplasti e di competizione per luce riduce i compromessi
per la strategia fotosintetica. L'energia elevata nonostante azioni fallite sostiene
l'ipotesi di debole vantaggio selettivo per navigazione efficiente, non prova assenza
di selezione o adattamento causale. La dinamica genealogica mostra sostituzione
delle linee; la funzione dei diversi genomi non è stata confrontata.

Prima di nuove modifiche ecologiche, distinguere cause dei movimenti falliti e
opportunità riproduttive effettivamente bloccate dal cap. Un confronto diagnostico
su stessa mappa/genoma potrebbe misurare i vantaggi di navigare; questi interventi
non sono stati eseguiti. Nessun premio al movimento o modifica di risorse/cap
introdotti da questa analisi.

### Protocollo e artefatti

```sh
python3 scripts/analyze_stability.py data_out/popx.sqlite \
  --run 1792138698673088289-2625920900 --tick 4690000 \
  --output data_out/audit/stability_followup_4690000
python3 scripts/analyze_lineages.py data_out/popx.sqlite \
  --run 1792138698673088289-2625920900 --tick 4690000
```

Le letture live originali usavano `--live-seconds 10` e poi5; il secondo rapporto
è in `actions/`. Lo script ora registra anche delta per singola azione nel CSV.
Rapporti, CSV, grafici, genealogie e controllo aggiuntivo `extra.txt` nella cartella
indicata, ignorata da Git; nessuna esportazione JSON statistica. Le finestre live
sono successive al cutoff e non riproducibili a posteriori. La verifica aggiuntiva
raggruppa gli eventi per radice ereditata dal genitore e confronta i genomi dei vivi
al cutoff; usa solo gli eventi già persistiti. Nessuna nuova simulazione o suite
C++ eseguita per questa analisi in sola lettura del modello.

## Osservazione della run con acqua metabolica — 16 ottobre 2026

Analisi richiesta dall'utente della run attiva `1792138698673088289-2625920900`,
modello `reserve_regulated_water_v2`, prefisso al tick227.800. Nessuna modifica dei
parametri o comando di controllo; due letture `agents` sincronizzate sul thread
principale, senza snapshot neurali, ai tick228.051 e230.058. Il processo è rimasto
in esecuzione. Metadata: commit `f6b6fbd042a3cc17eb8da59002ea15cab54e0bcd` con diff
registrato delle modifiche autorizzate, SHA256 binario
`1c5fcdc06d464660d9aadf0e1e28da28bf4fa33deb77c8bd1abd4c99dd243bdc`.
Nessuna differenza negli hash dei sorgenti registrati rispetto ai file correnti;
nessuna lacuna di telemetria registrata. Configurazione effettiva nei metadata.

### Stabilità demografica e risorse

Al cutoff:120 vivi,483 nascite naturali,0 forzate,0 recuperi dopo estinzione,
443 decessi (212 energia,126 temperatura,57 vecchiaia,48 NewGen). Tutti gli80
fondatori originali sono morti; restano discendenti di11 radici genealogiche.
Profondità massima4: vivi di profondità1:5,2:55,3:57,4:3. Il rinnovo naturale è
quindi osservato oltre la durata massima individuale200.000, ma solo attraverso
poche generazioni; non dimostra ancora stabilità indefinita o ottimizzazione.

Confronto descrittivo allo **stesso tick227.800** con la precedente run
`1792105551429747751-4236462813` (ambienti/genomi iniziali indipendenti, nessun
accoppiamento RNG; più modifiche produttive intercorse):

| Misura | Run precedente | Acqua metabolica |
|---|---:|---:|
|Vivi|120|120|
|Nascite naturali|5.544|483|
|Morti per energia|5.005|212|
|Morti per temperatura|451|126|
|Morti per vecchiaia|0|57|
|Acqua totale iniziale|8.786.249|8.794.281|
|Acqua totale finale|6.116.103|8.670.084|
|Glucosio totale iniziale|3.767.191|3.814.060|
|Glucosio totale finale|4.453.541|5.892.379|
|Energia media dei vivi|50,33|97,49|

La nuova run conserva98,59% dell'acqua iniziale e ha54,49% di glucosio totale in
più rispetto all'inizio. Bilanci centrali delle sei risorse e dell'energia nulli.
La coppia fotosintesi–respirazione mantiene in circolo l'acqua; la fotosintesi
può fornire glucosio localmente. Quasi tutti gli agenti del campione breve sono
fotosinteticamente attivi (118/119 individui presenti in entrambe le letture).
Questo sostiene l'interpretazione di nicchie alimentate localmente, ma non misura
la disponibilità delle risorse in ogni cella né isola causalmente l'effetto della
sola acqua. La vecchia run non aveva ancora avuto forzature a questo stesso tick
e manteneva anch'essa120 vivi: il numero dei vivi da solo non distingueva i modelli.

### Movimento, riproduzione e pressione selettiva

Nei2.007 tick fra le due letture dei vivi,119 individui sono presenti in entrambe
(le nascite/morti non appaiate sono escluse dal confronto individuale):

-116 senza alcun movimento riuscito;54 senza tentativi,62 con tentativi ma tutti
  falliti. Solo3 si sono mossi;41.306 tentativi,1.340 successi (3,24%).
-118 hanno effettuato fotosintesi;117 soddisfano alla lettura finale le condizioni
  naturali di età, energia e glucosio. Energia mediana99,60, glucosio mediano827,
  acqua mediana49; queste mediane non descrivono gli individui morti.
-Il contatore dei tentativi conta le azioni di movimento eseguite; un fallimento
  non distingue occupazione, confine, pendenza o altre condizioni. Senza telemetria
  aggiuntiva non attribuire i62 casi bloccati a una causa specifica.

Nella finestra200.000–227.800 la nuova run registra4,74 movimenti riusciti ogni1000
agent-tick contro37,53 nella vecchia run allo stesso intervallo. Le esposizioni
sono ricavate dal costo basale cumulativo/0,02 (approssimazione della costante
float produttiva). Sono differenze descrittive fra run, non effetto causale stimato.

Cap120 raggiunto in1.130/1.140 campioni (99,12%). Coda riproduttiva64 al cutoff;
12.810.698 rifiuti di accodamento cumulativi: **tentativi ripetuti**, non altrettanti
figli perduti o individui distinti. La mortalità ridotta libera pochi posti e quindi
rallenta il ricambio genealogico: stabilità ecologica e velocità evolutiva sono
misure diverse. NewGen continua a intervenire ogni50.000 tick e seleziona sul numero
di figli e sull'energia, non sulla persistenza dei discendenti.

Interpretazione: minore evidenza di stress energetico; maggiore limitazione delle
opportunità riproduttive da spazio globale disponibile e FIFO. Non è dimostrata una
riduzione universale della selezione. Il codice non assegna un costo specifico di
mantenimento ai cloroplasti né competizione per la luce fra agenti: una nicchia
fotosintetica favorevole può sostenere individui immobili. Molti movimenti falliti
possono persistere quando il relativo costo è facilmente compensato. Non introdurre
premi al movimento o scarsità artificiale come risposta a questa osservazione.
Anche l'innalzamento del cap richiederebbe una prova separata: può ampliare il numero
di posti, ma non garantisce ricambio continuo dopo una nuova saturazione.

Protocollo ripetibile e artefatti:

```sh
python3 scripts/analyze_stability.py data_out/popx.sqlite \
  --run 1792138698673088289-2625920900 --tick 227800 \
  --baseline 1792105551429747751-4236462813 \
  --output data_out/audit/stability_2026-10-16 \
  > data_out/audit/stability_2026-10-16/report.txt
python3 scripts/analyze_lineages.py data_out/popx.sqlite \
  --run 1792138698673088289-2625920900 --tick 227800
```

L'esecuzione originale aggiungeva `--live-seconds 10` per il confronto dei vivi;
quelle letture non sono riproducibili a posteriori. Conservate in
`live_movement.csv` (identità, posizione finale, età, tratti, risorse, delta di
movimento/reazioni/figli), con tick iniziale/finale nel rapporto. Grafico
`comparison.png`, rapporto e verifica genealogica `lineages.txt` nella stessa
cartella ignorata da Git. Nessuna esportazione JSON statistica. Conteggi genealogici
verificati contro `Stats`; nessuna nuova simulazione o regressione C++ eseguita
per questa analisi, che non modifica il modello.

## Stato corrente — acqua metabolica, 16 ottobre 2026

Implementata l'estensione autorizzata: ogni reazione respiratoria **effettivamente
completata** produce una unità d'acqua, complementare alla fotosintesi nelle unità
astratte1:1 già usate dal simulatore:

- fotosintesi: CO2 + acqua → glucosio + O2;
- respirazione: glucosio + O2 → CO2 + acqua, con energia/calore regolati come prima.

Queste sono reazioni del modello, non equazioni molecolari reali. Nessuna risorsa
H2/H+ né nuova reazione indipendente è stata introdotta. L'acqua prodotta viene
ceduta alla cella fino alla sua capacità; la parte non accettata resta nella
riserva dell'agente. `released[1]` registra solo la quantità effettivamente ceduta.
La redistribuzione alla morte approvata in precedenza continua a gestire l'eccesso
locale. Nessuna produzione per capacità non utilizzata, credito frazionario,
scarsità di substrati, temperatura inibitoria o riserva energetica già soddisfatta.
Nessuna modifica a rendimento, calore, soglie riproduttive, cap, eredità dei pesi
appresi o tasso di mutazione.

Il ledger centrale `Stats` e quello indipendente dell'audit usano ora
`delta_acqua_metabolica = respirazioni_completate - fotosintesi_completate`.
Il numero di unità d'acqua prodotte è quindi già disponibile nel contatore
`respirations`, senza nuove esportazioni o contatori ridondanti. Le nuove sessioni
produttive e diagnostiche sono identificate come `reserve_regulated_water_v2`;
i dati storici `reserve_regulated_v1` conservano i propri bilanci originali.

### Validazione dell'acqua metabolica

Eseguiti build Release target `popx`, tutte le sette regressioni standalone,
audit `probes` e pilot2000 tick:

```sh
bash scripts/run_regressions.sh
bash scripts/run_audit.sh /tmp/popx_water_conservation_20261016.sqlite probes
bash scripts/run_audit.sh /tmp/popx_water_pilot_20261016.sqlite 2000
```

La regressione respiratoria verifica ora anche: acqua prodotta per reazione,
cella parzialmente piena/piena, conservazione nella riserva e conteggio delle
sole cessioni accettate, target energetico raggiunto a metà batch, nessuna acqua
senza glucosio/ossigeno o con respirazione inibita, credito frazionario e ciclo
fotosintesi–respirazione senza rifornimenti. Il ciclo deterministico ripristina
acqua e CO2 iniziali. La probe indipendente misura delta acqua+1 per respirazione
e−1 per fotosintesi.

Pilot concluso a2000 tick (`short_tick_budget`),120 vivi,126 nascite naturali e
0 forzate. Respirazioni44.099, fotosintesi40.951, overflow acqua alla morte10.786:
`9.957.836 + 44.099 - 40.951 - 10.786 = 9.950.198` unità d'acqua finali mondo+vivi.
Residui inspiegati nulli per tutte le sei risorse e per l'energia, sia nel ledger
indipendente sia in `Stats` (roundoff energetico contabilizzato). Output SQLite,
provenienza e log in `data_out/audit/metabolic_water_2026-10-16/`, ignorati da Git.
Il pilot non è accoppiato a quello precedente e non dimostra un miglioramento
causale della stabilità evolutiva; le differenze di overflow tra pilot non sono
un confronto della capacità di riciclo.

La coppia di reazioni chiude il riciclo metabolico dell'acqua nelle unità del
modello, non l'intero ecosistema: glucosio/ossigeno rigenerati e semi sono ingressi
esterni; overflow rimane una perdita. La lacuna preesistente dei ledger sugli
apporti dei semi, descritta nell'analisi della run lunga, non è stata modificata.
La run lunga già avviata resta sul vecchio processo: il nuovo modello richiede
un nuovo avvio dell'eseguibile, non soltanto Restart della run nella GUI.

## Stato corrente — riproduzione naturale e recupero locale dell'acqua, 16 ottobre 2026

Implementate le tre modifiche approvate dopo l'analisi della run lunga:

- **Nessuna nascita forzata da individui vivi sotto20**. Raccolta candidati,
  estrazione dalla FIFO e `try_reproduce` richiedono l'idoneità naturale attuale.
  Restano età>1000, energia>50 e glucosio>20, costo dissipato e trasferimento1/8.
  Un candidato diventato inidoneo mentre attende non produce figli né paga costi.
- **Nessun moltiplicatore di mutazione sotto20**. Genoma e PHY usano sempre il
  tasso configurato (INI0,01); l'ereditarietà dei pesi appresi resta true.
- **Acqua restituita alla morte:** prima alla cella occupata, poi l'eccesso agli
  otto vicini validi, in quote ripartite fra quelli con capacità disponibile.
  Ripartizione ripetuta dopo la saturazione di un vicino; ordine iniziale ruotato
  casualmente per non privilegiare una direzione nei resti interi. Non modifica
  `MaxH2o`, dotazione iniziale o altre risorse. Solo le quantità accettate vengono
  sottratte e registrate in `released[1]`; il residuo non collocabile rimane sink
  esplicito del morto. Nessuna propagazione oltre il vicinato, nessuna diffusione
  per tick. La morte resta idempotente e viene elaborata nelle fasi sequenziali.

Il budget extra `forced_repro_priority_per_tick` rimane leggibile per compatibilità
INI, ma è dichiarato inattivo nello schema. `MinPopulationAllowed=20` resta il
numero obiettivo dei semi di recupero dopo **estinzione completa**, non un minimo
mantenuto tramite riproduzione. Nelle run infinite questo recupero resta attivo,
con `forced=true`, `recovery_seed=true` e genitore vuoto. Il flag legacy di recupero
indica ora l'avvenuta reintroduzione fino al successivo aggiornamento; non abilita
riproduzioni da superstiti. Cap120, durata massima200000 e NewGen invariati.

### Validazione delle modifiche

Eseguiti:

```sh
bash scripts/run_regressions.sh
bash scripts/run_audit.sh /tmp/popx_conservation_20261016.sqlite probes
bash scripts/run_audit.sh /tmp/popx_pilot_20261016.sqlite 2000
build-release/src/popx --validate-config config/popx.ini
```

Build Release del solo target `popx` riuscita. Sette regressioni standalone
riuscite (respirazione, ciclo, parallelismo, codifica, feromoni, stats/runtime,
nuova `recovery`). La nuova fixture controlla separatamente maturità/energia/
glucosio, candidati FIFO divenuti inidonei, nascita naturale sotto20, assenza di
riscritture del tasso di mutazione, trasferimenti parent–figlio, restituzione acqua
locale/vicini, capacità diverse, confini, mondo1×1, residui, ripartizione uniforme
entro un'unità e doppia chiamata di morte. Le fixture impostano esplicitamente gli
stati necessari; non rappresentano popolazioni evolute. La fixture di nascita
nell'audit generale è stata resa esplicitamente naturale (età1001 e glucosio100),
senza introdurre deroghe nella produzione.

Audit `probes` passato. Pilot produttivo diagnostico2000 tick:133 nascite naturali,
0 forzate,120 vivi finali, conclusione `short_tick_budget`; residui inspiegati nulli
per le sei risorse e per l'energia dopo il roundoff contabilizzato. Overflow acqua
residuo2707: la redistribuzione non garantisce capacità locale infinita. Protocollo,
provenienza e risultati SQLite/log conservati in
`data_out/audit/recovery_2026-10-16/` (ignorati da Git). Il pilot stocastico non è un
confronto causale col vecchio modello e non dimostra ancora stabilità di un ceppo.

La run lunga esistente non è stata fermata o riavviata. Continua sul vecchio
eseguibile già caricato; il nuovo binario richiede un nuovo avvio del **processo**.
Start/Restart dell'interfaccia ricrea la run nello stesso processo e non carica
il codice appena compilato.

### Estensione idrica discussa, non implementata

Non è stato aggiunto idrogeno né acqua metabolica. La respirazione biologica
produce acqua attraverso riduzione dell'ossigeno con elettroni e protoni; per
modellare il ritorno d'acqua non è necessario simulare un serbatoio di H2 libero.
Il modello PopX usa reazioni astratte1:1, non coefficienti molecolari bilanciati:
una futura reazione complementare alla fotosintesi va definita nelle stesse unità
oppure va ricalibrata insieme all'intera stechiometria. Richiede aggiornamento dei
ledger e prove per reazioni effettivamente completate, riserve e capacità; non
basta aggiungere acqua in presenza di ossigeno. Glucosio rigenerato, semi e overflow
mantengono comunque aperto il sistema. Fonte biochimica consultata:
[Mailloux2015, trasporto elettronico mitocondriale](https://pubmed.ncbi.nlm.nih.gov/25744690/).

## Analisi della run lunga — 16 ottobre 2026

Obiettivo: persistenza genealogica e rinnovo con nascite naturali, non identità
immutabile del genoma o semplice permanenza del processo. Analisi osservazionale
in sola lettura della run `1792105551429747751-4236462813`, prefisso concluso al
tick **6.242.200**; processo lasciato in esecuzione, nessun Restart/Stop o comando
all'inspector. Nessuna modifica a parametri INI, fisiologia o codice produttivo.

### Evidenza e provenienza

- Commit registrato `f6b6fbd042a3cc17eb8da59002ea15cab54e0bcd`, modello
  `reserve_regulated_v1`, Clang18.1.3; diff produttivo registrato vuoto.
- SHA256 binario `64cb6ca45bd70105a4973cdece6047758c456b1fd5dd7ef4633727ba34526c05`,
  verificato uguale al binario locale. Hash sorgenti registrati confrontati con i
  file presenti: unica differenza `scripts/render_readme_figures.py`, estranea al
  simulatore. INI effettivo, costanti, backend, RNG e provenienza completa restano
  nei metadata SQLite; nessuna garanzia di riproducibilità globale.
- Nessuna lacuna registrata; zero messaggi persi nel campione finale. Ricostruzione
  con zero genitori mancanti; conteggi verificati contro i campioni cumulativi.
  Gli individui vivi al cutoff sono censurati, non trattati come vite concluse.
- 80 fondatori, **169.767 nascite naturali**, **256.383 nascite forzate con genitore**,
  **3.120 nuovi semi senza genitore**, in **156 recuperi dopo estinzione completa**.
  I contatori della dashboard includono i semi nelle 259.503 nascite forzate.
- 429.230 vite concluse: 372.061 morti per energia, 56.569 per temperatura, 600
  abbattimenti NewGen. **Nessuna morte per vecchiaia**; massimo osservato112.804,
  contro `MaxAge=200000`.
- **Primi337.573 tick senza nascite forzate né nuovi semi**. Al campione337.400:
  9.451 nascite naturali,28 vivi. Tutti gli80 fondatori iniziali erano già morti
  entro46.757 tick: il rinnovo naturale è quindi realmente avvenuto. Questa fase
  comprende gli abbattimenti NewGen, non è una popolazione priva di interventi.
- Profondità genealogica massima203; massima catena di archi di nascita naturale89.
  La catena naturale riparte da zero a ogni nascita forzata; ciò non esclude antenati
  forzati precedenti. Al cutoff120 vivi, tutti da una radice, profondità naturale
  consecutiva2–8: una fotografia di espansione, non una prova di stabilità futura.

### Colli di bottiglia osservati e ipotesi

**Soccorso riproduttivo sotto20.** Tra i figli forzati: energia iniziale mediana
0,413, glucosio mediano0, età alla morte mediana34 tick e media166,5. Il79,8% nasce
senza glucosio;147.003 nascite forzate su256.383 provengono da genitori con età
<=1000, non ancora maturi per la riproduzione naturale. L'energia mediana del
parent prima del costo/trasferimento, ricostruita da `8*energia_figlio+0,3`, è3,604.
La produzione sottrae risorse ai genitori già fragili e può ripetersi: il soccorso
non è gratuito né garantisce recupero. Solo1.354 figli forzati conclusi (0,53%)
hanno avuto almeno un figlio naturale. È il candidato principale a un confronto
controllato; questi dati non quantificano quante estinzioni avrebbe evitato la sua
rimozione. Sotto20 resta inoltre la mutazione×10, anche PHY e anche per eventuali
nascite naturali: amplifica le perturbazioni ereditarie proprio nel collo
popolazionale. Nessun confronto causale della mutazione è stato eseguito qui.

**Ecologia non stazionaria e acqua finita.** Acqua totale mondo+vivi da8.786.249 a
1.533.032 (-82,55%). Bilancio indipendente al cutoff:
`8.786.249 + 312.000 (semi) - 5.667.168 (fotosintesi) - 1.898.049 (overflow morti)
= 1.533.032`. La fotosintesi consuma acqua; respirazione e rigenerazione corrente
non la restituiscono. Glucosio e ossigeno, invece, rigenerano. Questo limita la
persistenza delle strategie fotosintetiche e cambia l'ambiente selettivo: attendere
più tick non equivale a ripetere l'evoluzione nello stesso ambiente. Al primo
collasso l'acqua globale era ancora circa4,48 milioni: non è provata una causa
unica, e disponibilità globale non implica accesso locale. Il77,8% dei figli
naturali nasce senza acqua, ma può acquisirla dall'ambiente; non è automaticamente
una condanna alla morte.

**Sopravvivenza dei figli naturali.** Mediana680 tick, media1.591;106.261 su169.647
vite naturali concluse terminano entro1000 tick (62,6%). Solo21.617 (12,7%) hanno
prodotto figli naturali, per148.242 figli naturali totali:0,874 per individuo
concluso di questa coorte. È una media osservata con cap, ecologia variabile e
forzature, non una stima causale del tasso riproduttivo intrinseco. La soglia
naturale (`age>1000`, energia>50, glucosio>20) è superabile, come attestano le
catene naturali. Abbassarla può anticipare trasferimenti e aumentare nascite fragili;
non risolve da sola scarsità, termica o conservazione dei caratteri favorevoli.

**Cap e NewGen.** Cap120 presente in5.713/31.212 campioni (18,3%), nel79,5% dei
campioni dei primi500.000 tick. Può limitare espansione/diversità: l'occupazione
campionata non conta però le nascite effettivamente impedite. Non è il limite nelle
fasi di collasso sotto20; aumentarlo può accelerare esaurimento delle risorse.
NewGen abbatte600 individui,416 dei quali giovani; il punteggio usa numero totale
di figli, inclusi i forzati, e riserva energetica, non discendenti autosufficienti.
Il conteggio ridotto non esclude effetti sulle linee rare, ma non prova causalità.

**Limite dei bilanci attuali.** Residui energia312.000, glucosio312.000,
acqua312.000, ossigeno62.400 e lipidi62.400 coincidono con le dotazioni dei3.120
semi di recupero. Sono ingressi esterni non separati nei ledger, non una prova di
creazione spontanea durante le reazioni. Gli eventi `recovery_seed` permettono la
riconciliazione. Tenere distinta questa lacuna contabile dalla demografia.

### Decisioni e validazione dell'analisi

Conservati durata massima, soglie riproduttive e cap: non emerge una giustificazione
sufficiente per cambiarli come rimedio alla stabilità. Allungare la vita non
intercetta alcuna morte osservata; anticipare la riproduzione o aumentare il cap
può aggravare il drenaggio. Conservata l'eredità dei pesi appresi true.
Priorità sperimentali: isolare il costo del soccorso sui genitori immaturi/deboli,
il moltiplicatore di mutazione e la sostenibilità del ciclo idrico; confronti su
stesso ambiente/coorti prima di trasferire correzioni alla produzione. Nessuna
nuova politica, dotazione o rigenerazione è stata introdotta in questa analisi.

Protocollo ripetibile (lettura SQLite, output testuale; grafico facoltativo con
Matplotlib già disponibile nel Python di sistema):

```sh
python3 scripts/analyze_lineages.py /pop_proj/data_out/popx.sqlite \
  --run 1792105551429747751-4236462813 --tick 6242200 \
  --plot data_out/audit/lineages_2026-10-16/demography.png \
  > data_out/audit/lineages_2026-10-16/report.txt
```

Script e protocollo conservati; rapporto e grafico locali ignorati da Git.
Verifica su dati reali: conteggi nascite/decessi, vivi ricostruiti e somma età delle
vite concluse confrontati con `Stats`. Nessuna nuova simulazione, regressione C++
o prova evolutiva controfattuale eseguita; il codice produttivo non è cambiato.

## Stato corrente — osservabilità, controllo e dashboard locale, 14 ottobre 2026

L'intervento separa processo, run e finestra. Fisiologia, ereditarietà dei pesi
appresi, riproduzione e ordine sense→think→act restano quelli autorizzati,
descritti nella sezione fisiologica seguente. I vecchi comandi con output JSON
nelle sezioni storiche sono evidenza del protocollo precedente: i nuovi output
statistici e diagnostici sono SQLite. Gli artefatti storici non vengono cancellati.

### Avvio e configurazione

```sh
# Ambiente e asset locali già preparati in questo workspace:
bash scripts/run_popx.sh --headless
# Prima preparazione di un altro checkout, con installazione esplicita:
bash scripts/run_popx.sh --setup --build --headless
bash scripts/run_popx.sh --help
# Eseguibile senza servizio web; la simulazione continua anche senza ricevitore:
build-release/src/popx --headless --config config/popx.ini --speed 2
# Schema e validazione non inizializzano SFML né richiedono DISPLAY:
build-release/src/popx --config-schema
build-release/src/popx --validate-config config/popx.ini
```

Dashboard: `http://127.0.0.1:8000`, UDP loopback8001. Launcher: `--headless`,
`--config`, `--binary`, `--db`, `--http-port`, `--udp-port`, `--speed 1|2`,
`--setup`, `--build`. L'avvio ordinario non installa né ricompila. La build usa
solo il target `popx` di `build-release`; le dipendenze C++ devono già essere
configurate. FastAPI/Uvicorn sono isolati in `.venv`; Chart.js4.4.9 e
Cytoscape.js3.31.2 sono scaricati da `--setup`, verificati con SHA256 e serviti
localmente, incluse le licenze. Il launcher verifica il PID del backend pronto,
propaga i segnali e raccoglie entrambi i figli.

Nuovi parametri INI: `map_size=120`, `max_duration_minutes=0`, `gui_fps=30`.
La durata è in **minuti simulati**, con scadenza al tick
`ceil(minuti * 6000)`. Con valore zero la run è endless: all'estinzione il mondo
resta in memoria e il manager inserisce fino a20 semi di nascita forzata, marcati
`forced=true`, `recovery_seed=true` e senza genitore; tick e statistiche non si
resettano. Se la durata è limitata, l'estinzione e Stop concludono la run.
Non è stata identificata la causa della
vecchia interruzione attorno a704.200 tick: l'ultimo campione con19 vivi non
contiene il motivo terminale. Non esiste un limite speciale a704.000 tick.

`Simulator` possiede il tick64 autorevole e gli stati running/paused/finished.
100 tick/s a1×,200 a2×, sempre10 ms biologici per tick; attese tra gruppi di
massimo8 tick, comandi elaborati anche in pausa. Non si recupera il tempo
trascorso in pausa. La cadenza richiesta resta soggetta al costo del calcolo.
Start/Restart prepara una nuova configurazione, mondo, popolazione e arena TBB
prima di sostituire la run. Un errore di validazione lascia intatta la precedente.
Start è disponibile dopo la conclusione; Restart anche durante l'esecuzione.
Stop lascia il processo disponibile; Esci lo termina.

Lo schema C++ centralizza tipi, limiti, default, descrizioni e parametri legacy.
Validazione incrociata: lunghezza genoma, intervallo alpha, capacità e celle di
spawn; ricerca di spawn con fallback finito. Gli override `@N` sono conservati e
validati ma non applicati durante la run. La pagina INI usa lo stesso binario per
schema/validazione, mantiene commenti e righe invariate, controlla la revisione
contro modifiche esterne e sostituisce atomicamente il file. Il salvataggio non
modifica la run corrente. `brain_serialization_enabled` è deprecato e inerte;
`nnets_dir` è usato esclusivamente dai salvataggi manuali.

### Statistiche e persistenza

`Stats` raccoglie campioni iniziali, periodici (ogni200 tick) e finali, eventi
individuali, censura, bilanci e risultati diagnostici. `IStats` espone record
tipizzati di sessione, campione, evento, bilancio e risultato diagnostico; i
campi estensibili mantengono un payload JSON. Fondatori e
nascite registrano identità, genoma, PHY e genealogia; morti/abbattimenti
registrano lo stato prima della rimozione. I riepiloghi delle vite concluse
conservano aggregati senza trattenere i `Pop`. Restano separati richieste di
azione, tentativi/successi di movimento, acquisizioni/restituzioni, riproduzione,
backlog, respirazione, fotosintesi, calore, cap, overflow e roundoff.

Le medie dei vivi, le medie dei campioni e le medie delle vite concluse hanno
denominatori distinti; un campione finale allo stesso tick sostituisce quello
periodico senza duplicarne il peso. La finestra degli indicatori di frequenza
contiene10 campioni, mentre somme/contatori dell'intera run sopravvivono alla
rotazione. L'energia distingue i costi d'azione contabilizzati ma non ancora
pagati: una morte fra ACT e fisiologia non crea un falso residuo. Sono distinti
tempo simulato, tempo reale, latenza del tick/frame e secondi CPU di processo;
questi ultimi includono tutti i thread, trasporto e rendering, non solo la fisica.

`Stats` possiede `ICommunicator`/`UdpCommunicator`. Il worker gestisce copie di
messaggi immutabili e non legge agenti o mondo. Protocollo1: processo, run, tick,
sequenza, tipo e request_id; frammenti ASCII da512 byte, ricomposizione limitata,
ACK dopo commit SQLite, retry e deduplicazione. I comandi ricevono conferma dopo
l'applicazione; il simulatore conserva2048 risposte e rifiuta run superate e
richieste scadute. Snapshot completi vengono costruiti sul thread sequenziale.

Memoria di trasporto limitata: coda4096 messaggi/32 MiB,32 messaggi pendenti entro
32 MiB, messaggio singolo massimo16 MiB; retry ogni500 ms per12 tentativi.
Ricomposizione Python:128 messaggi/32 MiB, scadenza15 s. L'ambiente iniziale è
inviato in blocchi di2048 celle. L'assenza del backend non ferma la simulazione:
al superamento dei limiti si registrano perdite e intervalli mancanti. I cumulativi
riallineano gli aggregati, ma non ricostruiscono eventi/genomi iniziali persi.
Una sessione con lacune non costituisce un audit completo. Stato e collegamento
sono pubblicati ogni secondo reale, anche in pausa o a run conclusa.

Il backend è l'unico scrittore SQLite/WAL: sessioni, campioni, eventi, ambiente,
risultati, lacune e indice snapshot. Provenienza: INI effettivo, commit/diff,
sorgenti non tracciati, hash sorgenti/binario, compilatore, costanti e limiti RNG.
Le RNG produttive restano random_device/rand non centralizzati: nessuna nuova
garanzia di riproducibilità globale. Gli audit e i confronti termici/strategie
usano lo stesso collettore e un ricevitore temporaneo; dichiarano salvataggio
riuscito soltanto dopo conferma della persistenza. Le preparazioni/asserzioni
indipendenti delle fixture restano diagnostiche. Gli script di analisi leggono
SQLite e, in sola lettura, vecchi report JSON; non scrivono nuovi JSON statistici.

### Interfacce

SFML/ImGui: finestra massimizzata sul display X predefinito, con barra del titolo
di sistema trascinabile; non è un fullscreen senza bordi che possa aprirsi a
cavallo fra due monitor. Mondo quadrato scalato uniformemente a sinistra,
widget modulari nella fascia destra, tema scuro/verde acqua. Stato, tempi,
popolazione, prestazioni, connessione, comandi e livelli mappa; nessun chart né
inspector. Geometrie e buffer vengono riutilizzati. `gui_fps` è il massimo;
il rendering riduce la cadenza se il suo costo misurato supera il budget CPU
obiettivo del20%. In pausa si ridisegna per eventi o al massimo per aggiornamenti
periodici. Headless distrugge finestra, contesto e ImGui; Mostra GUI li ricrea
sulla stessa run. Senza display la richiesta restituisce errore e la run continua.
Il file utente `imgui.ini` non viene letto/scritto dal nuovo layout.

Frontend: storico selezionabile, SSE per statistiche, grafici senza animazioni,
serie limitate e query per intervallo (massimo2000 punti), eventi e run paginati.
Inspector solo su richiesta esplicita: lista vivi, ricerca/ordine, dettagli e tick
della fotografia. Un agente morto prima della richiesta restituisce un errore,
non uno stato vecchio presentato come attuale. Il grafo visualizza matrici
correnti, attivazioni disponibili, segno/peso, livelli, sensori/azioni e geni
silenti senza rieseguire la rete. Salvataggio manuale atomico in
`nnets_dir/run_id/pop_id/tick_snapshot_id.json`, indicizzato in SQLite e
consultabile dopo morte/restart. Nessuna esportazione automatica a nascita/NewGen
né cancellazione automatica dei vecchi snapshot.

### Validazione dell'architettura

Comandi ripetibili (standalone, non CTest):

```sh
cmake --build build-release --target popx -j 2
bash scripts/run_regressions.sh
.venv/bin/python -m unittest tests/dashboard_protocol.py tests/dashboard_integration.py -v
.venv/bin/python -m tests.gui_smoke
.venv/bin/python tests/launcher_smoke.py
.venv/bin/python -m tests.runtime_pilot data_out/audit/architecture_2026-10-14/runtime
bash scripts/run_audit.sh /tmp/conservation.sqlite probes
bash scripts/run_audit.sh /tmp/pilot.sqlite 2000
bash scripts/run_thermal_comparison.sh /tmp/moderate.sqlite moderate
bash scripts/run_strategy_comparison.sh /tmp/strategy.sqlite /tmp/pilot.sqlite all
```

Le verifiche GUI richiedono un display reale. Verifiche eseguite: Release;
regressioni respirazione, ciclo, parallelismo, genoma, feromoni e stats/runtime;
UDP frammentato, pacchetti mancanti/duplicati/riordinati, input malformato e
persistenza; avvio senza DISPLAY, pause/velocità, restart con mondo/thread/rete
variati, INI invalido preservando la run, scadenza esatta a6 tick, rifiuto run
superata, NewGen duplicato eseguito una volta, snapshot e database dopo riavvio.
La disconnessione iniziale di7 s produce lacune esplicite e recupero del servizio.
Il clock controllato verifica1.000.000 iterazioni senza limite temporale; questo non è
un esperimento evolutivo completo di1.000.000 tick.

GUI: verificata su desktop1920×1080, incluse ripetute transizioni
headless→GUI→headless nella stessa run in pausa. Il pilot nativo2000 tick
confronta osservazioni indipendenti e `Stats`: residui nulli per le sei risorse e
per l'energia; campioni/eventi/censura sono presenti in SQLite. Il confronto
termico moderate ha eseguito180 controlli e144 popolazioni; il confronto strategie
full64 prove e replay. Risultati locali in
`data_out/audit/architecture_2026-10-14/`, esclusi da Git.

Pilot con scheduler produttivo a2×,2000 tick e INI produttivo (solo durata
modificata), prima del budget adattivo: headless2,28 s CPU /11,79 s reali,
GUI12,78 s CPU /18,51 s reali. Dopo il budget: headless2,43 s CPU /11,77 s reali,
GUI4,59 s CPU /12,02 s reali; entrambi terminano esattamente a2000 tick per durata.
La GUI ha ridotto la cadenza media a5,43 FPS sul display disponibile; `gui_fps=30`
è un massimo, non una promessa di frame rate. I tempi reali del test includono
avvio e osservazione del completamento; la CPU di Stats parte dall'inizio sessione.
Le run sono distinte/stocastiche, quindi i numeri non isolano causalmente il rendering.

Il frontend è stato verificato tramite API, asset locali e sintassi JavaScript;
non è stata eseguita una verifica visuale in un browser. Il confronto GUI/headless
usa run stocastiche separate: misura locale di costo/scheduling, non una stima
causale né una prova di equivalenza evolutiva.

## Fisiologia di riferimento — respirazione regolata, 14 ottobre 2026

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
