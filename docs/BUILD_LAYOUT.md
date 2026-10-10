# Aggiornamento 0.7.3

1. Per i percorsi attuali e la separazione IDE/runtime/export consulta [DEVELOPMENT_DISTRIBUTION.md](DEVELOPMENT_DISTRIBUTION.md).
2. Per installazione e verifiche segui [TEST_DISTRIBUTION_IDE.md](TEST_DISTRIBUTION_IDE.md); le descrizioni storiche sottostanti vanno lette con questi aggiornamenti.

# Struttura della build 0.7

Il comando usuale resta `scripts/build-editor.ps1`. Cache, librerie e programmi sono ora separati:

| Percorso | Contenuto |
| --- | --- |
| `build/bin/Debug` oppure `Release` | Hub, editor, runtime e file necessari al loro avvio |
| `build/bin/<config>/managed` | API C#, host .NET e DLL della demo distribuita con l’engine |
| `build/cmake/windows-csharp` | Cache CMake, solution/progetti MSVC, download dipendenze e intermedi nativi |
| `build/lib/<config>` | Librerie native di collegamento |
| `build/managed` | Output/intermedi per progetto C#, staging della demo e ripristino NuGet |
| `build/tests/<config>` | Eseguibili di test e fixture C# |
| `build/legacy/<data-id>` | Vecchie build e precedenti cartelle managed/bin/obj preservate durante la migrazione |
| `build/variants/<preset>` | Output dei preset alternativi, separati dalla build Windows C# principale |
| `reports/local-...` | Report creati dalle verifiche locali |

Il tuo progetto di gioco resta nel percorso scelto nell’Hub. Non va creato dentro `build`, perché quest’ultima è la cartella dei risultati di compilazione. La cache di scripting di ogni progetto rimane nella sua `.twodog`, separata dai suoi asset.

## Aggiornamento da 0.5

Chiudi Hub, editor e runtime, aggiorna i sorgenti e lancia `build-editor.ps1`. Il comando archivia automaticamente le vecchie cartelle note (`build/windows-csharp`, `build/windows`, `build/windows-graphics`, `build/headless`, `build/csharp-staging`, e i `bin/obj` dei progetti C# dell’engine) in `build/legacy`. **Non le elimina**: possono contenere progetti modificati aprendo direttamente i vecchi eseguibili.

La nuova cache CMake parte nel nuovo percorso: il primo build dopo l’aggiornamento ricompila il motore e può riscaricare le dipendenze. Non riutilizzare la vecchia soluzione MSVC e non spostare una cache CMake già configurata dentro il nuovo percorso.

I progetti personali esterni e `samples/EditorDemo/assets` non vengono spostati. Se avevi lavorato su una demo dentro la vecchia build, la ritrovi sotto `build/legacy/<data-id>/build/windows-csharp/<config>/projects`: copiala in una cartella personale e apri il suo `project.json` dall’Hub. Conserva il backup finché hai recuperato ciò che ti serve.

Non cancellare tutto `build/legacy` senza controllare quei progetti. La migrazione è ripetibile: quando non trova vecchie cartelle, non fa nulla. Puoi richiamarla separatamente con `scripts/archive-legacy-build.ps1`.

Le copie delle demo accanto agli eseguibili escludono `.twodog`, `.vs`, `bin` e `obj`, evitando di duplicare le cache dei progetti. Gli intermedi .NET nuovi non vengono più creati sotto `managed/<progetto>/bin` e `obj`.

## Comandi diretti

```powershell
cmake --preset windows-csharp
cmake --build build/cmake/windows-csharp --config Debug --parallel 2
ctest --test-dir build/cmake/windows-csharp -C Debug --output-on-failure
```

Gli script `run-editor.ps1`, `run-project.ps1`, `run-hub.ps1` e quelli di verifica sono aggiornati. Per compiere l’intera migrazione al primo aggiornamento usa `build-editor.ps1`, anziché soltanto i comandi CMake diretti.

1. Le distribuzioni dei giochi si creano in una cartella esterna scelta in File > Build for Windows. Non usare build/bin come destinazione del gioco.
2. build/bin/Release contiene anche runtime-template.json e ExportLicenses: sono necessari per esportare. Non copiarli manualmente nel gioco; provvede export-windows.ps1.
3. Gli script temporanei di compilazione export sono in <progetto>/.twodog/exports e vengono rimossi alla fine. I log vengono conservati per la diagnosi.
4. Per la pulizia dei vecchi documenti e verificatori usa scripts/clean-project.ps1; con -WhatIf mostra soltanto le eliminazioni. I file modificati localmente vengono conservati. Progetti, asset, vecchi build e report non vengono eliminati automaticamente.
