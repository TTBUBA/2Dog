1. Chiudi Hub ed editor, aggiorna i sorgenti e compila con `.\scripts\build-editor.ps1 -Configuration Debug`. Avvia `.\scripts\run-hub.ps1 -Configuration Debug`. Risultato atteso: Hub scuro con pannello di creazione a sinistra e lista Recenti a destra.

2. Crea un progetto di prova scegliendo nome e cartella, anche con spazi e parentesi. Risultato atteso: si apre l'editor e il progetto compare nei Recenti con nome, percorso e data.

3. Chiudi l'editor. Seleziona il progetto nei Recenti e premi Apri progetto. Ripeti con doppio clic e con Invio sulla lista. Risultato atteso: ogni metodo apre il progetto scelto. Chiudi ogni istanza prima della prova successiva.

4. Apri un secondo progetto con Apri da disco, poi riapri il primo. Risultato atteso: il primo torna in cima senza creare duplicati. Chiudi e riapri l'Hub: ordine e voci devono restare salvati.

5. Premi Rimuovi dalla lista su un progetto di prova. Risultato atteso: la voce scompare ma la cartella e tutti i suoi file rimangono sul disco. Riaprilo con Apri da disco per aggiungerlo di nuovo.

6. A editor chiuso, sposta temporaneamente la cartella di un progetto di prova, poi riapri l'Hub. Risultato atteso: la voce indica Non disponibile e Apri progetto e disabilitato. Rimuovi la voce oppure apri il project.json dal nuovo percorso.

7. Ridimensiona l'Hub e prova la scala schermo Windows al 100% e al 150%, riavviando l'Hub dopo il cambio. Risultato atteso: pulsanti, campi e lista sono leggibili e utilizzabili; i percorsi lunghi sono abbreviati nella riga. Prova Tab per raggiungere i controlli.

8. Nell'editor controlla il nuovo tema: pannelli scuri, selezioni azzurre, campi piu distanziati, testo leggibile, pulsante Play evidenziato. La barra mostra EDITING, PLAYING o PAUSED e segnala le modifiche non salvate.

9. Crea un oggetto, modifica un componente, premi Apply e salva. Prova selezione, Undo/Redo e ridimensionamento dei pannelli. Risultato atteso: il nuovo stile non impedisce le operazioni di authoring.

10. Prova Play, Pause, Step e Stop. Poi crea uno script, aprilo in Visual Studio e salva una modifica. Risultato atteso: anteprima e compilazione C# mantengono il comportamento precedente.

11. Se trovi testo tagliato, sovrapposizioni o un controllo poco leggibile, inviami il numero del test, uno screenshot, la risoluzione dello schermo e la percentuale di scala Windows.
