#include "pch.h"
#include <iostream>
#include <fstream>
#include <windows.h>
#include <string>
#include "coreclr_delegates.h"
#include "hostfxr.h"
#include "logger.h"

HINSTANCE theDll = nullptr;



// Simple wrapper to call a managed method
static DWORD WINAPI run_mod(LPVOID)
{
    write_log(L"run_mod");

    // debug - check working dir
    wchar_t buf[MAX_PATH];
    wchar_t dir[MAX_PATH];
    DWORD result = GetCurrentDirectoryW(MAX_PATH, dir);
    swprintf_s(buf, L"working dir is %s", dir);
    write_log(buf);

    //
    // Step 1: Find and load the hostfxr library.
    //
    HMODULE hostfxrLib = NULL;
    // The official way to find the hostfxr path is using the nethost library, but that requires
    // another dependency. For simplicity in this snippet, we will assume it's in a known location
    // or loaded by the process itself (which is often true for .NET applications).
    // In a production environment, you would use a more robust search logic or the nethost library.
    hostfxrLib = GetModuleHandleW(L"hostfxr.dll");
    if (!hostfxrLib) {
        write_log(L"Failed to find hostfxr.dll. Make sure the host process is a .NET Core app.");
        return 1;
    }
    write_log(L"hostfxr acquired");

    //
    // Step 2a: Get function pointers to the hostfxr API.
    //
    //hostfxr_initialize_for_runtime_config_fn hostfxr_initialize_for_runtime_config =
        //(hostfxr_initialize_for_runtime_config_fn)GetProcAddress(hostfxrLib, "hostfxr_initialize_for_runtime_config");
    //if (!hostfxr_initialize_for_runtime_config) {
        //write_log(L"Failed to get hostfxr_initialize_for_runtime_config.");
        //return 1;
    //}
    //write_log(L"hostfxr_initialize_for_runtime_config acquired");

    //
    // Step 2b: Get a delegate to the hostfxr_get_runtime_delegate function.
    //
    hostfxr_get_runtime_delegate_fn hostfxr_get_delegate = (hostfxr_get_runtime_delegate_fn)GetProcAddress(
        hostfxrLib, "hostfxr_get_runtime_delegate");
    if (!hostfxr_get_delegate) {
        write_log(L"Failed to get hostfxr_get_runtime_delegate.");
        return 1;
    }
    write_log(L"hostfxr_get_runtime_delegate acquired");

    // 2c CLOSE is not used - we are using existing framework
    //hostfxr_close_fn hostfxr_close = (hostfxr_close_fn)GetProcAddress(hostfxrLib, "hostfxr_close");
    //if (!hostfxr_close) {
        //write_log(L"Failed to get hostfxr_close.");
        //return 1;
    //}
    //write_log(L"hostfxr_close acquired");

    //
    // Step 3: Initialize the host context from the runtime.
    // This is the CRUCIAL step to get access to the already loaded CLR.
    // rc=2 - not found
    // 800080A5 - StatusCode::InvalidRuntimeConfig (Invalid Runtime Configuration) - cannot parse??? probably due to extra info
    // 80008093 - StatusCode::RuntimeConfigHasInvalidVersions   (Incompatible Runtime Version)
    //const wchar_t* runtimeConfigPath = L"C:\\Repos\\WWR-Mods\\TestMod\\x64\\Release\\TestMod.runtimeconfig.json";

    /* SKIP
    hostfxr_handle hostContext = NULL;
    int rc = hostfxr_initialize_for_runtime_config(
        //runtimeConfigPath, // Path to the runtime config file for your assembly
        //C:\Steam\steamapps\common\Worldwide Rush\Worldwide Rush.runtimeconfig.json
        //L"Worldwide Rush.runtimeconfig.json",
        L"TestMod.runtimeconfig.json",
        nullptr,
        &hostContext);
    if (rc != 0 || hostContext == NULL) {
        write_log(L"Failed to initialize host context.", (HRESULT)rc);
        return 1;
    }
    write_log(L"host context initialized");
    */

    //
    // Step 3: Get a delegate to the load_assembly_and_get_function method.
    //
    // This is actually taken from coreclr_delegates.h so it can be omitted
    /*
    typedef int (WINAPI* load_assembly_and_get_function_pointer_fn)(
        const wchar_t* assembly_path,
        const wchar_t* type_name,
        const wchar_t* method_name,
        const wchar_t* delegate_type_name,
        //void* unknown, // there is 6 params in the example here https://learn.microsoft.com/en-us/dotnet/core/tutorials/netcore-hosting 
        void** delegate);
    */

    load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer = nullptr;
    // The delegate_type parameter is a specific enum from the hostfxr.h header.
    // The value 1 is for "hdt_com_unmanaged_callers_only_method" which is not the same as a normal delegate
    // The correct delegate_type is not publicly exposed and depends on the specific host.
    // However, the most robust way is to use a specific hosting delegate type, as shown below.
    int rc = hostfxr_get_delegate(
        //hostContext,
        nullptr, // Use nullptr to indicate that we want to use the existing host context.
        hdt_load_assembly_and_get_function_pointer,
        (void**) & load_assembly_and_get_function_pointer);
    if (rc != 0 || load_assembly_and_get_function_pointer == nullptr) {
        write_log(L"Failed to get pointer to load_assembly_and_get_function from the existing host.", rc);
        //hostfxr_close(hostContext);
        return 1;
    }
    write_log(L"load_assembly_and_get_function from existing host acquired");

    //
    // Prepare input file with mod names
    //
    wchar_t configPath[MAX_PATH];
    GetModuleFileNameW(theDll, configPath, MAX_PATH); // // We use the HMODULE that was passed to DllMain to get our DLL's path.
    size_t length = wcslen(configPath);
    configPath[length - 3] = L't';
    configPath[length - 2] = L'x';
    configPath[length - 1] = L't';
    swprintf_s(buf, L"config file: %s", configPath);
    write_log(buf); // Log the path to verify it

    // 
    // Read mod names from the config file
    // 
    wchar_t modName[32]; 
    std::wifstream inputFile(configPath); // Open the file for reading in wide character mode.
    // Check if the file was opened successfully.
    if (!inputFile.is_open()) {
        write_log(L"error: could not open the config file");
        return 1;
    }
    // Read the file line by line until the end is reached.
    while (inputFile.getline(modName, 32)) {
        swprintf_s(buf, L"loading mod: %s", modName);
        write_log(buf);

        // PROCESS MOD
    }
    inputFile.close(); // Close the file stream.

    //
    // Step 4: Get a pointer to our C# method using the delegate.
    //
    //const wchar_t* assemblyPath = L"C:\\Repos\\WWR-Mods\\TestMod\\x64\\Release\\TestMod.dll"; // TODO!
    const wchar_t* assemblyPath = L"Mods\\TestMod.dll"; // TODO!
    const wchar_t* typeName = L"TestMod.ModEntry, TestMod"; // Use fully qualified name <namespace>.<class>, <dll>
    const wchar_t* methodName = L"InitializeMod";


    // The actual function pointer to the managed method.
    // This matches the signature of our C# InitializeMod method.
    typedef HRESULT(__stdcall* initialize_mod_fn)();

    initialize_mod_fn managedMethod = NULL;
    rc = load_assembly_and_get_function_pointer(
        assemblyPath,
        typeName,
        methodName,
        UNMANAGEDCALLERSONLY_METHOD,
        0, /* Extensibility parameter (currently unused and must be 0) */
        (void**)&managedMethod
    );

    if (rc != 0 || managedMethod == nullptr) {
        write_log(L"Failed to get managed method pointer.", rc);
        //hostfxr_close(hostContext);
        return 1;
        // 80131502 - ERROR_MOD_NOT_FOUND
        // 80070057 - E_INVALIDARG
        // 80131509 - COR_E_FILELOAD; The CLR found your assembly, but failed to load it.  or COR_E_INVALIDOPERATION
        // 80070002 - ERROR_FILE_NOT_FOUND
    }
    write_log(L"pointer to InitializeMod acquired");

    //
    // Step 5: Call the C# method.
    //
    int managedMethodReturnValue = managedMethod();
    if (managedMethodReturnValue != S_OK) { // Assuming the C# method returns an HRESULT
        write_log(L"Managed method call failed.");
        //hostfxr_close(hostContext);
        return 1;
    }

    write_log(L"Successfully loaded and invoked C# method.");

    // The hostfxr.dll is managed by the host process, so we don't need to free the library.
    // FreeLibrary(hostfxrLib);
    //hostfxr_close(hostContext);

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        write_log(L"DllMain");
        theDll = hModule;
        DisableThreadLibraryCalls(hModule);
        HANDLE h = CreateThread(nullptr, 0, run_mod, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
