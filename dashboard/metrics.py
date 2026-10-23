"""Italian metric guide. Generate the static browser catalog with python -m dashboard.metrics."""

RESOURCES = ('Glucosio', 'Acqua', 'Calcio / carbonato', 'Anidride carbonica', 'Ossigeno', 'Lipidi')
ACTIONS = ('Avanti', 'Sinistra', 'Destra', 'Direzione casuale', 'Est', 'Ovest', 'Nord', 'Sud',
           'Emetti FOOD', 'Emetti DANGER', 'Emetti MATE', 'Emetti HOME', 'Preleva glucosio',
           'Rilascia glucosio', 'Preleva acqua', 'Rilascia acqua', 'Preleva calcio', 'Rilascia calcio',
           'Brucia calorie', 'Imposta oscillatore', 'Aumenta reattività', 'Attendi (DO_NOTHING)', 'Uccidi davanti')


def catalog():
    result = {}

    def add(path, label, calculation, meaning, interpretation):
        result[path] = dict(label=label, calculation=calculation, meaning=meaning, interpretation=interpretation,
                            description=f'Calcolo: {calculation} Rappresenta: {meaning} Lettura: {interpretation}')

    cumulative = ('Una curva crescente indica nuovi eventi; un tratto orizzontale indica che il conteggio è rimasto '
                  'uguale. A parità di intervallo in tick, una salita più ripida indica eventi più frequenti.')
    live_average = ('Si calcola prima il valore di ogni agente vivo, poi si sommano i valori e si divide per il '
                    'numero dei vivi al campione. ')
    learning = {
        'reward': ('Ricompensa media dei vivi',
                   'Per ogni agente lo scorer somma cinque contributi pesati: età, energia disponibile, variazione '
                   'energetica, figli e temperatura; limita il risultato a [−1, 1]. Si fa poi la media sui vivi.',
                   'Il punteggio medio con cui il modello valuta la condizione corrente degli agenti.',
                   'Valori più alti indicano condizioni valutate più favorevolmente dallo scorer. Il segno e la '
                   'grandezza dipendono dalla somma dei cinque contributi; per valutare i risultati nel tempo, '
                   'affiancare sopravvivenza e nascite.'),
        'baseline': ('Ricompensa recente media dei vivi',
                     'Ogni agente aggiorna la propria media: nuova media = vecchia media + learning_baseline_rate × '
                     '(ricompensa − vecchia media). La curva mostra la media di queste memorie fra i vivi.',
                     'Il livello recente di ricompensa usato come riferimento per apprendere.',
                     'Una salita indica che il riferimento recente sta migliorando. Un learning_baseline_rate '
                     'più piccolo produce una risposta più lenta. Ogni nuovo agente avvia una propria memoria.'),
        'signal': ('Segnale medio di apprendimento',
                   'Per ogni agente: ricompensa corrente meno la sua media mobile precedente, limitata a [−1, 1]. '
                   'La curva è la media sui vivi; il primo segnale individuale vale zero.',
                   'Quanto l’esito corrente si discosta dall’esperienza recente degli agenti.',
                   'Positivo: esito sopra il riferimento recente; negativo: sotto il riferimento. Vicino a zero: '
                   'esiti vicini al riferimento oppure compensazione fra segnali individuali di segno opposto.'),
        'mean_abs_signal': ('Intensità media del segnale',
                           'Si fa la media del valore assoluto del segnale individuale fra tutti i vivi.',
                           'L’intensità degli scostamenti dalla ricompensa recente, conservando entrambi i segni.',
                           'Vicino a zero: scostamenti piccoli. Un valore alto con media con segno bassa indica compensazione fra individui.'),
        'signal_std': ('Dispersione del segnale',
                       'Radice della media dei quadrati degli scostamenti del segnale dalla media dei vivi.',
                       'La diversità degli esiti relativi all’esperienza recente fra individui.',
                       'Zero indica segnali uguali; valori maggiori indicano differenze più ampie.'),
        'positive_signal_fraction': ('Quota di segnali positivi',
                                    'Numero di vivi con segnale maggiore di zero diviso il numero dei vivi.',
                                    'La quota di agenti con ricompensa sopra il proprio riferimento recente.',
                                    '0 indica nessun segnale positivo; 0,5 la metà; 1 tutti i vivi.'),
        'near_zero_signal_fraction': ('Quota di segnali quasi nulli',
                                     'Numero di vivi con |segnale| < 0,0001 diviso il numero dei vivi.',
                                     'La quota con scostamenti molto piccoli secondo questa soglia diagnostica.',
                                     'Vicino a 1 indica segnali quasi nulli diffusi; vicino a 0 indica scostamenti oltre la soglia.'),
        'mean_abs_plasticity': ('Plasticità proposta per sinapsi',
                               live_average + 'Per ogni agente si media |learning_rate × segnale × traccia| sulle connessioni presenti nell’ultimo aggiornamento.',
                               'L’intensità della modifica proposta dall’apprendimento prima di decadimento e limiti.',
                               'Zero indica nessuna proposta; valori piccoli possono derivare da segnale o tracce piccoli. Confrontare il decadimento sulla stessa scala.'),
        'mean_abs_decay': ('Decadimento proposto per sinapsi',
                          live_average + 'Per ogni agente si media learning_weight_decay × max(0, |peso| − 1/8192) sulle connessioni presenti nell’ultimo aggiornamento.',
                          'L’intensità della riduzione dei pesi proposta dal decadimento.',
                          'Un valore superiore alla plasticità indica una maggiore ampiezza media del termine di decadimento; la variazione effettiva include segni e limiti.'),
        'mean_abs_delta': ('Variazione assoluta media dei pesi',
                           live_average + 'Il valore individuale è la media di |peso dopo − peso prima| '
                           'nell’ultimo aggiornamento, comprendendo plasticità, decadimento e limiti.',
                           'L’intensità del cambiamento effettivo delle connessioni neurali.',
                           'Zero indica pesi fermi nell’ultimo aggiornamento; valori maggiori indicano cambiamenti '
                           'più ampi. L’utilità di questi cambiamenti si valuta insieme a sopravvivenza e discendenza.'),
        'mean_abs_trace': ('Traccia media delle azioni',
                           live_average + 'Il valore individuale è il modulo medio delle tracce sinaptiche usate '
                           'nell’ultimo aggiornamento. Le azioni aggiungono coattivazione alle tracce, che ogni '
                           'tick vengono moltiplicate per learning_trace_decay.',
                           'La memoria recente delle attività neurali cui viene attribuito il segnale di apprendimento.',
                           'Zero indica tracce assenti; valori maggiori indicano una memoria recente più intensa. '
                           'A parità di segnale, tracce maggiori producono un termine di plasticità più ampio.'),
        'mean_abs_weight': ('Modulo medio dei pesi',
                            live_average + 'Il valore individuale è la media dei valori assoluti delle connessioni '
                            'cablate, includendo gli zeri; una rete vuota contribuisce con zero.',
                            'La grandezza media delle connessioni neurali degli agenti osservati.',
                            'Vicino a zero indica connessioni deboli in media; valori più alti indicano pesi più '
                            'grandi. Il modulo riunisce pesi positivi e negativi.'),
        'zero_fraction': ('Frazione media di pesi nulli',
                          live_average + 'Per ogni rete si divide il numero di pesi esattamente zero per il numero '
                          'di connessioni cablate; una rete vuota contribuisce con zero.',
                          'La quota media di connessioni cablate che hanno peso nullo.',
                          '0 significa quota nulla nelle reti presenti, oppure reti vuote; 0,5 significa metà '
                          'delle connessioni in media; 1 significa tutte nulle in ciascuna rete osservata.'),
        'n': ('Popolazione osservata per apprendimento', 'Si contano gli agenti vivi inclusi nel campione.',
              'La dimensione del gruppo usato per le medie di apprendimento.',
              'Valori maggiori indicano più agenti nel confronto. Nascite e morti cambiano il gruppo e possono '
              'spostarne la media; con zero agenti le medie pubblicate valgono zero per convenzione.'),
    }
    for key, args in learning.items():
        add('learning.' + key, *args)
    components = [
        ('age', 'Età', '0,10 × età / 200.000, con il rapporto limitato a [0, 1].',
         'Il contributo della durata di vita.', 'Va da 0 alla nascita a 0,10 al limite di età; aumenta con l’età.'),
        ('energy', 'Energia', '0,30 × ((energia / 400 − 0,25) / 0,75), con il rapporto limitato a [−1, 1].',
         'Il contributo della riserva energetica.', 'Vale zero a energia 100, è positivo sopra 100 e negativo sotto; '
         'raggiunge 0,30 a energia 400.'),
        ('delta', 'Variazione energetica', '0,35 × (energia corrente − energia precedente) / capacità respiratoria, '
         'con il rapporto limitato a [−1, 1]. La capacità usa i mitocondri e l’energia per respirazione configurata.',
         'Il contributo del guadagno o della perdita recente di energia.',
         'Positivo indica energia in aumento, negativo energia in diminuzione; il contributo va da −0,35 a +0,35.'),
        ('offspring', 'Discendenza', '0,05 × figli / 25, con il rapporto limitato a [0, 1].',
         'Il contributo dei figli naturali prodotti.', 'Zero corrisponde a zero figli; cresce fino a 0,05 con 25 figli.'),
        ('temperature', 'Temperatura', '0,20 × (2 × adeguatezza − 1), dove adeguatezza = '
         '2 elevato a −((temperatura − ottimo) / phy_efficiency_half_range_c)².',
         'Il contributo della vicinanza alla temperatura ottimale.',
         'Vale +0,20 all’ottimo, zero a uno scarto pari all’intervallo configurato; tende gradualmente a −0,20 allontanandosi.'),
    ]
    for key, label, calculation, meaning, reading in components:
        add('learning.' + key + '_component', label + ' · contributo alla ricompensa',
            'Si calcola il contributo individuale e se ne fa la media sui vivi. Formula individuale: ' + calculation,
            meaning, reading + ' La curva riassume agenti con condizioni anche diverse.')

    reproduction = [
        ('eligible_agent_ticks', 'Idoneità riproduttiva cumulativa',
         'A ogni tick si aggiunge il numero di agenti vivi con energia > 50, età > 1.000 tick e glucosio > 20, '
         'le soglie di idoneità del modello corrente.',
         'Il tempo complessivo trascorso in condizioni riproduttive, espresso in agenti × tick.',
         'Un agente idoneo per 100 tick aggiunge 100; dieci agenti idonei per 10 tick aggiungono ancora 100. '
         'Una crescita rapida indica più agenti idonei o idoneità più duratura.'),
        ('cap_blocked_agent_ticks', 'Idoneità bloccata dal cap',
         'Si sommano gli agenti vivi con energia > 50, età > 1.000 tick e glucosio > 20 nei tick in cui la raccolta '
         'dei candidati inizia con la popolazione al limite.',
         'L’esposizione degli agenti idonei al limite di popolazione, in agenti × tick.',
         'Zero indica assenza di idoneità bloccata. Se cresce quasi come l’idoneità totale, gran parte del tempo '
         'riproduttivo disponibile viene trascorso al cap.'),
        ('cap_blocked_ticks', 'Tick al cap',
         'Si aggiunge uno per ogni tick che inizia la raccolta candidati con popolazione al limite.',
         'Il numero cumulativo di tick trascorsi al cap di popolazione.',
         'Il rapporto con i tick della run dà la quota di tempo al cap; vicino a uno indica saturazione persistente.'),
        ('attempts', 'Tentativi effettivi di riproduzione',
         'Si conta ogni ricerca di una cella di nascita avviata dopo i controlli di idoneità, costo e cap.',
         'Quante occasioni riproduttive sono arrivate alla ricerca di spazio adiacente.',
         cumulative + ' Nel modello corrente il totale è nascite riuscite + fallimenti per spazio.'),
        ('no_space', 'Riproduzioni senza spazio',
         'Si conta un fallimento quando tutte le posizioni entro offspring_dispersal_radius risultano inaccessibili '
         'per occupazione, barriere di calcio o bordo.', 'Le occasioni riproduttive ostacolate dallo spazio locale.',
         'Zero indica che ogni tentativo ha trovato spazio. Il rapporto con i tentativi dà la quota di fallimenti '
         'per spazio; una quota alta indica un ostacolo frequente.'),
        ('births', 'Nascite da tentativi processati',
         'Si aggiunge uno dopo ogni riproduzione naturale riuscita e inserimento del figlio nel mondo.',
         'Il numero cumulativo di figli nati dai tentativi riproduttivi.',
         cumulative + ' Il rapporto con i tentativi misura la quota di tentativi riusciti.'),
        ('dispersed_births', 'Nascite oltre la prima corona',
         'Si conta una nascita quando le otto celle immediate erano indisponibili e il figlio è stato deposto '
         'a distanza di almeno due celle dal genitore, entro offspring_dispersal_radius.',
         'I figli nati grazie alla dispersione locale estesa.',
         'Una crescita rapida rispetto alle nascite totali indica pressione spaziale attorno ai genitori.'),
        ('capacity', 'Cap di popolazione', 'Si legge max_population_allowed dall’INI caricato all’avvio della run.',
         'La capacità massima di popolazione della run.',
         'Confrontarlo con i vivi: quando coincidono, le nuove riproduzioni attendono che si liberi capacità.'),
        ('recovery_bank_size', 'Profili nella riserva ereditaria',
         'Si conta il numero di profili archiviati alla morte, distinti per genoma di nascita e tratti fisici; '
         'un nuovo donatore dello stesso profilo sostituisce il precedente.',
         'La varietà di donatori recenti disponibile per il recupero dopo estinzione.',
         'Zero indica riserva vuota; da 1 a 64 indica quanti profili possono essere estratti al recupero.'),
        ('recovery_bank_capacity', 'Capienza della riserva ereditaria',
         'Si legge il limite di 64 profili conservati in memoria e nel checkpoint.',
         'Il numero massimo di profili recenti conservabili.',
         'Quando la riserva è piena, un nuovo profilo sostituisce quello archiviato da più tempo.'),
    ]
    for key, *args in reproduction:
        add('reproduction.' + key, *args)

    entries = [
        ('tick', 'Tick', 'Si conta un passo dopo ogni aggiornamento completo di agenti e ambiente.',
         'L’avanzamento della simulazione.', '100 tick equivalgono a un secondo simulato; la curva si ferma durante una pausa.'),
        ('simulated_seconds', 'Tempo simulato', 'Tick completati × 0,01 secondi.',
         'Il tempo biologico trascorso nel modello.', '60 corrisponde a un minuto simulato; il turbo percorre più tempo simulato per secondo reale.'),
        ('wall_seconds', 'Tempo reale trascorso', 'Si misura il tempo dell’orologio dall’avvio della run al campione, comprese le pause.',
         'La durata reale della sessione.', 'Valori crescenti indicano tempo trascorso; confrontare le differenze con quelle dei tick per stimare la velocità.'),
        ('process_cpu_seconds', 'Tempo CPU', 'Si somma il tempo di CPU consumato dai thread del processo dalla partenza della run.',
         'Il lavoro di calcolo cumulativo del simulatore.',
         'Un aumento di 2 secondi CPU in un secondo reale equivale all’uso medio di circa due core in quell’intervallo.'),
        ('alive', 'Agenti vivi', 'Si contano gli individui vivi presenti al campionamento.',
         'La popolazione attuale.', 'Una salita indica più nascite che morti nel periodo; una discesa indica il contrario. Un plateau al cap indica saturazione.'),
        ('generation', 'Ciclo NewGen', 'Il contatore aumenta quando viene eseguito il ciclo gestionale NewGen.',
         'Quanti cicli di selezione NewGen sono stati eseguiti.',
         'Un gradino segnala un nuovo ciclo gestionale; la discendenza biologica si segue tramite genitori e figli.'),
        ('peak_alive', 'Massimo di agenti vivi', 'Si conserva il massimo numero simultaneo di vivi registrato tramite campioni ed eventi.',
         'Il picco di popolazione raggiunto nella run.', 'Sale solo quando viene superato il record precedente; confrontarlo col cap mostra quanto la popolazione vi si è avvicinata.'),
        ('max_distance_alive', 'Distanza massima dei vivi', 'Si prende il maggiore percorso individuale dalla nascita fra gli agenti vivi.',
         'Il percorso dell’agente attualmente vivo che ha viaggiato più lontano, in celle.',
         'Sale con un nuovo record fra i vivi e può scendere alla morte del detentore. Una diagonale unitaria aggiunge √2 celle.'),
        ('max_lifetime_distance', 'Record di distanza individuale', 'Si conserva il massimo dei percorsi osservati nei campioni e alla morte.',
         'Il maggiore percorso individuale registrato nella run, in celle.', 'Una salita indica un nuovo record; il record resta conservato dopo la morte dell’agente.'),
        ('corr_offspring_distance_alive', 'Correlazione figli–distanza dei vivi',
         'Si calcola il coefficiente di Pearson fra figli e distanza dalla nascita degli agenti vivi.',
         'L’associazione lineare fra discendenza e percorso nel gruppo osservato.',
         '+1 indica associazione crescente, −1 decrescente; valori vicini a zero indicano associazione lineare debole. '
         'Il calcolo storico assegna zero anche con varianza nulla. Età e selezione dei superstiti contribuiscono al confronto.'),
        ('birth_rate_simulated_second', 'Nascite per secondo simulato',
         'Si dividono le nascite naturali e forzate della finestra recente per la sua durata in secondi simulati; '
         'la finestra usa fino a dieci campioni precedenti.', 'La frequenza recente delle nascite.',
         'Zero indica una finestra senza nascite; 2 significa due nascite per secondo simulato. Confrontare la curva con la frequenza delle morti.'),
        ('death_rate_simulated_second', 'Morti per secondo simulato',
         'Si dividono le morti della finestra recente, inclusi gli abbattimenti, per la durata simulata; '
         'la finestra usa fino a dieci campioni precedenti.', 'La frequenza recente delle morti.',
         'Zero indica una finestra senza morti; valori sopra la frequenza delle nascite accompagnano una popolazione in calo.'),
        ('backlog', 'Genitori in coda', 'Si conta il numero di candidati presenti nella coda FIFO riproduttiva.',
         'Il lavoro riproduttivo ancora in attesa di elaborazione.',
         'Zero indica coda vuota; una crescita prolungata indica che gli ingressi superano la capacità di elaborazione o che il cap rallenta lo smaltimento.'),
        ('backlog_peak', 'Massimo della coda riproduttiva', 'Si conserva la massima lunghezza della coda raggiunta nella run.',
         'Il maggiore accumulo di candidati riproduttivi osservato.', 'Sale solo a un nuovo massimo; confrontarlo con max_repro_backlog per valutare la pressione sulla coda.'),
        ('backlog_dropped', 'Richieste escluse dalla coda', 'Si conta ogni tentativo di accodamento rifiutato dal limite della coda.',
         'La pressione cumulativa sulla capacità di accodamento.',
         cumulative + ' Uno stesso genitore può contribuire più volte se ripresenta la richiesta.'),
        ('reproduction_attempts_last_tick', 'Candidati riproduttivi elaborati', 'Si contano i candidati estratti ed elaborati dalla coda nell’ultimo tick.',
         'Il carico recente di elaborazione riproduttiva.', 'Zero indica assenza di candidati elaborati; valori vicini al budget indicano pieno utilizzo della capacità per tick.'),
        ('avg_tick_us', 'Costo medio di un tick', 'Si sommano i microsecondi trascorsi negli aggiornamenti della simulazione e si divide per i tick misurati.',
         'Il tempo medio di calcolo di un tick dall’avvio, in microsecondi.',
         'Valori più bassi consentono più tick al secondo. 1.000 µs equivale a circa 1.000 tick/s teorici dedicando tutto il tempo agli aggiornamenti; la media cambia lentamente nelle run lunghe.'),
        ('avg_frame_us', 'Costo medio di un frame', 'Si sommano i microsecondi di preparazione e presentazione dei frame e si divide per i frame misurati.',
         'Il tempo medio per disegnare la GUI.', 'Valori più bassi indicano rendering più rapido; in headless rimane la media dei frame precedenti, oppure zero se la GUI è sempre stata chiusa.'),
        ('fps', 'Frame per secondo reale', 'Numero di frame × 1.000.000 diviso per la somma dei loro intervalli in microsecondi.',
         'La frequenza media di presentazione della GUI.', 'Valori più alti indicano aggiornamenti visivi più frequenti; in headless il valore conserva la storia dei frame già disegnati.'),
        ('world_nitrogen', 'Azoto nelle celle', 'Si somma la quantità di azoto di tutte le celle.',
         'Lo stock ambientale di azoto, in unità del modello.', 'Nel modello corrente l’azoto parte da zero e resta zero, perché il ciclo metabolico usa altre risorse.'),
        ('calcium_saturated_cells', 'Celle sature di calcio', 'Si contano le celle che contengono esattamente 500 unità di carbonato.',
         'Il numero di celle che formano una barriera di calcio.',
         'Zero indica assenza di barriere; una salita indica più celle bloccanti. La possibilità di muoversi dipende anche dalla loro posizione rispetto agli agenti.'),
    ]
    for path, *args in entries:
        add(path, *args)

    # Retained for API/archive readers; the dashboard hides this family of charts.
    for key, label, calculation, meaning, reading in [
        ('pearson', 'Correlazione calcio–distanza recente',
         'Si calcola Pearson fra calcio medio nei vicini validi al secondo campione e distanza percorsa fra i due campioni, per gli stessi agenti.',
         'L’associazione lineare fra calcio locale e movimento recente dei superstiti.',
         '+1 indica associazione crescente, −1 decrescente; vicino a zero indica associazione lineare debole. '
         'Il valore è assente con meno di due coppie o varianza nulla; occupazione, pendenza ed età influenzano l’associazione.'),
        ('n', 'Agenti nel confronto calcio–distanza', 'Si contano gli agenti presenti in entrambi i campioni con almeno un vicino valido.',
         'La dimensione del gruppo confrontato.', 'Con meno di due agenti la correlazione resta indefinita; più agenti forniscono più coppie al calcolo.'),
        ('window_ticks', 'Intervallo del confronto calcio–distanza', 'Tick del secondo campione meno tick del primo.',
         'La durata della finestra di movimento.', 'Di norma vale 200; zero identifica il campione iniziale. Finestre diverse possono contenere percorsi diversi.'),
        ('mean_neighbour_calcium', 'Calcio medio nel vicinato del confronto', 'Si media il calcio dei vicini validi di ogni agente appaiato e poi si mediano i risultati.',
         'La concentrazione locale media attorno agli agenti confrontati.', 'Va da 0 a 500 unità per cella; valori maggiori indicano vicinati più ricchi di calcio.'),
        ('mean_distance', 'Distanza recente media del confronto', 'Si sommano gli incrementi di percorso degli agenti appaiati e si divide per il loro numero.',
         'Il percorso medio nella finestra, in celle.', 'Zero indica assenza di spostamenti nel gruppo; valori maggiori indicano percorsi più lunghi durante la stessa finestra.'),
    ]:
        add('calcium_distance_correlation.' + key, label, calculation, meaning, reading)

    # Definition at the individual level, then explicit aggregation for each scope.
    totals = {
        'energy': ('Energia', 'Si legge la riserva energetica individuale.', 'L’energia disponibile.',
                   'Valori più alti indicano più riserva; un agente muore se la propria energia scende a zero.'),
        'distance': ('Distanza percorsa', 'Si sommano le lunghezze degli spostamenti riusciti dalla nascita; una diagonale unitaria vale √2 celle.',
                     'La lunghezza del percorso effettuato, in celle.', 'Zero indica percorso nullo; valori maggiori indicano più strada percorsa.'),
        'offspring': ('Figli naturali', 'Si conta ogni figlio naturale prodotto dall’agente.', 'La discendenza diretta prodotta.',
                      'Zero indica assenza di figli; valori maggiori indicano più figli naturali.'),
        'movement_attempts': ('Tentativi di movimento', 'Si contano le azioni di movimento richieste dall’agente.',
                              'La frequenza cumulativa delle richieste di spostamento.',
                              'Confrontare con i movimenti riusciti: la differenza conta le richieste bloccate.'),
        'movement_successes': ('Movimenti riusciti', 'Si aggiunge uno ogni volta che l’agente cambia effettivamente cella.',
                               'Quanti spostamenti sono riusciti.',
                               'Il rapporto con i tentativi misura la quota di successo; vicino a uno indica pochi blocchi.'),
        'movement_blocked_occupied': ('Movimenti bloccati da occupazione',
                                      'Si conta un tentativo quando la destinazione contiene un Pop o una Source.',
                                      'Le richieste respinte perché la cella scelta è occupata.',
                                      'Un valore alto rispetto ai tentativi indica congestione nelle direzioni scelte.'),
        'movement_blocked_calcium': ('Movimenti bloccati dal calcio',
                                     'Si conta un tentativo quando la destinazione libera ha calcio al massimo della cella.',
                                     'Le richieste respinte da una barriera di carbonato.',
                                     'Un valore alto indica che barriere sature impediscono l’accesso alle celle scelte.'),
        'movement_blocked_slope': ('Movimenti bloccati dalla pendenza',
                                   'Si conta un tentativo quando il dislivello supera la soglia scalabile del Pop.',
                                   'Le richieste respinte dal limite individuale di salita.',
                                   'Un valore alto indica che la direzione scelta incontra frequentemente salite non percorribili.'),
        'movement_blocked_boundary': ('Movimenti fuori mappa',
                                      'Si conta un tentativo con destinazione oltre uno dei bordi della mappa.',
                                      'Le richieste respinte dal bordo del mondo.',
                                      'Un valore alto indica azioni dirette spesso fuori dal mondo.'),
        'movement_no_displacement': ('Movimenti senza spostamento richiesto',
                                     'Si conta un tentativo la cui destinazione coincide con la cella di partenza.',
                                     'Le richieste che non definiscono una nuova cella.',
                                     'Un valore alto può indicare estrazione casuale nulla o direzione nulla.'),
        'movement_blocked_other': ('Altri movimenti falliti',
                                   'Si contano i rifiuti del mondo senza una causa classificata e i mondi non disponibili.',
                                   'I fallimenti residui da indagare.',
                                   'Dovrebbe restare vicino a zero; una crescita segnala una condizione non coperta dalle altre cause.'),
        'respirations': ('Reazioni di respirazione', 'Si contano le reazioni respiratorie completate dalla nascita.',
                         'L’attività respiratoria accumulata.', 'Valori maggiori indicano più reazioni completate e più trasformazioni dei substrati.'),
        'photosynthesis': ('Reazioni di fotosintesi', 'Si contano le reazioni fotosintetiche completate dalla nascita.',
                           'L’attività fotosintetica accumulata.', 'Valori maggiori indicano più conversioni fotosintetiche completate.'),
        'respiration_gain': ('Energia dalla respirazione', 'Si somma l’energia ottenuta dalle reazioni respiratorie completate.',
                             'La produzione energetica cumulativa.', 'Una crescita maggiore indica più energia prodotta; confrontare con i costi per valutarne l’impiego.'),
        'respiration_heat': ('Calore dalla respirazione', 'Si somma il calore attribuito alle reazioni respiratorie completate.',
                             'La produzione cumulativa di calore metabolico, in unità del modello.',
                             'Valori maggiori indicano più calore prodotto; la temperatura corporea dipende anche dagli scambi con l’ambiente.'),
        'metabolism_cost': ('Costo del metabolismo', 'Si somma l’energia spesa per il mantenimento basale a ogni aggiornamento.',
                            'La spesa energetica di mantenimento.', 'Valori maggiori indicano maggiore spesa accumulata durante la vita.'),
        'movement_cost': ('Costo del movimento', 'Si sommano i costi delle azioni di movimento, incluse quelle bloccate.',
                          'La spesa energetica associata ai tentativi di spostamento.',
                          'Una crescita accompagnata da pochi movimenti riusciti indica energia spesa su azioni spesso bloccate.'),
        'other_action_cost': ('Costo delle altre azioni attive',
                              'Si sommano 0,01 unità per ogni tentativo di emissione, trasferimento, regolazione interna o attacco, anche senza effetto.',
                              'L’energia spesa per azioni attive diverse dal movimento e dal riscaldamento.',
                              'Una crescita senza risorse acquisite o effetti osservabili indica tentativi inefficaci ripetuti.'),
        'pending_action_cost': ('Costo di azione da regolare', 'Si legge il costo registrato dalle azioni e in attesa di addebito alla riserva.',
                                'L’energia già impegnata da azioni recenti.', 'Zero indica contabilità già regolata; valori positivi indicano un addebito ancora pendente.'),
        'reproduction_cost': ('Costo della riproduzione', 'Si somma l’energia dissipata come costo delle riproduzioni riuscite.',
                              'La spesa energetica necessaria per produrre figli.',
                              'La crescita segue le nascite riuscite. L’energia trasferita ai figli resta nelle loro riserve ed è contabilizzata nello stock dei vivi.'),
        'thermoregulation_cost': ('Costo della termoregolazione', 'Si somma l’energia consumata dall’azione che produce calore.',
                                  'La spesa energetica per il riscaldamento attivo.', 'Valori maggiori indicano un maggiore impiego di energia nell’azione termica.'),
        'energy_cap': ('Energia persa al limite', 'Si somma l’energia eccedente scartata quando la riserva raggiunge il massimo.',
                       'L’energia prodotta oltre la capacità di accumulo.', 'Zero indica assenza di scarti al cap; una crescita indica surplus energetico eliminato.'),
        'roundoff': ('Arrotondamento energetico', 'Per ogni variazione si accumula energia dopo − energia prima − variazione richiesta.',
                     'La correzione dovuta alla precisione numerica del calcolo.',
                     'Valori piccoli positivi o negativi sono arrotondamenti accumulati; il bilancio energetico li usa per chiudere i conti.'),
        'body_temperature': ('Temperatura corporea', 'Si legge la temperatura corporea dopo gli scambi termici e le reazioni.',
                              'Lo stato termico del corpo, in °C.', 'Confrontare con l’ottimo configurato; valori più distanti indicano condizioni termiche più lontane dall’ottimo.'),
        'abs_temp_delta': ('Scarto dall’ottimo termico', 'Si calcola |temperatura corporea − temperatura ottimale configurata|.',
                            'La distanza termica dall’ottimo, in °C.', 'Zero indica coincidenza con l’ottimo; 5 indica uno scarto di cinque gradi in una delle due direzioni.'),
        'repro_eligible_ticks': ('Tick riproduttivi idonei', 'Si conta un tick per ogni idoneità riproduttiva osservata.', 'L’esposizione alle opportunità riproduttive.', 'Confrontare ammissioni ed esclusioni a parità di esposizione.'),
        'repro_cap_ticks': ('Tick bloccati dal cap', 'Si contano i tick idonei con popolazione al cap.', 'Il blocco imposto dal limite globale.', 'Un rapporto alto rispetto ai tick idonei indica forte limitazione globale.'),
        'repro_queue_exclusions': ('Esclusioni dalla coda', 'Si contano i tick idonei sotto cap con coda piena e individuo ancora fuori coda.', 'La pressione sulla capacità di ammissione.', 'Valori elevati richiedono confronto con durata di vita e ammissioni.'),
        'repro_admissions': ('Ammissioni riproduttive', 'Si conta ogni inserimento nella FIFO.', 'Le opportunità di accedere al servizio riproduttivo.', 'Più ammissioni significano più accessi; i tentativi possono ancora fallire per spazio.'),
        'repro_wait_ticks': ('Attesa idonea corrente', 'Si accumulano tick idonei fuori coda, incluso il blocco al cap; si azzera all’ammissione.', 'L’attesa idonea dalla precedente ammissione.', 'Zero indica attesa azzerata; valori crescenti segnalano attesa ancora aperta.'),
        'repro_wait_max': ('Massima attesa idonea', 'Si conserva il massimo dei tick idonei fuori coda fra ammissioni, includendo l’attesa ancora aperta.', 'La maggiore attesa individuale osservata.', 'Valori alti indicano attese lunghe; la somma sui vivi aggrega massimi individuali.'),
        'learning_updates': ('Aggiornamenti dei pesi', 'Si conta ogni aggiornamento neurale applicato dalla nascita, includendo quelli con variazione nulla.',
                              'Quante volte è stata applicata la regola di apprendimento.', 'Valori maggiori indicano più aggiornamenti eseguiti; affiancare la variazione dei pesi per leggerne l’intensità.'),
        'learning_abs_weight_change': ('Variazione assoluta cumulativa dei pesi', 'Si sommano |peso dopo − peso prima| per tutte le connessioni e tutti gli aggiornamenti.',
                                       'La quantità complessiva di cambiamento dei pesi, inclusi decadimento e limiti.', 'Valori maggiori indicano più cambiamento accumulato; leggere insieme a durata di vita e numero di aggiornamenti.'),
        'learning_abs_plasticity': ('Plasticità proposta cumulativa', 'Si somma |learning_rate × segnale × traccia| per tutte le connessioni a ogni aggiornamento, prima di decadimento e limiti.',
                                    'L’intensità cumulativa delle variazioni proposte dal segnale di apprendimento.', 'Zero indica termine di plasticità nullo; valori maggiori indicano proposte di cambiamento complessivamente più ampie.'),
    }
    groups = {
        'active': ('Somma dei vivi', 'Si sommano i valori degli agenti vivi al campione.',
                   'complessiva degli agenti attualmente vivi',
                   'Nascite e morti cambiano il gruppo: la curva può scendere quando muoiono agenti con valori elevati.'),
        'averages': ('Media dei vivi', 'Si sommano i valori dei vivi e si divide per il numero dei vivi; con gruppo vuoto si pubblica zero.',
                     'media per agente attualmente vivo',
                     'Il confronto descrive il valore per individuo. Cambiamenti della popolazione possono spostare la media.'),
        'totals': ('Totale della run', 'Si sommano i valori attuali dei vivi e quelli registrati alla morte degli individui già deceduti.',
                   'aggregata dei vivi e delle vite concluse',
                   'Per i contatori cumulativi, una salita indica attività aggiuntiva e un plateau indica attività ferma. '
                   'Per energia, temperature e costi pendenti il totale combina valori attuali e terminali; usare le medie dei vivi per lo stato corrente individuale.'),
        'sample_means': ('Media nel tempo delle medie dei vivi', 'Si calcola la media aritmetica delle medie dei vivi raccolte nei campioni, assegnando uguale peso a ciascun campione.',
                          'media dei valori per agente osservati nel tempo',
                          'Riassume l’intera storia campionata e reagisce lentamente ai cambiamenti recenti; ogni campione pesa ugualmente, inclusi iniziale e finale.'),
    }
    for group, (scope, aggregation, meaning_scope, reading_scope) in groups.items():
        for key, (label, calculation, meaning, reading) in totals.items():
            add(f'{group}.{key}', f'{label} · {scope}', calculation + ' ' + aggregation,
                meaning + ' La curva ne mostra la misura ' + meaning_scope + '.', reading + ' ' + reading_scope)
    add('sample_means.corr_offspring_distance_alive', 'Media nel tempo della correlazione figli–distanza',
        'Si sommano i coefficienti figli–distanza dei campioni e si divide per il numero di campioni.',
        'L’associazione media osservata nei gruppi di vivi nel tempo.',
        'Valori positivi indicano prevalenza di associazioni crescenti e negativi decrescenti; il calcolo include gli zeri convenzionali dei campioni con varianza nulla.')

    counts = {
        'founders': ('Fondatori iniziali', 'individui introdotti all’avvio', 'La dimensione della popolazione iniziale'),
        'natural_births': ('Nascite naturali', 'figli prodotti da riproduzione con trasferimento delle risorse parentali', 'Il ricambio generato dalla riproduzione naturale'),
        'forced_births': ('Nascite forzate / ripopolamenti', 'individui introdotti come ripopolamento dopo estinzione', 'Il ricambio introdotto dal recupero automatico della popolazione'),
        'inherited_recovery_births': ('Ripopolamenti da riserva ereditaria', 'individui introdotti dopo estinzione usando genoma ereditabile e tratti di un donatore archiviato', 'La quota cumulativa dei ripopolamenti che prosegue informazione ereditaria preesistente'),
        'deaths': ('Morti totali', 'morti per tutte le cause, inclusi gli abbattimenti NewGen', 'La perdita cumulativa di individui'),
        'culled': ('Abbattimenti NewGen', 'morti prodotte dalla selezione gestionale NewGen', 'La quota delle morti attribuita ai cicli NewGen'),
        'death_cause_age': ('Morti per età', 'morti classificate per superamento del limite di età', 'La mortalità attribuita alla durata massima della vita'),
        'death_cause_energy': ('Morti per energia', 'morti classificate per esaurimento della riserva energetica', 'La mortalità attribuita alla carenza energetica'),
        'death_cause_temperature': ('Morti per temperatura', 'morti classificate per temperatura fuori dai limiti di sopravvivenza', 'La mortalità attribuita a condizioni termiche estreme'),
        'death_cause_external': ('Morti per cause esterne', 'morti classificate per cause esterne, inclusi abbattimenti e uccisioni', 'La mortalità attribuita ad azioni esterne sull’agente'),
        'death_cause_unknown': ('Morti con causa sconosciuta', 'eventi di morte privi di una causa riconosciuta', 'La parte della mortalità con causa ancora da identificare'),
    }
    for key, (label, counted, meaning) in counts.items():
        add('counts.' + key, label, 'Si contano gli ' + counted + '.' if key == 'founders' else 'Si accumula il numero di ' + counted + '.',
            meaning + '.', 'Il valore resta quello registrato alla fondazione.' if key == 'founders' else cumulative)

    for index, resource in enumerate(RESOURCES):
        add(f'world_resources.{index}', f'{resource} nelle celle', f'Si sommano le unità di {resource.lower()} presenti in tutte le celle.',
            'Lo stock ambientale disponibile nelle celle, espresso in unità del modello.',
            'Una discesa indica riduzione dello stock ambientale; una salita indica aumento. Gli scambi con gli agenti '
            'e le reazioni contribuiscono alla variazione. Per lo stock dell’intero sistema usare il bilancio delle risorse.')
        ledgers = [
            ('initial', 'Stock iniziale', 'Si sommano le risorse delle celle e degli agenti al campione iniziale.',
             'La dotazione iniziale dell’intero sistema.', 'Resta costante durante la run e serve come riferimento per il bilancio.'),
            ('final', 'Stock attuale', 'Si sommano le risorse delle celle e degli agenti vivi al campione.',
             'La quantità attualmente presente nell’intero sistema.', 'Una variazione riflette reazioni, rigenerazione, ingressi e perdite; gli scambi fra celle e agenti conservano questo totale.'),
            ('death_overflow', 'Perdita alla morte', 'Si sommano le riserve residue alla morte che eccedono la capacità di restituzione alle celle.',
             'La quantità cumulativa rimossa dal sistema alla morte degli agenti.', 'Zero indica restituzione completa; una crescita indica risorse perse perché lo spazio di deposito disponibile era insufficiente.'),
            ('residual', 'Residuo del bilancio', 'Stock attuale − stock iniziale − trasformazioni nette − ingressi da ripopolamento + perdite alla morte.',
             'Lo scarto fra quantità osservata e quantità prevista dalla contabilità.',
             'Zero indica bilancio chiuso; positivo indica quantità in eccesso rispetto ai conti e negativo una quantità in difetto. Le run anteriori a calcium_v3 includono qui anche le dotazioni dei ripopolamenti.'),
            ('recovery_input', 'Ingresso da ripopolamento', 'Si sommano le dotazioni dei nuovi agenti introdotti dal recupero dopo estinzione.',
             'L’apporto cumulativo di risorse dovuto ai ripopolamenti automatici.', 'Zero indica assenza di apporti; un gradino indica risorse aggiunte dal recupero della popolazione.'),
        ]
        for key, label, calculation, meaning, reading in ledgers:
            add(f'resource_ledger.{key}.{index}', f'{resource} · {label}', calculation, meaning, reading)
        for key, label, calculation, meaning in [
            ('acquired', 'Acquisizione totale', 'Si sommano i prelievi effettivi dalle celle eseguiti da vivi e morti.', 'La quantità complessivamente raccolta dagli agenti.'),
            ('released', 'Rilascio totale', 'Si sommano i rilasci effettivamente accettati dalle celle, incluso il ritorno alla morte, per vivi e morti.', 'La quantità complessivamente restituita all’ambiente.'),
            ('resources', 'Overflow dei morti', 'Si sommano le riserve terminali dei morti rimaste dopo la restituzione alle celle.', 'La quantità cumulativa persa alla morte per capacità di deposito insufficiente.'),
        ]:
            add(f'totals.{key}.{index}', f'{resource} · {label}', calculation, meaning,
                'Zero indica quantità cumulativa nulla; una crescita mostra ulteriore quantità trasferita o persa secondo la misura selezionata. Confrontare incrementi su uguali intervalli in tick.')
    add('resource_ledger.regenerated_glucose', 'Glucosio distribuito dalle Source',
        'Si sommano le unità depositate dalle Source nelle otto celle adiacenti o raggiunte dal flood e realmente accettate dalle celle.',
        'L’apporto cumulativo di glucosio del fronte mobile.',
        'Sale quando il fronte deposita glucosio; può crescere più lentamente se le celle visitate sono piene o sul bordo.')
    add('resource_ledger.regenerated_o2', 'Rigenerazione di ossigeno',
        'Si sommano gli incrementi di ossigeno realmente accettati dalle celle nella rigenerazione locale.',
        'L’apporto cumulativo di ossigeno dall’ambiente.',
        'Sale quando le celle ricevono ossigeno; può rallentare se sono già piene.')
    for key, label, calculation, meaning, reading in [
        ('initial', 'Energia iniziale', 'Si sommano le riserve energetiche dei fondatori.', 'La dotazione energetica iniziale della popolazione.', 'Resta costante e costituisce il riferimento del bilancio.'),
        ('final_alive', 'Energia nei vivi', 'Si sommano le riserve energetiche degli agenti vivi al campione.', 'L’energia attualmente conservata nella popolazione viva.', 'Una salita indica più energia accumulata; una discesa può derivare da costi o perdita di agenti. Confrontare con il numero dei vivi.'),
        ('dead_sink', 'Energia persa alla morte', 'Si somma l’energia terminale rimossa con ogni individuo morto.', 'L’energia cumulativa uscita dalla popolazione attraverso le morti.', 'Una salita indica morti con energia residua; un plateau indica assenza di nuovi apporti a questa perdita.'),
        ('residual', 'Residuo del bilancio energetico',
         'Energia nei vivi − energia iniziale − guadagni respiratori + costi metabolici, motori, riproduttivi e termici '
         '+ scarti al cap + energia dei morti − correzioni di arrotondamento − costi pendenti − energia da ripopolamento.',
         'Lo scarto fra energia osservata e ricostruita dai flussi.',
         'Circa zero indica bilancio chiuso entro la precisione numerica; valori positivi o negativi indicano rispettivamente eccesso o difetto rispetto ai conti. Prima di calcium_v3 il residuo includeva l’energia dei ripopolamenti.'),
        ('recovery_input', 'Energia da ripopolamento', 'Si sommano le riserve iniziali degli agenti introdotti dal recupero dopo estinzione.', 'L’energia cumulativa aggiunta attraverso i ripopolamenti.', 'Zero indica assenza di questo apporto; ogni gradino indica nuova energia introdotta dal recupero.'),
    ]:
        add('energy_ledger.' + key, label, calculation, meaning, reading)
    for index, action in enumerate(ACTIONS):
        add(f'totals.actions.{index}', f'Azione: {action}',
            'Si contano le richieste di questa azione dalla nascita di ogni agente e si sommano i contatori dei vivi e dei morti.',
            'La frequenza cumulativa con cui la popolazione ha richiesto questa azione.',
            cumulative + ' L’effetto ottenuto si legge nei contatori di risultato, come movimenti riusciti e risorse acquisite.')
    add('totals.actions.21', 'Azione: attendi (DO_NOTHING)',
        'Si contano le attese scelte dall’uscita 21 o assegnate quando le uscite sono neutre o a pari merito; si sommano vivi e morti.',
        'La frequenza cumulativa dei turni trascorsi in attesa mentre metabolismo e fisiologia proseguono.',
        cumulative + ' Confrontare con le altre azioni per valutare la prevalenza dell’attesa. Questa azione è operativa da neural_v4.')
    for key, label, calculation, meaning, reading in [
        ('count', 'Vite concluse', 'Si contano gli individui morti, inclusi gli abbattimenti.', 'Il numero di vite complete disponibili per il confronto.', cumulative),
        ('age_sum', 'Durata totale delle vite concluse', 'Si sommano le età alla morte di tutti gli individui deceduti.', 'Il tempo vissuto complessivamente dalle vite concluse, in tick.', 'Aumenta con ogni nuova morte; dividere per il numero di vite concluse per ottenere la durata media.'),
        ('mean_age', 'Durata media delle vite concluse', 'Somma delle età alla morte divisa per il numero dei morti; con zero morti si pubblica zero.', 'La durata media delle vite terminate, in tick.', 'Valori maggiori indicano vite concluse mediamente più lunghe. Moltiplicare per 0,01 per ottenere secondi simulati.'),
        ('mean_distance', 'Distanza media delle vite concluse', 'Somma dei percorsi alla morte divisa per il numero dei morti; con zero morti si pubblica zero.', 'Il percorso medio completo degli individui deceduti, in celle.', 'Valori maggiori indicano percorsi di vita più lunghi; leggere insieme all’età media per distinguere durata e intensità del movimento.'),
        ('mean_offspring', 'Figli medi delle vite concluse', 'Somma dei figli naturali prodotti dai morti divisa per il numero dei morti; con zero morti si pubblica zero.', 'La discendenza diretta media delle vite terminate.', 'Zero indica assenza di figli fra le vite concluse o assenza di vite concluse; valori maggiori indicano più figli per vita terminata. Il ricambio complessivo include anche i vivi ancora fertili.'),
    ]:
        add('completed_lives.' + key, label, calculation, meaning, reading)
    for key, label, calculation, meaning, reading in [
        ('acknowledged', 'Messaggi confermati', 'Si contano i messaggi confermati dal ricevitore dopo la persistenza dei dati.', 'L’avanzamento della consegna dei dati dal simulatore alla dashboard.', 'Una crescita indica nuovi messaggi acquisiti; confrontare con la coda per valutare lo smaltimento.'),
        ('dropped_messages', 'Messaggi persi', 'Si contano i messaggi scartati dal trasporto durante la vita del processo.', 'Le perdite registrate dalla telemetria.', 'Zero indica assenza di scarti registrati; una crescita segnala intervalli con dati mancanti.'),
        ('queued', 'Messaggi in attesa', 'Si legge la lunghezza della coda di trasmissione del simulatore.', 'La quantità di dati ancora in attesa di conferma.', 'Vicino a zero indica smaltimento rapido; una crescita persistente indica accumulo. A 128 messaggi il turbo attende per consentire lo smaltimento.'),
        ('connected', 'Telemetria connessa', 'Si legge lo stato della connessione del trasporto al ricevitore locale.', 'La disponibilità del collegamento telemetrico.', 'Vero indica collegamento attivo; falso indica collegamento assente.'),
    ]:
        add('transport.' + key, label, calculation, meaning, reading)
    return result


def browser_source():
    """Static definitions only: no simulation samples or statistical exports."""
    import json
    return '// Generated by python -m dashboard.metrics; edit dashboard/metrics.py.\n' + \
        'window.POPX_METRICS = ' + json.dumps(catalog(), ensure_ascii=False, indent=2) + ';\n'


if __name__ == '__main__':
    import argparse
    from pathlib import Path
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Check that the browser catalog matches the Python source')
    args = parser.parse_args()
    target = Path(__file__).parent / 'static' / 'metrics.js'
    source = browser_source()
    if args.check:
        if not target.exists() or target.read_text() != source:
            raise SystemExit('Regenerate dashboard/static/metrics.js with python -m dashboard.metrics')
    else:
        target.write_text(source)
