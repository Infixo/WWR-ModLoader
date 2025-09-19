// Injector.cs
// Simple DLL injector using CreateRemoteThread + LoadLibraryW.
// Usage:
//   Injector.exe <processName|pid> <fullPathToBootstrapperDll>
//
// Examples:
//   Injector.exe MyGame "C:\mods\bootstrapper.dll"
//   Injector.exe 12345 "C:\mods\bootstrapper.dll"

using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

class Injector
{
    // Win32 APIs
    [Flags]
    private enum ProcessAccessFlags : uint
    {
        CreateThread = 0x0002,
        QueryInformation = 0x0400,
        VMOperation = 0x0008,
        VMWrite = 0x0020,
        VMRead = 0x0010,
        DupHandle = 0x0040,
        Synchronize = 0x00100000,
        All = 0x001F0FFF
    }

    private const uint MEM_COMMIT = 0x00001000;
    private const uint MEM_RESERVE = 0x00002000;
    private const uint PAGE_READWRITE = 0x04;

    [DllImport("kernel32", SetLastError = true)]
    private static extern IntPtr OpenProcess(ProcessAccessFlags dwDesiredAccess, bool bInheritHandle, int dwProcessId);

    [DllImport("kernel32", SetLastError = true)]
    private static extern bool CloseHandle(IntPtr hObject);

    [DllImport("kernel32", SetLastError = true)]
    private static extern IntPtr VirtualAllocEx(IntPtr hProcess, IntPtr lpAddress,
        uint dwSize, uint flAllocationType, uint flProtect);

    [DllImport("kernel32", SetLastError = true)]
    private static extern bool VirtualFreeEx(IntPtr hProcess, IntPtr lpAddress,
        uint dwSize, uint dwFreeType);

