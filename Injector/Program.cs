using System;
using System.Diagnostics;
using System.Runtime.Remoting;
using EasyHook;

namespace Injector;

class Program
{
    static void Main(string[] args)
    {
        Console.WriteLine("Starting Worldwide Rush mod injector.");

        // Name of the target MonoGame process
        string gameProcessName = "Worldwide Rush";
        Process? gameProcess = null;

        // Find the running game process
        foreach (var p in Process.GetProcessesByName(gameProcessName))
        //foreach (var p in Process.GetProcesses())
        {
            //Console.WriteLine($"{p.ProcessName} {p.Id}");
            gameProcess = p;
            break;
        }

        if (gameProcess == null)
        {
            Console.WriteLine($"Game process {gameProcessName} not found.");
            return;
        }

        Console.WriteLine($"Game process {gameProcessName} found at {gameProcess.Id}.");

        string bootstrapperPath = "Bootstrapper.dll";

        try
        {
            Console.WriteLine($"Injecting {bootstrapperPath} into PID {gameProcess.Id}...");

            // Inject into the target process
            /*
            RemoteHooking.Inject(
                gameProcess.Id,
                InjectionOptions.Default,
                bootstrapperPath,  // 32-bit DLL (if game is x86)
                bootstrapperPath   // 64-bit DLL (if game is x64)
            );
            */

            // Inject native DLL
            NativeAPI.RhInjectLibrary(
                gameProcess.Id,
                0,
                (int)InjectionOptions.Default,
                bootstrapperPath, // 32-bit DLL path, can be null if target is x64
                bootstrapperPath, // 64-bit DLL path
                IntPtr.Zero,
                0
            );

            Console.WriteLine("Injection successful!");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Injection failed: {ex.Message}");
        }
        
    }
}
