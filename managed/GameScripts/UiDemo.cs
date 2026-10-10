using System.Text.Json.Nodes;
using TwoDog;
namespace GameScripts;
public sealed class UiDemo : Behaviour
{
    private int count;
    public override void OnCreate()
    {
        UI.Text(Engine.Find("UI Counter"), "CRATES: 0");
        Engine.Log("UI ready: button spawns one crate; slider controls music volume.");
    }
    public override void OnMessage(string topic, JsonNode? payload)
    {
        if (topic == "demo.spawn.click")
        {
            Engine.Instantiate("prefab.crate", new Vec2(-2, 3));
            UI.Text(Engine.Find("UI Counter"), $"CRATES: {++count}");
            Engine.Log($"UI button: created crate {count}");
        }
        if (topic == "demo.volume.change")
        {
            float volume = payload?["value"]?.GetValue<float>() ?? .5f;
            Engine.Volume("music", volume);
            UI.Text(Engine.Find("UI Counter"), $"MUSIC: {(int)(volume * 100)}");
        }
    }
}
