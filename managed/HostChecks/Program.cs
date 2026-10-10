using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json.Nodes;
using TwoDog.ManagedHost;

internal static unsafe class Program
{
    private static readonly List<string> events = [];
    private static int passed;
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int Native(nint context, int op, byte* input, int length, byte* output, int capacity)
    {
        try
        {
            var request = JsonNode.Parse(new ReadOnlySpan<byte>(input, length))!;
            string result = "null";
            if (op == 1) events.Add(request["message"]!.GetValue<string>());
            if (op == 2)
            {
                byte[] error = Encoding.UTF8.GetBytes("intentional native rejection\0");
                error.CopyTo(new Span<byte>(output, capacity)); return -1;
            }
            byte[] bytes = Encoding.UTF8.GetBytes(result);
            bytes.CopyTo(new Span<byte>(output, capacity)); return bytes.Length;
        }
        catch { return -1; }
    }
    private static int Call(int op, JsonNode? data = null, double dt = .016)
    {
        byte[] bytes = Encoding.UTF8.GetBytes((data?.ToJsonString() ?? "") + "\0");
        delegate* unmanaged[Cdecl]<int, byte*, double, nint, nint, int> command = &Entry.Command;
        delegate* unmanaged[Cdecl]<nint, int, byte*, int, byte*, int, int> native = &Native;
        fixed (byte* p = bytes) return command(op, p, dt, (nint)native, 0);
    }
    private static JsonObject Binding(string label, int order = 0, string type = "Recorder") => new()
    {
        ["entity"] = label, ["slot"] = "main", ["type"] = "TestScripts." + type, ["order"] = order,
        ["language"] = "C#", ["properties"] = new JsonObject { ["label"] = label }
    };
    private static JsonObject Load(string path, params JsonObject[] bindings) => new()
    {
        ["assembly"] = path, ["api"] = 1, ["bindings"] = new JsonArray(bindings.Select(b => (JsonNode)b).ToArray())
    };
    private static void Check(string name, bool success)
    {
        if (!success) throw new Exception("FAIL " + name + ": " + string.Join(", ", events));
        Console.WriteLine("PASS " + name); ++passed;
    }
    private static int Main(string[] args)
    {
        try
        {
            if (args.Length != 1) throw new ArgumentException("Pass path to TestScripts.dll");
            string path = Path.GetFullPath(args[0]);
            Check("math_helpers", TwoDog.Mathf.Lerp(0, 10, 2) == 10 &&
                  TwoDog.Mathf.MoveTowards(9, 10, 5) == 10 && TwoDog.Mathf.MoveTowards(5, 0, 2) == 3);
            Check("initialize", Call(0) == 0);
            TwoDog.Debug.Log("facade-info"); TwoDog.Debug.LogWarning("facade-warning"); TwoDog.Debug.LogError("facade-error");
            Check("debug_facade_native_bridge", events.SequenceEqual(["facade-info", "facade-warning", "facade-error"]));
            events.Clear();
            Check("physics_facade_miss", TwoDog.Physics2D.Raycast(new(0, 0), new(1, 0)) is null);
            Check("prepare_no_lifecycle", Call(1, Load(path, Binding("B", 10), Binding("A", -10))) == 0 && events.Count == 0);
            Check("create_order", Call(2) == 0 && events.SequenceEqual(["create:A:1", "create:B:2"]));
            events.Clear(); Call(3); Call(4);
            Check("update_and_fixed_order", events.SequenceEqual(["update:A", "update:B", "fixed:A", "fixed:B"]));
            events.Clear(); Call(5, new JsonObject { ["entity"] = "B", ["topic"] = "ping", ["payload"] = null });
            Check("targeted_message", events.SequenceEqual(["message:B:ping"]));
            events.Clear(); Call(5, new JsonObject { ["entity"] = "", ["topic"] = "all", ["payload"] = null });
            Check("broadcast_order", events.SequenceEqual(["message:A:all", "message:B:all"]));
            events.Clear(); Check("invalid_reload_rejected", Call(1, Load(path, Binding("bad", 0, "MissingType"))) != 0);
            Call(3); Check("old_scripts_survive_reload_error", events.SequenceEqual(["update:A", "update:B"]));
            events.Clear(); Call(7, new JsonArray(Binding("A", -10)));
            Check("destroy_removed_instance_once", events.SequenceEqual(["destroy:B"]));
            events.Clear(); Call(6); Call(6); Check("stop_idempotent", events.SequenceEqual(["destroy:A"]));
            events.Clear(); Call(1, Load(path, Binding("reset"))); Call(2);
            Check("new_context_resets_statics", events.SequenceEqual(["create:reset:1"]));
            Call(6); events.Clear(); Call(1, Load(path, Binding("fault", -10, "Fault"), Binding("healthy"))); Call(2); events.Clear(); Call(3); Call(3);
            Check("script_exception_isolation", events.Count(e => e == "fault:update") == 1 && events.Count(e => e == "update:healthy") == 2);
            events.Clear(); Call(6); Check("failed_script_destroyed", events.Contains("fault:destroy"));
            events.Clear(); Call(1, Load(path, Binding("thread", 0, "ThreadProbe"), Binding("error", 0, "ApiErrorProbe"))); Call(2);
            Check("reject_background_thread", events.Contains("thread:rejected") && !events.Contains("WRONG-THREAD"));
            Check("native_error_becomes_exception", events.Contains("native-error:intentional native rejection"));
            Call(6); Console.WriteLine($"C# HOST CHECKS: {passed} passed"); return 0;
        }
        catch (Exception error) { Console.Error.WriteLine(error); return 1; }
    }
}
