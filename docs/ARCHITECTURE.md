# Architettura del motore

Per la struttura aggiornata dell’editor 0.7.1, vedere EDITOR_ARCHITECTURE.md.

Il core è una libreria statica. `TwoDogCore` contiene loop, CLI e JSON; `TwoDogGameplay` contiene scena, input, fisica, progetti e World2D persistente; `TwoDogPlatform` contiene finestra, renderer, asset visuali, font, audio, project player e hosting C#. Runtime ed editor condividono questi moduli. `TwoDogEditorTools` e `TwoDogImGui` sono collegati solo a TwoDogEditor; il runtime non dipende da ImGui.

Il percorso precedente `Sandbox` resta separato ed è ancora il default senza `--project`. La nuova demo usa `ProjectPlayer`, carica `Project`, costruisce un `World2D` e un `SceneView`, poi inizializza gli script. Questo mantiene accessibile il riferimento della 0.2 durante le verifiche.

## Proprietà delle risorse

Window/OpenGL sopravvivono al renderer e ai suoi asset. Il project player distrugge gli script prima del mondo nativo, così OnDestroy può ancora usare l'API. Il renderer mantiene le texture; miniaudio mantiene gruppi e voci tramite oggetti non spostati in memoria. I contesti gameplay .NET sono collectible, mentre bootstrap e CoreCLR restano nel processo.

Gli handle `Scene` usano indice/generazione, i body includono il token del mondo. Gli ID stringa persistenti appartengono al livello `World2D` e sono gli unici handle esposti sul confine di scripting; non escono puntatori nativi.

## Rendering

Batch OpenGL 4.1, sorting stabile layer/depth, split per texture e capacità. Il renderer della 0.2 rimane alla base. SceneView aggiunge tilemap, animazioni a stati, camera follow/bounds/shake, parallax, particelle e testo. Il polling delle texture mantiene l'handle logico; i file invalidi non sostituiscono quelli già decodificati.

L'editor esegue import sincroni espliciti, gestisce scene e componenti e usa due framebuffer per Scene/Game. `PROJECT_FORMAT.md` specifica i formati Tiled e font effettivamente supportati.

## Scheduling e mutazioni

Il loop esegue zero o più fixed update, poi un update di frame e rendering interpolato. Box2D usa 60 Hz e 4 substep. L'input conserva gli edge tra frame privi di simulazione e li consuma una volta per passo. La minimizzazione azzera l'accumulatore del clock.

Le distruzioni richieste da C# vengono applicate alle barriere, fuori dal solver e fuori dall'iterazione managed. Il bootstrap riceve prima l'elenco delle istanze ancora valide ed esegue OnDestroy, poi il corpo viene eliminato. Le creazioni sono native e l'istanza managed si aggiunge alla barriera successiva. I messaggi prodotti da un callback vengono accodati, mai consegnati ricorsivamente.

I caricamenti di scena costruiscono un mondo e una vista candidati e preparano i tipi C# prima di distruggere la scena attiva. Il reload dei soli script prepara un nuovo contesto prima del commit. Questa transazionalità riguarda la preparazione; gli effetti arbitrari dei callback utente non possono essere annullati come una transazione di database.

## Binding

`Api.def` definisce nomi/opcode/schema. Il C++ lo include per generare l'enumerazione; PowerShell genera la superficie RawApi C#. Il trasporto JSON UTF-8 usa una callback Cdecl con contesto e buffer controllati; il motore rifiuta chiamate da thread estranei. `BatchRead` riduce i passaggi per interrogazioni multiple.

La superficie è predisposta per il generatore Python futuro; la parità tra linguaggi e la comunicazione mista non sono ancora implementate né verificate.

## Stato verifiche

La revisione del codice non sostituisce compilazione, test o misure. Nessuna verifica eseguibile è stata avviata qui per la 0.4. La checklist locale e le suite predisposte sono descritte in `VERIFICATION.md`.

## Editor e UI

La scena di authoring resta un documento JSON separato da World2D in Play. L'editor valida candidate World2D/SceneView e layout UI prima di sostituire l'anteprima; la cronologia conserva fino a 64 documenti. La modifica dei campi dell'Inspector richiede Apply. Play costruisce ProjectPlayer da una copia del documento; Stop lo distrugge, senza sovrascrivere la scena di authoring.

GLFW mantiene una sola finestra/context. ImGui concatena i callback della finestra esistente; il Game riceve coordinate locali del framebuffer e input solo quando la vista è attiva. La UI runtime usa GameUI/SpriteRenderer, filtra gli input del gameplay e accoda eventi sul bridge C# esistente. Il layout è risolto dai parent con controllo di cicli e profondità. VisualAssets rilascia le proprie texture quando viene sostituito.
