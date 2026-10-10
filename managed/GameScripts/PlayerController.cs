using System.Text.Json.Nodes;
using TwoDog;
namespace GameScripts;

public sealed class PlayerController : Behaviour
{
    private const float SpeedMultiplier = 1; // Change to 2 for the local hot-reload check.
    private string music = "";
    private bool muted;
    private int updates, steps;
    public override void OnCreate()
    {
        Engine.Log($"Player OnCreate: {Entity.Id}; speed={Properties["speed"]}");
        Engine.Follow(Entity);
        Engine.Volume("master", .35f);
        music = Engine.Play("audio.music", "music", true);
    }
    public override void OnFixedUpdate(float dt)
    {
        float direction = (Engine.Input("right").Down ? 1 : 0) - (Engine.Input("left").Down ? 1 : 0);
        if (Math.Abs(Engine.Axis(0)) > .18f) direction = Engine.Axis(0);
        var velocity = Entity.Velocity;
        velocity = velocity with { X = direction * (Properties["speed"]?.GetValue<float>() ?? 4.5f) * SpeedMultiplier };
        var position = Entity.Transform.Position;
        var ground = Engine.Raycast(position, new(0, -.58f), Entity);
        if (Engine.Input("jump").Pressed && ground is { } hit && hit.Normal.Y > .5f)
        {
            velocity = velocity with { Y = 6.2f };
            Engine.Play("audio.jump");
            Engine.Emit(position + new Vec2(0, -.4f), 18, new(.4f, 1, .75f));
        }
        Entity.Velocity = velocity;
        Entity.State(Math.Abs(direction) > .01f ? "run" : "idle");
        if (++steps == 60) Engine.Log("LIFECYCLE: 60 OnFixedUpdate calls reached.");
        if (position.Y < -12) Entity.Transform = Entity.Transform with { Position = new(-6, -2) };
    }
    public override void OnUpdate(float dt)
    {
        if (Engine.Input("spawn").Pressed)
        {
            var crate = Engine.Instantiate("prefab.crate", Engine.Pointer);
            crate.Send("spawned", new JsonObject { ["source"] = Entity.Id });
        }
        if (Engine.Input("music").Pressed) { muted = !muted; Engine.Volume("music", muted ? 0 : 1); }
        if (Engine.Input("message").Pressed) Engine.Broadcast("demo.ping", JsonValue.Create("Hello through the native engine"));
        if (Engine.Input("shake").Pressed) Engine.Shake(.15f);
        if (++updates == 120) Engine.Log("LIFECYCLE: 120 OnUpdate calls reached (independent of fixed steps).");
    }
    public override void OnMessage(string topic, JsonNode? payload)
    {
        if (topic == "trigger.begin") Engine.Log($"Player trigger with {payload?["other"]}");
    }
    public override void OnDestroy()
    {
        Engine.StopAudio(music);
        Engine.Log($"Player OnDestroy: update={updates}; fixed={steps}");
    }
}
