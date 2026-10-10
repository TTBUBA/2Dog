using System.Text.Json.Nodes;
using TwoDog;
namespace GameScripts;

public sealed class Crate : Behaviour
{
    private float age;
    public override void OnCreate() => Engine.Log($"Crate OnCreate {Entity.Id}");
    public override void OnFixedUpdate(float dt)
    {
        age += dt;
        if (age > (Properties["lifetime"]?.GetValue<float>() ?? 15)) Entity.Destroy();
    }
    public override void OnMessage(string topic, JsonNode? payload)
    {
        if (topic == "spawned") Engine.Log($"Crate received queued message: {payload}");
    }
    public override void OnDestroy() => Engine.Log($"Crate OnDestroy {Entity.Id}");
}
public sealed class Beacon : Behaviour
{
    private float elapsed;
    public override void OnCreate() => Engine.Log($"Beacon OnCreate {Entity.Id}");
    public override void OnUpdate(float dt)
    {
        elapsed += dt;
        if (elapsed < .8f) return;
        elapsed = 0;
        Engine.Emit(Entity.Transform.Position, 5, new(1, .6f, .25f));
    }
    public override void OnMessage(string topic, JsonNode? payload)
    {
        if (topic == "demo.ping")
        {
            Engine.Log($"Beacon received: {payload}");
            var sprite = Entity.Component("sprite")!;
            sprite["color"] = new JsonArray(.95f, .3f, .85f, 1f);
            Entity.Component("sprite", sprite);
        }
    }
    public override void OnDestroy() => Engine.Log("Beacon OnDestroy");
}
// Enable by setting the fault entity property 'enabled' to true, then F6.
public sealed class FaultProbe : Behaviour
{
    public override void OnUpdate(float dt)
    {
        if (Properties["enabled"]?.GetValue<bool>() == true)
            throw new InvalidOperationException("Intentional C# exception for local verification.");
    }
    public override void OnDestroy() => Engine.Log("FaultProbe OnDestroy (also after a script exception)");
}
public sealed class OrderProbe : Behaviour
{
    private int count;
    public override void OnCreate() => Engine.Log($"ORDER create {Properties["label"]}");
    public override void OnFixedUpdate(float dt)
    {
        if (count++ == 0) Engine.Log($"ORDER fixed {Properties["label"]}");
    }
    public override void OnDestroy() => Engine.Log($"ORDER destroy {Properties["label"]}");
}
