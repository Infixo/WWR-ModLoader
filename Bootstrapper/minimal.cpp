#include "pch.h"
#include <windows.h>
#include <fstream>
#include <chrono>

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

static DWORD WINAPI run_mod(LPVOID)
{
    write_log(L"run_mod");
    wchar_t buf[MAX_PATH];

    // debug - check working dir
    wchar_t dir[MAX_PATH];
    DWORD result = GetCurrentDirectoryW(MAX_PATH, dir);
    swprintf_s(buf, L"working dir is %s", dir);
    write_log(buf);

    // load library
    const wchar_t* modPath = L"C:\\Repos\\WWR-Mods\\TestMod\\x64\\Release\\TestMod.dll"; // TODO!
    HMODULE mod = LoadLibraryW(modPath);
    swprintf_s(buf, L"loadlibrary %x", mod != NULL);
    write_log(buf);
    if (mod == NULL) return 1;

    // call mod entry point
    auto fn = (void(*)())GetProcAddress(mod, "InitializeMod");
    if (fn)
    {
        write_log(L"entry point retrived, initializing mod");
        fn();  // actually executes managed code
    }
    else
    {
        write_log(L"failed to retrieve entry point");
        return 2;
    }

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID)
{
    //write_log(L"DllMain");

    if (fdwReason == DLL_PROCESS_ATTACH) {
        HANDLE h = CreateThread(nullptr, 0, run_mod, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    return TRUE;
}

