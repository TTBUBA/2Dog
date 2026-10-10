# Aggiornamento 0.7.3

1. Per i percorsi attuali e la separazione IDE/runtime/export consulta [DEVELOPMENT_DISTRIBUTION.md](DEVELOPMENT_DISTRIBUTION.md).
2. Per installazione e verifiche segui [TEST_DISTRIBUTION_IDE.md](TEST_DISTRIBUTION_IDE.md); le descrizioni storiche sottostanti vanno lette con questi aggiornamenti.

# C# nell’editor 0.6

## Dal file al GameObject

1. Avvia con `scripts/run-editor.ps1`. Al primo avvio l’editor prepara il progetto C# e avvia una compilazione in background. Attendi `C# ready` o `C# compiled` in Assets.
2. In **Assets**, seleziona `assets`. Usa **Create → Create folder** per creare, per esempio, `Scripts`. Puoi creare altre sottocartelle e navigare con albero, doppio clic e percorso in alto.
3. **Create → C# class name**: inserisci `MyBehaviour`, quindi **Create C# Behaviour**. Il file reale è `assets/Scripts/MyBehaviour.cs`, visibile nella griglia con icona C#. Il nome deve iniziare con una maiuscola, senza spazi o estensione.
4. Crea o seleziona un oggetto in Hierarchy. Trascina lo script sulla riga dell’oggetto, oppure sul riquadro **Drop a C# script here** nell’Inspector. In alternativa seleziona il file e premi **Attach to selected object**. L’assegnazione è immediata; Ctrl+Z/Ctrl+Y la annulla/ripristina. Ctrl+S salva la scena.
5. Doppio clic sullo script, oppure **Open in Visual Studio**. Si apre esplicitamente la soluzione C# del progetto e il file selezionato. Esplora soluzioni mostra tutti gli script con i percorsi `assets/...`, anche quelli nelle altre cartelle; i file generati della cache sono esclusi dalla vista. L’editor cerca prima Visual Studio Community mediante `vswhere`, poi altre edizioni complete installate.
6. Modifica il codice e salva in Visual Studio con Ctrl+S. L’editor rileva il cambiamento, attende circa 0,8 secondi senza ulteriori modifiche e compila. Non serve premere Build in Visual Studio o eseguire PowerShell per ogni modifica.
7. Premi **Play** dopo una compilazione riuscita. Lo script assegnato riceve `OnCreate`, poi `OnUpdate`/`OnFixedUpdate`. Se salvi durante Play, una nuova compilazione riuscita provoca il reload automatico, senza riavviare l’editor.

Un file non assegnato a un oggetto viene compilato ma non esegue callback. Quando Play è fermo, Save compila soltanto: il gameplay non parte da solo. La finestra Assets mostra lo stato della compilazione; la Console mostra il relativo output al termine.

## Esempio della condizione richiesta

Crea `MyBehaviour` dall’editor, assegnalo a un oggetto e sostituisci il contenuto con:

```csharp
using TwoDog;

namespace GameScripts;

public sealed class MyBehaviour : Behaviour
{
    public override void OnCreate()
    {
        int a = 5;
        int b = 3;
        if (a > b)
        {
            Engine.Log("CONDIZIONE V1: a è maggiore di b");
        }
    }
}
```

Salva, attendi la compilazione e premi Play: la Console deve mostrare il messaggio una volta per l’istanza. Durante Play cambia `V1` in `V2` e salva: dopo compilazione e reload compare il nuovo messaggio perché `OnCreate` viene richiamato sulle nuove istanze. Per logica ripetuta usa `OnUpdate(float deltaTime)`; per la fisica usa `OnFixedUpdate(float fixedDeltaTime)`. Evita log a ogni frame se non necessari.

## Errori e reload

