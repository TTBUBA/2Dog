# Dipendenze 0.7

Le librerie native sono scaricate da CMake con versioni e SHA-256 in `cmake/Dependencies.cmake`. Nessuna libreria runtime aggiunta fuori dallo stack della specifica.

| Dipendenza | Versione | Licenza | Uso |
| --- | --- | --- | --- |
| GLFW | 3.4 | zlib/libpng | Finestra/input |
| GLM | 1.0.1 | MIT o Happy Bunny | Matematica |
| Box2D | 3.1.1 | MIT | Fisica |
| stb | f0569113c93ad095470c54bf34a17b36646bbbb5 | MIT o public domain | stb_image e stb_truetype |
| miniaudio | 0.11.23 | MIT o public domain | Audio, voci e bus |
| Dear ImGui | 1.91.9b docking | MIT | Solo editor, backend GLFW/OpenGL3 |
| .NET | SDK/runtime 10.0.x x64 installato localmente | MIT per runtime e hosting; licenze/notice delle distribuzioni SDK | Compilatore C#, nethost, hostfxr, CoreCLR |

Il codice C# usa solo le librerie incluse in .NET, senza pacchetti di test esterni. Il parser JSON nativo è codice 2Dog.

Fonti delle versioni native:

- https://github.com/glfw/glfw/tree/3.4
- https://github.com/g-truc/glm/tree/1.0.1
- https://github.com/erincatto/box2d/tree/v3.1.1
- https://github.com/nothings/stb/tree/f0569113c93ad095470c54bf34a17b36646bbbb5
- https://github.com/mackron/miniaudio/tree/0.11.23
- https://github.com/dotnet/runtime
- https://github.com/ocornut/imgui/tree/v1.91.9b-docking

L'archivio dei sorgenti non incorpora codice/binari di queste dipendenze. Contiene atlas e font bitmap originali e due WAV generati proceduralmente per la demo. Nessun TTF esterno viene distribuito. Il build C# copia nethost.dll dall'host pack del tuo SDK nella propria directory di output; l'export Windows copia un runtime .NET 10 x64 privato, con LICENSE.txt e ThirdPartyNotices.txt, senza includere l’SDK.

L’export copia le licenze native da ExportLicenses. Per miniaudio conserva il suo header come documento di terze parti, che contiene anche i notice incorporati. OSMesa rimane soltanto un backend opzionale di verifica Linux e non viene distribuito col runtime Windows.
