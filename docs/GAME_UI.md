# UI di gioco 0.4

La UI runtime è indipendente da ImGui. È un componente `ui` delle entità, disegnato da SpriteRenderer sopra la scena, con input e messaggi nativi verso C#. Python è ancora una fase successiva.

Tipi: `Label`, `Button`, `Image`, `Panel`, `Slider`, `Layout`. Tutti si aggiungono dall'Inspector. Il rettangolo UI usa coordinate dall'angolo alto sinistro; il Transform2D dell'entità resta separato.

| Campo | Significato |
| --- | --- |
| anchorMin / anchorMax | Coppie normalizzate 0..1 nel parent; uguali per ancoraggio singolo, diverse per stretch |
| pivot | Coppia 0..1; 0,0 alto sinistra, 0.5,0.5 centro, 1,1 basso destra |
| offset / size | Offset e dimensioni aggiuntive in pixel di riferimento |
| parent | ID di un'altra entità UI; vuoto indica il canvas |
| order | Ordinamento crescente dei fratelli; a parità ordine per ID, parent sempre prima dei figli |
| visible | Nasconde l'elemento e i discendenti |
| interactable | Abilita Button e Slider; gli elementi disabilitati possono comunque bloccare il puntatore |
| blockPointer | Impedisce di inviare il clic UI al gameplay sottostante |
| text / font / fontScale | Testo, asset font (`bitmap` di default), scala |
| color / textColor | RGBA 0..1 |
| texture | Asset texture per Image |
| value | Slider normalizzato 0..1 |
| topic / target | Prefisso dei messaggi e ID del destinatario; target omesso usa l'elemento, stringa vuota invia broadcast |
| direction / padding / spacing | Layout verticale o orizzontale, margine e distanza |

Le scene precedenti senza Canvas esplicito mantengono il riferimento 1280×720 e la scala uniforme minima fra larghezza/1280 e altezza/720. I nuovi Canvas espliciti riempiono il viewport, espongono `referenceResolution` e `match` (0 = larghezza, 1 = altezza) e interpolano logaritmicamente le due scale. I figli ereditano la scala del Canvas. Le ancore sono riferite all'intera area visibile, quindi funzionano anche con rapporti d'aspetto diversi. Per ogni asse: dimensione = dimensione parent × (anchorMax − anchorMin) + size; posizione = origine parent + dimensione parent × anchorMin + offset − pivot × size. Un Layout dispone i figli visibili in ordine, usando size sull'asse principale e lo spazio del parent sull'altro asse.

Il runtime rifiuta cicli, parent inesistenti, profondità oltre 64, valori fuori intervallo e rettangoli non positivi. Non sono presenti scroll view, clipping per contenitore, wrapping automatico, maschere o navigazione gamepad della UI. Il testo conserva il supporto ASCII/TTF della fase precedente, non introduce layout Unicode completo.

## Eventi

Mouse: hover/exit, clic completo (pressione e rilascio sullo stesso Button), trascinamento Slider. Tab passa al prossimo Button/Slider; Invio o Spazio attivano il pulsante; le frecce cambiano lo slider di 0,05. Fare clic nello sfondo libera il focus tastiera. La cattura UI filtra i comandi del gioco, compresi gli input pending del passo fisso.

Ogni evento usa `Behaviour.OnMessage(topic, payload)`. Il topic è il prefisso seguito da `.hover`, `.exit`, `.click` o `.change`. Il payload contiene `source` (ID UI) e `value` (valore corrente). Gli eventi sono accodati alle barriere del runtime, senza chiamate ricorsive agli script.

```csharp
public override void OnMessage(string topic, System.Text.Json.Nodes.JsonNode? payload)
{
    if (topic == "menu.play.click")
        TwoDog.UI.Text(TwoDog.Engine.Find("Status"), "PLAY");
    if (topic == "menu.volume.change")
        TwoDog.Engine.Volume("music", payload!["value"]!.GetValue<float>());
}
```

`TwoDog.UI` espone getter/setter Text e Value, più Visible e Interactable. I restanti campi si leggono/modificano con `Entity.Component("ui")` e il setter esistente. Le scritture vengono validate prima di essere applicate. La distruzione di un parent riposiziona i figli come elementi root; l'editor richiede invece di riassegnarli prima di cancellare il contenitore.

In EditorDemo, UiDemo gestisce `demo.spawn.click` e `demo.volume.change`. Il contatore torna al valore iniziale a ogni nuovo Play. Le modifiche UI durante Play seguono le stesse regole di ripristino degli altri componenti.

Per le nuove funzioni e le prove numerate: [TEST_UI_CANVAS_CSHARP.md](TEST_UI_CANVAS_CSHARP.md).
