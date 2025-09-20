#include "pch.h"
#include <windows.h>
#include <chrono>
#include <fstream>
#include "logger.h"

void write_log(const wchar_t* msg)
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
    std::wstring f = std::wstring(path) + L"wwrmodloader.txt";
    //std::wstring f = L"C:\\Temp\\bootstrapper.txt";
    std::wofstream out(f, std::ios::app);

    // write formatted time
    //out << msg << L"\r\n";
    out << L"[" << time_buffer << L"] Bootstrapper: " << msg << L"\n";
    out.close();
}

void write_log(const wchar_t* msg, int result)
{
    wchar_t buf[MAX_PATH];
    swprintf_s(buf, L"%s, result=0x%X", msg, result);
    write_log(buf);
}
