using System.Text.Json.Nodes;
namespace TwoDog;

// UI uses the shared component/message bridge; no dependency on the editor.
public static class UI
{
    private static JsonObject Read(Entity entity) => entity.Component("ui")
        ?? throw new InvalidOperationException($"Entity {entity.Id} has no UI component.");
    public static string Text(Entity entity) => Read(entity)["text"]?.GetValue<string>() ?? "";
    public static void Text(Entity entity, string text)
    {
        var ui = Read(entity); ui["text"] = text; entity.Component("ui", ui);
    }
    public static float Value(Entity entity) => Read(entity)["value"]?.GetValue<float>() ?? .5f;
    public static void Value(Entity entity, float value)
    {
        if (!float.IsFinite(value) || value < 0 || value > 1) throw new ArgumentOutOfRangeException(nameof(value));
        var ui = Read(entity); ui["value"] = value; entity.Component("ui", ui);
    }
    public static void Visible(Entity entity, bool visible)
    {
        var ui = Read(entity); ui["visible"] = visible; entity.Component("ui", ui);
    }
    public static void Interactable(Entity entity, bool interactable)
    {
        var ui = Read(entity); ui["interactable"] = interactable; entity.Component("ui", ui);
    }
}
