using System.Runtime.CompilerServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
[assembly: InternalsVisibleTo("TwoDog.ManagedHost")]
namespace TwoDog;

internal static unsafe class Native
{
    private static delegate* unmanaged[Cdecl]<nint, int, byte*, int, byte*, int, int> callback;
    private static nint context;
    private static int owner;
    private static readonly byte[] response = new byte[1024 * 1024];
    private static bool busy;
    internal static void Configure(nint function, nint user)
    {
        callback = (delegate* unmanaged[Cdecl]<nint, int, byte*, int, byte*, int, int>)function;
        context = user; owner = Environment.CurrentManagedThreadId;
    }
    internal static JsonNode? Call(int code, JsonObject request)
    {
        if (callback == null || Environment.CurrentManagedThreadId != owner)
            throw new InvalidOperationException("Engine API is available only on the engine main thread.");
        if (busy) throw new InvalidOperationException("Recursive native API call.");
        byte[] bytes = Encoding.UTF8.GetBytes(request.ToJsonString());
        if (bytes.Length > 1024 * 1024) throw new ArgumentException("API request exceeds 1 MiB.");
        busy = true;
        try
        {
            fixed (byte* input = bytes)
            fixed (byte* output = response)
            {
                int length = callback(context, code, input, bytes.Length, output, response.Length);
                if (length < 0)
                {
                    int end = Array.IndexOf(response, (byte)0);
                    throw new InvalidOperationException(Encoding.UTF8.GetString(response, 0, end < 0 ? response.Length : end));
                }
                return JsonNode.Parse(response.AsSpan(0, length));
            }
        }
        finally { busy = false; }
    }
}
public readonly record struct Vec2(float X, float Y)
{
    internal JsonArray Json() => new(X, Y);
    internal static Vec2 Read(JsonNode node) => new(node[0]!.GetValue<float>(), node[1]!.GetValue<float>());
    public static Vec2 operator +(Vec2 a, Vec2 b) => new(a.X + b.X, a.Y + b.Y);
    public static Vec2 operator *(Vec2 a, float b) => new(a.X * b, a.Y * b);
}
public readonly record struct Color(float R, float G, float B, float A = 1)
{
    internal JsonArray Json() => new(R, G, B, A);
}
public readonly record struct Transform(Vec2 Position, Vec2 Size, float Rotation = 0)
{
    internal JsonObject Json() => new() { ["position"] = Position.Json(), ["size"] = Size.Json(), ["rotation"] = Rotation };
    internal static Transform Read(JsonNode node) => new(Vec2.Read(node["position"]!), Vec2.Read(node["size"]!), node["rotation"]!.GetValue<float>());
}
public readonly record struct ActionState(bool Down, bool Pressed, bool Released);
public readonly record struct Hit(Entity Entity, Vec2 Point, Vec2 Normal, float Fraction);
public sealed partial record Entity(string Id)
{
    public bool Alive => !string.IsNullOrEmpty(Id) && RawApi.Alive(new() { ["entity"] = Id })!.GetValue<bool>();
    public Transform Transform
    {
        get => global::TwoDog.Transform.Read(RawApi.GetTransform(new() { ["entity"] = Id })!);
        set => RawApi.SetTransform(new() { ["entity"] = Id, ["transform"] = value.Json() });
    }
    public Vec2 Velocity
    {
        get => Vec2.Read(RawApi.GetVelocity(new() { ["entity"] = Id })!);
        set => RawApi.SetVelocity(new() { ["entity"] = Id, ["value"] = value.Json() });
    }
    public void Destroy() => RawApi.Destroy(new() { ["entity"] = Id });
    public void Impulse(Vec2 value) => RawApi.Impulse(new() { ["entity"] = Id, ["value"] = value.Json() });
    public void Force(Vec2 value) => RawApi.Force(new() { ["entity"] = Id, ["value"] = value.Json() });
    public JsonObject? Component(string name) => RawApi.GetComponent(new() { ["entity"] = Id, ["component"] = name }) as JsonObject;
    public void Component(string name, JsonObject value) => RawApi.SetComponent(new() { ["entity"] = Id, ["component"] = name, ["value"] = value.DeepClone() });
    public void State(string state) => RawApi.SetState(new() { ["entity"] = Id, ["state"] = state });
    public void Send(string topic, JsonNode? payload = null) => Engine.Send(this, topic, payload);
}
public static class Engine
{
    public static void Log(string message, string level = "info") => RawApi.Log(new() { ["level"] = level, ["message"] = message });
    public static Entity Find(string name) => new(RawApi.Find(new() { ["name"] = name })!.GetValue<string>());
    public static Entity Create(JsonObject document) => new(RawApi.Create(new() { ["entity"] = document.DeepClone() })!.GetValue<string>());
    public static Entity Instantiate(string prefab, Vec2 position) => new(RawApi.Instantiate(new() { ["prefab"] = prefab, ["position"] = position.Json() })!.GetValue<string>());
    public static Entity[] Entities() => RawApi.ListEntities(new())!.AsArray().Select(x => new Entity(x!.GetValue<string>())).ToArray();
    public static ActionState Input(string action)
    {
        var n = RawApi.Input(new() { ["action"] = action })!;
        return new(n["down"]!.GetValue<bool>(), n["pressed"]!.GetValue<bool>(), n["released"]!.GetValue<bool>());
    }
    public static float Axis(int axis) => RawApi.Axis(new() { ["axis"] = axis })!.GetValue<float>();
    public static Vec2 Pointer => Vec2.Read(RawApi.Pointer(new())!);
    public static Hit? Raycast(Vec2 origin, Vec2 translation, Entity? ignore = null)
    {
        var n = RawApi.Raycast(new() { ["origin"] = origin.Json(), ["translation"] = translation.Json(), ["ignore"] = ignore?.Id ?? "" });
        return n is null ? null : new Hit(new(n["entity"]!.GetValue<string>()), Vec2.Read(n["point"]!), Vec2.Read(n["normal"]!), n["fraction"]!.GetValue<float>());
    }
    public static string Play(string asset, string bus = "sfx", bool loop = false) => RawApi.PlayAudio(new() { ["asset"] = asset, ["bus"] = bus, ["loop"] = loop })!.GetValue<string>();
    public static void StopAudio(string voice) => RawApi.StopAudio(new() { ["voice"] = voice });
    public static void Volume(string bus, float value) => RawApi.Volume(new() { ["bus"] = bus, ["value"] = value });
    public static void LoadScene(string asset) => RawApi.LoadScene(new() { ["asset"] = asset });
    public static void SaveScene(string relativePath) => RawApi.SaveScene(new() { ["path"] = relativePath });
    public static void Send(Entity target, string topic, JsonNode? payload = null) => RawApi.Send(new() { ["entity"] = target.Id ?? "", ["topic"] = topic, ["payload"] = payload?.DeepClone() });
    public static void Broadcast(string topic, JsonNode? payload = null) => Send(new Entity(""), topic, payload);
    public static void Follow(Entity entity) => RawApi.Camera(new() { ["follow"] = entity.Id });
    public static void Shake(float intensity) => RawApi.Camera(new() { ["shake"] = intensity });
    public static void Emit(Vec2 position, int count, Color color) => RawApi.Emit(new() { ["position"] = position.Json(), ["count"] = count, ["color"] = color.Json() });
}
public abstract class Behaviour
{
    public Entity Entity { get; internal set; } = new("");
    // Per-instance exposed JSON properties are restored from scene data after Stop/reload.
    public JsonObject Properties { get; internal set; } = new();
    public virtual void OnCreate() { }
    public virtual void OnUpdate(float deltaTime) { }
    public virtual void OnFixedUpdate(float fixedDeltaTime) { }
    public virtual void OnDestroy() { }
    public virtual void OnMessage(string topic, JsonNode? payload) { }
}
