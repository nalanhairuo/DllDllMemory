/*
 * Memory DLL loading code
 * Version 0.0.4
 *
 * Copyright (c) 2004-2015 by Joachim Bauch / mail@joachim-bauch.de
 * http://www.joachim-bauch.de
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 2.0 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is MemoryModule.c
 *
 * The Initial Developer of the Original Code is Joachim Bauch.
 *
 * Portions created by Joachim Bauch are Copyright (C) 2004-2015
 * Joachim Bauch. All Rights Reserved.
 *
 */

#include <windows.h>
#include <winnt.h>
#include <stddef.h>
#include <tchar.h>
//#include "debug.h"
#ifdef DEBUG_OUTPUT
#include <stdio.h>
#endif

#if _MSC_VER
// Disable warning about data -> function pointer conversion
#pragma warning(disable:4055)
// C4244: conversion from 'uintptr_t' to 'DWORD', possible loss of data.
#pragma warning(error: 4244)
// C4267: conversion from 'size_t' to 'int', possible loss of data.
#pragma warning(error: 4267)

#define inline __inline
#endif

#ifndef IMAGE_SIZEOF_BASE_RELOCATION
// Vista SDKs no longer define IMAGE_SIZEOF_BASE_RELOCATION!?
#define IMAGE_SIZEOF_BASE_RELOCATION (sizeof(IMAGE_BASE_RELOCATION))
#endif

#ifdef _WIN64
#define HOST_MACHINE IMAGE_FILE_MACHINE_AMD64
#else
#define HOST_MACHINE IMAGE_FILE_MACHINE_I386
#endif

#include "MemoryModule.h"

typedef BOOL (WINAPI *DllEntryProc)(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);
typedef int (WINAPI *ExeEntryProc)(void);

typedef struct
{
    PIMAGE_NT_HEADERS headers;
    unsigned char *codeBase;
    HCUSTOMMODULE *modules;
    int numModules;
    BOOL initialized;
    BOOL isDLL;
    BOOL isRelocated;
    ExeEntryProc exeEntry;
    DWORD pageSize;
} MEMORYMODULE, *PMEMORYMODULE;

class CMemoryModuleFun
{
  public:
    PMEMORYMODULE m_p;

  public:
    PMEMORYMODULE operator ->()
    {
        return m_p;
    }

    operator HMEMORYMODULE()
    {
        return (HMEMORYMODULE)m_p;
    }

    static PMEMORYMODULE createModuleHandle(unsigned char *code, PIMAGE_NT_HEADERS headers)
    {
        PMEMORYMODULE module = (PMEMORYMODULE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MEMORYMODULE));

        if (module == NULL)
        {
            SetLastError(ERROR_OUTOFMEMORY);
            return NULL;
        }

        module->codeBase = code;

        module->isDLL = (headers->FileHeader.Characteristics & IMAGE_FILE_DLL) != 0;

        module->headers = headers;

        return module;
    }

    HMODULE getModuleHandle()
    {
        return (HMODULE)m_p->codeBase;
    }

    void setModuleHandle(PMEMORYMODULE p)
    {
        m_p = p;
    }

    unsigned char* getCodeBase()
    {
        return m_p->codeBase;
    }

    void setRelocated(BOOL b)
    {
        m_p->isRelocated = b;
    }

    BOOL DllEntry(PIMAGE_NT_HEADERS headers)
    {
        // get entry point of loaded library
        if (headers->OptionalHeader.AddressOfEntryPoint != 0)
        {
            if (m_p->isDLL)
            {
                DllEntryProc DllEntry = (DllEntryProc)(LPVOID)(getCodeBase() + headers->OptionalHeader.AddressOfEntryPoint);
                // notify library about attaching to process
                BOOL successfull = (*DllEntry)(getModuleHandle(), DLL_PROCESS_ATTACH, 0);

                if (!successfull)
                {
                    SetLastError(ERROR_DLL_INIT_FAILED);
                    return FALSE;
                }

                m_p->initialized = TRUE;
            }

            else
            {
                m_p->exeEntry = (ExeEntryProc)(LPVOID)(getCodeBase() + headers->OptionalHeader.AddressOfEntryPoint);
            }
        }

        else
        {
            m_p->exeEntry = NULL;
        }

        return TRUE;
    }

    void DllFail(PIMAGE_NT_HEADERS headers)
    {
        if (m_p == NULL)
        {
            return;
        }

        if (m_p->modules != NULL)
        {
            // free previously opened libraries
            int i;

            for (i = 0; i < m_p->numModules; i++)
            {
                if (m_p->modules[i] != NULL)
                {
                    freeLibrary(m_p->modules[i]);
                }
            }

            ::free(m_p->modules);
        }

        if (m_p->codeBase != NULL)
        {
            // release memory of library
            free(m_p->codeBase, 0, MEM_RELEASE);
        }

        HeapFree(GetProcessHeap(), 0, m_p);
    }

    static LPVOID alloc(LPVOID address, SIZE_T size, DWORD allocationType, DWORD protect)
    {
        return ::VirtualAlloc(address, size, allocationType, protect);
    }

    static BOOL free(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType)
    {
        return ::VirtualFree(lpAddress, dwSize, dwFreeType);
    }

    static HCUSTOMMODULE loadLibrary(LPCSTR filename)
    {
        return (HCUSTOMMODULE)::LoadLibraryA(filename);
    }

    static FARPROC getProcAddress(HCUSTOMMODULE module, LPCSTR name)
    {
        return (FARPROC)::GetProcAddress((HMODULE)module, name);
    }

    static void freeLibrary(HCUSTOMMODULE module)
    {
        ::FreeLibrary((HMODULE)module);
    }


    BOOL addModule(HCUSTOMMODULE handle)
    {
        HCUSTOMMODULE* tmp = (HCUSTOMMODULE *)realloc(m_p->modules, (m_p->numModules + 1) * (sizeof(HCUSTOMMODULE)));

        if (tmp == NULL)
        {
            SetLastError(ERROR_OUTOFMEMORY);
            return FALSE;
        }

        m_p->modules = tmp;
        m_p->modules[m_p->numModules++] = handle;
        return TRUE;
    }
};

class CCustomModuleFun
{
  public:
    HMODULE m_h;

