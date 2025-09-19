#include <iostream>
#include <windows.h>
#include <mscoree.h>
#include <atlbase.h> // For CComPtr
#include <chrono>
#include <fstream>

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


// Ensure you have the necessary libraries linked in your project: mscoree.lib
// For a Visual Studio project, this can be done via Project Properties -> Linker -> Input -> Additional Dependencies

//
// This is the function that will be called to initialize the C# assembly.
// We use a typedef to make the function signature clearer.
// The function in C# would be public static void InitializeMod(int value)
//
typedef HRESULT(__stdcall* InitializeModMethod)(int);

int main() {
    // CLR host interfaces
    CComPtr<ICorRuntimeHost> pCorRuntimeHost;
    CComPtr<IUnknown> pAppDomainThunk;
    //CComPtr<_AppDomain> pAppDomain;

    // We will use a wide-character string for the assembly path as
    // CLR hosting APIs generally expect wide characters.
    const wchar_t* assemblyPath = L"TestMod.dll";
    const wchar_t* typeName = L"TestMod.MyClass";
    const wchar_t* methodName = L"InitializeMod";

    //
    // Step 1: Start the CLR.
    //
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        write_log(L"Failed to initialize COM.");
        return 1;
    }

    hr = CorBindToRuntimeEx(
        NULL,           // Default version of the runtime
        L"wks",         // Workstation GC mode
        0,              // No startup flags
        CLSID_CorRuntimeHost,
        IID_ICorRuntimeHost,
        (LPVOID*)&pCorRuntimeHost
    );
    if (FAILED(hr)) {
        write_log(L"Failed to get CLR host interface.");
        CoUninitialize();
        return 1;
    }

    hr = pCorRuntimeHost->Start();
    if (FAILED(hr)) {
        write_log(L"Failed to start the CLR.");
        CoUninitialize();
        return 1;
    }

    //
    // Step 2: Get the default AppDomain.
    //
    hr = pCorRuntimeHost->GetDefaultDomain(&pAppDomainThunk);
    if (FAILED(hr)) {
        std::cerr << "Failed to get default AppDomain." << std::endl;
        pCorRuntimeHost->Stop();
        CoUninitialize();
        return 1;
    }

    hr = pAppDomainThunk->QueryInterface(IID__AppDomain, (LPVOID*)&pAppDomain);
    if (FAILED(hr)) {
        write_log(L"Failed to query for AppDomain interface.");
        pCorRuntimeHost->Stop();
        CoUninitialize();
        return 1;
    }

    //
    // Step 3: Load the C# assembly into the AppDomain.
    //
    CComVariant loadedAssemblyVariant;
    hr = pAppDomain->Load_2(CComBSTR(assemblyPath), &loadedAssemblyVariant);
    if (FAILED(hr)) {
        write_log(L"Failed to load assembly.");
        pCorRuntimeHost->Stop();
        CoUninitialize();
        return 1;
    }

    // Convert the VARIANT result to an _Assembly interface.
    CComPtr<_Assembly> pAssembly;
    hr = loadedAssemblyVariant.pdispVal->QueryInterface(IID_IUnknown, (LPVOID*)&pAssembly);
    if (FAILED(hr)) {
        write_log(L"Failed to get assembly interface.");
        pCorRuntimeHost->Stop();
        CoUninitialize();
        return 1;
    }

    //
    // Step 4: Call the method using the MethodInfo.
    //
    CComVariant classInstance; // Holds the instance of the class (if it's not static)
    CComVariant retVal;        // Holds the return value of the method call

    // InvokeMember is the primary way to call methods on types
    hr = pAssembly->InvokeMember_3(
        CComBSTR(typeName),
        CComBSTR(methodName),
        BindingFlags_Static | BindingFlags_InvokeMethod, // Static method call
        NULL,
        NULL,
        NULL,
        NULL,
        &retVal
    );

    if (FAILED(hr)) {
        write_log(L"Failed to invoke method.");
        pCorRuntimeHost->Stop();
        CoUninitialize();
        return 1;
    }

    write_log(L"Successfully loaded and invoked C# method.");

    //
    // Step 5: Clean up
    //
    pCorRuntimeHost->Stop();
    CoUninitialize();

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        write_log(L"DllMain");
        DisableThreadLibraryCalls(hModule);
        HANDLE h = CreateThread(nullptr, 0, run_mod, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
