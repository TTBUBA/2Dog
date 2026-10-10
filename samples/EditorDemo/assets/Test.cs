using TwoDog;

namespace GameScripts;

public sealed class Test : Behaviour
{
    public override void OnCreate()
    {
        Engine.Log(Requirement<Rigidbody2D>());
    }

    public override void OnUpdate(float deltaTime)
    {
        // Game logic runs here while Play is active.
    }

    public override void OnFixedUpdate(float fixedDeltaTime)
    {
        // Physics logic goes here.
    }
}
