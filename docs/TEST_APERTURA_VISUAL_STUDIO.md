1. Chiudi 2Dog Editor, Hub e Visual Studio. Aggiorna i sorgenti con questo archivio e, dalla cartella che contiene `CMakeLists.txt`, esegui `.\scripts\build-editor.ps1 -Configuration Debug`. Risultato atteso: compilazione riuscita e launcher aggiornato anche in `build/bin/Debug/EditorSupport/open-script.ps1`.

2. Avvia `.\scripts\run-hub.ps1 -Configuration Debug` e apri il tuo progetto. Per controllare i percorsi con spazi, puoi creare un progetto di prova chiamato `Test apertura VS` in una cartella personale fuori da `build`.

3. In Assets crea `assets/Scripts/Cartella A/ScriptA.cs` e `assets/Scripts/Cartella B/ScriptB.cs`, usando Create C# Behaviour. Attendi la compilazione C# riuscita.

4. Con Visual Studio chiuso, fai doppio click su `ScriptA.cs`. Risultato atteso: si apre la soluzione `GameScripts.sln`, senza l'errore sui file della command line. In Esplora soluzioni devono comparire entrambi gli script con le rispettive cartelle, e `ScriptA.cs` deve essere aperto nell'editor. Il primo avvio puo richiedere alcuni secondi; completa eventuali finestre iniziali di Visual Studio.

5. Lascia Visual Studio aperto e fai doppio click su `ScriptB.cs` in 2Dog. Risultato atteso: lo script si apre nella stessa soluzione, senza creare un'altra finestra per quel progetto.

6. Apri una soluzione diversa in Visual Studio e poi fai doppio click su uno script di 2Dog. Risultato atteso: viene usata la finestra del progetto corretto oppure ne viene aperta una dedicata; la soluzione diversa non viene sostituita.

7. Con 2Dog e Visual Studio aperti, crea `assets/Scripts/Cartella C/ScriptC.cs` dall'editor. Risultato atteso: in Esplora soluzioni compare `GameScripts > assets > Scripts > Cartella C > ScriptC.cs`. Se Visual Studio chiede di ricaricare il progetto modificato esternamente, accetta. Gli altri script devono rimanere presenti.

8. In un progetto di prova, aggiungi da Esplora file un `.cs` con una classe dal nome univoco in `Logica/Extra.cs`, fuori da assets ma dentro il progetto del gioco. Poi spostalo in `Logica/Seconda/Extra.cs`. Risultato atteso: con 2Dog aperto, Esplora soluzioni aggiorna anche questo percorso. Le cartelle generate `.twodog`, `.vs`, `bin`, `obj` e `build` non devono aggiungere script alla soluzione.

9. Elimina soltanto lo script di prova del punto 8. Risultato atteso: scompare dal progetto C#; gli altri file rimangono. Modifica invece una riga di codice in ScriptA e salva: il progetto C# non deve essere rigenerato solo per questa modifica al contenuto.

10. Assegna uno script a un oggetto, salva la scena, modifica un messaggio `Engine.Log` in `OnCreate` e salva il file da Visual Studio. Risultato atteso: 2Dog compila automaticamente; premendo Play compare il messaggio aggiornato in Console.

11. Se vedi soltanto File vari, un progetto non caricato oppure la soluzione senza gli script, controlla gli errori di caricamento di `GameScripts.csproj`. Il progetto usa .NET 10: per il supporto completo serve Visual Studio 2026, versione 18.0 o successiva, con strumenti .NET. Il launcher ora segnala il mancato caricamento del file nel progetto anziche considerare riuscita l'apertura di un file esterno. Espandi il nodo GameScripts e rimuovi eventuali filtri di ricerca in Esplora soluzioni.

12. Se qualcosa fallisce, invia il numero del test, una schermata di Esplora soluzioni, la versione di Visual Studio e `<cartella del tuo gioco>/.twodog/ide.log`. Avvia editor e Visual Studio con lo stesso livello di privilegi. Non serve cancellare `.twodog`, asset o scene per applicare questa correzione.
