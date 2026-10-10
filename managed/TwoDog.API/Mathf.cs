namespace TwoDog;

/// <summary>Single-precision math helpers. Calculated in C# without crossing the native API boundary.</summary>
public static class Mathf
{
    public const float PI = MathF.PI;
    public const float Deg2Rad = PI / 180f;
    public const float Rad2Deg = 180f / PI;
    public static float Abs(float value) => MathF.Abs(value);
    public static float Min(float a, float b) => MathF.Min(a, b);
    public static float Max(float a, float b) => MathF.Max(a, b);
    public static float Sqrt(float value) => MathF.Sqrt(value);
    public static float Sin(float radians) => MathF.Sin(radians);
    public static float Cos(float radians) => MathF.Cos(radians);
    /// <summary>Constrains value to [min, max]. Throws when min exceeds max.</summary>
    public static float Clamp(float value, float min, float max) => Math.Clamp(value, min, max);
    public static float Clamp01(float value) => Clamp(value, 0, 1);
    /// <summary>Interpolates from a to b with t clamped to [0, 1].</summary>
    public static float Lerp(float a, float b, float t) => a + (b - a) * Clamp01(t);
    /// <summary>Moves towards target without overshooting. maxDelta must be nonnegative.</summary>
    public static float MoveTowards(float current, float target, float maxDelta)
    {
        if (maxDelta < 0) throw new ArgumentOutOfRangeException(nameof(maxDelta));
        float delta = target - current;
        return Abs(delta) <= maxDelta ? target : current + MathF.Sign(delta) * maxDelta;
    }
}