  public:
    operator HMODULE()
    {
        return (HMODULE)m_h;
    }

    static HMODULE createModuleHandle(unsigned char *code, PIMAGE_NT_HEADERS headers)
    {
        return (HMODULE)code;
    }

    HMODULE getModuleHandle()
    {
        return (HMODULE)m_h;
    }

    void setModuleHandle(HMODULE h)
    {
        m_h = h;
    }

    void setRelocated(BOOL b)
    {
    }

    unsigned char* getCodeBase()
    {
        return (unsigned char*)m_h;
    }

    BOOL DllEntry(PIMAGE_NT_HEADERS headers)
    {
        // get entry point of loaded library
        if (headers->OptionalHeader.AddressOfEntryPoint != 0)
        {
            if ((headers->FileHeader.Characteristics & IMAGE_FILE_DLL) != 0)
            {
                DllEntryProc DllEntry = (DllEntryProc)(LPVOID)(getCodeBase() + headers->OptionalHeader.AddressOfEntryPoint);
                // notify library about attaching to process
                BOOL successfull = (*DllEntry)(getModuleHandle(), DLL_PROCESS_ATTACH, 0);

                if (!successfull)
                {
                    SetLastError(ERROR_DLL_INIT_FAILED);
                    return FALSE;
                }
            }
        }

        return TRUE;
    }

    void DllFail(PIMAGE_NT_HEADERS headers)
    {
        // release memory of library
        free(m_h, 0, MEM_RELEASE);
    }

    static LPVOID alloc(LPVOID address, SIZE_T size, DWORD allocationType, DWORD protect)
    {
        return ::VirtualAlloc(address, size, allocationType, protect);
    }

    static BOOL free(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType)
    {
        return ::VirtualFree(lpAddress, dwSize, dwFreeType);
    }

    static HCUSTOMMODULE loadLibrary(LPCSTR filename)
    {
        return (HCUSTOMMODULE)::LoadLibraryA(filename);
    }

    static FARPROC getProcAddress(HCUSTOMMODULE module, LPCSTR name)
    {
        return (FARPROC)::GetProcAddress((HMODULE)module, name);
    }

    static void freeLibrary(HCUSTOMMODULE module)
    {
        ::FreeLibrary((HMODULE)module);
    }


    BOOL addModule(HCUSTOMMODULE handle)
    {
        return TRUE;
    }
};

typedef struct
{
    LPVOID address;
    LPVOID alignedAddress;
    SIZE_T size;
    DWORD characteristics;
    BOOL last;
} SECTIONFINALIZEDATA, *PSECTIONFINALIZEDATA;

#define GET_HEADER_DICTIONARY(headers, idx)  &(headers)->OptionalHeader.DataDirectory[idx]

static inline uintptr_t
AlignValueDown(uintptr_t value, uintptr_t alignment)
{
    return value & ~(alignment - 1);
}

static inline LPVOID
AlignAddressDown(LPVOID address, uintptr_t alignment)
{
    return (LPVOID) AlignValueDown((uintptr_t) address, alignment);
}

static inline size_t
AlignValueUp(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static inline void*
OffsetPointer(void* data, ptrdiff_t offset)
{
    return (void*)((uintptr_t) data + offset);
}

static inline void
OutputLastError(const char *msg)
{
#ifndef DEBUG_OUTPUT
    UNREFERENCED_PARAMETER(msg);
#else
    LPVOID tmp;
    char *tmpmsg;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&tmp, 0, NULL);
    tmpmsg = (char *)LocalAlloc(LPTR, strlen(msg) + strlen(tmp) + 3);
    sprintf(tmpmsg, "%s: %s", msg, tmp);
    OutputDebugString(tmpmsg);
    LocalFree(tmpmsg);
    LocalFree(tmp);
#endif
}

static BOOL
CheckSize(size_t size, size_t expected)
{
    if (size < expected)
    {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }

    return TRUE;
}

template<class TFun>
static BOOL CopySections(const unsigned char *data, size_t size, PIMAGE_NT_HEADERS old_headers, TFun module, PIMAGE_NT_HEADERS nt_headers)
{
    int i, section_size;
    unsigned char *codeBase = module.getCodeBase();
    unsigned char *dest;
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt_headers);

    for (i = 0; i < old_headers->FileHeader.NumberOfSections; i++, section++)
    {
        if (section->SizeOfRawData == 0)
        {
            // section doesn't contain data in the dll itself, but may define
            // uninitialized data
            section_size = old_headers->OptionalHeader.SectionAlignment;

            if (section_size > 0)
            {
                dest = (unsigned char *)module.alloc(codeBase + section->VirtualAddress,
                                                     section_size,
                                                     MEM_COMMIT,
                                                     PAGE_READWRITE);

                if (dest == NULL)
                {
                    return FALSE;
                }

                // Always use position from file to support alignments smaller
                // than page size (allocation above will align to page size).
                dest = codeBase + section->VirtualAddress;
                // NOTE: On 64bit systems we truncate to 32bit here but expand
                // again later when "PhysicalAddress" is used.
                section->Misc.PhysicalAddress = (DWORD)((uintptr_t) dest & 0xffffffff);
                memset(dest, 0, section_size);
            }

            // section is empty
            continue;
        }

        if (!CheckSize(size, section->PointerToRawData + section->SizeOfRawData))
        {
            return FALSE;
        }

        // commit memory block and copy data from dll
        dest = (unsigned char *)module.alloc(codeBase + section->VirtualAddress,
                                             section->SizeOfRawData,
                                             MEM_COMMIT,
                                             PAGE_READWRITE);

        if (dest == NULL)
        {
            return FALSE;
        }

        // Always use position from file to support alignments smaller
        // than page size (allocation above will align to page size).
        dest = codeBase + section->VirtualAddress;
        memcpy(dest, data + section->PointerToRawData, section->SizeOfRawData);
        // NOTE: On 64bit systems we truncate to 32bit here but expand
        // again later when "PhysicalAddress" is used.
        section->Misc.PhysicalAddress = (DWORD)((uintptr_t) dest & 0xffffffff);
    }

    return TRUE;
}

