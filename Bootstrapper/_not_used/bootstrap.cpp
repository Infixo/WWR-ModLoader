// bootstrap.cpp
// Minimal native bootstrapper (Windows). Build as a native DLL (x64 or x86 to match target).
// Inject this DLL into the target process; on load it will try to start .NET and call the managed method.
//
// Notes:
// - For reliability use nethost::get_hostfxr_path to locate hostfxr.dll. This example uses LoadLibraryW("hostfxr.dll")
//   for minimalism.
// - Make sure MyMod.dll is accessible to the target process (working dir or full path).
// - Build with Visual Studio, matching target bitness.

#include "pch.h"
#include <windows.h>
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>

//
// Logger that writes to <temp>\bootstrapper.txt file
//
static void write_log(const wchar_t* msg)
{
    // Get the current time point from the system clock
    auto now = std::chrono::system_clock::now();

    // Convert the time point to a time_t object
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);

    // Convert the time_t object to a local time structure
    std::tm time_info;
    localtime_s(&time_info, &now_c); // Use localtime_s for thread safety on Windows

    // Format the time as a wide character string
    wchar_t time_buffer[80];
    wcsftime(time_buffer, sizeof(time_buffer) / sizeof(wchar_t), L"%Y-%m-%d %H:%M:%S", &time_info);

    // open log file
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    std::wstring f = std::wstring(path) + L"bootstrapper.txt";
    //std::wstring f = L"C:\\Temp\\bootstrapper.txt";
    std::wofstream out(f, std::ios::app);

    // write formatted time
    //out << msg << L"\r\n";
    out << L"[" << time_buffer << L"] Bootstrapper: " << msg << L"\n";
    out.close();
}

// call write_log(L"before calling managed entry");
// after entry call write_log(L"after managed entry");


using load_assembly_and_get_function_pointer_fn = int(__stdcall*)(
    const char* assembly_path,
    const char* type_name,
    const char* method_name,
    const char* delegate_type_name,
    void* reserved,
    /*out*/ void** delegate);

using hostfxr_initialize_for_runtime_config_fn = int(__stdcall*)(const wchar_t*, void*, void**); //  Initializes a host context and prepares for initialization of the .NET runtime using the specified runtime configuration.
using hostfxr_get_runtime_delegate_fn = int(__stdcall*)(void*, int, void**); // Gets a delegate for runtime functionality.
using hostfxr_close_fn = int(__stdcall*)(void*); // Closes a host context.

// Minimal enum for the hostfxr delegate types (we only need the one below)
enum hostfxr_delegate_type
{
    hdt_com_activation = 0,
    hdt_load_assembly_and_get_function_pointer = 1,
    hdt_winrt_create_instance = 2
};

static HMODULE load_hostfxr()
{
    write_log(L"load_hostfxr");

    // Minimal approach: rely on hostfxr.dll being on the loader path.
    // For robustness, use nethost::get_hostfxr_path (recommended for production).
    // Details here: https://learn.microsoft.com/en-us/dotnet/core/tutorials/netcore-hosting
    return LoadLibrary(L"hostfxr.dll"); 
    /*
    LoadLibraryW is the Unicode version of the function, and it expects a wide character string(e.g., wchar_t* or LPCWSTR) as its input.
    LoadLibrary is a macro that resolves to either LoadLibraryA(the ANSI version) or LoadLibraryW based on a project's character set settings.
    */
}

using mod_bootstrap_fn = int(void*);

// This function runs on a thread (avoid doing heavy work inside DllMain).
static void initialize_dotnet_and_run()
{
    write_log(L"initialize_dotnet_and_run");

    const char* assembly_path = "TestMod.dll";
    // e.g. if your type is in a namespace: "MyNamespace.ModEntry, MyMod"
    const char* type_name = "TestMod.ModEntry, TestMod";
    const char* method_name = "InitializeMod";

    HMODULE hostfxr = load_hostfxr();
    if (!hostfxr) {
        write_log(L"initialize_dotnet_and_run: couldn't find hostfxr");
        return;
    }

    // Get exports from hostfxr (function-pointer types, not pointer-to-pointer).
    auto init_f = (hostfxr_initialize_for_runtime_config_fn)GetProcAddress(hostfxr, "hostfxr_initialize_for_runtime_config");
    auto get_delegate_f = (hostfxr_get_runtime_delegate_fn)GetProcAddress(hostfxr, "hostfxr_get_runtime_delegate");
    auto close_f = (hostfxr_close_fn)GetProcAddress(hostfxr, "hostfxr_close");

    if (!init_f || !get_delegate_f || !close_f) {
        write_log(L"initialize_dotnet_and_run: couldn't get hostfxr methods");
        return;
    }

    // Initialize host context. For simplicity we pass nullptr (no runtimeconfig).
    void* hostctx = nullptr;
    std::wstring runtimeconfig = L"TestMod.runtimeconfig.json";
    int rc = init_f(runtimeconfig.c_str(), nullptr, &hostctx); // int rc = init_f(nullptr, nullptr, &hostctx);
    if (rc != 0 || hostctx == nullptr) {
        // try fallback: still return on failure for this minimal example
        wchar_t buf[256];
        swprintf_s(buf, L"couldn't initialize host context. rc=%X", rc);
        write_log(buf);
        return;
    }
    write_log(L"host context initialized");

    // Get the runtime delegate for loading an assembly and getting a function pointer.
    void* load_assembly_and_get_function_pointer_ptr = nullptr;
    rc = get_delegate_f(hostctx, hdt_load_assembly_and_get_function_pointer, &load_assembly_and_get_function_pointer_ptr);
    if (rc != 0 || load_assembly_and_get_function_pointer_ptr == nullptr) {
        write_log(L"initialize_dotnet_and_run: couldn't get the delegate");
        close_f(hostctx);
        return;
    }

    auto load_assembly_and_get_function_pointer = (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer_ptr;

    // Ask the runtime to give us the function pointer to the UnmanagedCallersOnly method.
    void* function_ptr = nullptr;
    rc = load_assembly_and_get_function_pointer(
        assembly_path,
        type_name,
        method_name,
        /*delegate_type_name*/ nullptr,
        /*reserved*/ nullptr,
        &function_ptr);

    if (rc != 0 || function_ptr == nullptr) {
        write_log(L"initialize_dotnet_and_run: failed to get assembly and function pointer");
        close_f(hostctx);
        return;
    }

    // Call the managed entry (match your UnmanagedCallersOnly signature).
    auto entry = (mod_bootstrap_fn*)function_ptr;
    entry(nullptr);

    close_f(hostctx);
}

// Run initialization on a separate thread to avoid loader lock issues.
static DWORD WINAPI thread_proc(LPVOID)
{
    write_log(L"thread_proc");

    initialize_dotnet_and_run();
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID)
{
    //write_log(L"DllMain");

    if (fdwReason == DLL_PROCESS_ATTACH) {
        HANDLE h = CreateThread(nullptr, 0, thread_proc, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
