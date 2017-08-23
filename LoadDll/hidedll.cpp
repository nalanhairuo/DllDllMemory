// hidedll.cpp : 定义 DLL 应用程序的导出函数。

#include <Windows.h>
#include <winnt.h>

typedef struct _UNICODE_STRING
{
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING;

typedef UNICODE_STRING *PUNICODE_STRING;
typedef const UNICODE_STRING *PCUNICODE_STRING;

//
// Loader Data Table Entry
//
typedef struct _LDR_DATA_TABLE_ENTRY
{
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    ULONG Flags;
    USHORT LoadCount;
    USHORT TlsIndex;
    union
    {
        LIST_ENTRY HashLinks;
        PVOID SectionPointer;
    };
    ULONG CheckSum;
    union
    {
        ULONG TimeDateStamp;
        PVOID LoadedImports;
    };
    PVOID EntryPointActivationContext;
    PVOID PatchInformation;
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

//
// Loader Data Table Entry Flags
//
#define LDRP_STATIC_LINK                        0x00000002
#define LDRP_IMAGE_DLL                          0x00000004
#define LDRP_LOAD_IN_PROGRESS                   0x00001000
#define LDRP_UNLOAD_IN_PROGRESS                 0x00002000
#define LDRP_ENTRY_PROCESSED                    0x00004000
#define LDRP_ENTRY_INSERTED                     0x00008000
#define LDRP_CURRENT_LOAD                       0x00010000
#define LDRP_FAILED_BUILTIN_LOAD                0x00020000
#define LDRP_DONT_CALL_FOR_THREADS              0x00040000
#define LDRP_PROCESS_ATTACH_CALLED              0x00080000
#define LDRP_DEBUG_SYMBOLS_LOADED               0x00100000
#define LDRP_IMAGE_NOT_AT_BASE                  0x00200000
#define LDRP_COR_IMAGE                          0x00400000
#define LDR_COR_OWNS_UNMAP                      0x00800000
#define LDRP_SYSTEM_MAPPED                      0x01000000
#define LDRP_IMAGE_VERIFYING                    0x02000000
#define LDRP_DRIVER_DEPENDENT_DLL               0x04000000
#define LDRP_ENTRY_NATIVE                       0x08000000
#define LDRP_REDIRECTED                         0x10000000
#define LDRP_NON_PAGED_DEBUG_INFO               0x20000000
#define LDRP_MM_LOADED                          0x40000000
#define LDRP_COMPAT_DATABASE_PROCESSED          0x80000000

typedef struct _PEB_LDR_DATA
{
    ULONG                  Length;
    BOOLEAN              Initialized;
    PVOID                  SsHandle;
    LIST_ENTRY            InLoadOrderModuleList;        //按加载顺序
    LIST_ENTRY            InMemoryOrderModuleList;      //按内存顺序
    LIST_ENTRY            InInitializationOrderModuleList;//按初始化顺序
    PVOID         EntryInProgress;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

//每个模块信息的LDR_MODULE部分
typedef struct _LDR_MODULE
{
    LIST_ENTRY          InLoadOrderModuleList;
    LIST_ENTRY          InMemoryOrderModuleList;
    LIST_ENTRY          InInitializationOrderModuleList;
    PVOID               BaseAddress;
    PVOID               EntryPoint;
    ULONG               SizeOfImage;
    UNICODE_STRING      FullDllName;
    UNICODE_STRING      BaseDllName;
    ULONG               Flags;
    SHORT               LoadCount;
    SHORT               TlsIndex;
    LIST_ENTRY          HashTableEntry;
    ULONG               TimeDateStamp;
} LDR_MODULE, *PLDR_MODULE;

void PreprocessUnloadDll(HMODULE hLibModule)
{
    PPEB_LDR_DATA   pLdr = NULL;
    PLDR_MODULE     FirstModule = NULL;
    PLDR_MODULE     GurrentModule = NULL;

    __try
    {
        __asm
        {
            mov esi, fs:[0x30]
            mov esi, [esi + 0x0C]
            mov pLdr, esi
        }

        FirstModule = (PLDR_MODULE)(pLdr->InLoadOrderModuleList.Flink);
        GurrentModule = FirstModule;

        while (!(GurrentModule->BaseAddress == hLibModule))
        {
            GurrentModule = (PLDR_MODULE)(GurrentModule->InLoadOrderModuleList.Blink);

            if (GurrentModule == FirstModule)
            {
                return;
            }
        }

        //
        //  设置 LDRP_PROCESS_ATTACH_CALLED
        //
        GurrentModule->Flags |= LDRP_PROCESS_ATTACH_CALLED;

        //
        //  设置
        //
        int oldLoadCount = GurrentModule->LoadCount;
        GurrentModule->LoadCount = 1;
        return;
    }

    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return;
    }
}
//隐藏DLL方法:断链
VOID HideModule(HMODULE hLibModule)
{
    PPEB_LDR_DATA   pLdr = NULL;
    PLDR_MODULE     FirstModule = NULL;
    PLDR_MODULE     GurrentModule = NULL;

    __try
    {
        __asm
        {
            mov esi, fs:[0x30]
            mov esi, [esi + 0x0C]
            mov pLdr, esi
        }

        FirstModule = (PLDR_MODULE)(pLdr->InLoadOrderModuleList.Flink);
        GurrentModule = FirstModule;

        while (!(GurrentModule->BaseAddress == hLibModule))
        {
            GurrentModule = (PLDR_MODULE)(GurrentModule->InLoadOrderModuleList.Blink);

            if (GurrentModule == FirstModule)
            {
                break;
            }
        }

        if (GurrentModule->BaseAddress != hLibModule)
        {
            return;
        }

        //
        //  Dll解除链接
        //
        ((PLDR_MODULE)(GurrentModule->InLoadOrderModuleList.Flink))->InLoadOrderModuleList.Blink = GurrentModule->InLoadOrderModuleList.Blink;
        ((PLDR_MODULE)(GurrentModule->InLoadOrderModuleList.Blink))->InLoadOrderModuleList.Flink = GurrentModule->InLoadOrderModuleList.Flink;

        memset(GurrentModule->FullDllName.Buffer, 0, GurrentModule->FullDllName.Length);
        memset(GurrentModule, 0, sizeof(PLDR_MODULE));

        PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hLibModule;
        PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(LPBYTE(hLibModule) + dosHeader->e_lfanew);

        if ((dosHeader->e_magic == IMAGE_DOS_SIGNATURE) && (ntHeaders->Signature == IMAGE_NT_SIGNATURE))
        {
            //
            // 这里可能引起写保护异常，需要用 VirtualProtect 解除写保护。
            //
            memset(dosHeader, 0, sizeof(*dosHeader));
            memset(ntHeaders, 0, sizeof(*ntHeaders));
        }
    }

    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return;
    }
}