// Protection flags for memory pages (Executable, Readable, Writeable)
static int ProtectionFlags[2][2][2] =
{
    {
        // not executable
        {PAGE_NOACCESS, PAGE_WRITECOPY},
        {PAGE_READONLY, PAGE_READWRITE},
    }, {
        // executable
        {PAGE_EXECUTE, PAGE_EXECUTE_WRITECOPY},
        {PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE},
    },
};

static SIZE_T GetRealSectionSize(PIMAGE_NT_HEADERS headers, PIMAGE_SECTION_HEADER section)
{
    DWORD size = section->SizeOfRawData;

    if (size == 0)
    {
        if (section->Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA)
        {
            size = headers->OptionalHeader.SizeOfInitializedData;
        }

        else if (section->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA)
        {
            size = headers->OptionalHeader.SizeOfUninitializedData;
        }
    }

    return (SIZE_T) size;
}

template<class TFun>
static BOOL FinalizeSection(TFun module, PIMAGE_NT_HEADERS headers, PSECTIONFINALIZEDATA sectionData, DWORD pagesize)
{
    DWORD protect, oldProtect;
    BOOL executable;
    BOOL readable;
    BOOL writeable;

    if (sectionData->size == 0)
    {
        return TRUE;
    }

    if (sectionData->characteristics & IMAGE_SCN_MEM_DISCARDABLE)
    {
        // section is not needed any more and can safely be freed
        if (sectionData->address == sectionData->alignedAddress &&
                (sectionData->last ||
                 headers->OptionalHeader.SectionAlignment == pagesize ||
                 (sectionData->size % pagesize) == 0)
           )
        {
            // Only allowed to decommit whole pages
            module.free(sectionData->address, sectionData->size, MEM_DECOMMIT);
        }

        return TRUE;
    }

    // determine protection flags based on characteristics
    executable = (sectionData->characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
    readable = (sectionData->characteristics & IMAGE_SCN_MEM_READ) != 0;
    writeable = (sectionData->characteristics & IMAGE_SCN_MEM_WRITE) != 0;
    protect = ProtectionFlags[executable][readable][writeable];

    if (sectionData->characteristics & IMAGE_SCN_MEM_NOT_CACHED)
    {
        protect |= PAGE_NOCACHE;
    }

    // change memory access flags
    if (VirtualProtect(sectionData->address, sectionData->size, protect, &oldProtect) == 0)
    {
        OutputLastError("Error protecting memory page");
        return FALSE;
    }

    return TRUE;
}

template<class TFun>
static BOOL FinalizeSections(TFun module, PIMAGE_NT_HEADERS headers, DWORD pagesize)
{
    int i;
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(headers);
#ifdef _WIN64
    // "PhysicalAddress" might have been truncated to 32bit above, expand to
    // 64bits again.
    uintptr_t imageOffset = ((uintptr_t) module->headers->OptionalHeader.ImageBase & 0xffffffff00000000);
#else
    static const uintptr_t imageOffset = 0;
#endif
    SECTIONFINALIZEDATA sectionData;
    sectionData.address = (LPVOID)((uintptr_t)section->Misc.PhysicalAddress | imageOffset);
    sectionData.alignedAddress = AlignAddressDown(sectionData.address, pagesize);
    sectionData.size = GetRealSectionSize(headers, section);
    sectionData.characteristics = section->Characteristics;
    sectionData.last = FALSE;
    section++;

    // loop through all sections and change access flags
    for (i = 1; i < headers->FileHeader.NumberOfSections; i++, section++)
    {
        LPVOID sectionAddress = (LPVOID)((uintptr_t)section->Misc.PhysicalAddress | imageOffset);
        LPVOID alignedAddress = AlignAddressDown(sectionAddress, pagesize);
        SIZE_T sectionSize = GetRealSectionSize(headers, section);

        // Combine access flags of all sections that share a page
        // TODO(fancycode): We currently share flags of a trailing large section
        //   with the page of a first small section. This should be optimized.
        if (sectionData.alignedAddress == alignedAddress || (uintptr_t) sectionData.address + sectionData.size > (uintptr_t) alignedAddress)
        {
            // Section shares page with previous
            if ((section->Characteristics & IMAGE_SCN_MEM_DISCARDABLE) == 0 || (sectionData.characteristics & IMAGE_SCN_MEM_DISCARDABLE) == 0)
            {
                sectionData.characteristics = (sectionData.characteristics | section->Characteristics) & ~IMAGE_SCN_MEM_DISCARDABLE;
            }

            else
            {
                sectionData.characteristics |= section->Characteristics;
            }

            sectionData.size = (((uintptr_t)sectionAddress) + ((uintptr_t) sectionSize)) - (uintptr_t) sectionData.address;
            continue;
        }

        if (!FinalizeSection(module, headers, &sectionData, pagesize))
        {
            return FALSE;
        }

        sectionData.address = sectionAddress;
        sectionData.alignedAddress = alignedAddress;
        sectionData.size = sectionSize;
        sectionData.characteristics = section->Characteristics;
    }

    sectionData.last = TRUE;

    if (!FinalizeSection(module, headers, &sectionData, pagesize))
    {
        return FALSE;
    }

    return TRUE;
}

template<class TFun>
static BOOL ExecuteTLS(TFun module, PIMAGE_NT_HEADERS headers)
{
    unsigned char *codeBase = module.getCodeBase();
    PIMAGE_TLS_DIRECTORY tls;
    PIMAGE_TLS_CALLBACK* callback;

#if(0)
    DWORD Param[30] = { 0 };
    Param[6] = (DWORD)module.getModuleHandle();

    NTSTATUS(WINAPI * LdrpHandleTlsData)(LPCVOID);
    (DWORD&)LdrpHandleTlsData = (DWORD)GetModuleHandle(_T("ntdll.dll")) + 0x40742;

    LdrpHandleTlsData(Param);
#endif

    PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY(headers, IMAGE_DIRECTORY_ENTRY_TLS);

    if (directory->VirtualAddress == 0)
    {
        return TRUE;
    }

    tls = (PIMAGE_TLS_DIRECTORY)(codeBase + directory->VirtualAddress);
    callback = (PIMAGE_TLS_CALLBACK *) tls->AddressOfCallBacks;

    if (callback)
    {
        while (*callback)
        {
            (*callback)((LPVOID) codeBase, DLL_PROCESS_ATTACH, NULL);
            callback++;
        }
    }

    return TRUE;
}

template<class TFun>
static BOOL PerformBaseRelocation(TFun module, PIMAGE_NT_HEADERS headers, ptrdiff_t delta)
{
    unsigned char *codeBase = module.getCodeBase();
    PIMAGE_BASE_RELOCATION relocation;

    PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY(headers, IMAGE_DIRECTORY_ENTRY_BASERELOC);

    if (directory->Size == 0)
    {
        return (delta == 0);
    }

    relocation = (PIMAGE_BASE_RELOCATION)(codeBase + directory->VirtualAddress);

    for (; relocation->VirtualAddress > 0;)
    {
        DWORD i;
        unsigned char *dest = codeBase + relocation->VirtualAddress;
        unsigned short *relInfo = (unsigned short*) OffsetPointer(relocation, IMAGE_SIZEOF_BASE_RELOCATION);

        for (i = 0; i < ((relocation->SizeOfBlock - IMAGE_SIZEOF_BASE_RELOCATION) / 2); i++, relInfo++)
        {
            // the upper 4 bits define the type of relocation
            int type = *relInfo >> 12;
            // the lower 12 bits define the offset
            int offset = *relInfo & 0xfff;

            switch (type)
            {
                case IMAGE_REL_BASED_ABSOLUTE:
                    // skip relocation
                    break;

                case IMAGE_REL_BASED_HIGHLOW:
                    // change complete 32 bit address
                    {
                        DWORD *patchAddrHL = (DWORD *)(dest + offset);
                        *patchAddrHL += (DWORD) delta;
                    }
                    break;

#ifdef _WIN64

                case IMAGE_REL_BASED_DIR64:
                    {
                        ULONGLONG *patchAddr64 = (ULONGLONG *)(dest + offset);
                        *patchAddr64 += (ULONGLONG) delta;
                    }
                    break;
#endif

                default:
                    //printf("Unknown relocation: %d\n", type);
                    break;
            }
        }

        // advance to next relocation block
        relocation = (PIMAGE_BASE_RELOCATION) OffsetPointer(relocation, relocation->SizeOfBlock);
    }

    return TRUE;
}

template<class TFun>
static BOOL BuildImportTable(TFun module, PIMAGE_NT_HEADERS headers)
{
    //    KdFunctionLog();
    unsigned char *codeBase = module.getCodeBase();
    PIMAGE_IMPORT_DESCRIPTOR importDesc;
    BOOL result = TRUE;

    PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY(headers, IMAGE_DIRECTORY_ENTRY_IMPORT);

    if (directory->Size == 0)
    {
        return TRUE;
    }

    importDesc = (PIMAGE_IMPORT_DESCRIPTOR)(codeBase + directory->VirtualAddress);

    for (; !IsBadReadPtr(importDesc, sizeof(IMAGE_IMPORT_DESCRIPTOR)) && importDesc->Name; importDesc++)
    {
        uintptr_t *thunkRef;
        FARPROC *funcRef;
        HCUSTOMMODULE handle = module.loadLibrary((LPCSTR)(codeBase + importDesc->Name));
        //        KdPrint((_T("loadLibrary : %hs\r\n"), (LPCSTR)(codeBase + importDesc->Name)));

        if (handle == NULL)
        {
            SetLastError(ERROR_MOD_NOT_FOUND);
            result = FALSE;
            break;
        }

        if (!module.addModule(handle))
        {
            module.freeLibrary(handle);
            result = FALSE;
            break;
        }

        if (importDesc->OriginalFirstThunk)
        {
            thunkRef = (uintptr_t *)(codeBase + importDesc->OriginalFirstThunk);
            funcRef = (FARPROC *)(codeBase + importDesc->FirstThunk);
        }

        else
        {
            // no hint table
            thunkRef = (uintptr_t *)(codeBase + importDesc->FirstThunk);
            funcRef = (FARPROC *)(codeBase + importDesc->FirstThunk);
        }

        for (; *thunkRef; thunkRef++, funcRef++)
        {
            if (IMAGE_SNAP_BY_ORDINAL(*thunkRef))
            {
                *funcRef = module.getProcAddress(handle, (LPCSTR)IMAGE_ORDINAL(*thunkRef));
            }

            else
            {
                PIMAGE_IMPORT_BY_NAME thunkData = (PIMAGE_IMPORT_BY_NAME)(codeBase + (*thunkRef));
                //                KdPrint((_T("GetProcAddress : %hs\r\n"), (LPCSTR)&thunkData->Name));
                *funcRef = module.getProcAddress(handle, (LPCSTR)&thunkData->Name);
            }

            if (*funcRef == 0)
            {
                result = FALSE;
                break;
            }
        }

        if (!result)
        {
            module.freeLibrary(handle);
            SetLastError(ERROR_PROC_NOT_FOUND);
            break;
        }
    }

    return result;
}

template<class TFun, class HMODULETYPE>
HMODULETYPE MemoryLoadLibraryEx(const void *data, size_t size)
{
    TFun result = { NULL };
    PIMAGE_DOS_HEADER dos_header;
    PIMAGE_NT_HEADERS old_header, nt_headers;
    unsigned char *code, *headers;
    ptrdiff_t locationDelta;
    SYSTEM_INFO sysInfo;
    PIMAGE_SECTION_HEADER section;
    DWORD i;
    size_t optionalSectionSize;
    size_t lastSectionEnd = 0;
    size_t alignedImageSize;

    if (!CheckSize(size, sizeof(IMAGE_DOS_HEADER)))
    {
        return NULL;
    }

    dos_header = (PIMAGE_DOS_HEADER)data;

    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
    {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return NULL;
    }

    if (!CheckSize(size, dos_header->e_lfanew + sizeof(IMAGE_NT_HEADERS)))
    {
        return NULL;
    }

    old_header = (PIMAGE_NT_HEADERS) & ((const unsigned char *)(data))[dos_header->e_lfanew];

    if (old_header->Signature != IMAGE_NT_SIGNATURE)
    {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return NULL;
    }

    //    _KdPrint((_T("MemoryLoadLibraryEx 0\r\n")));

    if (old_header->FileHeader.Machine != HOST_MACHINE)
    {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return NULL;
    }

    //    _KdPrint((_T("MemoryLoadLibraryEx 1\r\n")));

    if (old_header->OptionalHeader.SectionAlignment & 1)
    {
        // Only support section alignments that are a multiple of 2
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return NULL;
    }

    section = IMAGE_FIRST_SECTION(old_header);
    optionalSectionSize = old_header->OptionalHeader.SectionAlignment;

    for (i = 0; i < old_header->FileHeader.NumberOfSections; i++, section++)
    {
        size_t endOfSection;

        if (section->SizeOfRawData == 0)
        {
            // Section without data in the DLL
            endOfSection = section->VirtualAddress + optionalSectionSize;
        }

        else
        {
            endOfSection = section->VirtualAddress + section->SizeOfRawData;
        }

        if (endOfSection > lastSectionEnd)
        {
            lastSectionEnd = endOfSection;
        }
    }

    //    _KdPrint((_T("MemoryLoadLibraryEx 2\r\n")));

    GetNativeSystemInfo(&sysInfo);
    alignedImageSize = AlignValueUp(old_header->OptionalHeader.SizeOfImage, sysInfo.dwPageSize);

    if (alignedImageSize != AlignValueUp(lastSectionEnd, sysInfo.dwPageSize))
    {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return NULL;
    }

    // reserve memory for image of library
    // XXX: is it correct to commit the complete memory region at once?
    //      calling DllEntry raises an exception if we don't...
    code = (unsigned char *)result.alloc((LPVOID)(old_header->OptionalHeader.ImageBase),
                                         alignedImageSize,
                                         MEM_RESERVE | MEM_COMMIT,
                                         PAGE_READWRITE);

    //    _KdPrint((_T("MemoryLoadLibraryEx 3\r\n")));

    if (code == NULL)
    {
        // try to allocate memory at arbitrary position
        code = (unsigned char *)result.alloc(NULL,
                                             alignedImageSize,
                                             MEM_RESERVE | MEM_COMMIT,
                                             PAGE_READWRITE);

        if (code == NULL)
        {
            SetLastError(ERROR_OUTOFMEMORY);
            return NULL;
        }
    }

    // commit memory for headers
    headers = (unsigned char *)result.alloc(code,
                                            old_header->OptionalHeader.SizeOfHeaders,
                                            MEM_COMMIT,
                                            PAGE_READWRITE);

    // copy PE header to code
    memcpy(headers, dos_header, old_header->OptionalHeader.SizeOfHeaders);

    nt_headers = (PIMAGE_NT_HEADERS) & ((const unsigned char *)(headers))[dos_header->e_lfanew];

    //    _KdPrint((_T("MemoryLoadLibraryEx 4\r\n")));

    result.setModuleHandle(result.createModuleHandle(code, nt_headers));

    if ((HMODULETYPE)result == NULL)
    {
        goto error;
    }

    if (!CheckSize(size, old_header->OptionalHeader.SizeOfHeaders))
    {
        goto error;
    }

    // update position
    nt_headers->OptionalHeader.ImageBase = (uintptr_t)code;

    // copy sections from DLL file block to new memory location
    if (!CopySections((const unsigned char *)data, size, old_header, result, nt_headers))
    {
        goto error;
    }

    // adjust base address of imported data
    locationDelta = (ptrdiff_t)(nt_headers->OptionalHeader.ImageBase - old_header->OptionalHeader.ImageBase);
    result.setRelocated((locationDelta != 0) ? PerformBaseRelocation(result, nt_headers, locationDelta) : TRUE);

    //    KdPrint((_T("MemoryLoadLibraryEx 5\r\n")));

    // load required dlls and adjust function table of imports
    if (!BuildImportTable(result, nt_headers))
    {
        goto error;
    }

    //    _KdPrint((_T("MemoryLoadLibraryEx 6\r\n")));

    // mark memory pages depending on section headers and release
    // sections that are marked as "discardable"
    if (!FinalizeSections(result, nt_headers, sysInfo.dwPageSize))
    {
        goto error;
    }

    //    _KdPrint((_T("MemoryLoadLibraryEx 7\r\n")));

    // TLS callbacks are executed BEFORE the main loading
    if (!ExecuteTLS(result, nt_headers))
    {
        goto error;
    }

    //    _KdPrint((_T("MemoryLoadLibraryEx 8\r\n")));

    // get entry point of loaded library
    if (!result.DllEntry(nt_headers))
    {
        goto error;
    }

    //    _KdPrint((_T("MemoryLoadLibraryEx 9\r\n")));
    return result;

error:
    // cleanup
    result.DllFail(nt_headers);
    return NULL;
}

HMEMORYMODULE MemoryLoadLibrary(const void *data, size_t size)
{
    return MemoryLoadLibraryEx<CMemoryModuleFun, HMEMORYMODULE>(data, size);
}

HMODULE CustomLoadLibrary(const void *data, size_t size)
{
    return MemoryLoadLibraryEx<CCustomModuleFun, HMODULE>(data, size);
}

FARPROC MemoryGetProcAddress(HMODULE module, PIMAGE_NT_HEADERS headers, LPCSTR name)
{
    unsigned char *codeBase = (unsigned char *)module;
    DWORD idx = 0;
    PIMAGE_EXPORT_DIRECTORY exports;
    PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY(headers, IMAGE_DIRECTORY_ENTRY_EXPORT);

    if (directory->Size == 0)
    {
        // no export table found
        SetLastError(ERROR_PROC_NOT_FOUND);
        return NULL;
    }

    exports = (PIMAGE_EXPORT_DIRECTORY)(codeBase + directory->VirtualAddress);

    if (exports->NumberOfNames == 0 || exports->NumberOfFunctions == 0)
    {
        // DLL doesn't export anything
        SetLastError(ERROR_PROC_NOT_FOUND);
        return NULL;
    }

    if (HIWORD(name) == 0)
    {
        // load function by ordinal value
        if (LOWORD(name) < exports->Base)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return NULL;
        }

        idx = LOWORD(name) - exports->Base;
    }

    else
    {
        // search function name in list of exported names
        DWORD i;
        DWORD *nameRef = (DWORD *)(codeBase + exports->AddressOfNames);
        WORD *ordinal = (WORD *)(codeBase + exports->AddressOfNameOrdinals);
        BOOL found = FALSE;

        for (i = 0; i < exports->NumberOfNames; i++, nameRef++, ordinal++)
        {
            if (_stricmp(name, (const char *)(codeBase + (*nameRef))) == 0)
            {
                idx = *ordinal;
                found = TRUE;
                break;
            }
        }

        if (!found)
        {
            // exported symbol not found
            SetLastError(ERROR_PROC_NOT_FOUND);
            return NULL;
        }
    }

    if (idx > exports->NumberOfFunctions)
    {
        // name <-> ordinal number don't match
        SetLastError(ERROR_PROC_NOT_FOUND);
        return NULL;
    }

    // AddressOfFunctions contains the RVAs to the "real" functions
    return (FARPROC)(LPVOID)(codeBase + (*(DWORD *)(codeBase + exports->AddressOfFunctions + (idx * 4))));
}

