# Progetto e componenti — formato 1

Tutti i JSON sono UTF-8; un BOM UTF-8 è accettato in lettura. Duplicati di chiave, numeri non finiti, JSON incompleto e profondità oltre 64 vengono rifiutati. Dimensione massima di un JSON: 16 MiB. La serializzazione conserva gli ID delle entità; gli ID delle nuove istanze prefab vengono rigenerati.

## Catalogo

`samples/CSharpDemo/project.json` contiene versione, nome, `startScene`, array `assets` e mapping `input`. Un asset ha `id`, `type`, `path` relativo alla radice. Tipi: texture, audio, font, scene, prefab, tilemap. I percorsi vengono normalizzati e devono restare nella radice, anche dopo la risoluzione dei collegamenti del filesystem.

I riferimenti di scena/script usano l'ID (`texture.atlas`, `prefab.crate` ecc.). Per rinominare un file aggiorna il path nel catalogo e riavvia: gli ID nei componenti non cambiano. Il catalogo stesso non viene riletto automaticamente durante Play.

## Scene

Una scena contiene `version:1`, `id`, `entities`, e opzionalmente `gravity`, `camera`, `tilemaps`, `joints`. Ogni entità ha `id` e `transform`; `name` è facoltativo. ID duplicati vengono rifiutati. La nuova scena viene costruita e validata prima di sostituire quella attiva.

| Componente | Campi principali |
| --- | --- |
| transform | position `[x,y]`, size `[w,h]` positiva, rotation in radianti |
| sprite | texture ID o vuoto per bianco, color RGBA 0..1, uv `[u0,v0,u1,v1]`, layer intero, depth, parallax `[x,y]` |
| body | type static/kinematic/dynamic, fixedRotation, bullet, velocity, colliders |
| collider | shape box/circle/polygon; halfSize/radius/vertices; offset e angle locali; sensor; density/friction/restitution; category/mask |
| scripts | array di slot, language, type, order, properties |
| animation | state e states: ciascuno con frames UV, fps, loop, next opzionale |
| text | font ID o bitmap, value, scale, color |
| properties | oggetto JSON libero, accessibile agli script |

I poligoni sono strettamente convessi, da 3 a 8 vertici. Un body può avere più colliders. Le dimensioni dei colliders sono fisiche e distinte dalla dimensione grafica del transform. In questa versione category/mask serializzati usano interi non negativi a 31 bit; il wrapper fisico C++ mantiene il supporto a 64 bit.

Un prefab contiene `{ "version":1, "entity": ... }`. Instantiate carica il file aggiornato, applica la posizione richiesta e genera un nuovo ID. La versione corrente del prefab rappresenta **una entità**, anche con più componenti/script; gerarchie e prefab annidati sono lavoro successivo dell'editor.

## Giunti

I campi `a` e `b` sono ID delle entità con body. Tipo distance: length, hertz, damping. Tipi revolute/prismatic/weld: anchor in coordinate mondo; prismatic usa anche axis. Revolute e prismatic supportano limit/lower/upper e motor/speed/maxForce (coppia per revolute). Gli ID dei giunti runtime sono opachi e non equivalgono agli ID persistenti delle entità.

## Camera e animazioni

Camera: center, zoom, pixelsPerUnit, follow (ID entità), smoothing, bounds `[minX,minY,maxX,maxY]`, pixelPerfect. I bounds considerano l'estensione visibile: se il viewport è più grande dei bounds, viene centrato. H nella demo applica shake transitorio. Lo snapping allinea camera/sprite alla griglia dei pixel, ma non impone una risoluzione logica fissa né scaling intero del viewport.

Gli stati animazione cambiano via `SetState`; `next` attua una transizione a fine clip. Le condizioni di gameplay sono negli script (ad esempio velocità → run). Non è ancora presente un editor visuale delle transizioni.

## Tiled

Supportato: mappe JSON finite e ortogonali, tile layers, array GID non compressi, atlas embedded o tileset esterno JSON, visibilità, opacità, offset, parallax e flip orizzontale/verticale. Le immagini dei tileset devono essere asset texture registrati.

Rifiutati esplicitamente: mappe infinite/chunks, isometriche/hex, dati base64/compressi, layer object/group/image, image collections, flip diagonale/hex e animazioni/collisioni per tile. Le collisioni e animazioni equivalenti si possono definire nei componenti nativi della scena. L'import non pretende di coprire l'intero formato Tiled.

`tilemaps` della scena associa asset, origin, pixelsPerUnit e layer iniziale. Limiti: dimensioni mappa <=1024x1024, massimo 100.000 tile visibili importati.

## Asset, testo e audio

Texture: `stb_image`, RGBA, nearest filtering; file <=64 MiB, dimensioni <=8192. Reload del file ogni 0,5 s; un caricamento fallito conserva la texture precedente.

Font: bitmap originale incluso; TTF tramite `stb_truetype` con atlas ASCII 32–127. Nessun font commerciale è incluso. Font TTF locali registrati nel catalogo possono essere ricaricati. Font shaping, layout internazionale e fallback Unicode non sono implementati in questa milestone.

Audio: miniaudio, bus master/sfx/music; SFX decodificati, music streaming. Le voci hanno ID runtime e vengono rilasciate a fine riproduzione o Stop. Un file modificato viene usato quando viene ricaricato per una nuova voce dopo il rilascio della risorsa precedente: non viene sostituito dentro una voce in corso. Se non esiste un dispositivo audio, il motore segnala la condizione e continua silenziosamente; il test audio rimane da eseguire su hardware disponibile.

JSON scena/Tiled: F6 esegue il reload esplicito con validazione, ricostruendo scena e script. Prefab: riletti alla creazione. Assembly C#: watcher o F9. Non tutti i tipi di asset hanno una sostituzione automatica durante il loro utilizzo.

## Salvataggio e stato

F7 salva un nuovo file con i componenti correnti, trasformazioni, velocità e camera. La scrittura passa da un file temporaneo nella stessa directory e sostituisce la destinazione al termine. F8 carica il salvataggio validato.

Non sono salvati accumulatori del loop, contatti del solver Box2D, tempo interno degli animator, particelle attive, voci audio o campi privati C#. Play/Stop ricostruisce tali sistemi dalla baseline, invece di tentare di ripristinare puntatori o oggetti del runtime precedente.

## Estensioni 0.4

Le entità possono contenere `ui`, descritto in `GAME_UI.md`. `project.json/scriptTypes` è una lista facoltativa di nomi completi C# suggeriti dall'Inspector; non modifica il caricamento delle DLL. L'editor mantiene il formato versione 1 e preserva i componenti non modificati. `ui.parent` gestisce solo layout UI; non è parenting del Transform2D.
