# API C++ implementata

Questa pagina descrive l'API nativa. I binding C# della 0.3 sono descritti in `SCRIPTING_CSHARP.md`; Python rimane da implementare.
Tutte le operazioni del backend grafico, della demo e del wrapper fisico vanno
eseguite sul thread dell'applicazione. Non c'è accesso concorrente ai componenti.

| Tipo | Contratto |
| --- | --- |
| `Scene` | Crea/distrugge entità con indice e generazione. `get` rifiuta handle scaduti. Handle validi solo nella propria scena; non conservarli dopo reset o tra scene. |
| `SceneObject` | Transform2D sempre presente e Sprite opzionale. Non trattenere riferimenti ai componenti dopo create/destroy/reset. |
| `Transform2D` | Posizione e dimensione in unità mondo; rotazione in radianti. |
| `Sprite` | Texture handle locale al renderer, tinta RGBA in [0,1], UV bottom-left/top-right, layer e depth. Texture 0 è bianca. |
| `Camera2D` | Centro mondo, zoom e pixel per unità. Coordinate schermo con origine in alto a sinistra. |
| `SpriteAnimation` | Lista UV, FPS, loop; `advance` accetta dt finito tra 0 e 3600 s. Senza loop resta sull'ultimo frame e diventa finished. |
| `Input` | Bind multipli per azione. `beginFrame` azzera i fronti di frame, `consumeFixed` consuma quelli conservati per la simulazione. |
| `Physics` | Possiede un mondo Box2D. Unità: metri, secondi, radianti. `step` richiede dt in (0,0.1]. |
| `PhysicsEvent` | ContactBegin/End e TriggerBegin/End; valori copiati, validi come snapshot fino al prossimo step. Verificare `alive` prima di usare handle di corpi poi eliminati. |
| `SpriteRenderer` | Finestra/contesto devono sopravvivergli. `draw` ordina e invia sprite; `capture` legge il framebuffer prima di `present`. |

## Note fisica

`BodyConfig` crea il collider iniziale. `addCollider` aggiunge forme allo stesso corpo, con offset/rotazione locali: box con mezze dimensioni, cerchio
con raggio, oppure poligono strettamente convesso di 3–8 vertici non degeneri.
L'hull viene calcolato da Box2D; vertici collineari o interni vengono rifiutati.
La dimensione minima per box/raggio è 0,005 m. Friction e restitution sono in [0,1].
`category` e `mask` espongono i filtri di collisione.

`pose(body, alpha)` interpola tra i due stati con alpha in [0,1]; il valore di
default 1 restituisce lo stato corrente. L'angolo usa il percorso più breve.
`raycast(origin, translation, mask, ignore)` prende uno spostamento, non un punto finale,
e restituisce l'impatto più vicino. Come in Box2D, non segnala la forma che già
contiene l'origine. Esclude i sensori e può ignorare un body. Sono disponibili distance (con hertz > 0 per la molla), revolute, prismatic e weld; i due giunti mobili aggiuntivi supportano limiti e motori. `setPose` teletrasporta e riallinea l'interpolazione.

Gli eventi vengono letti dopo step. Se uno shape nativo è già stato distrutto,
un eventuale end-event nativo con ID scaduto viene scartato: la distruzione non
garantisce un evento End sintetico. Lo stesso comportamento è dichiarato nel contratto C#: non viene inventato un evento End durante Destroy.

## Note input

I codici sono quelli di GLFW: Keyboard 0–511, Mouse 0–7, Gamepad 0–14.
Gli assi del primo gamepad sono 0–5 con una dead zone di 0,18. La perdita del
controller rilascia i suoi pulsanti. Gli assi vengono esposti separatamente dalle
azioni digitali. `rebind` cambia le associazioni e il progetto JSON conserva i mapping iniziali. I nomi `engine.*` sono riservati.

## Esempio fisica

```cpp
twodog::Physics physics;
twodog::BodyConfig box;
box.position = {0.0f, 4.0f};
auto body = physics.create(box);
physics.step(1.0f / 60.0f);
auto pose = physics.pose(body);
physics.destroy(body);
```

La demo precedente è `Sandbox.cpp`; il progetto JSON/C# è integrato da `ProjectPlayer.cpp`.

Le classi `Project`, `World2D`, `VisualAssets`, `SceneView`, `Audio`, `Font` e `ManagedHost` completano il percorso della 0.3. Contratti persistenti e limiti sono in `PROJECT_FORMAT.md`.

## UI 0.4

`Entity.Component("ui")` legge il componente UI; il setter ora lo valida e applica. `TwoDog.UI` espone Text, Value, Visible e Interactable. Gli eventi click/change/hover/exit arrivano a `Behaviour.OnMessage` sul contratto esistente, senza nuovi opcode ABI. Schema e focus: `GAME_UI.md`.

Per Rigidbody2D, SpriteRenderer, UIText e il registro nativo ComponentSchema introdotti nella 0.7.2, vedere TYPED_COMPONENTS.md.