FARPROC MemoryGetProcAddress(HMEMORYMODULE module, LPCSTR name)
{
    return MemoryGetProcAddress((HMODULE)PMEMORYMODULE(module)->codeBase, PMEMORYMODULE(module)->headers, name);
}

FARPROC CustomGetProcAddress(HMODULE module, LPCSTR name)
{
    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)module;
    return MemoryGetProcAddress(module, (PIMAGE_NT_HEADERS) & ((const unsigned char *)(module))[dos_header->e_lfanew], name);
}

LPCVOID VirtualAddressToRawData(LPBYTE data, DWORD VirtualAddress, PIMAGE_NT_HEADERS nt_headers)
{
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt_headers);

    for (int i = 0; i < nt_headers->FileHeader.NumberOfSections; i++, section++)
    {
        if (section->SizeOfRawData > 0)
        {
            if ((section->VirtualAddress <= VirtualAddress) && (VirtualAddress < section->VirtualAddress + section->SizeOfRawData))
            {
                return data + section->PointerToRawData + (VirtualAddress - section->VirtualAddress);
            }
        }
    }

    return NULL;
}

FARPROC ResourceGetProcAddress(LPCVOID module, LPCSTR name)
{
    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)module;
    PIMAGE_NT_HEADERS headers = (PIMAGE_NT_HEADERS) & ((const unsigned char *)(module))[dos_header->e_lfanew];
    unsigned char *codeBase = (unsigned char *)module;
    DWORD idx = 0;

    PIMAGE_EXPORT_DIRECTORY exports;
    PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY(headers, IMAGE_DIRECTORY_ENTRY_EXPORT);

    if (directory->Size == 0)
    {
        // no export table found
        SetLastError(ERROR_PROC_NOT_FOUND);
        return NULL;
    }

    exports = (PIMAGE_EXPORT_DIRECTORY)VirtualAddressToRawData(codeBase, directory->VirtualAddress, headers);

    if (exports == NULL || exports->NumberOfNames == 0 || exports->NumberOfFunctions == 0)
    {
        return NULL;
    }

    if (HIWORD(name) == 0)
    {
        // load function by ordinal value
        if (LOWORD(name) < exports->Base)
        {
            return NULL;
        }

        idx = LOWORD(name) - exports->Base;
    }

    else
    {
        // search function name in list of exported names
        DWORD i;
        DWORD *nameRef = (DWORD *)VirtualAddressToRawData(codeBase, exports->AddressOfNames, headers);
        WORD *ordinal = (WORD *)VirtualAddressToRawData(codeBase, exports->AddressOfNameOrdinals, headers);
        BOOL found = FALSE;

        for (i = 0; i < exports->NumberOfNames; i++, nameRef++, ordinal++)
        {
            LPCSTR PointerToRawName = (LPCSTR)VirtualAddressToRawData(codeBase, *nameRef, headers);

            if (_stricmp(name, PointerToRawName) == 0)
            {
                idx = *ordinal;
                found = TRUE;
                break;
            }
        }

        if (!found)
        {
            // exported symbol not found
            return NULL;
        }
    }

    if (idx > exports->NumberOfFunctions)
    {
        // name <-> ordinal number don't match
        return NULL;
    }

    // AddressOfFunctions contains the RVAs to the "real" functions
    DWORD* AddressOfFunctions = (DWORD*)VirtualAddressToRawData(codeBase, exports->AddressOfFunctions, headers);

    if (AddressOfFunctions == NULL)
    {
        return NULL;
    }

    return (FARPROC)VirtualAddressToRawData(codeBase, AddressOfFunctions[idx], headers);
}


