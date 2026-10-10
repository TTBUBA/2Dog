using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json.Nodes;
using TwoDog;
using TwoDog.ManagedHost;

internal static unsafe class Program
{
    static readonly Dictionary<string, JsonObject> components = new()
    {
        ["body"] = JsonNode.Parse("""{"type":"dynamic","fixedRotation":true,"colliders":[{}]}""")!.AsObject(),
        ["sprite"] = JsonNode.Parse("""{"texture":"","color":[1,1,1,1],"uv":[0,0,1,1],"layer":0,"custom":"preserve"}""")!.AsObject(),
        ["ui"] = JsonNode.Parse("""{"kind":"Label","text":"before","parent":"canvas","textColor":[1,1,1,1]}""")!.AsObject()
    };
    static Vec2 velocity;
    static int lastOperation, writes, passed;
    static bool alive = true;
    static bool rejectWrite;
    static void Check(string name, bool value)
    {
        if (!value) throw new Exception("FAIL " + name);
        ++passed; Console.WriteLine("PASS " + name);
    }
    static bool Reject(Action action)
    {
        try { action(); return false; } catch (InvalidOperationException) { return true; }
        catch (ArgumentException) { return true; }
    }
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int Native(nint context, int operation, byte* input, int length, byte* output, int capacity)
    {
        try
        {
            var request = JsonNode.Parse(new ReadOnlySpan<byte>(input, length))!;
            if (request["entity"]!.GetValue<string>() != "player") throw new InvalidOperationException("wrong entity");
            if (!alive) throw new InvalidOperationException("Expired entity");
            JsonNode? result = null;
            lastOperation = operation;
            switch (operation)
            {
                case 22: components.TryGetValue(request["component"]!.GetValue<string>(), out var component); result = component; break;
                case 23:
                    if (rejectWrite) throw new InvalidOperationException("native validation rejected edit");
                    components[request["component"]!.GetValue<string>()] = request["value"]!.DeepClone().AsObject();
                    ++writes; break;
                case 12: result = new JsonArray(velocity.X, velocity.Y); break;
                case 13: velocity = new(request["value"]![0]!.GetValue<float>(), request["value"]![1]!.GetValue<float>()); break;
                case 14: case 15: break;
                default: throw new InvalidOperationException("Unexpected operation");
            }
            var bytes = Encoding.UTF8.GetBytes(result?.ToJsonString() ?? "null");
            bytes.CopyTo(new Span<byte>(output, capacity)); return bytes.Length;
        }
        catch (Exception e)
        {
            var bytes = Encoding.UTF8.GetBytes(e.Message + "\0");
            bytes.CopyTo(new Span<byte>(output, capacity)); return -1;
        }
    }
    static int Main()
    {
        try
        {
            delegate* unmanaged[Cdecl]<int, byte*, double, nint, nint, int> entry = &Entry.Command;
            delegate* unmanaged[Cdecl]<nint, int, byte*, int, byte*, int, int> callback = &Native;
            if (entry(0, null, 0, (nint)callback, 0) != 0) throw new Exception("Host initialization failed");
            var entity = new Entity("player");
            var body = entity.RequireComponent<Rigidbody2D>();
            Check("typed_body_configuration", body.BodyType == RigidbodyType2D.Dynamic && body.FixedRotation && body.ColliderCount == 1);
            body.Velocity = new(2, 3);
            Check("live_velocity", body.Velocity == new Vec2(2, 3));
            body.AddForce(new(1, 0)); Check("force_opcode", lastOperation == 15);
            body.AddImpulse(new(0, 1)); Check("impulse_opcode", lastOperation == 14);
            var sprite = entity.RequireComponent<SpriteRenderer>();
            sprite.Color = new(.2f, .3f, .4f); sprite.SortingLayer = 4;
            Check("sprite_updates_preserve_other_fields", sprite.SortingLayer == 4 && components["sprite"]["custom"]!.GetValue<string>() == "preserve" && sprite.Color.R == .2f);
            components["sprite"]["layer"] = 9;
            Check("no_stale_snapshot", sprite.SortingLayer == 9);
            var text = entity.RequireComponent<UIText>();
            text.Text = "Score: 10"; text.Visible = false;
            Check("text_updates_preserve_parent", text.Text == "Score: 10" && !text.Visible && components["ui"]["parent"]!.GetValue<string>() == "canvas");
            int before = writes;
            Check("invalid_values_never_write", Reject(() => text.FontScale = 0) && Reject(() => sprite.Color = new(2, 0, 0)) && Reject(() => body.Velocity = new(float.NaN, 0)) && writes == before);
            rejectWrite = true;
            Check("native_validation_propagates", Reject(() => text.Text = "bad") && text.Text == "Score: 10");
            rejectWrite = false;
            components["ui"]["kind"] = "Image";
            Check("ui_kind_and_existing_handle_checked", entity.GetComponent<UIText>() is null && Reject(() => text.Text = "bad"));
            components.Remove("sprite");
            Check("missing_component_contract", !entity.TryGetComponent<SpriteRenderer>(out var missing) && missing is null && Reject(() => entity.RequireComponent<SpriteRenderer>()) && Reject(() => sprite.Depth = 1));
            Check("main_thread_only", Task.Run(() => Reject(() => entity.GetComponent<Rigidbody2D>())).GetAwaiter().GetResult());
            alive = false;
            Check("expired_entity_rejected", Reject(() => body.AddImpulse(new(1, 0))) && Reject(() => entity.GetComponent<Rigidbody2D>()));
            Console.WriteLine($"TYPED COMPONENT CHECKS: {passed} passed (simulated native bridge)");
            return 0;
        }
        catch (Exception e) { Console.Error.WriteLine(e); return 1; }
    }
}
