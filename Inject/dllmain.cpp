// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
#include "tchar.h"

int WINAPI RunThread(LPVOID)
{
    MessageBox(NULL, _T("OK"), _T("Inject.dll"), MB_OK);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            {
                DWORD idThread;
                HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RunThread, NULL, 0, &idThread);
                CloseHandle(hThread);
            }
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}

