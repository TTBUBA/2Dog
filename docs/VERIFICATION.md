1. Versione verificata: patch 0.7, export Windows, 9 ottobre 2026. La base è la 0.6 con aggiornamento Canvas/API C#. La documentazione storica duplicata è stata consolidata; una funzione implementata non equivale automaticamente a un risultato F sul PC dell’utente.

2. Compilazione C++ effettivamente eseguita su Linux: TwoDogEditor e TwoDogRuntime, con warning trattati come errori. Esito: riuscita. Non è una compilazione MSVC e non verifica il driver grafico Windows.

3. Compilazione degli oggetti Windows x64 effettivamente eseguita con Zig/Clang: Editor.cpp, WindowsExport.cpp, Desktop.cpp e ManagedHost.cpp (TWODOG_CSHARP, header ufficiali .NET 10). Esito: riuscita con -Wall -Wextra -Werror. Questa verifica controlla anche i rami Win32; non è un collegamento completo del player con MSVC.

4. Suite nativa Linux, esclusi i test grafici: 75/75 superati. Comprende core, gameplay, dati, UI, workspace e CLI. Nessuna nuova prova OpenGL reale o avvio GUI Windows effettuato in questa consegna.

5. PowerShell 7.4: sintassi degli script, quoting dei percorsi IDE, migrazione non distruttiva e cinque scenari simulati del launcher Visual Studio: superati. L’apertura reale di Visual Studio rimane una verifica locale.

6. check-export-tools.ps1: 7/7 superati. Contenimento dei percorsi, asset in cartelle con spazi e parentesi quadre, rifiuto file mancanti/esterni/generati/C#, esclusione cache dalle sorgenti, escaping XML/MSBuild, controllo header PE x64 e UTF-8 senza BOM.

7. Pipeline PowerShell con compilatore e binari simulati: 4/4 superati. Pacchetto e dipendenze tilemap con manifest SHA-256; preservazione destinazione esistente; compilazione fallita senza pubblicazione e con rimozione dello staging; rifiuto destinazione dentro il progetto. Sono prove di filesystem e gestione errori: non dimostrano l’esecuzione di un gioco Windows o la correttezza dei binari .NET.

8. Regole CMake di preparazione template/licenze: configurate ed eseguite su un target minimo. Template Release e sei documenti di terze parti copiati correttamente.

9. Il player esportato individua data e managed rispetto alla directory dell’EXE; nethost cerca hostfxr nella cartella dotnet privata quando presente. Il CRT del player C++ è configurato staticamente. Il funzionamento completo deve essere confermato su Windows x64 senza SDK/runtime globale, come descritto in TEST_EXPORT_WINDOWS.md.

10. Da eseguire localmente: scripts/build-editor.ps1 -Configuration Release; scripts/verify.ps1 -Configuration Release; checklist TEST_EXPORT_WINDOWS.md e TEST_EDITOR.md. La suite completa Windows comprende 80 casi nativi quando è inclusa la grafica, più i controlli gestiti e PowerShell. I 15 controlli dell’host gestito fanno parte di verify.ps1, ma non sono stati rieseguiti in questa consegna.

11. Limiti dell’export attuale: cartella portabile, tutte le scene/asset registrati, C# senza riferimenti personalizzati NuGet/csproj, nessun installer/firma/icona personalizzata/stripping. File letti direttamente con System.IO e non registrati non vengono raccolti automaticamente. I salvataggi di scene restano nel percorso del progetto esportato: collocarlo in una directory scrivibile.

12. Riferimento del contratto di hosting verificato: https://github.com/dotnet/runtime/blob/v10.0.0/src/native/corehost/nethost/nethost.h e https://github.com/dotnet/runtime/blob/v10.0.0/src/native/corehost/hostfxr.h. Le scelte di export usano l’API dotnet_root del native host.

13. Pulizia verificata con fixture: -WhatIf conserva i file; un documento originale viene eliminato; una checklist modificata con risultati personali viene conservata.