void MemoryFreeLibrary(HMEMORYMODULE mod)
{
    PMEMORYMODULE module = (PMEMORYMODULE)mod;

    if (module == NULL)
    {
        return;
    }

    if (module->initialized)
    {
        // notify library about detaching from process
        DllEntryProc DllEntry = (DllEntryProc)(LPVOID)(module->codeBase + module->headers->OptionalHeader.AddressOfEntryPoint);
        (*DllEntry)((HINSTANCE)module->codeBase, DLL_PROCESS_DETACH, 0);
    }

    if (module->modules != NULL)
    {
        // free previously opened libraries
        int i;

        for (i = 0; i < module->numModules; i++)
        {
            if (module->modules[i] != NULL)
            {
                CMemoryModuleFun::freeLibrary(module->modules[i]);
            }
        }

        free(module->modules);
    }

    if (module->codeBase != NULL)
    {
        // release memory of library
        CMemoryModuleFun::free(module->codeBase, 0, MEM_RELEASE);
    }

    HeapFree(GetProcessHeap(), 0, module);
}

void MemoryFreeLibrary(HMODULE module)
{
    if (module == NULL)
    {
        return;
    }

    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)module;
    PIMAGE_NT_HEADERS nt_headers = (PIMAGE_NT_HEADERS) & ((const unsigned char *)(module))[dos_header->e_lfanew];

    // notify library about detaching from process
    DllEntryProc DllEntry = (DllEntryProc)(LPVOID)((unsigned char*)module + nt_headers->OptionalHeader.AddressOfEntryPoint);
    (*DllEntry)((HINSTANCE)module, DLL_PROCESS_DETACH, 0);

    PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY(nt_headers, IMAGE_DIRECTORY_ENTRY_IMPORT);

    if (directory->Size > 0)
    {
        PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((unsigned char*)module + directory->VirtualAddress);

        for (; !IsBadReadPtr(importDesc, sizeof(IMAGE_IMPORT_DESCRIPTOR)) && importDesc->Name; importDesc++)
        {
            HMODULE handle = GetModuleHandleA((LPCSTR)((unsigned char*)module + importDesc->Name));

            if (handle)
            {
                FreeLibrary(handle);
            }
        }
    }

    // release memory of library
    CCustomModuleFun::free(module, 0, MEM_RELEASE);
}

