1. A Hub, editor e runtime chiusi, aggiorna i sorgenti ed esegui `.\scripts\build-editor.ps1 -Configuration Debug`, poi `.\scripts\run-hub.ps1 -Configuration Debug`. Segna F se il test funziona, N in caso contrario.

2. H1: Avvia Hub, inserisci nome, Scegli cartella e Crea progetto. Risultato atteso: Nuova cartella sotto il percorso scelto; l’editor si apre sulla scena vuota.

3. H2: Controlla Hierarchy e Assets del nuovo progetto. Risultato atteso: Nessun oggetto demo; presenti Scenes/Main.scene.json e cartella Scripts.

4. H3: Crea un secondo progetto in un altro percorso. Risultato atteso: Usa lo stesso editor; scene, asset e script dei due progetti restano separati.

5. H4: Prova nome già esistente o nome riservato, come CON. Risultato atteso: Messaggio d’errore; nessun file preesistente sovrascritto.

6. H5: Salva una scena, chiudi editor e usa Apri da disco nell’Hub. Risultato atteso: Stessa scena e modifiche recuperate.

7. H6: Crea un progetto con spazi e caratteri accentati nel percorso. Risultato atteso: Creazione e apertura corrette, senza perdita del percorso Unicode.

8. V1: Crea ScriptA in assets/X e ScriptB in assets/Y. Risultato atteso: Entrambi visibili nelle rispettive cartelle in Assets.

9. V2: Chiudi Visual Studio e fai doppio clic su ScriptA. Risultato atteso: Si apre GameScripts.sln, con il progetto completo in Esplora soluzioni; A e B nelle cartelle assets/X e assets/Y.

10. V3: Con quella soluzione attiva, apri ScriptB dall’editor. Risultato atteso: Viene aperto il file della stessa soluzione; non compare tra i soli file vari/miscellaneous.

11. V4: Salva una modifica a uno script assegnato e avvia Play. Risultato atteso: Compilazione automatica e comportamento aggiornato; conserva il flusso 0.5.

12. V5: Apri uno script del secondo progetto con la prima soluzione aperta. Risultato atteso: La soluzione aperta deve appartenere al secondo progetto, senza confondere gli asset dei due.

13. B1: Aggiorna un checkout che aveva build/windows-csharp. Risultato atteso: Vecchi output preservati sotto build/legacy; nuovi eseguibili in build/bin/Debug.

14. B2: Ricompila due volte. Risultato atteso: Nessuna nuova cartella managed/<progetto>/bin o obj e nessun ulteriore backup a vuoto.

15. B3: Avvia anche con run-editor.ps1 e run-project.ps1. Risultato atteso: Entrambi trovano i nuovi eseguibili e le dipendenze.

16. Verifica il nuovo Hub e il tema dell’editor seguendo la lista in `TEST_HUB_EDITOR_STILE.md`; la cronologia dei recenti e persistente fra gli avvii.

17. Per il progetto C# usa Visual Studio 2026 (18.0+) con strumenti .NET. Controlla il caricamento di GameScripts nella checklist `TEST_APERTURA_VISUAL_STUDIO.md`. Il preset nativo C++ resta Visual Studio 2022.

18. A editor e runtime chiusi, esegui `.\scripts\verify.ps1 -Configuration Debug`. La suite contiene 80 test nativi (75 con -SkipGraphics), 15 controlli host C#, 3 controlli PowerShell e 5 verifiche simulate del launcher IDE e 7 controlli dell’export. I 6 casi workspace includono anche persistenza e rimozione non distruttiva dei progetti recenti.

19. Se un test fallisce, invia il numero, i passaggi e il primo errore completo. Per problemi grafici aggiungi screenshot, risoluzione e scala Windows; per Visual Studio aggiungi versione e `.twodog/ide.log`. I report automatici sono in `reports/local-...`.

20. Verifica creazione/rimozione componenti, Undo/Redo, Save e riapertura; prova Play/Pause/Step/Stop e verifica il ripristino della scena iniziale.

21. Verifica input tastiera/gamepad, movimento e salto, collisioni e trigger, raycast, prefab, audio, cambio scena e messaggi C#. In pausa un singolo Step deve avanzare un solo passo fisico.

22. Modifica uno script valido e verifica il reload; introduci poi un errore C#: la sessione avviata conserva la DLL valida e la console segnala il problema. Ripristina lo script prima della prossima prova.

23. Verifica cambio focus, minimizzazione, zoom e selezione dopo ridimensionamento; i tasti non devono rimanere bloccati e il click deve rimanere allineato.

24. Esegui la checklist numerata TEST_EXPORT_WINDOWS.md per il pacchetto distribuibile. La compilazione interna non sostituisce il collaudo su un PC Windows pulito.
