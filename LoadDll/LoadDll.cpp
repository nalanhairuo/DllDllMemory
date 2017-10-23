// LoadDll.cpp : ���� DLL Ӧ�ó���ĵ���������

#include "MemoryModule.h"
#include "resource.h"
#include <tchar.h>
#include <stdio.h>

// ǿ�Ƽ��� User32.dll
#pragma comment(linker, "/include:_wsprintfA")
void PreprocessUnloadDll(HMODULE hLibModule);
typedef int (* FnInject)();
HMODULE g_hModule;

extern "C" __declspec(dllexport) int example()
{
    int ret = 43;
    printf("loaddll������������\n");
    return ret;
}


BOOL GetThisModuleFileNameA(LPSTR lpFilename, DWORD nSize)
{
    HMODULE hModule;//Ӧ�ó��������ģ��

    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)GetThisModuleFileNameA, &hModule))
    {
        return GetModuleFileNameA(hModule, lpFilename, nSize);
    }

    return FALSE;
}

// װ��
extern "C" __declspec(dllexport) HMODULE LoadInjectModule()
{
    HMODULE hModule = g_hModule;

    HMODULE memdll = NULL;
    HRSRC hRsrc = FindResource(hModule, MAKEINTRESOURCE(IDR_INJECT), _T("DLL"));//ָ����Դ�ľ��

    //ȷ��ָ��ģ����ָ�����ͺ����Ƶ���Դ����λ��
    if (hRsrc)
    {
        HGLOBAL hGlobal = ::LoadResource(hModule, hRsrc);//װ��ָ����Դ��ȫ�ִ洢��

        if (hGlobal)
        {
            LPCVOID pData = ::LockResource(hGlobal);//������Դ���õ���Դ���ڴ��еĵ�һ���ֽڵ�ָ��

            WCHAR ProcessMappedFileNameW[MAX_PATH];
            //��Դ������ַ�����CString����
            size_t nLength = LoadString(hModule, IDR_INJECT, ProcessMappedFileNameW, sizeof(ProcessMappedFileNameW));

            if (nLength < 1)
            {
                wcscpy_s(ProcessMappedFileNameW, L"LOADDLL_UNNAMED");
                nLength = 15;
            }

            swprintf_s(ProcessMappedFileNameW + nLength, sizeof(ProcessMappedFileNameW) / sizeof(ProcessMappedFileNameW[0]) - nLength, _T("#%04X"), GetCurrentProcessId());

            WNDCLASSEX wndcls = { sizeof(WNDCLASSEX) };

            if (GetClassInfoEx((HINSTANCE)GetModuleHandle(NULL), ProcessMappedFileNameW, &wndcls))
            {
                memdll = (HMODULE)wndcls.lpfnWndProc;

                if (memdll)
                {
                    // KdPrint((_T("DLL �ٴμ��� �� %x\r\n"), memdll));
                }
            }

            else
            {
                memdll = (HMODULE)MemoryLoadLibrary(pData, SizeofResource(hModule, hRsrc));
                //LPSTR ThisModuleNameA = (LPSTR)MemoryGetProcAddress(memdll, "ThisModuleFileNameA");

                FnInject inject = (FnInject) MemoryGetProcAddress(memdll, "fnInject");

                inject();

                /*if (ThisModuleNameA)
                {
                    GetThisModuleFileNameA(ThisModuleNameA, MAX_PATH);
                }*/

                if (memdll)
                {
                    wndcls.hInstance = (HINSTANCE)GetModuleHandle(NULL);
                    wndcls.lpfnWndProc = (WNDPROC)memdll;
                    wndcls.lpszClassName = ProcessMappedFileNameW;
                }

                ATOM w = RegisterClassEx(&wndcls);

                // KdPrint((_T("DLL �״μ��� �� %x\r\n"), memdll));
            }

            UnlockResource(hRsrc);
        }
    }

    else
    {
        //hRsrc == NULL;

    }

    return memdll;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            {
                // attach to process
                // return FALSE to fail DLL load
                //LoadInjectModule(hModule);
                g_hModule = hModule;
                //
                //  ��Ҫǿ���ͷ� DLL �⣬����װ�ش���Ϊ 1, �����ó�ʼ����ɣ������ͷ�ʱ�� DLL_PROCESS_DETACH ������ں���
                //
                //PreprocessUnloadDll(hModule);

            }
            break;

        case DLL_THREAD_ATTACH:

        // attach to thread
        case DLL_THREAD_DETACH:

        // detach from thread
        case DLL_PROCESS_DETACH:

            // detach from thread
            break;
    }

    return TRUE;
}

