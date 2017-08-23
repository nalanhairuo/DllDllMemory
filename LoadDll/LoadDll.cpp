// LoadDll.cpp : ���� DLL Ӧ�ó���ĵ���������


#include "MemoryModule.h"
#include "resource.h"
#include <tchar.h>
#include <stdio.h>


// ǿ�Ƽ��� User32.dll
#pragma comment(linker, "/include:_wsprintfA")
void PreprocessUnloadDll(HMODULE hLibModule);

BOOL GetThisModuleFileNameA(LPSTR lpFilename, DWORD nSize)
{
    HMODULE hModule;

    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)GetThisModuleFileNameA, &hModule))
    {
        return GetModuleFileNameA(hModule, lpFilename, nSize);
    }

    return FALSE;
}

// װ��
HMODULE LoadInjectModule(HMODULE hModule)
{
    //    KdFunctionLog();
    HMODULE memdll = NULL;
    HRSRC hRsrc = FindResource(hModule, MAKEINTRESOURCE(IDR_INJECT), _T("DLL"));

    if (hRsrc)
    {
        HGLOBAL hGlobal = ::LoadResource(hModule, hRsrc);

        if (hGlobal)
        {
            LPCVOID pData = ::LockResource(hGlobal);

            WCHAR ProcessMappedFileNameW[MAX_PATH];
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
                memdll = CustomLoadLibrary(pData, SizeofResource(hModule, hRsrc));
                LPSTR ThisModuleNameA = (LPSTR)CustomGetProcAddress(memdll, "ThisModuleFileNameA");

                if (ThisModuleNameA)
                {
                    GetThisModuleFileNameA(ThisModuleNameA, MAX_PATH);
                }

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

    return memdll;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            {
                LoadInjectModule(hModule);

                //
                //  ��Ҫǿ���ͷ� DLL �⣬����װ�ش���Ϊ 1, �����ó�ʼ����ɣ������ͷ�ʱ�� DLL_PROCESS_DETACH ������ں���
                //
                PreprocessUnloadDll(hModule);

            }
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}

extern "C" __declspec(dllexport) int example()
{
    int ret = 43;
    return ret;
}