    [DllImport("kernel32", SetLastError = true)]
    private static extern bool WriteProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress,
        byte[] lpBuffer, uint nSize, out UIntPtr lpNumberOfBytesWritten);

    [DllImport("kernel32", CharSet = CharSet.Ansi, SetLastError = true)] // Unicode throws an error
    private static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr GetModuleHandle(string lpModuleName);

    [DllImport("kernel32", SetLastError = true)]
    private static extern IntPtr CreateRemoteThread(IntPtr hProcess,
        IntPtr lpThreadAttributes, uint dwStackSize, IntPtr lpStartAddress,
        IntPtr lpParameter, uint dwCreationFlags, out IntPtr lpThreadId);

    [DllImport("kernel32", SetLastError = true)]
    private static extern UInt32 WaitForSingleObject(IntPtr hHandle, UInt32 dwMilliseconds);

    static int Main(string[] args)
    {
        string currentDirectory = Directory.GetCurrentDirectory();
        WriteLog($"Starting in {currentDirectory}.");

        string target = "Worldwide Rush"; //  args[0];
        if (args.Length > 0) target = args[0];

        string dllPath = currentDirectory + "\\Bootstrapper.dll"; // args[1];
        if (!File.Exists(dllPath))
        {
            WriteLog("Error: DLL not found: " + dllPath);
            return 1;
        }
        WriteLog($"Game: {target} Dll: {dllPath} 64bit: {Environment.Is64BitProcess}");

        Process? proc = null;
        /*
        if (int.TryParse(target, out int pid))
        {
            try { proc = Process.GetProcessById(pid); }
            catch (Exception ex) { Console.WriteLine("[Injector] can't open PID: " + ex.Message); return 1; }
        }
        else
        {
        */
            var procs = Process.GetProcessesByName(target);
            if (procs.Length == 0)
            {
                WriteLog("Error: no process with name: " + target);
                return 1;
            }
            // take first match
            proc = procs[0];
        //}

        WriteLog($"Target PID={proc.Id} Name={proc.ProcessName}");

        // Open target process
        IntPtr hProc = OpenProcess(
            ProcessAccessFlags.CreateThread | ProcessAccessFlags.QueryInformation |
            ProcessAccessFlags.VMOperation | ProcessAccessFlags.VMWrite | ProcessAccessFlags.VMRead,
            false, proc.Id);

        if (hProc == IntPtr.Zero)
        {
            WriteLog("OpenProcess failed, error: " + Marshal.GetLastWin32Error());
            return 1;
        }

        try
        {
            // Allocate space for the DLL path (unicode)
            byte[] dllPathBytes = Encoding.Unicode.GetBytes(dllPath + "\0");
            uint size = (uint)dllPathBytes.Length;

            IntPtr remoteMem = VirtualAllocEx(hProc, IntPtr.Zero, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (remoteMem == IntPtr.Zero)
            {
                WriteLog("VirtualAllocEx failed, error: " + Marshal.GetLastWin32Error());
                return 1;
            }

            try
            {
                if (!WriteProcessMemory(hProc, remoteMem, dllPathBytes, size, out UIntPtr written) || written.ToUInt32() != size)
                {
                    WriteLog("WriteProcessMemory failed, error: " + Marshal.GetLastWin32Error());
                    return 1;
                }

                // Get address of LoadLibraryW in local process
                IntPtr hKernel32 = GetModuleHandle("kernel32.dll");
                if (hKernel32 == IntPtr.Zero)
                {
                    WriteLog("GetModuleHandle(kernel32) failed, error: " + Marshal.GetLastWin32Error());
                    return 1;
                }

                IntPtr loadLibraryAddr = GetProcAddress(hKernel32, "LoadLibraryW");
                if (loadLibraryAddr == IntPtr.Zero)
                {
                    WriteLog("GetProcAddress(LoadLibraryW) failed, error: " + Marshal.GetLastWin32Error());
                    return 1;
                }

                // Create remote thread that calls LoadLibraryW(remoteMem)
                IntPtr hThread = CreateRemoteThread(hProc, IntPtr.Zero, 0, loadLibraryAddr, remoteMem, 0, out IntPtr threadId);
                if (hThread == IntPtr.Zero)
                {
                    WriteLog("CreateRemoteThread failed, error: " + Marshal.GetLastWin32Error());
                    return 1;
                }

                WriteLog("remote thread created (tid=" + threadId + "), waiting...");

                // Wait for the remote thread to complete (timeout 10s)
                const uint WAIT_TIMEOUT = 10000;
                WaitForSingleObject(hThread, WAIT_TIMEOUT);

                WriteLog("SUCCESS - bootstrapper should be loaded.");
            }
            finally
            {
                // Try to free the remote memory (best-effort; may fail if remote thread still running)
                VirtualFreeEx(hProc, remoteMem, 0, 0x8000); // MEM_RELEASE = 0x8000
            }
        }
        finally
        {
            CloseHandle(hProc);
        }

        return 0;
    }

    /// <summary>
    /// Writes a string message to a log file in the user's temporary directory.
    /// </summary>
    /// <param name="logMessage">The message to write to the log.</param>
    public static void WriteLog(string logMessage)
    {
        // Simple console output
        Console.WriteLine(logMessage);

        // Get the path to the user's temporary directory.
        string tempPath = Path.GetTempPath();

        // Combine the temporary path with the log file name.
        string filePath = Path.Combine(tempPath, "bootstrapper.txt");

        try
        {
            // Create a StreamWriter in append mode. This will create the file if it doesn't exist,
            // or open it and add the new content to the end.
            using (StreamWriter writer = new StreamWriter(filePath, true))
            {
                // Format the current date and time to prepend to the message.
                string timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
                // Write the log message and a new line.
                writer.WriteLine($"[{timestamp}] Injector: {logMessage}");
            }
        }
        catch (Exception ex)
        {
            // Log any potential errors to the console.
            Console.WriteLine($"Error writing to log file: {ex.Message}");
        }
    }
}
