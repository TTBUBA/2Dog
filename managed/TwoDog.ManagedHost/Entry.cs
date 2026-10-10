using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Text.Json.Nodes;
using TwoDog;
namespace TwoDog.ManagedHost;

internal sealed class GameContext : AssemblyLoadContext
{
    private readonly AssemblyDependencyResolver resolver;
    internal GameContext(string path) : base($"Game-{Guid.NewGuid()}", isCollectible: true) => resolver = new(path);
    protected override Assembly? Load(AssemblyName name)
    {
        if (name.Name == typeof(Behaviour).Assembly.GetName().Name) return typeof(Behaviour).Assembly;
        var path = resolver.ResolveAssemblyToPath(name);
        if (path is null) return null;
        using var stream = new MemoryStream(File.ReadAllBytes(path));
        return LoadFromStream(stream);
    }
    protected override nint LoadUnmanagedDll(string name) => throw new NotSupportedException("Game scripts cannot load native plug-ins through this context.");
}
internal sealed record Binding(string Entity, string Slot, string Type, int Order, JsonObject Properties)
{
    internal string Key => Entity + "\n" + Slot;
    internal static Binding[] Read(JsonArray array) => array.Select(n =>
    {
        var j = n!.AsObject();
        if ((j["language"]?.GetValue<string>() ?? "C#") != "C#") throw new ArgumentException("Unsupported script language.");
        return new Binding(j["entity"]!.GetValue<string>(), j["slot"]!.GetValue<string>(), j["type"]!.GetValue<string>(), j["order"]!.GetValue<int>(), (JsonObject?)j["properties"]?.DeepClone() ?? new());
    }).OrderBy(b => b.Order).ThenBy(b => b.Entity, StringComparer.Ordinal).ThenBy(b => b.Slot, StringComparer.Ordinal).ToArray();
}
internal sealed class Slot(Binding binding, Behaviour instance)
{
    internal Binding Binding = binding;
    internal Behaviour Instance = instance;
    internal bool Failed;
    internal void Invoke(string phase, Action<Behaviour> action, bool destroying = false)
    {
        if (Failed && !destroying) return;
        try { action(Instance); }
        catch (Exception error)
        {
            Failed = true;
            Console.Error.WriteLine($"[2Dog][C#][{Binding.Entity}][{Binding.Type}][{phase}] {error}");
        }
    }
}
internal sealed class Session
{
    internal readonly GameContext Context;
    internal readonly Assembly Assembly;
    internal Binding[] Bindings;
    internal readonly List<Slot> Slots = [];
    internal Session(string path, Binding[] bindings)
    {
        Context = new(path); Bindings = bindings;
        try
        {
            using var dll = new MemoryStream(File.ReadAllBytes(path));
            var pdbPath = Path.ChangeExtension(path, ".pdb");
            if (File.Exists(pdbPath))
            {
                using var pdb = new MemoryStream(File.ReadAllBytes(pdbPath)); Assembly = Context.LoadFromStream(dll, pdb);
            }
            else Assembly = Context.LoadFromStream(dll);
            Validate(bindings);
        }
        catch { Context.Unload(); throw; }
    }
    internal void Validate(Binding[] bindings)
    {
        var keys = new HashSet<string>(StringComparer.Ordinal);
        foreach (var binding in bindings)
        {
            if (!keys.Add(binding.Key)) throw new ArgumentException("Duplicate script binding.");
            var type = Assembly.GetType(binding.Type, throwOnError: true)!;
            if (!typeof(Behaviour).IsAssignableFrom(type) || type.IsAbstract || type.GetConstructor(Type.EmptyTypes) is null)
                throw new ArgumentException($"{binding.Type} must be a concrete Behaviour with a public parameterless constructor.");
        }
    }
    internal void Sync(Binding[] bindings)
    {
        // Newly instantiated prefabs may have a bad script type. Isolate that binding.
        bindings = bindings.Where(binding =>
        {
            try { Validate([binding]); return true; }
            catch (Exception error)
            {
                Console.Error.WriteLine($"[2Dog][C#][{binding.Entity}][{binding.Type}][binding] {error.Message}");
                return false;
            }
        }).ToArray();
        var requested = bindings.ToDictionary(x => x.Key, StringComparer.Ordinal);
        foreach (var slot in Slots.ToArray())
        {
            if (!requested.TryGetValue(slot.Binding.Key, out var b) || b.Type != slot.Binding.Type)
            {
                slot.Invoke("OnDestroy", x => x.OnDestroy(), true); Slots.Remove(slot);
            }
        }
        foreach (var binding in bindings)
        {
            if (Slots.Any(x => x.Binding.Key == binding.Key)) continue;
            try
            {
                var instance = (Behaviour)Activator.CreateInstance(Assembly.GetType(binding.Type, true)!)!;
                instance.Entity = new(binding.Entity); instance.Properties = (JsonObject)binding.Properties.DeepClone();
                var slot = new Slot(binding, instance); Slots.Add(slot); slot.Invoke("OnCreate", x => x.OnCreate());
            }
            catch (Exception error) { Console.Error.WriteLine($"[2Dog][C#][{binding.Entity}][{binding.Type}][constructor] {error}"); }
        }
        Slots.Sort((a, b) =>
        {
            int order = a.Binding.Order.CompareTo(b.Binding.Order);
            if (order == 0) order = StringComparer.Ordinal.Compare(a.Binding.Entity, b.Binding.Entity);
            return order == 0 ? StringComparer.Ordinal.Compare(a.Binding.Slot, b.Binding.Slot) : order;
        });
        Bindings = bindings;
    }
    internal void Stop()
    {
        // Same deterministic order as updates; all destruction callbacks get one attempt.
        foreach (var slot in Slots) slot.Invoke("OnDestroy", x => x.OnDestroy(), true);
        Slots.Clear(); Bindings = [];
        Context.Unload();
    }
}
public static unsafe class Entry
{
    private static Session? active, prepared;
    private static readonly List<WeakReference> retired = [];
    private static void Retire(Session? session)
    {
        if (session is null) return;
        var weak = new WeakReference(session.Context); session.Stop(); retired.Add(weak);
    }
    // No exception is allowed to cross the C ABI boundary.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static int Command(int operation, byte* json, double delta, nint callback, nint context)
    {
        try
        {
            string text = Marshal.PtrToStringUTF8((nint)json) ?? "";
            switch (operation)
            {
                case 0: Native.Configure(callback, context); break;
                case 1:
                {
                    var request = JsonNode.Parse(text)!;
                    if (request["api"]!.GetValue<int>() != RawApi.Version) throw new InvalidOperationException("Engine binding ABI mismatch.");
                    var candidate = new Session(Path.GetFullPath(request["assembly"]!.GetValue<string>()), Binding.Read(request["bindings"]!.AsArray()));
                    Retire(prepared); prepared = candidate;
                    Console.WriteLine("[2Dog][C#] Assembly prepared."); break;
                }
                case 2:
                {
                    if (prepared is null) throw new InvalidOperationException("No prepared game assembly.");
                    Retire(active); active = prepared; prepared = null; active.Sync(active.Bindings);
                    Console.WriteLine("[2Dog][C#] Play / reload committed."); break;
                }
                case 3:
                    if (active is not null) foreach (var slot in active.Slots) slot.Invoke("OnUpdate", b => b.OnUpdate((float)delta));
                    break;
                case 4:
                    if (active is not null) foreach (var slot in active.Slots) slot.Invoke("OnFixedUpdate", b => b.OnFixedUpdate((float)delta));
                    break;
                case 5:
                {
                    var message = JsonNode.Parse(text)!; string entity = message["entity"]!.GetValue<string>(), topic = message["topic"]!.GetValue<string>();
                    if (active is not null) foreach (var slot in active.Slots)
                        if (entity.Length == 0 || slot.Binding.Entity == entity)
                            slot.Invoke("OnMessage", b => b.OnMessage(topic, message["payload"]?.DeepClone()));
                    break;
                }
                case 6:
                    Retire(active); Retire(prepared); active = prepared = null;
                    Console.WriteLine("[2Dog][C#] Stop: instances released; collectible context scheduled for unload.");
                    break;
                case 7:
                    active?.Sync(Binding.Read(JsonNode.Parse(text)!.AsArray()));
                    break;
                case 8:
                    Retire(active); active = null; break;
                default: throw new ArgumentException("Unknown host operation.");
            }
            retired.RemoveAll(w => !w.IsAlive);
            if (retired.Count > 8) Console.Error.WriteLine("[2Dog][C#] Several old contexts remain alive. Check script threads, timers and event subscriptions.");
            return 0;
        }
        catch (Exception error) { Console.Error.WriteLine($"[2Dog][C#][host:{operation}] {error}"); return -1; }
    }
}
