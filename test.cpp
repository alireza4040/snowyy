#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID r)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        FILE* f = fopen("SnowWeather_loaded.txt", "w");
        if (f) { fputs("ok", f); fclose(f); }
    }
    return TRUE;
}
