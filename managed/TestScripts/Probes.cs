using TwoDog;
using System.Text.Json.Nodes;
namespace TestScripts;
public sealed class Recorder : Behaviour
{
    private static int instances;
    public override void OnCreate() => Engine.Log($"create:{Properties["label"]}:{++instances}");
    public override void OnUpdate(float dt) => Engine.Log($"update:{Properties["label"]}");
    public override void OnFixedUpdate(float dt) => Engine.Log($"fixed:{Properties["label"]}");
    public override void OnMessage(string topic, JsonNode? payload) => Engine.Log($"message:{Properties["label"]}:{topic}");
    public override void OnDestroy() => Engine.Log($"destroy:{Properties["label"]}");
}
public sealed class Fault : Behaviour
{
    public override void OnUpdate(float dt) { Engine.Log("fault:update"); throw new InvalidOperationException("Expected test fault"); }
    public override void OnDestroy() => Engine.Log("fault:destroy");
}
public sealed class ThreadProbe : Behaviour
{
    public override void OnCreate()
    {
        bool rejected = false;
        var thread = new Thread(() => { try { Engine.Log("WRONG-THREAD"); } catch (InvalidOperationException) { rejected = true; } });
        thread.Start(); thread.Join();
        Engine.Log(rejected ? "thread:rejected" : "thread:accepted");
    }
}
public sealed class ApiErrorProbe : Behaviour
{
    public override void OnCreate()
    {
        try { Engine.Find("native-error"); }
        catch (InvalidOperationException e) { Engine.Log("native-error:" + e.Message); }
    }
}