int MemoryCallEntryPoint(HMEMORYMODULE mod)
{
    PMEMORYMODULE module = (PMEMORYMODULE)mod;

    if (module == NULL || module->isDLL || module->exeEntry == NULL || !module->isRelocated)
    {
        return -1;
    }

    return module->exeEntry();
}

#define DEFAULT_LANGUAGE        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)

HMEMORYRSRC MemoryFindResource(HMEMORYMODULE module, LPCTSTR name, LPCTSTR type)
{
    return MemoryFindResourceEx(module, name, type, DEFAULT_LANGUAGE);
}

static PIMAGE_RESOURCE_DIRECTORY_ENTRY _MemorySearchResourceEntry(
    void *root,
    PIMAGE_RESOURCE_DIRECTORY resources,
    LPCTSTR key)
{
    PIMAGE_RESOURCE_DIRECTORY_ENTRY entries = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(resources + 1);
    PIMAGE_RESOURCE_DIRECTORY_ENTRY result = NULL;
    DWORD start;
    DWORD end;
    DWORD middle;

    if (!IS_INTRESOURCE(key) && key[0] == TEXT('#'))
    {
        // special case: resource id given as string
        TCHAR *endpos = NULL;
        long int tmpkey = (WORD) _tcstol((TCHAR *) &key[1], &endpos, 10);

        if (tmpkey <= 0xffff && lstrlen(endpos) == 0)
        {
            key = MAKEINTRESOURCE(tmpkey);
        }
    }

    // entries are stored as ordered list of named entries,
    // followed by an ordered list of id entries - we can do
    // a binary search to find faster...
    if (IS_INTRESOURCE(key))
    {
        WORD check = (WORD)(uintptr_t) key;
        start = resources->NumberOfNamedEntries;
        end = start + resources->NumberOfIdEntries;

        while (end > start)
        {
            WORD entryName;
            middle = (start + end) >> 1;
            entryName = (WORD) entries[middle].Name;

            if (check < entryName)
            {
                end = (end != middle ? middle : middle - 1);
            }

            else if (check > entryName)
            {
                start = (start != middle ? middle : middle + 1);
            }

            else
            {
                result = &entries[middle];
                break;
            }
        }
    }

    else
    {
        LPCWSTR searchKey;
        size_t searchKeyLen = _tcslen(key);
#if defined(UNICODE)
        searchKey = key;
#else
        // Resource names are always stored using 16bit characters, need to
        // convert string we search for.
#define MAX_LOCAL_KEY_LENGTH 2048
        // In most cases resource names are short, so optimize for that by
        // using a pre-allocated array.
        wchar_t _searchKeySpace[MAX_LOCAL_KEY_LENGTH + 1];
        LPWSTR _searchKey;

        if (searchKeyLen > MAX_LOCAL_KEY_LENGTH)
        {
            size_t _searchKeySize = (searchKeyLen + 1) * sizeof(wchar_t);
            _searchKey = (LPWSTR) malloc(_searchKeySize);

            if (_searchKey == NULL)
            {
                SetLastError(ERROR_OUTOFMEMORY);
                return NULL;
            }
        }

        else
        {
            _searchKey = &_searchKeySpace[0];
        }

        mbstowcs(_searchKey, key, searchKeyLen);
        _searchKey[searchKeyLen] = 0;
        searchKey = _searchKey;
#endif
        start = 0;
        end = resources->NumberOfNamedEntries;

        while (end > start)
        {
            int cmp;
            PIMAGE_RESOURCE_DIR_STRING_U resourceString;
            middle = (start + end) >> 1;
            resourceString = (PIMAGE_RESOURCE_DIR_STRING_U) OffsetPointer(root, entries[middle].Name & 0x7FFFFFFF);
            cmp = _wcsnicmp(searchKey, resourceString->NameString, resourceString->Length);

            if (cmp == 0)
            {
                // Handle partial match
                if (searchKeyLen > resourceString->Length)
                {
                    cmp = 1;
                }

                else if (searchKeyLen < resourceString->Length)
                {
                    cmp = -1;
                }
            }

            if (cmp < 0)
            {
                end = (middle != end ? middle : middle - 1);
            }

            else if (cmp > 0)
            {
                start = (middle != start ? middle : middle + 1);
            }

            else
            {
                result = &entries[middle];
                break;
            }
        }

#if !defined(UNICODE)

        if (searchKeyLen > MAX_LOCAL_KEY_LENGTH)
        {
            free(_searchKey);
        }

#undef MAX_LOCAL_KEY_LENGTH
#endif
    }

    return result;
}

