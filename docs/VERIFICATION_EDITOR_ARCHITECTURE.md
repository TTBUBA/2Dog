# Verifiche interne 0.7.1 — 10 ottobre 2026

1. Configurazione CMake e compilazione Debug completa riuscite su Linux, incluso il collegamento di TwoDogEditor. Flag dei sorgenti del progetto: -Wall -Wextra -Wpedantic -Werror. La build interna usa il backend GLFW null; questo esito non certifica il funzionamento interattivo della GUI Windows.
2. CTest non grafico: 83 test superati su 83. Comprende 28 core, 12 data, 17 gameplay, 9 UI, 6 workspace, 3 CLI e 8 nuovi test editor.
3. Nuovi controlli: ripristino della selezione con Undo/Redo; rifiuto transazionale di documenti invalidi; dirty e nuovo ramo dopo Undo; assegnazione script con duplicati rifiutati; blocco delle modifiche durante Play; modifica proprietà senza cambiare ID; pulizia dei riferimenti durante l'eliminazione; limite della cronologia a 64 passi.
4. I test grafici, la compilazione MSVC, l'apertura di Visual Studio, il reload C# e l'esecuzione del pacchetto esportato non sono stati eseguiti in questo ambiente. Seguire TEST_EDITOR_ARCHITECTURE.md sul PC Windows. Nessun cambiamento alle API C# o al formato dei progetti è incluso in questa patch.

5. Compilazione incrociata degli oggetti di tutti i sorgenti editor aggiunti/modificati e apps/editor.cpp riuscita per x86_64-windows-gnu con Zig/Clang, C++20 e -Wall -Wextra -Werror. Include i rami Windows di importazione asset, caricamento font e nuovo entry point GUI. Non è un link completo MSVC né una prova di esecuzione Windows.
