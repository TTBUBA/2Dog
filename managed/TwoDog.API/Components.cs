using System.Diagnostics.CodeAnalysis;
using System.Text.Json.Nodes;
namespace TwoDog;

/// <summary>A live view of an existing component; it never caches component JSON.</summary>
public abstract class Component
{
    public Entity Entity { get; }
    internal abstract string Key { get; }
    internal Component(Entity entity) => Entity = entity;
    internal virtual bool Accepts(JsonObject value) => true;
    internal bool Exists()
    {
        var value = Entity.Component(Key);
        return value is not null && Accepts(value);
    }
    internal JsonObject Read()
    {
        var value = Entity.Component(Key);
        if (value is null || !Accepts(value))
            throw new InvalidOperationException($"Entity '{Entity.Id}' has no {GetType().Name} component.");
        return value;
    }
    internal void Write(string property, JsonNode? value)
    {
        var document = Read();
        document[property] = value;
        Entity.Component(Key, document);
    }
    internal static void Finite(float value, string name, float limit = 100000)
    {
        if (!float.IsFinite(value) || MathF.Abs(value) > limit)
            throw new ArgumentOutOfRangeException(name);
    }
    internal static void Finite(Vec2 value, string name) { Finite(value.X, name); Finite(value.Y, name); }
    internal static Color ReadColor(JsonNode? value) => value is null ? new(1, 1, 1, 1) :
        new(value[0]!.GetValue<float>(), value[1]!.GetValue<float>(), value[2]!.GetValue<float>(), value[3]!.GetValue<float>());
    internal static void ValidateColor(Color value, bool unitRange)
    {
        foreach (float channel in new[] { value.R, value.G, value.B, value.A })
        {
            Finite(channel, nameof(value));
            if (unitRange && (channel < 0 || channel > 1)) throw new ArgumentOutOfRangeException(nameof(value));
        }
    }
}

public sealed partial record Entity
{
    /// <summary>Returns an existing typed component, or null if absent. Expired entities still report native errors.</summary>
    public T? GetComponent<T>() where T : Component
    {
        Component component = typeof(T) == typeof(Rigidbody2D) ? new Rigidbody2D(this) :
            typeof(T) == typeof(SpriteRenderer) ? new SpriteRenderer(this) :
            typeof(T) == typeof(UIText) ? new UIText(this) :
            throw new NotSupportedException($"Typed component {typeof(T).Name} is not supported.");
        return component.Exists() ? (T)component : null;
    }
    public bool TryGetComponent<T>([NotNullWhen(true)] out T? component) where T : Component
    {
        component = GetComponent<T>();
        return component is not null;
    }
    public T RequireComponent<T>() where T : Component => GetComponent<T>()
        ?? throw new InvalidOperationException($"Entity '{Id}' has no {typeof(T).Name} component.");
}

public enum RigidbodyType2D { Static, Kinematic, Dynamic }

/// <summary>Existing native body. Configure type, colliders and flags in the editor before Play.</summary>
public sealed class Rigidbody2D : Component
{
    internal Rigidbody2D(Entity entity) : base(entity) { }
    internal override string Key => "body";
    public RigidbodyType2D BodyType => (Read()["type"]?.GetValue<string>() ?? "dynamic") switch
    {
        "static" => RigidbodyType2D.Static,
        "kinematic" => RigidbodyType2D.Kinematic,
        "dynamic" => RigidbodyType2D.Dynamic,
        _ => throw new InvalidOperationException("Unknown native body type.")
    };
    public bool FixedRotation => Read()["fixedRotation"]?.GetValue<bool>() ?? false;
    public bool Bullet => Read()["bullet"]?.GetValue<bool>() ?? false;
    public int ColliderCount => Read()["colliders"]!.AsArray().Count;
    public Vec2 Velocity
    {
        get => Entity.Velocity;
        set { Finite(value, nameof(value)); Entity.Velocity = value; }
    }
    public void AddForce(Vec2 force) { Finite(force, nameof(force)); Entity.Force(force); }
    public void AddImpulse(Vec2 impulse) { Finite(impulse, nameof(impulse)); Entity.Impulse(impulse); }
}

/// <summary>Normalized texture coordinates; each coordinate must be in the range 0..1.</summary>
public readonly record struct SpriteUV(float U0, float V0, float U1, float V1);

public sealed class SpriteRenderer : Component
{
    internal SpriteRenderer(Entity entity) : base(entity) { }
    internal override string Key => "sprite";
    public string TextureId
    {
        get => Read()["texture"]?.GetValue<string>() ?? "";
        set { ArgumentNullException.ThrowIfNull(value); Write("texture", JsonValue.Create(value)); }
    }
    public Color Color
    {
        get => ReadColor(Read()["color"]);
        set { ValidateColor(value, true); Write("color", value.Json()); }
    }
    public int SortingLayer
    {
        get => Read()["layer"]?.GetValue<int>() ?? 0;
        set => Write("layer", JsonValue.Create(value));
    }
    public float Depth
    {
        get => Read()["depth"]?.GetValue<float>() ?? 0;
        set { Finite(value, nameof(value)); Write("depth", JsonValue.Create(value)); }
    }
    public Vec2 Parallax
    {
        get { var value = Read()["parallax"]; return value is null ? new(1, 1) : Vec2.Read(value); }
        set { Finite(value, nameof(value)); Write("parallax", value.Json()); }
    }
    public SpriteUV UV
    {
        get
        {
            var value = Read()["uv"];
            return value is null ? new(0, 0, 1, 1) : new(value[0]!.GetValue<float>(), value[1]!.GetValue<float>(), value[2]!.GetValue<float>(), value[3]!.GetValue<float>());
        }
        set
        {
            Finite(value.U0, nameof(value)); Finite(value.V0, nameof(value));
            Finite(value.U1, nameof(value)); Finite(value.V1, nameof(value));
            if (value.U0 < 0 || value.U0 > 1 || value.V0 < 0 || value.V0 > 1 || value.U1 < 0 || value.U1 > 1 || value.V1 < 0 || value.V1 > 1) throw new ArgumentOutOfRangeException(nameof(value));
            Write("uv", new JsonArray(value.U0, value.V0, value.U1, value.V1));
        }
    }
}

/// <summary>Text on a UI Label or Button. World text remains a separate component.</summary>
public sealed class UIText : Component
{
    internal UIText(Entity entity) : base(entity) { }
    internal override string Key => "ui";
    internal override bool Accepts(JsonObject value) => value["kind"]?.GetValue<string>() is "Label" or "Button";
    public string Text
    {
        get => Read()["text"]?.GetValue<string>() ?? "";
        set { ArgumentNullException.ThrowIfNull(value); Write("text", JsonValue.Create(value)); }
    }
    public Color Color
    {
        get => ReadColor(Read()["textColor"]);
        set { ValidateColor(value, true); Write("textColor", value.Json()); }
    }
    public string FontId
    {
        get => Read()["font"]?.GetValue<string>() ?? "bitmap";
        set { ArgumentNullException.ThrowIfNull(value); Write("font", JsonValue.Create(value)); }
    }
    public float FontScale
    {
        get => Read()["fontScale"]?.GetValue<float>() ?? 2;
        set
        {
            if (!float.IsFinite(value) || value <= 0 || value > 10) throw new ArgumentOutOfRangeException(nameof(value));
            Write("fontScale", JsonValue.Create(value));
        }
    }
    public bool Visible
    {
        get => Read()["visible"]?.GetValue<bool>() ?? true;
        set => Write("visible", JsonValue.Create(value));
    }
}
