#include "pch.h"
#include <windows.h>
#include <fstream>
#include <chrono>
#include <metahost.h>
#include <comdef.h>
#include <string>
#pragma comment(lib, "mscoree.lib")

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

static void write_log(const wchar_t* msg, HRESULT result)
{
    wchar_t buf[MAX_PATH];
    swprintf_s(buf, L"%s, result=%X", msg, result);
    write_log(buf);
}

// Simple wrapper to call a managed method
static DWORD WINAPI run_mod(LPVOID)
{
    write_log(L"run_mod");

    wchar_t buf[MAX_PATH];
    HRESULT hr;
    ICLRMetaHost* pMetaHost = nullptr;
    IEnumUnknown* pLoadedRuntimes = nullptr;
    ICLRRuntimeInfo* pRuntimeInfo = nullptr;
    ICLRRuntimeHost* pHost = nullptr;

    const std::wstring assemblyPath = L"C:\\Repos\\WWR-Mods\\TestMod\\x64\\Release\\TestMod.dll"; // TODO!
    const std::wstring typeName = L"TestMod.ModEntry";
    const std::wstring methodName = L"InitializeMod";
    const std::wstring argument = L"";

    // debug - check working dir
    wchar_t dir[MAX_PATH];
    DWORD result = GetCurrentDirectoryW(MAX_PATH, dir);
    swprintf_s(buf, L"working dir is %s", dir);
    write_log(buf);
    
    //
    // Step 1: Get the CLR host.
    //
    //hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_PPV_ARGS(&pMetaHost)); //chat
    hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_ICLRMetaHost, (LPVOID*)&pMetaHost); //gem
    if (FAILED(hr)) { write_log(L"CLRCreateInstance failed"); return false; }
    write_log(L"ok ICLRMetaHost interface");

    //
    // Step 2: Get the runtime info for the loaded CLR version.
    // fails with code 80131700 - COR_E_BADIMAGEFORMAT
    // It need full version number, wildcards are not allowed, may change at any time
    // current is 8.0.1124.51707
    //
    //hr = pMetaHost->GetRuntime(L"v8.0.11", IID_PPV_ARGS(&pRuntimeInfo)); // .NET 8
    //if (FAILED(hr)) { write_log(L"GetRuntime failed", hr); return false; }
  
    //
    // Step 2a: Enumerate all runtimes loaded in the current process.
    //
    hr = pMetaHost->EnumerateLoadedRuntimes(GetCurrentProcess(), &pLoadedRuntimes);
    if (FAILED(hr)) {
        write_log(L"Failed to enumerate loaded runtimes.", hr);
        pMetaHost->Release();
        return 1;
    }

    //
    // Step 2b: Get the first loaded runtime info.
    // We'll just take the first one we find.
    //
    IUnknown* pUnk = NULL;
    ULONG fetched = 0;
    hr = pLoadedRuntimes->Next(1, &pUnk, &fetched);
    if (FAILED(hr) || fetched == 0) {
        write_log(L"Failed to find any loaded runtime.", hr);
        pLoadedRuntimes->Release();
        pMetaHost->Release();
        return 1;
    }
    write_log(L"ok ICLRRuntimeInfo interface");

    BOOL bLoadable = FALSE;
    hr = pRuntimeInfo->IsLoadable(&bLoadable);
    if (FAILED(hr) || !bLoadable) { write_log(L"Runtime not loadable"); return false; }

    hr = pRuntimeInfo->GetInterface(CLSID_CLRRuntimeHost, IID_PPV_ARGS(&pHost));
    if (FAILED(hr)) { write_log(L"GetInterface failed"); return false; }

    hr = pHost->Start(); // attach to existing runtime
    if (FAILED(hr)) { write_log(L"CLR Start failed"); return false; }

    DWORD ret = 0;
    hr = pHost->ExecuteInDefaultAppDomain(
        assemblyPath.c_str(),
        typeName.c_str(),
        methodName.c_str(),
        argument.c_str(),
        &ret
    );
    if (FAILED(hr)) { write_log(L"ExecuteInDefaultAppDomain failed"); return false; }

    write_log(L"Managed method executed successfully");
    pHost->Release();
    pRuntimeInfo->Release();
    pMetaHost->Release();
    return true;
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
