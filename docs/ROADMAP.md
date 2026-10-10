# Roadmap aggiornata — 0.7.2

1. Completata la separazione dei sei pannelli editor: Hierarchy, Inspector, Assets, Scene, Game e Console.
2. Introdotto EditorContext per lo stato condiviso e le risorse della sessione.
3. Centralizzate le modifiche di scena in SceneDocument/EditorActions, con selezione nella cronologia, Undo/Redo e stato modificato. Checklist: TEST_EDITOR_ARCHITECTURE.md; struttura: EDITOR_ARCHITECTURE.md.
4. Implementato nella 0.7.2: registro ComponentSchema condiviso, valori iniziali e validazione dei componenti separati dall’Inspector.
5. Implementato nella 0.7.2: Rigidbody2D, SpriteRenderer, UIText e GetComponent<T>/TryGetComponent<T>/RequireComponent<T>. Guida: TYPED_COMPONENTS.md; verifiche locali: TEST_COMPONENTS_CSHARP.md.
6. Successivamente: isolare ulteriormente preparazione della distribuzione e strumenti di sviluppo. Python rimane successivo a queste attività.
7. Le verifiche grafiche Windows e il collaudo con Visual Studio restano da effettuare localmente per questa patch.

Ordine aggiornato su richiesta dell'utente: editor e UI di gioco ora, Python e distribuzione dopo.

| Fase | Contenuto | Stato |
| --- | --- | --- |
| 0.1 | Finestra, OpenGL, loop fisso, CLI/build | Base storica |
| 0.2 | Sprite, atlas, input, fisica, playground | Verifiche Windows comunicate F |
| 0.3 A–D | Progetti, scene, prefab, moduli 2D, hosting/lifecycle C# e reload | Build Debug locale riuscita; 64/65 test nativi; checklist con gameplay confermato. Salto playground e suite C# automatica restavano aperti nel log |
| 0.4 A | Editor ImGui, Hierarchy/Inspector, Scene/Game, componenti e script | Avvio editor e creazione oggetti confermati dall’utente; altre verifiche non individualmente confermate |
| 0.4 B | Gizmo 2D, snap, asset/prefab, Undo/Redo, salvataggio, Play/Pause/Stop, console | Sorgenti implementati; da verificare |
| 0.4 C | Authoring animazioni tramite campi/stati/frame e pittura celle Tiled | Prima implementazione, con limiti documentati; da verificare |
| 0.4 D | Label/Button/Image/Panel/Slider/Layout, ancore/scala/input/eventi e C# | Sorgenti e EditorDemo implementati; da verificare |
| 0.5 A | Assets visuali, cartelle, miniature, ricerca, filtri e import nativo | Cartelle e albero confermati dall’utente; altri dettagli non individualmente confermati |
| 0.5 B | Creazione C#, drag & drop sugli oggetti e apertura Visual Studio | Apertura IDE confermata; problema della soluzione completa corretto nella 0.6 |
| 0.5 C | Compilazione automatica dopo Save, reload, errori e conservazione DLL valida | Implementato nei sorgenti, da verificare con checklist 0.5 |
| 0.6 | Hub per progetti vuoti, soluzione Visual Studio con cartelle, output build ordinati | Implementato; verifiche interne e checklist Windows in VERIFICATION.md/TEST_EDITOR.md |
| Successiva | Correzioni dai riscontri 0.6; rinomina/spostamento asset con riferimenti stabili, anteprime prefab e ulteriori strumenti di authoring | Da implementare |
| 0.7 | Build for Windows: pannello editor, player Release x64, C# ricompilato, runtime .NET privato, asset e licenze | Implementato; verifiche interne in VERIFICATION.md, collaudo Windows in TEST_EXPORT_WINDOWS.md |
| Successiva | Breakout creato nell'editor ed esportato, anche con script misti | Da implementare |
| Successiva | Python sul contratto API comune | Da implementare |
| Finale | Parità C#/Python, scene miste e suite condivisa, inclusa UI | Da implementare |

L'editor attuale offre gli strumenti di authoring documentati in `EDITOR.md`; non dichiara equivalenza con tutte le funzioni di Unity. Tilemap finite ortogonali e TTF mantengono il perimetro della 0.3. Timeline di animazione a nodi, parenting dei Transform2D fisici e UI con clipping/scroll non sono inclusi in questa consegna.

Dalla 0.6 l’utente autorizza i test interni. Esiti effettivi in `VERIFICATION.md`; usare `TEST_EDITOR.md` e `scripts/verify.ps1` per Windows. Le prove non compilate dall'utente nella precedente checklist non vengono trasformate automaticamente in risultati F. La Definition of Done finale rimane aperta.

## 0.7.3 — sviluppo e distribuzione

1. Implementata la separazione del servizio export dal pannello ImGui.
2. Aggiunti template runtime dedicati e percorsi distinti per cache, log e impostazioni.
3. Separati soluzione IDE .NET 8 e build runtime .NET 10; riferimenti alle API tipizzate aggiornabili.
4. Verifiche Windows interattive da effettuare secondo TEST_DISTRIBUTION_IDE.md.
