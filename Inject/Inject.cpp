#include "Inject.h"
#include <stdio.h>
#include <tchar.h>
#include <process.h>
// ���ǵ���������һ��ʾ��
DLL_API int nInject = 0;

// ���ǵ���������һ��ʾ����
DLL_API int fnInject(void)
{
    printf("inject.dll������������\n");
    return 42;
}

// �����ѵ�����Ĺ��캯����
// �й��ඨ�����Ϣ������� Inject.h
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