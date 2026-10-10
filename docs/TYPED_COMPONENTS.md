# Componenti e API C# tipizzate — 0.7.2

1. **Scopo.** I componenti del progetto mantengono il formato JSON esistente. Il motore ora condivide descrizioni, valori iniziali e validazione attraverso ComponentSchema, senza dipendere da ImGui. In C# puoi leggere e modificare i componenti supportati senza costruire JSON. Le API precedenti, comprese Entity.Component, Entity.Velocity e UI.Text, restano disponibili.

2. **Come trovare un componente.** Usa Entity.GetComponent<T>() per ricevere un componente oppure null se assente. Usa Entity.TryGetComponent<T>(out var component) per una verifica condizionale. Usa Entity.RequireComponent<T>() se lo script richiede quel componente: se manca, viene sollevata un'eccezione con il nome del componente. Questi metodi cercano componenti esistenti: non li aggiungono. I tipi supportati sono Rigidbody2D, SpriteRenderer e UIText.

   ```csharp
   using TwoDog;

   public sealed class Movement : Behaviour
   {
       private Rigidbody2D body = null!;

       public override void OnCreate()
       {
           body = Entity.RequireComponent<Rigidbody2D>();
       }

       public override void OnFixedUpdate(float fixedDeltaTime)
       {
           float direction = (Engine.Input("right").Down ? 1 : 0)
                           - (Engine.Input("left").Down ? 1 : 0);
           body.Velocity = body.Velocity with { X = direction * 4 };
       }
   }
   ```

   L'oggetto deve avere un body con almeno un collider; le azioni left e right devono essere configurate nel progetto. Sono già usate dalle demo. La proprietà Entity dello script è l'oggetto a cui lo script è assegnato.

3. **Rigidbody2D.** Velocity legge e scrive la velocità corrente della simulazione come Vec2, in unità del mondo al secondo. AddForce(Vec2) applica una forza; richiamala nei passi fisici in cui deve agire. AddImpulse(Vec2) applica un impulso istantaneo, per esempio in risposta a un evento. BodyType restituisce RigidbodyType2D.Static, Kinematic o Dynamic. FixedRotation, Bullet e ColliderCount leggono la configurazione esistente. Queste quattro proprietà sono in sola lettura: per modificarle usa l'editor prima del Play. Non sono stati aggiunti setter fittizi che modificano soltanto il documento senza aggiornare Box2D.

   ```csharp
   body.AddForce(new Vec2(5, 0));
   body.AddImpulse(new Vec2(0, 2));
   ```

4. **SpriteRenderer.** TextureId contiene l'ID di una texture registrata nel progetto, non un percorso assoluto. Una stringa vuota usa la texture predefinita del motore. Color è un Color RGBA con canali tra 0 e 1; SortingLayer è un intero; Depth è la profondità; Parallax è un Vec2; UV è uno SpriteUV(U0, V0, U1, V1), con coordinate tra 0 e 1. Le proprietà sono leggibili e modificabili. Gli ID degli asset rimangono stringhe perché identificano risorse del progetto.

   ```csharp
   var sprite = Entity.RequireComponent<SpriteRenderer>();
   sprite.Color = new Color(1, .5f, .2f);
   sprite.SortingLayer = 3;
   sprite.UV = new SpriteUV(0, 0, .5f, 1);
   ```

5. **UIText.** Rappresenta il testo di una UI Label o Button. Su Image, Canvas, Slider, Layout o Panel, GetComponent<UIText>() restituisce null. Text è la stringa mostrata; Color modifica textColor, quindi il colore del testo; FontId seleziona un font registrato o bitmap; FontScale deve essere maggiore di 0 e non superiore a 10; Visible controlla la visibilità dell'intero elemento UI. UIText non rappresenta il componente World Text.

   ```csharp
   var hud = Engine.Find("ScoreLabel");
   if (!string.IsNullOrEmpty(hud.Id) &&
       hud.TryGetComponent<UIText>(out var text))
   {
       text.Text = "Score: 10";
       text.Color = new Color(1, 1, 0);
   }
   ```

6. **Aggiornamenti e durata.** I wrapper conservano il riferimento all'entità e rileggono i dati nativi quando servono; non conservano una copia JSON obsoleta. Un setter modifica il solo campo interessato e conserva gli altri. Puoi tenere un wrapper in un campo dello script durante la vita dell'oggetto. Dopo la distruzione dell'entità, le chiamate vengono rifiutate dal motore. Dopo Stop o reload, recupera i componenti in OnCreate della nuova istanza. Le chiamate al motore restano consentite solo sul thread principale, anche con le nuove classi.

7. **Errori.** NaN, valori infiniti e valori fuori intervallo vengono rifiutati dove previsto. Gli errori della validazione nativa restano eccezioni C# e compaiono in Console tramite l'host. GetComponent restituisce null per un componente assente, ma non nasconde errori come un'entità scaduta o una chiamata da un thread non consentito. L'host può interrompere le successive callback di uno script che genera un'eccezione: usa TryGetComponent per dipendenze facoltative.

8. **Esempio completo.** managed/GameScripts/TypedComponentsDemo.cs è compilato con le demo. Per provarlo in un progetto già esistente, crea TypedComponentsDemo.cs nell'editor e sostituiscine il contenuto con quello dell'esempio. Assegnalo a un oggetto con body e sprite. Una Label facoltativa chiamata ScoreLabel mostra il tempo trascorso. L'esempio usa left/right e non contiene accessi JSON ai componenti.

9. **Estensione dei componenti nativi.** ComponentSchema::components(), in src/data/ComponentSchema.cpp, registra chiave JSON, nome visualizzato, factory e validatore. Il menu Add Component e le etichette dell'Inspector usano questo registro. Project::validateEntity lo richiama per scene e prefab; SceneDocument lo usa prima dei commit; SceneView lo usa prima di acquisire le risorse visuali. Per un componente fatto di campi semplici, una nuova voce rende disponibili i controlli generici. Il comportamento runtime del nuovo componente va comunque implementato nel sistema competente.

10. **Fisica e UI condivise.** ComponentSchema::collider/body trasformano i dati in BodyConfig. Physics::validateCollider/validateBody applicano le stesse regole geometriche e materiali usate dalla fisica nativa, comprese dimensioni minime e hull convessi dei poligoni, senza creare un mondo. La validazione dei singoli elementi UI risiede nello schema; la risoluzione di parent, cicli e layout resta in GameUI perché dipende dalla scena completa. Risoluzione e disponibilità di texture/font restano nei servizi degli asset.

11. **Compatibilità e limiti.** Nessun nuovo opcode, formato di progetto o migrazione è necessario. Le validazioni vengono eseguite prima, quindi documenti invalidi possono essere rifiutati già al caricamento invece che al rendering. Questo è il primo gruppo di wrapper tipizzati: non comprende AddComponent<T>(), rimozione di componenti fisici durante Play o wrapper per ogni sistema. Il nuovo codice non elimina JSON dalle API avanzate.
