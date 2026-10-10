# Verifiche effettuate — 0.7.3

1. Compilazione nativa GCC completata; 91/91 test CTest senza grafica superati, inclusi servizio export, persistenza delle impostazioni e workspace.
2. Compilazione oggetti Windows x64 con Zig/Clang riuscita per ExportService, WindowsExport, ScriptWorkspace e AssetsPanel. Non sostituisce la compilazione completa MSVC né il collegamento Windows.
3. Soluzione generata realmente compilata con SDK 8.0.408: zero errori e warning, con uno script che usa Rigidbody2D, SpriteRenderer e UIText. Il compilatore design-time riceve il riferimento TwoDog.API.dll.
4. Progetto runtime generato realmente compilato con SDK 10.0.100: zero errori e warning. L’hash del project.assets.json della soluzione IDE è rimasto invariato. Verificate anche impostazioni SDK/props incompatibili poste nella cartella superiore: i progetti generati usano le proprie impostazioni.
5. Il lancio della verifica completa da PowerShell su questo ambiente Linux ha incontrato un errore del processo dotnet nella lettura di /proc, prima della compilazione. I passaggi di compilazione e design-time sono stati quindi eseguiti direttamente dalla shell con successo. Il nuovo script check-script-workspace.ps1 deve ancora essere eseguito integralmente su Windows.
6. Superati 3 controlli degli strumenti di progetto, 5 casi di apertura IDE con DTE simulato e 7 controlli degli strumenti export; sintassi di tutti gli script PowerShell verificata.
7. Pipeline export eseguita con compilatore e binari fixture: verificati pacchetto, dipendenze tileset, hash, conservazione output esistente, mancata pubblicazione in caso di errore, pulizia staging e rifiuto output interno al progetto o ai sorgenti engine. Non è una prova di avvio del gioco Windows.
8. Target CMake TwoDogRuntimeTemplate configurato ed eseguito con fixture: prodotti player, file managed, nethost, Builtin, licenze e metadati nelle cartelle previste. Non è una compilazione .NET/MSVC del template Windows reale.
9. Non verificati qui: GUI Visual Studio, menu di completamento, automazione COM reale, grafica e avvio su Windows senza SDK. Eseguire TEST_DISTRIBUTION_IDE.md sul PC locale.