- Il compilatore lavora in un processo separato; una sola compilazione dell’editor alla volta. Il rendering continua. Il rilevamento prosegue anche quando la finestra è minimizzata.
- Più salvataggi ravvicinati vengono raggruppati. Se salvi mentre il build è in corso, il risultato superato non viene pubblicato: l’editor ricompila gli ultimi sorgenti.
- Un errore di sintassi lascia intatta la DLL runtime precedente. Il Play già avviato continua con il vecchio codice; correggi e salva per riprovare automaticamente. Un nuovo Play viene bloccato finché i sorgenti non compilano.
- **Compile C#** forza un nuovo tentativo, utile se hai installato l’SDK dopo un errore senza modificare i file. Il normale flusso non richiede questo pulsante.
- Una compilazione riuscita non garantisce che lo script sia valido per il motore: tipo rinominato ma ancora referenziato dalla scena, costruttore non valido e altri errori di caricamento sono riportati dal bridge. Correggi il tipo nel componente o riassocia lo script. Un errore nel codice di un callback è distinto da un errore di compilazione.
- Il reload ricrea le istanze C#: i campi privati tornano ai valori iniziali e le proprietà JSON vengono recuperate dai componenti. Non è Edit and Continue con conservazione dello stack o di tutti i campi.
- Chiudere l’editor interrompe il compilatore che ha avviato, se ancora in esecuzione. Visual Studio resta aperto.

## Dove sono i file

| Percorso nel progetto | Funzione |
| --- | --- |
| `assets/**/*.cs` | I tuoi sorgenti: creati, importati o salvati da Visual Studio; da conservare insieme agli asset |
| `project.json` e scene | Catalogo asset e componenti; l’associazione dello script è salvata nella scena |
| `.twodog/GameScripts.sln` e `.csproj` | Soluzione generata per l’IDE e la compilazione; rigenerata all’avvio |
| `.twodog/Builtin` | Sorgenti di compatibilità dei progetti demo/precedenti; esclusi dai progetti vuoti creati nell’Hub |
| `.twodog/runtime/GameScripts.dll` | Ultima compilazione pubblicata con successo, usata dall’editor e dal runtime del progetto |
| `.twodog/compile.log` e `ide.log` | Output dell’ultimo build e dell’apertura IDE, utili per diagnosticare errori |

I progetti dell’Hub usano `includeDemoScripts: false` in `project.json`, così non ereditano i Behaviour di esempio. Il comportamento dei progetti precedenti resta compatibile.

La cartella `.twodog` è generata, nascosta nel browser Assets ed esclusa dai sorgenti distribuiti. Eliminandola a editor chiuso, verrà rigenerata: non contiene i tuoi script. Per trasferire il progetto conserva anche le scene e l’intera cartella `assets`.

Il runtime separato usa `.twodog/runtime/GameScripts.dll` quando presente; altrimenti mantiene la DLL della demo distribuita con l’eseguibile. Non compila da solo: prima usa l’editor per ottenere una build valida. `scripts/build-csharp.ps1` resta il flusso manuale della demo legacy in `managed/GameScripts`; non compila `assets/**/*.cs` del progetto.

## Requisiti e limiti attuali

- Windows x64 e .NET 10 SDK accessibile come `dotnet`. Per supporto completo del progetto `net10.0` nell’IDE serve Visual Studio Community 2026, versione 18.0 o successiva, con strumenti .NET. Visual Studio 2022 resta utilizzato dal preset C++ esistente; si possono installare affiancati. La compilazione automatica dell’editor è affidata al CLI .NET. [Matrice ufficiale Microsoft](https://learn.microsoft.com/en-us/dotnet/core/porting/versioning-sdk-msbuild-vs).
- Usa una sola istanza dell’editor per lo stesso progetto mentre modifichi gli script.
- Il trascinamento riconosce il formato generato: un solo tipo per file, namespace semplice, classe derivata direttamente da `TwoDog.Behaviour`. Codice con basi intermedie o più classi Behaviour richiede il tipo completo nel componente; non c’è ancora un browser a riflessione degli assembly.
- I file C# del progetto devono essere sotto `assets`, con estensione `.cs`. L’importazione degli altri asset usa il catalogo `project.json`; copiare manualmente una texture nella cartella non la registra automaticamente. Gli script invece sono inclusi automaticamente dal progetto C#.
- Texture con anteprime reali; audio, font, scene, prefab e script hanno icone di tipo. Non sono ancora presenti anteprime renderizzate dei prefab, riproduzione audio nel browser, spostamento/rinomina con aggiornamento dei riferimenti o gestione NuGet dall’interfaccia.
- I nuovi campi C# non sono automaticamente trasformati in controlli dell’Inspector: le proprietà esposte restano il contratto JSON esistente. DLL esterne e modifica dei file di progetto generati non fanno parte di questa fase.
- Modificare API o host richiede chiudere 2Dog e ricompilare il motore; il salvataggio automatico riguarda il gameplay.
