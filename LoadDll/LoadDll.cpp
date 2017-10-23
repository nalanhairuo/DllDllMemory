// LoadDll.cpp : 定义 DLL 应用程序的导出函数。

#include "MemoryModule.h"
#include "resource.h"
#include <tchar.h>
#include <stdio.h>

// 强制加载 User32.dll
#pragma comment(linker, "/include:_wsprintfA")
void PreprocessUnloadDll(HMODULE hLibModule);
typedef int (* FnInject)();
HMODULE g_hModule;

extern "C" __declspec(dllexport) int example()
{
    int ret = 43;
    printf("loaddll导出函数加载\n");
    return ret;
}


BOOL GetThisModuleFileNameA(LPSTR lpFilename, DWORD nSize)
{
    HMODULE hModule;//应用程序载入的模块

    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)GetThisModuleFileNameA, &hModule))
    {
        return GetModuleFileNameA(hModule, lpFilename, nSize);
    }

    return FALSE;
}

// 装载
extern "C" __declspec(dllexport) HMODULE LoadInjectModule()
{
    HMODULE hModule = g_hModule;

    HMODULE memdll = NULL;
    HRSRC hRsrc = FindResource(hModule, MAKEINTRESOURCE(IDR_INJECT), _T("DLL"));//指向资源的句柄

    //确定指定模块中指定类型和名称的资源所在位置
    if (hRsrc)
    {
        HGLOBAL hGlobal = ::LoadResource(hModule, hRsrc);//装载指定资源到全局存储器

        if (hGlobal)
        {
            LPCVOID pData = ::LockResource(hGlobal);//锁定资源并得到资源在内存中的第一个字节的指针

            WCHAR ProcessMappedFileNameW[MAX_PATH];
            //资源里加载字符串到CString对象
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
                    // KdPrint((_T("DLL 再次加载 ： %x\r\n"), memdll));
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

                // KdPrint((_T("DLL 首次加载 ： %x\r\n"), memdll));
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
                //  需要强制释放 DLL 库，设置装载次数为 1, 并设置初始化完成，以在释放时以 DLL_PROCESS_DETACH 调用入口函数
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