HMEMORYRSRC MemoryFindResourceEx(HMEMORYMODULE module, LPCTSTR name, LPCTSTR type, WORD language)
{
    unsigned char *codeBase = ((PMEMORYMODULE) module)->codeBase;
    PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY(((PMEMORYMODULE)module)->headers, IMAGE_DIRECTORY_ENTRY_RESOURCE);
    PIMAGE_RESOURCE_DIRECTORY rootResources;
    PIMAGE_RESOURCE_DIRECTORY nameResources;
    PIMAGE_RESOURCE_DIRECTORY typeResources;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY foundType;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY foundName;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY foundLanguage;

    if (directory->Size == 0)
    {
        // no resource table found
        SetLastError(ERROR_RESOURCE_DATA_NOT_FOUND);
        return NULL;
    }

    if (language == DEFAULT_LANGUAGE)
    {
        // use language from current thread
        language = LANGIDFROMLCID(GetThreadLocale());
    }

    // resources are stored as three-level tree
    // - first node is the type
    // - second node is the name
    // - third node is the language
    rootResources = (PIMAGE_RESOURCE_DIRECTORY)(codeBase + directory->VirtualAddress);
    foundType = _MemorySearchResourceEntry(rootResources, rootResources, type);

    if (foundType == NULL)
    {
        SetLastError(ERROR_RESOURCE_TYPE_NOT_FOUND);
        return NULL;
    }

    typeResources = (PIMAGE_RESOURCE_DIRECTORY)(codeBase + directory->VirtualAddress + (foundType->OffsetToData & 0x7fffffff));
    foundName = _MemorySearchResourceEntry(rootResources, typeResources, name);

    if (foundName == NULL)
    {
        SetLastError(ERROR_RESOURCE_NAME_NOT_FOUND);
        return NULL;
    }

    nameResources = (PIMAGE_RESOURCE_DIRECTORY)(codeBase + directory->VirtualAddress + (foundName->OffsetToData & 0x7fffffff));
    foundLanguage = _MemorySearchResourceEntry(rootResources, nameResources, (LPCTSTR)(uintptr_t) language);

    if (foundLanguage == NULL)
    {
        // requested language not found, use first available
        if (nameResources->NumberOfIdEntries == 0)
        {
            SetLastError(ERROR_RESOURCE_LANG_NOT_FOUND);
            return NULL;
        }

        foundLanguage = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(nameResources + 1);
    }

    return (codeBase + directory->VirtualAddress + (foundLanguage->OffsetToData & 0x7fffffff));
}

