using System;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using HarmonyLib;

namespace TestMod;

public static class ModEntry
{
    public static readonly string harmonyId = "Infixo." + nameof(TestMod);

    // mod's instance and asset
    //public static Mod instance { get; private set; }
    //public static ExecutableAsset modAsset { get; private set; }
    // logging
    //public static ILog log = LogManager.GetLogger($"{nameof(InfoLoom)}").SetShowsErrorsInUI(false);

    //[UnmanagedCallersOnly(EntryPoint = "InitializeMod")] // not needed when called via CLR
    //[ModuleInitializer] // only works with CLR, not native loads
    public static void InitializeMod()
    {
        //log.Info(nameof(OnLoad));
        // Harmony
        Log.Write($"Initialize called. HarmonyId is {harmonyId}.");
        var harmony = new Harmony(harmonyId);
        //harmony.PatchAll(typeof(Mod).Assembly);
        harmony.PatchAll();
        var patchedMethods = harmony.GetPatchedMethods().ToArray();
        //log.Info($"Plugin {harmonyId} made patches! Patched methods: " + patchedMethods.Length);
        foreach (var patchedMethod in patchedMethods)
        {
            Log.Write($"Patched method: {patchedMethod.Module.Name}:{patchedMethod.DeclaringType.Name}.{patchedMethod.Name}");
        }
        // do stuff here to initialize
        //return 0;
    }

    public static void OnDispose()
    {
        Log.Write(nameof(OnDispose));
        // Harmony
        var harmony = new Harmony(harmonyId);
        harmony.UnpatchAll(harmonyId);
    }
}

// Example patch
/*
[HarmonyPatch(typeof(Mod), "GetDefault")]
public static class LabelPatch
{
    static void Postfix(ref Label __result)
    {
        //__result.Text += " [MODDED]";
    }
}
*/

public static class Log
{
    /// <summary>
    /// Writes a string message to a log file in the user's temporary directory.
    /// </summary>
    /// <param name="logMessage">The message to write to the log.</param>
    public static void Write(string logMessage)
    {
        // Simple console output
        Console.WriteLine(logMessage);

        // Get the path to the user's temporary directory.
        string tempPath = Path.GetTempPath();

        // Combine the temporary path with the log file name.
        string filePath = Path.Combine(tempPath, "testmod.txt");

        try
        {
            // Create a StreamWriter in append mode. This will create the file if it doesn't exist,
            // or open it and add the new content to the end.
            using (StreamWriter writer = new StreamWriter(filePath, true))
            {
                // Format the current date and time to prepend to the message.
                string timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
                // Write the log message and a new line.
                writer.WriteLine($"[{timestamp}] {logMessage}");
            }
        }
        catch (Exception ex)
        {
            // Log any potential errors to the console.
            Console.WriteLine($"Error writing to log file: {ex.Message}");
        }
    }

}
