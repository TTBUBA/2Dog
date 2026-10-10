using TwoDog;
namespace GameScripts;

// Attach to an object with Rigidbody2D and SpriteRenderer.
// Optional: name a UI Label "ScoreLabel". Uses the existing "left"/"right" actions.
public sealed class TypedComponentsDemo : Behaviour
{
    private Rigidbody2D body = null!;
    private SpriteRenderer sprite = null!;
    private UIText? label;
    private float elapsed;
    private int lastSecond = -1;

    public override void OnCreate()
    {
        body = Entity.RequireComponent<Rigidbody2D>();
        sprite = Entity.RequireComponent<SpriteRenderer>();
        var hud = Engine.Find("ScoreLabel");
        if (!string.IsNullOrEmpty(hud.Id)) label = hud.GetComponent<UIText>();
        sprite.Color = new Color(.3f, .8f, .6f);
    }

    public override void OnFixedUpdate(float fixedDeltaTime)
    {
        float direction = (Engine.Input("right").Down ? 1 : 0) - (Engine.Input("left").Down ? 1 : 0);
        body.Velocity = body.Velocity with { X = direction * 4 };
    }

    public override void OnUpdate(float deltaTime)
    {
        elapsed += deltaTime;
        int second = (int)elapsed;
        if (second != lastSecond && label is not null && label.Entity.Alive)
        {
            label.Text = $"Time: {second}";
            lastSecond = second;
        }
    }
}
