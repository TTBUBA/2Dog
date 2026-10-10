1. Chiudi Hub, editor e runtime. Nella cartella sorgenti esegui `.\scripts\build-editor.ps1 -Configuration Release`. Questo prepara il player e il bootstrap C# Release: non serve ricompilare il C++ per ogni progetto esportato. Ripeti questo passo quando aggiorni i sorgenti del motore.

2. Avvia `.\scripts\run-hub.ps1 -Configuration Release`, apri il tuo progetto e seleziona **File > Build for Windows**. Puoi usare anche l’editor Debug, purché il campo **Release runtime folder** punti alla build Release aggiornata. Windows x64, toolchain C++ e SDK .NET 10 x64 servono sul PC di sviluppo.

3. Imposta **Executable name** (lettere A-Z, numeri, spazi, trattino o underscore), **Output folder (new)** e **Start scene**. Il pulsante di scelta seleziona una cartella genitore e propone una nuova sottocartella. La destinazione deve essere esterna al progetto e al runtime dell’engine, con genitore esistente. Una cartella già esistente viene rifiutata, anche se vuota.

4. Premi **Build**. La scena aperta viene salvata prima dell’export; applica prima eventuali modifiche ancora in bozza nell’Inspector. Attendi il messaggio Build completed. Il pannello deve restare utilizzabile e il log aggiornarsi. Un export già avviato termina anche se chiudi l’editor: prima di modificare di nuovo i sorgenti attendi la fine del processo PowerShell e controlla il log.

5. Controlla il risultato: `<Nome>.exe`, `data/project.json`, gli asset registrati, `managed` con bootstrap/API e `game/GameScripts.dll`, `dotnet` con runtime privato, `licenses`, `build-settings.json`, `manifest.json` e README. Non devono esserci Hub, editor, SDK, soluzione Visual Studio, sorgenti C# o cache del progetto. L’header miniaudio in licenses è intenzionale: conserva i notice delle dipendenze incorporate.

6. Tutti gli asset registrati e tutte le scene registrate vengono inclusi, non soltanto la scena iniziale. Le tilemap includono anche tileset JSON esterni e immagini da essi referenziate. I file non registrati letti direttamente dal tuo codice tramite System.IO non sono automaticamente inclusi: in questa versione vanno distribuiti a parte. Non sono implementati selezione scene da escludere, stripping o pacchetti cifrati.

7. Esegui l’EXE con doppio clic. Deve avviarsi la scena selezionata, senza Hub o editor. Non basta distribuire il solo EXE: sposta o comprimi in ZIP l’intera cartella. Questa versione produce una cartella portabile, non un installer; non include firma digitale o icona personalizzata.

8. Ripeti con **Build and Run** e una nuova destinazione. Il gioco deve partire soltanto dopo il successo. Prova il lancio da una directory di lavoro diversa: i percorsi devono essere risolti rispetto all’EXE. Mantieni i file esportati in una cartella scrivibile se usi SceneSave; non è ancora implementata la migrazione automatica dei salvataggi in AppData.

9. Verifica uno script C# osservabile (movimento, contatore o log), input tastiera/gamepad, gravità, collisioni, trigger, audio e UI. La compilazione degli script viene rifatta in Release dai sorgenti del progetto, contro la API del player. Gli script della demo vengono inclusi secondo includeDemoScripts, come nell’editor. I riferimenti NuGet o csproj personalizzati non sono supportati dall’export di questa versione.

10. Esporta un progetto con percorso contenente spazi, parentesi, parentesi quadre e caratteri accentati. Esporta anche in una destinazione con questi caratteri. Avvio, asset e script devono funzionare. Per il nome dell’EXE sono invece ammessi solo i caratteri indicati al punto 3.

11. Introduci un errore di sintassi in uno script salvato e premi Build verso una nuova cartella. Atteso: errore C# nel log, nessun pacchetto finale; nessun riutilizzo silenzioso della vecchia DLL. Ripristina lo script. Le build precedenti devono essere intatte.

12. Prova una destinazione già esistente, una dentro al progetto, un asset mancante e un percorso asset che esce dalla radice. Atteso: export rifiutato, senza sovrascritture. Link simbolici/junction non sono supportati per asset, radice o destinazione; i link nelle sorgenti C# vengono ignorati come nell’editor.

13. Cambia scena iniziale nel pannello ed esporta. Il gioco deve usare la nuova selezione; project.json del progetto originale deve conservare la sua startScene. Nome EXE, percorsi e selezione vengono conservati in `.twodog/export-settings.json`.

14. Esporta una scena con tilemap, tileset JSON esterno e immagine non registrata separatamente: la mappa deve apparire anche dopo aver spostato il pacchetto. Se usi un’immagine o tileset fuori dalla radice del progetto l’export deve fallire.

15. Copia l’intera cartella su un PC/VM Windows x64 senza .NET, SDK o Visual Studio. Avvia il gioco: C# deve funzionare usando il runtime incluso. Sono necessari sistema operativo supportato da .NET 10 e driver OpenGL 4.1. Questa prova resta obbligatoria: i controlli interni non certificano una macchina Windows pulita.

16. Per esportare da PowerShell: `.\scripts\export-windows.ps1 -Project 'C:\Progetti\MioGioco' -Output 'C:\Builds\MioGioco-01' -Name 'MioGioco' -StartScene 'scene.main' -Run`. StartScene è opzionale; senza usa quella del progetto. RuntimeDirectory è opzionale quando lo script viene eseguito dalla cartella scripts dei sorgenti. Il campo del pannello è necessario se usi una disposizione diversa degli eseguibili.

17. Esegui `.\scripts\check-export-tools.ps1` per i sette controlli mirati sui percorsi, copie, esclusione cache, XML/MSBuild, architettura e UTF-8. Per tutte le verifiche del motore usa `.\scripts\verify.ps1 -Configuration Release` (oppure Debug/Both). Non vengono precompilati risultati F per le prove manuali.

18. I log sono in `<progetto>/.twodog/exports`; **Open logs** apre la cartella, **Open output folder** apre la build. Le cartelle temporanee vengono rimosse quando l’export termina normalmente o con errore gestito. Un arresto forzato di Windows/PowerShell può lasciare una `.2dog-export-*` vicino alla destinazione: rimuovila solo dopo aver verificato che non ci siano export attivi.

19. Segna F/N per ciascuna prova e invia il numero fallito insieme al primo errore e al log. Prima di distribuire un aggiornamento del runtime, ricompila il motore in Release ed esporta nuovamente; il pacchetto copia la versione stabile .NET 10 più recente presente nell’installazione x64 usata per la build.
