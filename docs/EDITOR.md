# Editor 0.7 — guida rapida

Questa versione amplia l’authoring Dear ImGui con Assets visuali e workflow C# automatico. Da questa versione l’utente ha autorizzato i test interni. Consulta VERIFICATION.md per gli esiti Linux e TEST_EDITOR.md per la prova dell’Hub e di Visual Studio su Windows.

## Compilazione e avvio

Chiudi ogni processo 2Dog prima di aggiornare i sorgenti. La vecchia build è archiviata e i nuovi output sono organizzati come descritto in BUILD_LAYOUT.md.

```powershell
.\scripts\build-editor.ps1 -Configuration Debug
.\scripts\run-editor.ps1
```

Il primo comando configura CMake e compila con parallelismo 2, senza test. Al primo aggiornamento scarica Dear ImGui 1.91.9b docking. Il secondo apre `samples/EditorDemo`, una copia separata della demo con UI, preservando `samples/CSharpDemo`.

Per un altro progetto: `scripts/run-editor.ps1 -Project C:\Percorso\Progetto`. `-NativeOnly` consente Play senza C#. L'EXE senza argomenti apre la copia di EditorDemo accanto all'eseguibile; lo script apre i sorgenti, quindi i salvataggi sono permanenti nel progetto sorgente.

Per creare un progetto vuoto in un percorso personale usa `scripts/run-hub.ps1`, dopo la build; vedi `HUB.md`. Il motore è condiviso tra i progetti.

## Pannelli

- **Hierarchy**: elenco oggetti, ricerca per nome, selezione, duplicazione e cancellazione con conferma. Gli oggetti UI con parent sono indentati. I parent UI definiscono il layout, non una gerarchia di trasformazioni fisiche.
- **Inspector**: campi tipizzati, colori, vettori, liste, componenti e proprietà C#. `Apply components` valida l'intero risultato prima di sostituire l'anteprima; `Revert fields` scarta i campi in corso. Cambiare selezione scarta campi non applicati. Nome e ID sono distinti: gli ID persistenti non sono modificabili.
- **Scene**: selezione con clic, trascinamento per Move/Rotate/Scale, rotella per zoom centrato al puntatore, trascinamento del tasto centrale per pan, griglia e snap. W/E/R scelgono lo strumento mentre il puntatore è nella vista. Rotazione in radianti, snap rotazione di 15 gradi. `Frame selected` centra l'oggetto. `UI overlay` consente di vedere/selezionare la UI e trascinarne gli offset; dimensioni e ancore si modificano nell'Inspector.
- **Game**: immagine del runtime in un framebuffer separato. Fare clic nella vista e mantenere il puntatore al suo interno per inviare input al gioco. Spostarsi verso un pannello rilascia i controlli del gioco.
- **Assets**: asset registrati, miniature texture, assegnazione sprite, apertura scene, scelta della scena di avvio, istanziazione prefab, import di file ed esportazione dell'oggetto selezionato come nuovo prefab.
- **Scene Settings**: gravity, camera, bounds/follow, tilemap e giunti. Apply applica queste impostazioni, preservando le entità. Reload fields riparte dalla scena attuale.
- **Tilemap**: apertura con doppio clic sulla tilemap in Assets, scelta layer/GID, pittura trascinando sulle celle. GID 0 cancella. Save valida la scena dopo la scrittura e ripristina il file precedente se il caricamento fallisce. Limite dell'interfaccia: mappe finite ortogonali fino a 128×128; non è un sostituto completo di Tiled.
- **Console**: log nativi e C# inviati dal bridge, errori evidenziati; 1000 righe per flusso, cancellabili. Non intercetta output arbitrario scritto direttamente da DLL o thread esterni al bridge.

I pannelli si possono spostare e agganciare dentro la finestra. `Reset layout` ripristina la disposizione iniziale. Il layout non viene salvato tra gli avvii; non sono abilitate finestre native multiple.

## Oggetti e componenti

`Create` aggiunge Empty, Sprite oppure un elemento UI. `Add Component` consente SpriteRenderer, Rigidbody2D con collider iniziale, altri collider Box/Circle/Polygon, Animator, World Text, Properties e UI. I collider appartengono al body, sono multipli e hanno offset/materiale/sensor. Sono espliciti: ridimensionare lo sprite non ridimensiona automaticamente i collider.

`Attach C# script` aggiunge uno slot con tipo completo, ordine e proprietà. `...` propone i tipi dichiarati in `project.json/scriptTypes`; si può digitare un altro tipo esistente nella DLL. Non è un browser a riflessione degli assembly. Un tipo inesistente viene rifiutato durante Play. Puoi anche creare un Behaviour in Assets e trascinarlo sulla riga di un oggetto in Hierarchy o sul riquadro dedicato nell’Inspector. Questa assegnazione è immediata e supporta Undo/Redo; Ctrl+S la salva nella scena. Doppio clic sul file apre Visual Studio. Il salvataggio degli script sotto `assets` avvia automaticamente la compilazione. Guida: `EDITOR_SCRIPTING.md`.

Per le animazioni: aggiungi Animator e SpriteRenderer, imposta lo stato iniziale, FPS/loop e rettangoli UV dei frame. Puoi duplicare/rimuovere frame e aggiungere stati con `Add animation state`. Play mostra l'animazione nel Game. Non c'è ancora una timeline grafica o un editor di transizioni a nodi.

Gli asset importati devono avere ID e destinazione nuovi. Sono copiati all'interno del progetto; non vengono sovrascritti file esistenti. L'import supporta i formati già caricati dal motore. Per asset Tiled con immagini/tileset esterni, importare e registrare anche le risorse referenziate mantenendo i percorsi relativi.

## Salvataggio e Play

Ctrl+S / File → Save salva la scena corrente. File → Save scene as new asset crea un nuovo file e lo registra nel catalogo. Le scene registrate si riaprono da Assets; `Use as startup scene` sceglie quella avviata dal runtime. La scrittura JSON usa il salvataggio sostitutivo già presente nel motore.

Undo/Redo (Ctrl+Z/Ctrl+Y) conserva fino a 64 stati della scena: componenti applicati, trasformazioni concluse, creazioni e cancellazioni. Importazioni, scrittura prefab, modifiche del catalogo e salvataggi tilemap non entrano in questa cronologia; Tilemap ha Revert prima del salvataggio. I cambiamenti non applicati nell'Inspector non sono ancora modifiche della scena.

Play/F5 crea un runtime separato dalla scena di authoring. L'Inspector resta bloccato durante Play. Pause/Resume e Step controllano la simulazione; Step è abilitato solo in pausa. Stop distrugge mondo/script/audio della simulazione e conserva la scena dell'editor; le variazioni durante Play non vengono salvate. Le modifiche degli script possono essere ricaricate durante Play con `build-csharp.ps1`; per cambiare l'API/host chiudere il motore.

Le scene con modifiche applicate e non salvate mostrano una conferma prima di cambiare scena, ricaricare o chiudere la finestra. Escape non chiude l'editor, così è utilizzabile nei campi e nei popup.

1. Per creare una distribuzione Windows usa **File > Build for Windows**. La guida e i test sono in `TEST_EXPORT_WINDOWS.md`.
