namespace TwoDog;

/// <summary>Queries and forces backed by the engine's native Box2D world. Use on the main thread.</summary>
public static class Physics2D
{
    /// <summary>Casts a segment from origin to origin + translation, in world units. Returns null on a miss.</summary>
    public static Hit? Raycast(Vec2 origin, Vec2 translation, Entity? ignore = null) => Engine.Raycast(origin, translation, ignore);
    /// <summary>Applies a force to an entity with a Rigidbody2D. Prefer OnFixedUpdate.</summary>
    public static void AddForce(Entity entity, Vec2 force) => entity.Force(force);
    /// <summary>Applies an instantaneous impulse to an entity with a Rigidbody2D.</summary>
    public static void AddImpulse(Entity entity, Vec2 impulse) => entity.Impulse(impulse);
}
