# Scripting C# — contratto 0.3

Stato: implementazione da verificare localmente. Windows x64, .NET 10, esecuzione in processo. Non viene ancora preparata una distribuzione standalone del runtime .NET.

## Workflow editor 0.5

Per creare script in Assets, aprirli in Visual Studio e compilarli automaticamente al salvataggio, seguire `EDITOR_SCRIPTING.md`. Il contratto e i callback descritti sotto restano validi. La build manuale di `managed/GameScripts` riguarda la demo precedente; non include gli script del singolo progetto in Assets.

## Struttura

- `Api.def`: nomi, codici numerici, schema di richiesta/risposta della superficie nativa. Unico contratto per i linguaggi.
- `tools/generate-bindings.ps1`: genera `RawApi.g.cs`; eseguito dal build C#.
- `TwoDog.API`: adattatore ABI, wrapper pratici `Engine`/`Entity`/`Behaviour` e tipi Vec2/Transform/Color.
- `TwoDog.ManagedHost`: bootstrap permanente che crea il contesto del gameplay.
- `GameScripts`: assembly della demo, caricato da byte per consentire la ricompilazione del file su disco.

Non modificare il file generato. Per cambiare il contratto aggiorna `Api.def`, il dispatcher C++ e i wrapper necessari; ricompila a motore chiuso. Il contratto ABI è versione 1. Python non è ancora implementato: la parità non è dichiarata verificata.

## Script e proprietà

Deriva da `TwoDog.Behaviour` e usa un costruttore pubblico senza argomenti. Il componente JSON sceglie il tipo completo:

```json
{"slot":"controller","language":"C#","type":"GameScripts.PlayerController","order":0,"properties":{"speed":4.5}}
```

`Entity` identifica l'entità proprietaria. `Properties` è una copia JSON delle proprietà esposte di quell'istanza. I campi privati C# e le modifiche a questa copia non sono salvataggi persistenti. Per stato condiviso usa componenti (`Entity.Component`) o messaggi (`Engine.Send/Broadcast`); nessuna chiamata diretta tra script è necessaria.

## Lifecycle e ordine

L'ordine è `(order, entity ID ordinale, slot ordinale)`, senza chiavi basate sul linguaggio. `OnCreate` precede gli aggiornamenti dell'istanza. L'ordine dei callback di una singola istanza è deterministico rispetto al loop nativo, non una promessa di determinismo numerico della fisica tra macchine diverse.

Nel loop:

1. Eventi finestra/input.
2. Zero o più passi fissi: applicazione delle richieste differite, `OnFixedUpdate(1/60)`, distruzioni/creazioni, Box2D, animazioni/camera/particelle, consegna messaggi/collisioni.
3. `OnUpdate(delta)` una volta per frame attivo, dopo i passi fissi; richieste differite.
4. Rendering interpolato e presentazione.

In pausa non vengono chiamati OnUpdate/OnFixedUpdate. N avanza esattamente un passo fisso e i sistemi che dipendono da quel passo. Stop esegue OnDestroy, interrompe audio, svuota richieste e messaggi e ricostruisce la scena di baseline. Play crea un nuovo contesto e nuove istanze.

Le istanze create da prefab entrano nel lifecycle alla barriera successiva. Destroy rende `Alive=false` immediatamente; OnDestroy viene invocato prima della rimozione fisica del corpo, e una sola volta. Le operazioni richieste da OnDestroy restano differite; non usare una catena infinita di creazioni/distruzioni nei callback.

## Input

`Engine.Input(action)` restituisce down/pressed/released. In OnFixedUpdate legge lo snapshot fisso: gli edge sono consumati una volta anche con più passi per frame. In OnUpdate legge gli edge del frame grafico. Non gestire la stessa azione distruttiva sia in OnUpdate sia in OnFixedUpdate se vuoi una sola attivazione.

`Engine.Axis(0)` è lo stick orizzontale. `Engine.Pointer` è in coordinate mondo con zoom, resize, snapping e shake della camera. I nomi `engine.*` sono riservati ai controlli del project player. I binding aggiunti da script sono azzerati su Stop/ripristino del progetto.

## Superficie API

