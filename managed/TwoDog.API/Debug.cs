namespace TwoDog;

/// <summary>Writes messages to the 2Dog editor Console and player output.</summary>
public static class Debug
{
    /// <summary>Logs an informational message. Call from a Behaviour on the engine main thread.</summary>
    public static void Log(object? message) => Engine.Log(message?.ToString() ?? "null");
    /// <summary>Logs a warning without stopping the game.</summary>
    public static void LogWarning(object? message) => Engine.Log(message?.ToString() ?? "null", "warning");
    /// <summary>Logs an error without throwing an exception.</summary>
    public static void LogError(object? message) => Engine.Log(message?.ToString() ?? "null", "error");
}