DWORD MemorySizeofResource(HMEMORYMODULE module, HMEMORYRSRC resource)
{
    PIMAGE_RESOURCE_DATA_ENTRY entry;
    UNREFERENCED_PARAMETER(module);
    entry = (PIMAGE_RESOURCE_DATA_ENTRY) resource;

    if (entry == NULL)
    {
        return 0;
    }

    return entry->Size;
}

LPVOID MemoryLoadResource(HMEMORYMODULE module, HMEMORYRSRC resource)
{
    unsigned char *codeBase = ((PMEMORYMODULE) module)->codeBase;
    PIMAGE_RESOURCE_DATA_ENTRY entry = (PIMAGE_RESOURCE_DATA_ENTRY) resource;

    if (entry == NULL)
    {
        return NULL;
    }

    return codeBase + entry->OffsetToData;
}

int
MemoryLoadString(HMEMORYMODULE module, UINT id, LPTSTR buffer, int maxsize)
{
    return MemoryLoadStringEx(module, id, buffer, maxsize, DEFAULT_LANGUAGE);
}

int
MemoryLoadStringEx(HMEMORYMODULE module, UINT id, LPTSTR buffer, int maxsize, WORD language)
{
    HMEMORYRSRC resource;
    PIMAGE_RESOURCE_DIR_STRING_U data;
    DWORD size;

    if (maxsize == 0)
    {
        return 0;
    }

    resource = MemoryFindResourceEx(module, MAKEINTRESOURCE((id >> 4) + 1), RT_STRING, language);

    if (resource == NULL)
    {
        buffer[0] = 0;
        return 0;
    }

    data = (PIMAGE_RESOURCE_DIR_STRING_U) MemoryLoadResource(module, resource);
    id = id & 0x0f;

    while (id--)
    {
        data = (PIMAGE_RESOURCE_DIR_STRING_U) OffsetPointer(data, (data->Length + 1) * sizeof(WCHAR));
    }

    if (data->Length == 0)
    {
        SetLastError(ERROR_RESOURCE_NAME_NOT_FOUND);
        buffer[0] = 0;
        return 0;
    }

    size = data->Length;

    if (size >= (DWORD) maxsize)
    {
        size = maxsize;
    }

    else
    {
        buffer[size] = 0;
    }

#if defined(UNICODE)
    wcsncpy_s(buffer, maxsize, data->NameString, size);
#else
    wcstombs(buffer, data->NameString, size);
#endif
    return size;
}

#ifdef TESTSUITE
#include <stdio.h>

#ifndef PRIxPTR
#ifdef _WIN64
#define PRIxPTR "I64x"
#else
#define PRIxPTR "x"
#endif
#endif

static const uintptr_t AlignValueDownTests[][3] =
{
    {16, 16, 16},
    {17, 16, 16},
    {32, 16, 32},
    {33, 16, 32},
#ifdef _WIN64
    {0x12345678abcd1000, 0x1000, 0x12345678abcd1000},
    {0x12345678abcd101f, 0x1000, 0x12345678abcd1000},
#endif
    {0, 0, 0},
};

static const uintptr_t AlignValueUpTests[][3] =
{
    {16, 16, 16},
    {17, 16, 32},
    {32, 16, 32},
    {33, 16, 48},
#ifdef _WIN64
    {0x12345678abcd1000, 0x1000, 0x12345678abcd1000},
    {0x12345678abcd101f, 0x1000, 0x12345678abcd2000},
#endif
    {0, 0, 0},
};

BOOL MemoryModuleTestsuite()
{
    BOOL success = TRUE;
    size_t idx;

    for (idx = 0; AlignValueDownTests[idx][0]; ++idx)
    {
        const uintptr_t* tests = AlignValueDownTests[idx];
        uintptr_t value = AlignValueDown(tests[0], tests[1]);

        if (value != tests[2])
        {
            printf("AlignValueDown failed for 0x%" PRIxPTR "/0x%" PRIxPTR ": expected 0x%" PRIxPTR ", got 0x%" PRIxPTR "\n",
                   tests[0], tests[1], tests[2], value);
            success = FALSE;
        }
    }

    for (idx = 0; AlignValueDownTests[idx][0]; ++idx)
    {
        const uintptr_t* tests = AlignValueUpTests[idx];
        uintptr_t value = AlignValueUp(tests[0], tests[1]);

        if (value != tests[2])
        {
            printf("AlignValueUp failed for 0x%" PRIxPTR "/0x%" PRIxPTR ": expected 0x%" PRIxPTR ", got 0x%" PRIxPTR "\n",
                   tests[0], tests[1], tests[2], value);
            success = FALSE;
        }
    }

    if (success)
    {
        printf("OK\n");
    }

    return success;
}
#endif