| Gruppo | Operazioni |
| --- | --- |
| Console/entità | Log, Find, Alive, ListEntities, Create, Destroy, Instantiate |
| Componenti | GetTransform, SetTransform, GetComponent, SetComponent |
| Input | Input, Axis, Pointer, BindAction |
| Fisica | GetVelocity, SetVelocity, Impulse, Force, Raycast, AddCollider, CreateJoint, DestroyJoint |
| Audio | PlayAudio, StopAudio, Volume (`master`, `sfx`, `music`) |
| Scene | LoadScene, SaveScene |
| Comunicazione | Send, target entità o ID vuoto per broadcast |
| Visuale | Camera, Emit, SetState |
| Batching | BatchRead, fino a 128 richieste di sola lettura |

I metodi comuni hanno wrapper in `Engine.cs`. Ogni operazione è disponibile anche in `RawApi`, con un `JsonObject` che segue `Api.def`. `BatchRead` usa `{ "queries": [{ "op": 4, "request": {"entity":"player"} }] }`; gli opcode ammessi sono solo quelli di lettura elencati dal dispatcher. Serve a ridurre gli attraversamenti del confine tra linguaggi, senza risultati di benchmark già dichiarati.

SetComponent sostituisce `sprite`, `animation`, `text` o `properties`. Trasformazioni e fisica usano le rispettive API. Body e collider hanno dimensioni esplicite: cambiare la dimensione grafica non ridimensiona automaticamente un collider. Per una nuova composizione di scena usa Create/Destroy/prefab oppure LoadScene.

Raycast restituisce il corpo più vicino, salta i sensori e può ignorare un'entità (`ignore`). Gli eventi arrivano a entrambi i corpi come `collision.begin`, `collision.end`, `trigger.begin`, `trigger.end`, con `payload.other`. I callback sono per corpo: non espongono ancora l'ID del singolo collider. Se un corpo viene distrutto, non è garantito un evento end sintetico.

SaveScene scrive un nuovo file JSON relativo alla radice del progetto. Non può sovrascrivere il catalogo o un asset registrato. LoadScene accetta un asset ID di tipo scene e viene applicato alla barriera successiva; un errore conserva la scena attiva ed è riportato in console.

## ABI e errori

Il confine è Cdecl, JSON UTF-8 e buffer massimo di 1 MiB per richiesta/risposta. Le entità sono ID stringa, mai puntatori o numeri double. Una richiesta viene eseguita una volta: nessun retry automatico di operazioni con effetti collaterali se il buffer non basta.

Le API motore sono disponibili **solo sul thread principale**. Il binding rifiuta chiamate da thread secondari. Un'eccezione nativa diventa eccezione C#; un'eccezione non gestita da uno script viene riportata con lingua, entità, tipo e fase, disabilitando quell'istanza. OnDestroy riceve comunque un tentativo. Eccezioni catturate dallo script non lo disabilitano.

Il limite è 10.000 entità, 1.024 messaggi in attesa, 4.096 particelle e 64 voci audio. I limiti impediscono accumuli accidentali, ma non sono misure prestazionali della 0.3.

## Reload

Il bootstrap prepara assembly e tipi in un contesto collectible senza eseguire OnCreate. Se la preparazione fallisce, continua il contesto precedente. Il commit esegue OnDestroy delle vecchie istanze, rilascia i riferimenti del bootstrap e crea le nuove. La scena rimane attiva durante il reload degli script, salvo le modifiche richieste dai loro callback.

Il watcher aspetta due osservazioni stabili del timestamp DLL; il build pubblica la DLL gameplay per ultima. Un build C# fallito non sostituisce quella in esecuzione. F9 consente una riprova esplicita.

L'unload dei contesti .NET è **cooperativo**: thread, timer e riferimenti esterni lasciati dagli script possono trattenerli. OnDestroy deve chiudere le risorse proprie dello script. Il motore rilascia i propri riferimenti e segnala un accumulo di contesti; non forza la terminazione di thread e non riavvia CoreCLR a ogni Stop. Gli assembly di bootstrap/API restano caricati per tutta la vita del processo.

## Riferimenti tecnici usati

- https://learn.microsoft.com/en-us/dotnet/core/tutorials/netcore-hosting
- https://learn.microsoft.com/en-us/dotnet/standard/assembly/unloadability
- https://github.com/dotnet/runtime/blob/v10.0.0/src/native/corehost/hostfxr.h
- https://github.com/dotnet/runtime/blob/v10.0.0/src/native/corehost/coreclr_delegates.h

Questi riferimenti descrivono le API .NET; non certificano l'esito delle verifiche di 2Dog.
