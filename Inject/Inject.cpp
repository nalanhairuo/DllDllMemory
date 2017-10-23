#include "Inject.h"
#include <stdio.h>
#include <tchar.h>
#include <process.h>
// 这是导出变量的一个示例
DLL_API int nInject = 0;

// 这是导出函数的一个示例。
DLL_API int fnInject(void)
{
    printf("inject.dll导出函数加载\n");
    return 42;
}

// 这是已导出类的构造函数。
// 有关类定义的信息，请参阅 Inject.h
CInject::CInject()
{
    return;
}

unsigned int __stdcall RunThread(LPVOID)
{
    MessageBox(NULL, _T("Inject attached!"), _T("Inject.dll"), MB_OK);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            {
                unsigned int idThread;
                HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, RunThread, NULL, 0, &idThread);
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