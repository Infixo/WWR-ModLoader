// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

// Bootstrapper.cpp
#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>
#include <assert.h>
#include <thread>

// Include nethost.h and hostfxr.h
#include <nethost.h>
#include <coreclr_delegates.h>
#include <hostfxr.h>

// Link with nethost.lib (comes with .NET 8 SDK)
#pragma comment(lib, "nethost.lib")

// Globals
hostfxr_initialize_for_runtime_config_fn init_fptr;
hostfxr_get_runtime_delegate_fn get_delegate_fptr;
hostfxr_close_fn close_fptr;
load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer = nullptr;

// Forward declarations
bool load_hostfxr();
load_assembly_and_get_function_pointer_fn get_dotnet_load_assembly(const std::wstring& config_path);
void call_managed(const std::wstring& assembly_path);

// Helper: write debug messages
void log(const std::string& msg)
{
    std::ofstream logFile("C:\\Temp\\bootstrapper.log", std::ios::app);
    logFile << msg << std::endl;
    logFile.close();
}

/* OLD VERSION
// Entry point when DLL is loaded
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        // Run bootstrap on a new thread (avoid loader lock)
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD
            {
                log("Bootstrapper loaded into process.");

                // Path to runtimeconfig.json (from your MyMod.dll build output)
                std::wstring config_path = L"C:\\Path\\To\\MyMod.runtimeconfig.json";

                // Path to MyMod.dll
                std::wstring assembly_path = L"C:\\Path\\To\\MyMod.dll";

                if (!load_hostfxr())
                {
                    log("Failed to load hostfxr.");
                    return 1;
                }

                load_assembly_and_get_function_pointer = get_dotnet_load_assembly(config_path);
                if (load_assembly_and_get_function_pointer == nullptr)
                {
                    log("Failed to get load_assembly_and_get_function_pointer.");
                    return 1;
                }

                call_managed(assembly_path);

                log("Bootstrapper finished.");
                return 0;
            }, nullptr, 0, nullptr);
    }
    return TRUE;
}
*/

// -----------------------------------------------------
// Implementation
// -----------------------------------------------------

bool load_hostfxr()
{
    char_t buffer[MAX_PATH];
    size_t buffer_size = sizeof(buffer) / sizeof(char_t);
    int rc = get_hostfxr_path(buffer, &buffer_size, nullptr);
    if (rc != 0) return false;

    HMODULE lib = ::LoadLibraryW(buffer);
    init_fptr = (hostfxr_initialize_for_runtime_config_fn)GetProcAddress(lib, "hostfxr_initialize_for_runtime_config");
    get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)GetProcAddress(lib, "hostfxr_get_runtime_delegate");
    close_fptr = (hostfxr_close_fn)GetProcAddress(lib, "hostfxr_close");

    return (init_fptr && get_delegate_fptr && close_fptr);
}

load_assembly_and_get_function_pointer_fn get_dotnet_load_assembly(const std::wstring& config_path)
{
    hostfxr_handle cxt = nullptr;
    int rc = init_fptr(config_path.c_str(), nullptr, &cxt);
    if (rc != 0 || cxt == nullptr)
    {
        log("hostfxr_initialize_for_runtime_config failed.");
        return nullptr;
    }

    void* load_assembly_and_get_function_pointer_fn_ptr = nullptr;
    rc = get_delegate_fptr(
        cxt,
        hdt_load_assembly_and_get_function_pointer,
        &load_assembly_and_get_function_pointer_fn_ptr);

    close_fptr(cxt);

    return (rc == 0) ? (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer_fn_ptr : nullptr;
}

void call_managed(const std::wstring& assembly_path)
{
    typedef void (CORECLR_DELEGATE_CALLTYPE* entry_point_fn)();

    entry_point_fn managed_entry = nullptr;

    int rc = load_assembly_and_get_function_pointer(
        assembly_path.c_str(),
        L"TestMod.Mod, TestMod",   // Assembly-qualified type name <Namespace>.<TypeName>, <AssemblyName>
        L"Initialize",               // Static method name
        UNMANAGEDCALLERSONLY_METHOD, // delegate type (NULL is also fine here)
        nullptr,
        (void**)&managed_entry);

    if (rc != 0 || managed_entry == nullptr)
    {
        log("Failed to get managed entry point.");
        return;
    }

    log("Calling managed Initialize()...");
    managed_entry();
    log("Managed Initialize() returned.");
}



// Thread to run bootstrap logic safely
DWORD WINAPI BootstrapThread(LPVOID lpParam)
{
    log("Bootstrap thread started.");

    std::wstring basePath = *(std::wstring*)lpParam;


    std::wstring config_path = basePath + L"\\Injector.runtimeconfig.json";
    std::wstring assembly_path = basePath + L"\\TestMod.dll";

    if (!load_hostfxr())
    {
        log("Failed to load hostfxr.");
        return 1;
    }

    load_assembly_and_get_function_pointer = get_dotnet_load_assembly(config_path);
    if (!load_assembly_and_get_function_pointer)
    {
        log("Failed to get load assembly function pointer.");
        return 1;
    }

    call_managed(assembly_path);

    log("Bootstrap thread finished.");
    return 0;

}

// EasyHook will be looking for this export to support DLL injection. If not found then 
// DLL injection will fail.
//extern "C" void __declspec(dllexport) __stdcall NativeInjectionEntryPoint(REMOTE_ENTRY_INFO* inRemoteInfo);

// EasyHook-required entry point
extern "C" __declspec(dllexport)
void __stdcall NativeInjectionEntryPoint(LPVOID lpParam)
{
    log("NativeInjectionEntryPoint called.");

    // Determine folder of this DLL
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW((HMODULE)lpParam, dllPath, MAX_PATH);
    std::wstring pathStr(dllPath);
    size_t pos = pathStr.find_last_of(L"\\/");
    std::wstring folder = pathStr.substr(0, pos);

    // Pass folder path to thread
    std::wstring* folderParam = new std::wstring(folder);

    // Run bootstrap in separate thread to avoid loader lock
    HANDLE hThread = CreateThread(nullptr, 0, BootstrapThread, folderParam, 0, nullptr);
    CloseHandle(hThread);

    //return 0;
}


// Minimal DllMain
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

