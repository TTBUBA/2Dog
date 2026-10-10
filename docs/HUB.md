1. Dalla cartella dei sorgenti esegui `.\scripts\build-editor.ps1 -Configuration Debug`, poi `.\scripts\run-hub.ps1 -Configuration Debug`. Puoi anche avviare `build/bin/Debug/TwoDogHub.exe`.

2. Per creare un gioco, inserisci il nome nel pannello a sinistra, scegli una cartella padre esistente e premi **Crea progetto**. La destinazione deve essere nuova. Il progetto nasce vuoto e l'editor si apre automaticamente.

3. Per recuperare un progetto esistente usa **Apri da disco...** e seleziona il suo `project.json`. I progetti personali vanno conservati fuori dalla cartella `build`.

4. Ogni progetto avviato dall'Hub entra nei **Recenti**, con nome, percorso e data. L'ultimo usato appare per primo; la lista conserva al massimo 20 progetti e non duplica lo stesso percorso.

5. Seleziona una voce e premi **Apri progetto**, oppure usa doppio clic o Invio sulla lista. Mantieni una sola istanza dell'editor per ogni progetto, per evitare scritture contemporanee.

6. **Rimuovi dalla lista** elimina soltanto il collegamento dalla cronologia. Non cancella il progetto o i suoi file. I percorsi non disponibili restano riconoscibili; puoi ritrovarli con Apri da disco.

7. Chiudi e riapri l'Hub per verificare che i recenti siano ancora presenti. La cronologia e salvata per l'utente Windows in `%LOCALAPPDATA%\2Dog\Hub\recent-projects.json`, separatamente dai giochi e dalle build. I progetti usati prima di questo aggiornamento compaiono dopo la prima apertura dal nuovo Hub.

8. Conserva `TwoDogHub.exe`, `TwoDogEditor.exe` e i relativi file di supporto nella stessa cartella di build. L'Hub usa l'engine accanto a se; non scarica ne gestisce versioni multiple. Chiudere l'Hub non chiude l'editor.
