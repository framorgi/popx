# Baseline — sandbox UX, 28 ottobre 2026

Audit prima delle modifiche, commit `4461095057c16401e8d8f1db072bc697c51a91ba`.
Build release Clang 18, C++23, target esplicito popx: PASS (nessuna ricompilazione necessaria).
INI attiva incorporata nella run da checkpoint; 180×180, cap 460, 57/8×5/22 neuroni,
learning 0.06, traccia 0.97, baseline 0.01, decay 0.00003; ereditarietà appresa attiva.
RNG produttive non centralizzate: questi campioni non costituiscono un confronto deterministico.

## Run reale

Run `1793188072303733755-2377286159`, campione tick 6.647.600:
460 vivi, 163.680 nascite naturali e altrettante morti (588 abbattimenti inclusi),
zero nascite di recupero. NewGen 15 indica interventi nella run: la persistenza
non dimostra da sola adattamento spontaneo. Cause: 113.535 energia, 49.081 temperatura,
476 età, 588 esterne. Movimenti cumulativi: 287.962.658 richieste, 9.388.880 riusciti
(3,26%), 229.288.228 occupazione (79,62%), 47.035.902 dislivello (16,33%).
Idoneità riproduttiva bloccata dal cap: 1.191.422.483 / 1.221.827.678 agente×tick (97,51%).
Questo descrive un sistema affollato con ricambio, non una prova di apprendimento direzionale.
Bilancio risorse residuo nullo; energia residuo −1,56e−6. DB circa 2,77 GB, cap 5 GB.

Learning dei vivi: segnale medio −0,00112, deviazione 0,02570, modulo medio 0,00926;
solo 1,74% quasi nullo. Plasticità media assoluta 1,88e−6, decadimento 1,26e−6,
pesi medi assoluti 0,0420, frazione zero 0. Il segnale medio piccolo è compatibile
con la baseline mobile; non equivale a rete spenta. Le medie non dimostrano che gli
aggiornamenti migliorino la sopravvivenza. Nessuna modifica ecologica automatica prevista.

## Architettura e learning

PopXApp possiede mondo, manager, simulator, Stats e GUI opzionale. Scheduler a passo
biologico fisso 10 ms, sense/think paralleli con barriera, act/mondo sequenziali.
GUI SFML/ImGui nello stesso thread; comandi GUI/UDP centralizzati fra tick.
Renderer già usa batch di vertici; dashboard FastAPI/SQLite/Chart.js/Cytoscape.
Stats copia i dati al worker; Python è unico scrittore. Checkpoint CBOR completi.

Scorer combina età (0,10), riserva energetica (0,30), variazione energia (0,35),
figli (0,05), idoneità termica gaussiana (0,20), clampa reward in [−1,1].
Segnale = reward − baseline precedente, poi EMA con coefficiente 0,01. Tracce
locali di eleggibilità e plasticità modulata dal segnale, senza backprop/Q-learning.
Decadimento limitato inferiormente; pesi appresi ereditabili, baseline/tracce no.
Selezione NewGen è figli + 0,01 energia: distinta dalla reward.

## Prestazioni e UX osservate

Campione status: tick medio cumulativo 2.839 us, frame 31.006 us, 4,04 FPS.
Processo ~190% CPU e 300 MiB RSS; backend ~82 MiB. Non sono percentili né un benchmark
isolato. Il throttling frame usa CPU di processo inclusi thread grafici e mira al 20%
di un core: su questo display software riduce molto la reattività.
Screenshot reale `data_out/audit/ux/before-gui.png` (locale, escluso da Git): finestra
1908×999, metà larghezza sprecata nella sidebar, controlli inferiori fuori vista,
heatmap arcobaleno molto satura, dettagli minuscoli, nessuna selezione/zoom/pan.
Cattura root X11 fallisce in WSL; cattura finestra tramite XGetImage riuscita.
Browser e Playwright non presenti: screenshot dashboard iniziale non disponibile.

Bug grafici: triangolo fotosintetico emesso tre volte; centri risorse/feromoni sul
vertice inferiore destro della cella; overlay non confrontabili per scala visiva.
Palette GUI e web hardcoded, inspector solo web; nessun mixer o editor runtime.

## Piano operativo

1. Token condivisi, preset persistenti e resa del mondo semplificata a basso costo.
2. Camera, selezione, follow, pannelli compatti e controlli a tick esatti.
3. API runtime validate e tracciate, conservate nei checkpoint; mixer con formula
   autorevole C++, senza nuovi premi arbitrari.
4. Inspector C++ basato sulla telemetria esistente, monitor neurale campionato,
   genoma navigabile; pagina sandbox web con medesimo contratto.
5. Regressioni, checkpoint, riavvio conservativo, verifica visiva e misure finali.
