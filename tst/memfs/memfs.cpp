/**
 * @file memfs.cpp
 *
 * @copyright 2015-2021 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#undef _DEBUG
#include "memfs.h"
#include <sddl.h>
#include <VersionHelpers.h>
#include <cassert>
#include <map>
#include <unordered_map>

/* SLOWIO */
#include <thread>

#define MEMFS_MAX_PATH                  512
FSP_FSCTL_STATIC_ASSERT(MEMFS_MAX_PATH > MAX_PATH,
    "MEMFS_MAX_PATH must be greater than MAX_PATH.");

/*
 * Define the MEMFS_STANDALONE macro when building MEMFS as a standalone file system.
 * This macro should be defined in the Visual Studio project settings, Makefile, etc.
 */
//#define MEMFS_STANDALONE

/*
 * Define the MEMFS_NAME_NORMALIZATION macro to include name normalization support.
 */
#define MEMFS_NAME_NORMALIZATION

/*
 * Define the MEMFS_REPARSE_POINTS macro to include reparse points support.
 */
#define MEMFS_REPARSE_POINTS

/*
 * Define the MEMFS_NAMED_STREAMS macro to include named streams support.
 */
#define MEMFS_NAMED_STREAMS

/*
 * Define the MEMFS_DIRINFO_BY_NAME macro to include GetDirInfoByName.
 */
#define MEMFS_DIRINFO_BY_NAME

/*
 * Define the MEMFS_SLOWIO macro to include delayed I/O response support.
 */
#define MEMFS_SLOWIO

/*
 * Define the MEMFS_CONTROL macro to include DeviceControl support.
 */
#define MEMFS_CONTROL

/*
 * Define the MEMFS_EA macro to include extended attributes support.
 */
#define MEMFS_EA

/*
 * Define the MEMFS_WSL macro to include WSLinux support.
 */
#define MEMFS_WSL

/*
 * Define the MEMFS_REJECT_EARLY_IRP macro to reject IRP's sent
 * to the file system prior to the dispatcher being started.
 */
#if defined(MEMFS_STANDALONE)
#define MEMFS_REJECT_EARLY_IRP
#endif

/*
 * Define the DEBUG_BUFFER_CHECK macro on Windows 8 or above. This includes
 * a check for the Write buffer to ensure that it is read-only.
 *
 * Since ProcessBuffer support in the FSD, this is no longer a guarantee.
 */
#if !defined(NDEBUG)
//#define DEBUG_BUFFER_CHECK
#endif

#define MEMFS_SECTOR_SIZE               512
#define MEMFS_SECTORS_PER_ALLOCATION_UNIT 1

/*
 * Large Heap Support
 */

typedef struct
{
    DWORD Options;
    SIZE_T InitialSize;
    SIZE_T MaximumSize;
    SIZE_T Alignment;
} LARGE_HEAP_INITIALIZE_PARAMS;
static INIT_ONCE LargeHeapInitOnce = INIT_ONCE_STATIC_INIT;
static HANDLE LargeHeap;
static SIZE_T LargeHeapAlignment;
static BOOL WINAPI LargeHeapInitOnceF(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    LARGE_HEAP_INITIALIZE_PARAMS *Params = (LARGE_HEAP_INITIALIZE_PARAMS *)Parameter;
    LargeHeap = HeapCreate(Params->Options, Params->InitialSize, Params->MaximumSize);
    LargeHeapAlignment = 0 != Params->Alignment ?
        FSP_FSCTL_ALIGN_UP(Params->Alignment, 4096) :
        16 * 4096;
    return TRUE;
}
static inline
BOOLEAN LargeHeapInitialize(
    DWORD Options,
    SIZE_T InitialSize,
    SIZE_T MaximumSize,
    SIZE_T Alignment)
{
    LARGE_HEAP_INITIALIZE_PARAMS Params;
    Params.Options = Options;
    Params.InitialSize = InitialSize;
    Params.MaximumSize = MaximumSize;
    Params.Alignment = Alignment;
    InitOnceExecuteOnce(&LargeHeapInitOnce, LargeHeapInitOnceF, &Params, 0);
    return 0 != LargeHeap;
}
static inline
PVOID LargeHeapAlloc(SIZE_T Size)
{
    return HeapAlloc(LargeHeap, 0, FSP_FSCTL_ALIGN_UP(Size, LargeHeapAlignment));
}
static inline
PVOID LargeHeapRealloc(PVOID Pointer, SIZE_T Size)
{
    if (0 != Pointer)
    {
        if (0 != Size)
            return HeapReAlloc(LargeHeap, 0, Pointer, FSP_FSCTL_ALIGN_UP(Size, LargeHeapAlignment));
        else
            return HeapFree(LargeHeap, 0, Pointer), 0;
    }
    else
    {
        if (0 != Size)
            return HeapAlloc(LargeHeap, 0, FSP_FSCTL_ALIGN_UP(Size, LargeHeapAlignment));
        else
            return 0;
    }
}
static inline
VOID LargeHeapFree(PVOID Pointer)
{
    if (0 != Pointer)
        HeapFree(LargeHeap, 0, Pointer);
}

/*
 * MEMFS
 */

static inline
UINT64 MemfsGetSystemTime(VOID)
{
    FILETIME FileTime;
    GetSystemTimeAsFileTime(&FileTime);
    return ((PLARGE_INTEGER)&FileTime)->QuadPart;
}

static inline
int MemfsFileNameCompare(PWSTR a, int alen, PWSTR b, int blen, BOOLEAN CaseInsensitive)
{
    PWSTR p, endp, partp, q, endq, partq;
    WCHAR c, d;
    int plen, qlen, len, res;

    if (-1 == alen)
        alen = lstrlenW(a);
    if (-1 == blen)
        blen = lstrlenW(b);

    for (p = a, endp = p + alen, q = b, endq = q + blen; endp > p && endq > q;)
    {
        c = d = 0;
        for (; endp > p && (L':' == *p || L'\\' == *p); p++)
            c = *p;
        for (; endq > q && (L':' == *q || L'\\' == *q); q++)
            d = *q;

        if (L':' == c)
            c = 1;
        else if (L'\\' == c)
            c = 2;
        if (L':' == d)
            d = 1;
        else if (L'\\' == d)
            d = 2;

        res = c - d;
        if (0 != res)
            return res;

        for (partp = p; endp > p && L':' != *p && L'\\' != *p; p++)
            ;
        for (partq = q; endq > q && L':' != *q && L'\\' != *q; q++)
            ;

        plen = (int)(p - partp);
        qlen = (int)(q - partq);

        len = plen < qlen ? plen : qlen;

        if (CaseInsensitive)
        {
            res = CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, partp, plen, partq, qlen);
            if (0 != res)
                res -= 2;
            else
                res = _wcsnicmp(partp, partq, len);
        }
        else
            res = wcsncmp(partp, partq, len);

        if (0 == res)
            res = plen - qlen;

        if (0 != res)
            return res;
    }

    return -(endp <= p) + (endq <= q);
}

static inline
BOOLEAN MemfsFileNameHasPrefix(PWSTR a, PWSTR b, BOOLEAN CaseInsensitive)
{
    int alen = (int)wcslen(a);
    int blen = (int)wcslen(b);

    return alen >= blen && 0 == MemfsFileNameCompare(a, blen, b, blen, CaseInsensitive) &&
        (alen == blen || (1 == blen && L'\\' == b[0]) ||
#if defined(MEMFS_NAMED_STREAMS)
            (L'\\' == a[blen] || L':' == a[blen]));
#else
            (L'\\' == a[blen]));
#endif
}

#if defined(MEMFS_EA)
static inline
int MemfsEaNameCompare(PSTR a, PSTR b)
{
    /* EA names are always case-insensitive in MEMFS (to be inline with NTFS) */

    int res;

    res = CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, a, -1, b, -1);
    if (0 != res)
        res -= 2;
    else
        res = _stricmp(a, b);

    return res;
}

struct MEMFS_FILE_NODE_EA_LESS
{
    MEMFS_FILE_NODE_EA_LESS()
    {
    }
    bool operator()(PSTR a, PSTR b) const
    {
        return 0 > MemfsEaNameCompare(a, b);
    }
};
typedef std::map<PSTR, FILE_FULL_EA_INFORMATION *, MEMFS_FILE_NODE_EA_LESS> MEMFS_FILE_NODE_EA_MAP;
#endif

typedef struct _MEMFS_FILE_NODE
{
    WCHAR FileName[MEMFS_MAX_PATH];
    FSP_FSCTL_FILE_INFO FileInfo;
    SIZE_T FileSecuritySize;
    PVOID FileSecurity;
    PVOID FileData;
#if defined(MEMFS_REPARSE_POINTS)
    SIZE_T ReparseDataSize;
    PVOID ReparseData;
#endif
#if defined(MEMFS_EA)
    MEMFS_FILE_NODE_EA_MAP *EaMap;
#endif
    volatile LONG RefCount;
#if defined(MEMFS_NAMED_STREAMS)
    struct _MEMFS_FILE_NODE *MainFileNode;
#endif
} MEMFS_FILE_NODE;

struct MEMFS_FILE_NODE_LESS
{
    MEMFS_FILE_NODE_LESS(BOOLEAN CaseInsensitive) : CaseInsensitive(CaseInsensitive)
    {
    }
    bool operator()(PWSTR a, PWSTR b) const
    {
        return 0 > MemfsFileNameCompare(a, -1, b, -1, CaseInsensitive);
    }
    BOOLEAN CaseInsensitive;
};
typedef std::map<PWSTR, MEMFS_FILE_NODE *, MEMFS_FILE_NODE_LESS> MEMFS_FILE_NODE_MAP;

typedef struct _MEMFS
{
    FSP_FILE_SYSTEM *FileSystem;
    MEMFS_FILE_NODE_MAP *FileNodeMap;
    ULONG MaxFileNodes;
    ULONG MaxFileSize;
#ifdef MEMFS_SLOWIO
    ULONG SlowioMaxDelay;
    ULONG SlowioPercentDelay;
    ULONG SlowioRarefyDelay;
    volatile LONG SlowioThreadsRunning;
#endif
    UINT16 VolumeLabelLength;
    WCHAR VolumeLabel[32];
} MEMFS;

static inline
NTSTATUS MemfsFileNodeCreate(PWSTR FileName, MEMFS_FILE_NODE **PFileNode)
{
    static UINT64 IndexNumber = 1;
    MEMFS_FILE_NODE *FileNode;

    *PFileNode = 0;

    FileNode = (MEMFS_FILE_NODE *)malloc(sizeof *FileNode);
    if (0 == FileNode)
        return STATUS_INSUFFICIENT_RESOURCES;

    memset(FileNode, 0, sizeof *FileNode);
    wcscpy_s(FileNode->FileName, sizeof FileNode->FileName / sizeof(WCHAR), FileName);
    FileNode->FileInfo.CreationTime =
    FileNode->FileInfo.LastAccessTime =
    FileNode->FileInfo.LastWriteTime =
    FileNode->FileInfo.ChangeTime = MemfsGetSystemTime();
    FileNode->FileInfo.IndexNumber = IndexNumber++;

    *PFileNode = FileNode;

    return STATUS_SUCCESS;
}

#if defined(MEMFS_EA)
static inline
VOID MemfsFileNodeDeleteEaMap(MEMFS_FILE_NODE *FileNode)
{
    if (0 != FileNode->EaMap)
    {
        for (MEMFS_FILE_NODE_EA_MAP::iterator p = FileNode->EaMap->begin(), q = FileNode->EaMap->end();
            p != q; ++p)
            free(p->second);
        delete FileNode->EaMap;
        FileNode->EaMap = 0;
        FileNode->FileInfo.EaSize = 0;
    }
}
#endif

static inline
VOID MemfsFileNodeDelete(MEMFS_FILE_NODE *FileNode)
{
#if defined(MEMFS_EA)
    MemfsFileNodeDeleteEaMap(FileNode);
#endif
#if defined(MEMFS_REPARSE_POINTS)
    free(FileNode->ReparseData);
#endif
    LargeHeapFree(FileNode->FileData);
    free(FileNode->FileSecurity);
    free(FileNode);
}

static inline
VOID MemfsFileNodeReference(MEMFS_FILE_NODE *FileNode)
{
    InterlockedIncrement(&FileNode->RefCount);
}

static inline
VOID MemfsFileNodeDereference(MEMFS_FILE_NODE *FileNode)
{
    if (0 == InterlockedDecrement(&FileNode->RefCount))
        MemfsFileNodeDelete(FileNode);
}

static inline
VOID MemfsFileNodeGetFileInfo(MEMFS_FILE_NODE *FileNode, FSP_FSCTL_FILE_INFO *FileInfo)
{
#if defined(MEMFS_NAMED_STREAMS)
    if (0 == FileNode->MainFileNode)
        *FileInfo = FileNode->FileInfo;
    else
    {
        *FileInfo = FileNode->MainFileNode->FileInfo;
        FileInfo->FileAttributes &= ~FILE_ATTRIBUTE_DIRECTORY;
            /* named streams cannot be directories */
        FileInfo->AllocationSize = FileNode->FileInfo.AllocationSize;
        FileInfo->FileSize = FileNode->FileInfo.FileSize;
    }
#else
    *FileInfo = FileNode->FileInfo;
#endif
}

#if defined(MEMFS_EA)
static inline
NTSTATUS MemfsFileNodeGetEaMap(MEMFS_FILE_NODE *FileNode, MEMFS_FILE_NODE_EA_MAP **PEaMap)
{
#if defined(MEMFS_NAMED_STREAMS)
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
#endif

    *PEaMap = FileNode->EaMap;
    if (0 != *PEaMap)
        return STATUS_SUCCESS;

    try
    {
        *PEaMap = FileNode->EaMap = new MEMFS_FILE_NODE_EA_MAP(MEMFS_FILE_NODE_EA_LESS());
        return STATUS_SUCCESS;
    }
    catch (...)
    {
        *PEaMap = 0;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
}

static inline
NTSTATUS MemfsFileNodeSetEa(
    FSP_FILE_SYSTEM *FileSystem, PVOID Context,
    PFILE_FULL_EA_INFORMATION Ea)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)Context;
    MEMFS_FILE_NODE_EA_MAP *EaMap;
    FILE_FULL_EA_INFORMATION *FileNodeEa = 0;
    MEMFS_FILE_NODE_EA_MAP::iterator p;
    ULONG EaSizePlus = 0, EaSizeMinus = 0;
    NTSTATUS Result;

    Result = MemfsFileNodeGetEaMap(FileNode, &EaMap);
    if (!NT_SUCCESS(Result))
        return Result;

    if (0 != Ea->EaValueLength)
    {
        EaSizePlus = FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) +
            Ea->EaNameLength + 1 + Ea->EaValueLength;
        FileNodeEa = (FILE_FULL_EA_INFORMATION *)malloc(EaSizePlus);
        if (0 == FileNodeEa)
            return STATUS_INSUFFICIENT_RESOURCES;
        memcpy(FileNodeEa, Ea, EaSizePlus);
        FileNodeEa->NextEntryOffset = 0;

        EaSizePlus = FspFileSystemGetEaPackedSize(Ea);
    }

    p = EaMap->find(Ea->EaName);
    if (p != EaMap->end())
    {
        EaSizeMinus = FspFileSystemGetEaPackedSize(Ea);

        free(p->second);
        EaMap->erase(p);
    }

    if (0 != Ea->EaValueLength)
    {
        try
        {
            EaMap->insert(MEMFS_FILE_NODE_EA_MAP::value_type(FileNodeEa->EaName, FileNodeEa));
        }
        catch (...)
        {
            free(FileNodeEa);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    FileNode->FileInfo.EaSize = FileNode->FileInfo.EaSize + EaSizePlus - EaSizeMinus;

    return STATUS_SUCCESS;
}

static inline
BOOLEAN MemfsFileNodeNeedEa(MEMFS_FILE_NODE *FileNode)
{
#if defined(MEMFS_NAMED_STREAMS)
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
#endif

    if (0 != FileNode->EaMap)
    {
        for (MEMFS_FILE_NODE_EA_MAP::iterator p = FileNode->EaMap->begin(), q = FileNode->EaMap->end();
            p != q; ++p)
            if (0 != (p->second->Flags & FILE_NEED_EA))
                return TRUE;
    }

    return FALSE;
}

static inline
BOOLEAN MemfsFileNodeEnumerateEa(MEMFS_FILE_NODE *FileNode,
    BOOLEAN (*EnumFn)(PFILE_FULL_EA_INFORMATION Ea, PVOID), PVOID Context)
{
#if defined(MEMFS_NAMED_STREAMS)
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
#endif

    if (0 != FileNode->EaMap)
    {
        for (MEMFS_FILE_NODE_EA_MAP::iterator p = FileNode->EaMap->begin(), q = FileNode->EaMap->end();
            p != q; ++p)
            if (!EnumFn(p->second, Context))
                return FALSE;
    }

    return TRUE;
}
#endif

static inline
VOID MemfsFileNodeMapDump(MEMFS_FILE_NODE_MAP *FileNodeMap)
{
    for (MEMFS_FILE_NODE_MAP::iterator p = FileNodeMap->begin(), q = FileNodeMap->end(); p != q; ++p)
        FspDebugLog("%c %04lx %6lu %S\n",
            FILE_ATTRIBUTE_DIRECTORY & p->second->FileInfo.FileAttributes ? 'd' : 'f',
            (ULONG)p->second->FileInfo.FileAttributes,
            (ULONG)p->second->FileInfo.FileSize,
            p->second->FileName);
}

static inline
BOOLEAN MemfsFileNodeMapIsCaseInsensitive(MEMFS_FILE_NODE_MAP *FileNodeMap)
{
    return FileNodeMap->key_comp().CaseInsensitive;
}

static inline
NTSTATUS MemfsFileNodeMapCreate(BOOLEAN CaseInsensitive, MEMFS_FILE_NODE_MAP **PFileNodeMap)
{
    *PFileNodeMap = 0;
    try
    {
        *PFileNodeMap = new MEMFS_FILE_NODE_MAP(MEMFS_FILE_NODE_LESS(CaseInsensitive));
        return STATUS_SUCCESS;
    }
    catch (...)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
}

static inline
VOID MemfsFileNodeMapDelete(MEMFS_FILE_NODE_MAP *FileNodeMap)
{
    for (MEMFS_FILE_NODE_MAP::iterator p = FileNodeMap->begin(), q = FileNodeMap->end(); p != q; ++p)
        MemfsFileNodeDelete(p->second);

    delete FileNodeMap;
}

static inline
SIZE_T MemfsFileNodeMapCount(MEMFS_FILE_NODE_MAP *FileNodeMap)
{
    return FileNodeMap->size();
}

static inline
MEMFS_FILE_NODE *MemfsFileNodeMapGet(MEMFS_FILE_NODE_MAP *FileNodeMap, PWSTR FileName)
{
    MEMFS_FILE_NODE_MAP::iterator iter = FileNodeMap->find(FileName);
    if (iter == FileNodeMap->end())
        return 0;
    return iter->second;
}

#if defined(MEMFS_NAMED_STREAMS)
static inline
MEMFS_FILE_NODE *MemfsFileNodeMapGetMain(MEMFS_FILE_NODE_MAP *FileNodeMap, PWSTR FileName0)
{
    WCHAR FileName[MEMFS_MAX_PATH];
    wcscpy_s(FileName, sizeof FileName / sizeof(WCHAR), FileName0);
    PWSTR StreamName = wcschr(FileName, L':');
    if (0 == StreamName)
        return 0;
    StreamName[0] = L'\0';
    MEMFS_FILE_NODE_MAP::iterator iter = FileNodeMap->find(FileName);
    if (iter == FileNodeMap->end())
        return 0;
    return iter->second;
}
#endif

static inline
MEMFS_FILE_NODE *MemfsFileNodeMapGetParent(MEMFS_FILE_NODE_MAP *FileNodeMap, PWSTR FileName0,
    PNTSTATUS PResult)
{
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;
    WCHAR FileName[MEMFS_MAX_PATH];
    wcscpy_s(FileName, sizeof FileName / sizeof(WCHAR), FileName0);
    FspPathSuffix(FileName, &Remain, &Suffix, Root);
    MEMFS_FILE_NODE_MAP::iterator iter = FileNodeMap->find(Remain);
    FspPathCombine(FileName, Suffix);
    if (iter == FileNodeMap->end())
    {
        *PResult = STATUS_OBJECT_PATH_NOT_FOUND;
        return 0;
    }
    if (0 == (iter->second->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        *PResult = STATUS_NOT_A_DIRECTORY;
        return 0;
    }
    return iter->second;
}

static inline
VOID MemfsFileNodeMapTouchParent(MEMFS_FILE_NODE_MAP *FileNodeMap, MEMFS_FILE_NODE *FileNode)
{
    NTSTATUS Result;
    MEMFS_FILE_NODE *Parent;
    if (L'\\' == FileNode->FileName[0] && L'\0' == FileNode->FileName[1])
        return;
    Parent = MemfsFileNodeMapGetParent(FileNodeMap, FileNode->FileName, &Result);
    if (0 == Parent)
        return;
    Parent->FileInfo.LastAccessTime =
    Parent->FileInfo.LastWriteTime =
    Parent->FileInfo.ChangeTime = MemfsGetSystemTime();
}

static inline
NTSTATUS MemfsFileNodeMapInsert(MEMFS_FILE_NODE_MAP *FileNodeMap, MEMFS_FILE_NODE *FileNode,
    PBOOLEAN PInserted)
{
    *PInserted = 0;
    try
    {
        *PInserted = FileNodeMap->insert(MEMFS_FILE_NODE_MAP::value_type(FileNode->FileName, FileNode)).second;
        if (*PInserted)
        {
            MemfsFileNodeReference(FileNode);
            MemfsFileNodeMapTouchParent(FileNodeMap, FileNode);
        }
        return STATUS_SUCCESS;
    }
    catch (...)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
}

static inline
VOID MemfsFileNodeMapRemove(MEMFS_FILE_NODE_MAP *FileNodeMap, MEMFS_FILE_NODE *FileNode)
{
    if (FileNodeMap->erase(FileNode->FileName))
    {
        MemfsFileNodeMapTouchParent(FileNodeMap, FileNode);
        MemfsFileNodeDereference(FileNode);
    }
}

static inline
BOOLEAN MemfsFileNodeMapHasChild(MEMFS_FILE_NODE_MAP *FileNodeMap, MEMFS_FILE_NODE *FileNode)
{
    BOOLEAN Result = FALSE;
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;
    MEMFS_FILE_NODE_MAP::iterator iter = FileNodeMap->upper_bound(FileNode->FileName);
    for (; FileNodeMap->end() != iter; ++iter)
    {
#if defined(MEMFS_NAMED_STREAMS)
        if (0 != wcschr(iter->second->FileName, L':'))
            continue;
#endif
        FspPathSuffix(iter->second->FileName, &Remain, &Suffix, Root);
        Result = 0 == MemfsFileNameCompare(Remain, -1, FileNode->FileName, -1,
            MemfsFileNodeMapIsCaseInsensitive(FileNodeMap));
        FspPathCombine(iter->second->FileName, Suffix);
        break;
    }
    return Result;
}

static inline
BOOLEAN MemfsFileNodeMapEnumerateChildren(MEMFS_FILE_NODE_MAP *FileNodeMap, MEMFS_FILE_NODE *FileNode,
    PWSTR PrevFileName0, BOOLEAN (*EnumFn)(MEMFS_FILE_NODE *, PVOID), PVOID Context)
{
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;
    MEMFS_FILE_NODE_MAP::iterator iter;
    BOOLEAN IsDirectoryChild;
    if (0 != PrevFileName0)
    {
        WCHAR PrevFileName[MEMFS_MAX_PATH + 256];
        size_t Length0 = wcslen(FileNode->FileName);
        size_t Length1 = 1 != Length0 || L'\\' != FileNode->FileName[0];
        size_t Length2 = wcslen(PrevFileName0);
        assert(MEMFS_MAX_PATH + 256 > Length0 + Length1 + Length2);
        memcpy(PrevFileName, FileNode->FileName, Length0 * sizeof(WCHAR));
        memcpy(PrevFileName + Length0, L"\\", Length1 * sizeof(WCHAR));
        memcpy(PrevFileName + Length0 + Length1, PrevFileName0, Length2 * sizeof(WCHAR));
        PrevFileName[Length0 + Length1 + Length2] = L'\0';
        iter = FileNodeMap->upper_bound(PrevFileName);
    }
    else
        iter = FileNodeMap->upper_bound(FileNode->FileName);
    for (; FileNodeMap->end() != iter; ++iter)
    {
        if (!MemfsFileNameHasPrefix(iter->second->FileName, FileNode->FileName,
            MemfsFileNodeMapIsCaseInsensitive(FileNodeMap)))
            break;
        FspPathSuffix(iter->second->FileName, &Remain, &Suffix, Root);
        IsDirectoryChild = 0 == MemfsFileNameCompare(Remain, -1, FileNode->FileName, -1,
            MemfsFileNodeMapIsCaseInsensitive(FileNodeMap));
#if defined(MEMFS_NAMED_STREAMS)
        IsDirectoryChild = IsDirectoryChild && 0 == wcschr(Suffix, L':');
#endif
        FspPathCombine(iter->second->FileName, Suffix);
        if (IsDirectoryChild)
        {
            if (!EnumFn(iter->second, Context))
                return FALSE;
        }
    }
    return TRUE;
}

#if defined(MEMFS_NAMED_STREAMS)
static inline
BOOLEAN MemfsFileNodeMapEnumerateNamedStreams(MEMFS_FILE_NODE_MAP *FileNodeMap, MEMFS_FILE_NODE *FileNode,
    BOOLEAN (*EnumFn)(MEMFS_FILE_NODE *, PVOID), PVOID Context)
{
    MEMFS_FILE_NODE_MAP::iterator iter = FileNodeMap->upper_bound(FileNode->FileName);
    for (; FileNodeMap->end() != iter; ++iter)
    {
        if (!MemfsFileNameHasPrefix(iter->second->FileName, FileNode->FileName,
            MemfsFileNodeMapIsCaseInsensitive(FileNodeMap)))
            break;
        if (L':' != iter->second->FileName[wcslen(FileNode->FileName)])
            break;
        if (!EnumFn(iter->second, Context))
            return FALSE;
    }
    return TRUE;
}
#endif

static inline
BOOLEAN MemfsFileNodeMapEnumerateDescendants(MEMFS_FILE_NODE_MAP *FileNodeMap, MEMFS_FILE_NODE *FileNode,
    BOOLEAN (*EnumFn)(MEMFS_FILE_NODE *, PVOID), PVOID Context)
{
    WCHAR Root[2] = L"\\";
    MEMFS_FILE_NODE_MAP::iterator iter = FileNodeMap->lower_bound(FileNode->FileName);
    for (; FileNodeMap->end() != iter; ++iter)
    {
        if (!MemfsFileNameHasPrefix(iter->second->FileName, FileNode->FileName,
            MemfsFileNodeMapIsCaseInsensitive(FileNodeMap)))
            break;
        if (!EnumFn(iter->second, Context))
            return FALSE;
    }
    return TRUE;
}

typedef struct _MEMFS_FILE_NODE_MAP_ENUM_CONTEXT
{
    BOOLEAN Reference;
    MEMFS_FILE_NODE **FileNodes;
    ULONG Capacity, Count;
} MEMFS_FILE_NODE_MAP_ENUM_CONTEXT;

static inline
BOOLEAN MemfsFileNodeMapEnumerateFn(MEMFS_FILE_NODE *FileNode, PVOID Context0)
{
    MEMFS_FILE_NODE_MAP_ENUM_CONTEXT *Context = (MEMFS_FILE_NODE_MAP_ENUM_CONTEXT *)Context0;

    if (Context->Capacity <= Context->Count)
    {
        ULONG Capacity = 0 != Context->Capacity ? Context->Capacity * 2 : 16;
        PVOID P = realloc(Context->FileNodes, Capacity * sizeof Context->FileNodes[0]);
        if (0 == P)
        {
            FspDebugLog(__FUNCTION__ ": cannot allocate memory; aborting\n");
            abort();
        }

        Context->FileNodes = (MEMFS_FILE_NODE **)P;
        Context->Capacity = Capacity;
    }

    Context->FileNodes[Context->Count++] = FileNode;
    if (Context->Reference)
        MemfsFileNodeReference(FileNode);

    return TRUE;
}

static inline
VOID MemfsFileNodeMapEnumerateFree(MEMFS_FILE_NODE_MAP_ENUM_CONTEXT *Context)
{
    if (Context->Reference)
    {
        for (ULONG Index = 0; Context->Count > Index; Index++)
        {
            MEMFS_FILE_NODE *FileNode = Context->FileNodes[Index];
            MemfsFileNodeDereference(FileNode);
        }
    }
    free(Context->FileNodes);
}

#ifdef MEMFS_SLOWIO
/*
 * SLOWIO
 *
 * This is included for two uses:
 *
 * 1) For testing winfsp, by allowing memfs to act more like a non-ram file system,
 *    with some IO taking many milliseconds, and some IO completion delayed.
 *
 * 2) As sample code for how to use winfsp's STATUS_PENDING capabilities.
 * 
 */

static inline UINT64 Hash(UINT64 x)
{
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    x =  x ^ (x >> 31);
    return x;
}

static inline ULONG PseudoRandom(ULONG to)
{
    /* John Oberschelp's PRNG */
    static UINT64 spin = 0;
    InterlockedIncrement(&spin);
    return Hash(spin) % to;
}

static inline BOOLEAN SlowioReturnPending(FSP_FILE_SYSTEM *FileSystem)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    if (0 == Memfs->SlowioMaxDelay)
        return FALSE;
    return PseudoRandom(100) < Memfs->SlowioPercentDelay;
}

static inline VOID SlowioSnooze(FSP_FILE_SYSTEM *FileSystem)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    if (0 == Memfs->SlowioMaxDelay)
        return;
    ULONG millis = PseudoRandom(Memfs->SlowioMaxDelay + 1) >> PseudoRandom(Memfs->SlowioRarefyDelay + 1);
    Sleep(millis);
}

void SlowioReadThread(
    FSP_FILE_SYSTEM *FileSystem,
    MEMFS_FILE_NODE *FileNode,
    PVOID Buffer, 
    UINT64 Offset, 
    UINT64 EndOffset,
    UINT64 RequestHint)
{
    SlowioSnooze(FileSystem);

    memcpy(Buffer, (PUINT8)FileNode->FileData + Offset, (size_t)(EndOffset - Offset));
    UINT32 BytesTransferred = (ULONG)(EndOffset - Offset);

    FSP_FSCTL_TRANSACT_RSP ResponseBuf;
    memset(&ResponseBuf, 0, sizeof ResponseBuf);
    ResponseBuf.Size = sizeof ResponseBuf;
    ResponseBuf.Kind = FspFsctlTransactReadKind;
    ResponseBuf.Hint = RequestHint;                         // IRP that is being completed
    ResponseBuf.IoStatus.Status = STATUS_SUCCESS;
    ResponseBuf.IoStatus.Information = BytesTransferred;    // bytes read
    FspFileSystemSendResponse(FileSystem, &ResponseBuf);

    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    InterlockedDecrement(&Memfs->SlowioThreadsRunning);
}

void SlowioWriteThread(
    FSP_FILE_SYSTEM *FileSystem,
    MEMFS_FILE_NODE *FileNode,
    PVOID Buffer, 
    UINT64 Offset, 
    UINT64 EndOffset,
    UINT64 RequestHint)
{
    SlowioSnooze(FileSystem);

    memcpy((PUINT8)FileNode->FileData + Offset, Buffer, (size_t)(EndOffset - Offset));
    UINT32 BytesTransferred = (ULONG)(EndOffset - Offset);

    FSP_FSCTL_TRANSACT_RSP ResponseBuf;
    memset(&ResponseBuf, 0, sizeof ResponseBuf);
    ResponseBuf.Size = sizeof ResponseBuf;
    ResponseBuf.Kind = FspFsctlTransactWriteKind;
    ResponseBuf.Hint = RequestHint;                         // IRP that is being completed
    ResponseBuf.IoStatus.Status = STATUS_SUCCESS;
    ResponseBuf.IoStatus.Information = BytesTransferred;    // bytes written
    MemfsFileNodeGetFileInfo(FileNode, &ResponseBuf.Rsp.Write.FileInfo);
    FspFileSystemSendResponse(FileSystem, &ResponseBuf);

    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    InterlockedDecrement(&Memfs->SlowioThreadsRunning);
}

void SlowioReadDirectoryThread(
    FSP_FILE_SYSTEM *FileSystem,
    ULONG BytesTransferred,
    UINT64 RequestHint)
{
    SlowioSnooze(FileSystem);

    FSP_FSCTL_TRANSACT_RSP ResponseBuf;
    memset(&ResponseBuf, 0, sizeof ResponseBuf);
    ResponseBuf.Size = sizeof ResponseBuf;
    ResponseBuf.Kind = FspFsctlTransactQueryDirectoryKind;
    ResponseBuf.Hint = RequestHint;                         // IRP that is being completed
    ResponseBuf.IoStatus.Status = STATUS_SUCCESS;
    ResponseBuf.IoStatus.Information = BytesTransferred;    // bytes of directory info read
    FspFileSystemSendResponse(FileSystem, &ResponseBuf);

    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    InterlockedDecrement(&Memfs->SlowioThreadsRunning);
}
#endif

/*
 * FSP_FILE_SYSTEM_INTERFACE
 */

#if defined(MEMFS_REPARSE_POINTS)
static NTSTATUS GetReparsePointByName(
    FSP_FILE_SYSTEM *FileSystem, PVOID Context,
    PWSTR FileName, BOOLEAN IsDirectory, PVOID Buffer, PSIZE_T PSize);
#endif

static NTSTATUS SetFileSizeInternal(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0, UINT64 NewSize, BOOLEAN SetAllocationSize);

static NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;

    VolumeInfo->TotalSize = Memfs->MaxFileNodes * (UINT64)Memfs->MaxFileSize;
    VolumeInfo->FreeSize = (Memfs->MaxFileNodes - MemfsFileNodeMapCount(Memfs->FileNodeMap)) *
        (UINT64)Memfs->MaxFileSize;
    VolumeInfo->VolumeLabelLength = Memfs->VolumeLabelLength;
    memcpy(VolumeInfo->VolumeLabel, Memfs->VolumeLabel, Memfs->VolumeLabelLength);

    return STATUS_SUCCESS;
}

static NTSTATUS SetVolumeLabel(FSP_FILE_SYSTEM *FileSystem,
    PWSTR VolumeLabel,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;

    Memfs->VolumeLabelLength = (UINT16)(wcslen(VolumeLabel) * sizeof(WCHAR));
    if (Memfs->VolumeLabelLength > sizeof Memfs->VolumeLabel)
        Memfs->VolumeLabelLength = sizeof Memfs->VolumeLabel;
    memcpy(Memfs->VolumeLabel, VolumeLabel, Memfs->VolumeLabelLength);

    VolumeInfo->TotalSize = Memfs->MaxFileNodes * Memfs->MaxFileSize;
    VolumeInfo->FreeSize =
        (Memfs->MaxFileNodes - MemfsFileNodeMapCount(Memfs->FileNodeMap)) * Memfs->MaxFileSize;
    VolumeInfo->VolumeLabelLength = Memfs->VolumeLabelLength;
    memcpy(VolumeInfo->VolumeLabel, Memfs->VolumeLabel, Memfs->VolumeLabelLength);

    return STATUS_SUCCESS;
}

static NTSTATUS GetSecurityByName(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode;
    NTSTATUS Result;

    FileNode = MemfsFileNodeMapGet(Memfs->FileNodeMap, FileName);
    if (0 == FileNode)
    {
        Result = STATUS_OBJECT_NAME_NOT_FOUND;

#if defined(MEMFS_REPARSE_POINTS)
        if (FspFileSystemFindReparsePoint(FileSystem, GetReparsePointByName, 0,
            FileName, PFileAttributes))
            Result = STATUS_REPARSE;
        else
#endif
            MemfsFileNodeMapGetParent(Memfs->FileNodeMap, FileName, &Result);

        return Result;
    }

#if defined(MEMFS_NAMED_STREAMS)
    UINT32 FileAttributesMask = ~(UINT32)0;
    if (0 != FileNode->MainFileNode)
    {
        FileAttributesMask = ~(UINT32)FILE_ATTRIBUTE_DIRECTORY;
        FileNode = FileNode->MainFileNode;
    }

    if (0 != PFileAttributes)
        *PFileAttributes = FileNode->FileInfo.FileAttributes & FileAttributesMask;
#else
    if (0 != PFileAttributes)
        *PFileAttributes = FileNode->FileInfo.FileAttributes;
#endif

    if (0 != PSecurityDescriptorSize)
    {
        if (FileNode->FileSecuritySize > *PSecurityDescriptorSize)
        {
            *PSecurityDescriptorSize = FileNode->FileSecuritySize;
            return STATUS_BUFFER_OVERFLOW;
        }

        *PSecurityDescriptorSize = FileNode->FileSecuritySize;
        if (0 != SecurityDescriptor)
            memcpy(SecurityDescriptor, FileNode->FileSecurity, FileNode->FileSecuritySize);
    }

    return STATUS_SUCCESS;
}

static NTSTATUS Create(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
    UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
#if defined(MEMFS_EA) || defined(MEMFS_WSL)
    PVOID ExtraBuffer, ULONG ExtraLength, BOOLEAN ExtraBufferIsReparsePoint,
#endif
    PVOID *PFileNode, FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
#if defined(MEMFS_NAME_NORMALIZATION)
    WCHAR FileNameBuf[MEMFS_MAX_PATH];
#endif
    MEMFS_FILE_NODE *FileNode;
    MEMFS_FILE_NODE *ParentNode;
    NTSTATUS Result;
    BOOLEAN Inserted;

    if (MEMFS_MAX_PATH <= wcslen(FileName))
        return STATUS_OBJECT_NAME_INVALID;

    if (CreateOptions & FILE_DIRECTORY_FILE)
        AllocationSize = 0;

    FileNode = MemfsFileNodeMapGet(Memfs->FileNodeMap, FileName);
    if (0 != FileNode)
        return STATUS_OBJECT_NAME_COLLISION;

    ParentNode = MemfsFileNodeMapGetParent(Memfs->FileNodeMap, FileName, &Result);
    if (0 == ParentNode)
        return Result;

    if (MemfsFileNodeMapCount(Memfs->FileNodeMap) >= Memfs->MaxFileNodes)
        return STATUS_CANNOT_MAKE;

    if (AllocationSize > Memfs->MaxFileSize)
        return STATUS_DISK_FULL;

#if defined(MEMFS_NAME_NORMALIZATION)
    if (MemfsFileNodeMapIsCaseInsensitive(Memfs->FileNodeMap))
    {
        WCHAR Root[2] = L"\\";
        PWSTR Remain, Suffix;
        size_t RemainLength, BSlashLength, SuffixLength;

        FspPathSuffix(FileName, &Remain, &Suffix, Root);
        assert(0 == MemfsFileNameCompare(Remain, -1, ParentNode->FileName, -1, TRUE));
        FspPathCombine(FileName, Suffix);

        RemainLength = wcslen(ParentNode->FileName);
        BSlashLength = 1 < RemainLength;
        SuffixLength = wcslen(Suffix);
        if (MEMFS_MAX_PATH <= RemainLength + BSlashLength + SuffixLength)
            return STATUS_OBJECT_NAME_INVALID;

        memcpy(FileNameBuf, ParentNode->FileName, RemainLength * sizeof(WCHAR));
        memcpy(FileNameBuf + RemainLength, L"\\", BSlashLength * sizeof(WCHAR));
        memcpy(FileNameBuf + RemainLength + BSlashLength, Suffix, (SuffixLength + 1) * sizeof(WCHAR));

        FileName = FileNameBuf;
    }
#endif

    Result = MemfsFileNodeCreate(FileName, &FileNode);
    if (!NT_SUCCESS(Result))
        return Result;

#if defined(MEMFS_NAMED_STREAMS)
    FileNode->MainFileNode = MemfsFileNodeMapGetMain(Memfs->FileNodeMap, FileName);
#endif

    FileNode->FileInfo.FileAttributes = (FileAttributes & FILE_ATTRIBUTE_DIRECTORY) ?
        FileAttributes : FileAttributes | FILE_ATTRIBUTE_ARCHIVE;

    if (0 != SecurityDescriptor)
    {
        FileNode->FileSecuritySize = GetSecurityDescriptorLength(SecurityDescriptor);
        FileNode->FileSecurity = (PSECURITY_DESCRIPTOR)malloc(FileNode->FileSecuritySize);
        if (0 == FileNode->FileSecurity)
        {
            MemfsFileNodeDelete(FileNode);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        memcpy(FileNode->FileSecurity, SecurityDescriptor, FileNode->FileSecuritySize);
    }

#if defined(MEMFS_EA) || defined(MEMFS_WSL)
    if (0 != ExtraBuffer)
    {
#if defined(MEMFS_EA)
        if (!ExtraBufferIsReparsePoint)
        {
            Result = FspFileSystemEnumerateEa(FileSystem, MemfsFileNodeSetEa, FileNode,
                (PFILE_FULL_EA_INFORMATION)ExtraBuffer, ExtraLength);
            if (!NT_SUCCESS(Result))
            {
                MemfsFileNodeDelete(FileNode);
                return Result;
            }
        }
#endif
#if defined(MEMFS_WSL)
        if (ExtraBufferIsReparsePoint)
        {
#if defined(MEMFS_REPARSE_POINTS)
            FileNode->ReparseDataSize = ExtraLength;
            FileNode->ReparseData = malloc(ExtraLength);
            if (0 == FileNode->ReparseData && 0 != ExtraLength)
            {
                MemfsFileNodeDelete(FileNode);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            FileNode->FileInfo.FileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
            FileNode->FileInfo.ReparseTag = *(PULONG)ExtraBuffer;
                /* the first field in a reparse buffer is the reparse tag */
            memcpy(FileNode->ReparseData, ExtraBuffer, ExtraLength);
#else
            MemfsFileNodeDelete(FileNode);
            return STATUS_INVALID_PARAMETER;
#endif
        }
#endif
    }
#endif

    FileNode->FileInfo.AllocationSize = AllocationSize;
    if (0 != FileNode->FileInfo.AllocationSize)
    {
        FileNode->FileData = LargeHeapAlloc((size_t)FileNode->FileInfo.AllocationSize);
        if (0 == FileNode->FileData)
        {
            MemfsFileNodeDelete(FileNode);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    Result = MemfsFileNodeMapInsert(Memfs->FileNodeMap, FileNode, &Inserted);
    if (!NT_SUCCESS(Result) || !Inserted)
    {
        MemfsFileNodeDelete(FileNode);
        if (NT_SUCCESS(Result))
            Result = STATUS_OBJECT_NAME_COLLISION; /* should not happen! */
        return Result;
    }

    MemfsFileNodeReference(FileNode);
    *PFileNode = FileNode;
    MemfsFileNodeGetFileInfo(FileNode, FileInfo);

#if defined(MEMFS_NAME_NORMALIZATION)
    if (MemfsFileNodeMapIsCaseInsensitive(Memfs->FileNodeMap))
    {
        FSP_FSCTL_OPEN_FILE_INFO *OpenFileInfo = FspFileSystemGetOpenFileInfo(FileInfo);

        wcscpy_s(OpenFileInfo->NormalizedName, OpenFileInfo->NormalizedNameSize / sizeof(WCHAR),
            FileNode->FileName);
        OpenFileInfo->NormalizedNameSize = (UINT16)(wcslen(FileNode->FileName) * sizeof(WCHAR));
    }
#endif

    return STATUS_SUCCESS;
}

static NTSTATUS Open(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
    PVOID *PFileNode, FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode;
    NTSTATUS Result;

    if (MEMFS_MAX_PATH <= wcslen(FileName))
        return STATUS_OBJECT_NAME_INVALID;

    FileNode = MemfsFileNodeMapGet(Memfs->FileNodeMap, FileName);
    if (0 == FileNode)
    {
        Result = STATUS_OBJECT_NAME_NOT_FOUND;
        MemfsFileNodeMapGetParent(Memfs->FileNodeMap, FileName, &Result);
        return Result;
    }

#if defined(MEMFS_EA)
    /* if the OP specified no EA's check the need EA count, but only if accessing main stream */
    if (0 != (CreateOptions & FILE_NO_EA_KNOWLEDGE)
#if defined(MEMFS_NAMED_STREAMS)
        && (0 == FileNode->MainFileNode)
#endif
        )
    {
        if (MemfsFileNodeNeedEa(FileNode))
        {
            Result = STATUS_ACCESS_DENIED;
            return Result;
        }
    }
#endif

    MemfsFileNodeReference(FileNode);
    *PFileNode = FileNode;
    MemfsFileNodeGetFileInfo(FileNode, FileInfo);

#if defined(MEMFS_NAME_NORMALIZATION)
    if (MemfsFileNodeMapIsCaseInsensitive(Memfs->FileNodeMap))
    {
        FSP_FSCTL_OPEN_FILE_INFO *OpenFileInfo = FspFileSystemGetOpenFileInfo(FileInfo);

        wcscpy_s(OpenFileInfo->NormalizedName, OpenFileInfo->NormalizedNameSize / sizeof(WCHAR),
            FileNode->FileName);
        OpenFileInfo->NormalizedNameSize = (UINT16)(wcslen(FileNode->FileName) * sizeof(WCHAR));
    }
#endif

    return STATUS_SUCCESS;
}

static NTSTATUS Overwrite(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize,
#if defined(MEMFS_EA)
    PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength,
#endif
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    NTSTATUS Result;

#if defined(MEMFS_NAMED_STREAMS)
    MEMFS_FILE_NODE_MAP_ENUM_CONTEXT Context = { TRUE };
    ULONG Index;

    MemfsFileNodeMapEnumerateNamedStreams(Memfs->FileNodeMap, FileNode,
        MemfsFileNodeMapEnumerateFn, &Context);
    for (Index = 0; Context.Count > Index; Index++)
    {
        LONG RefCount = Context.FileNodes[Index]->RefCount;
        MemoryBarrier();
        if (2 >= RefCount)
            MemfsFileNodeMapRemove(Memfs->FileNodeMap, Context.FileNodes[Index]);
    }
    MemfsFileNodeMapEnumerateFree(&Context);
#endif

#if defined(MEMFS_EA)
    MemfsFileNodeDeleteEaMap(FileNode);
    if (0 != Ea)
    {
        Result = FspFileSystemEnumerateEa(FileSystem, MemfsFileNodeSetEa, FileNode, Ea, EaLength);
        if (!NT_SUCCESS(Result))
            return Result;
    }
#endif

    Result = SetFileSizeInternal(FileSystem, FileNode, AllocationSize, TRUE);
    if (!NT_SUCCESS(Result))
        return Result;

    if (ReplaceFileAttributes)
        FileNode->FileInfo.FileAttributes = FileAttributes | FILE_ATTRIBUTE_ARCHIVE;
    else
        FileNode->FileInfo.FileAttributes |= FileAttributes | FILE_ATTRIBUTE_ARCHIVE;

    FileNode->FileInfo.FileSize = 0;
    FileNode->FileInfo.LastAccessTime =
    FileNode->FileInfo.LastWriteTime =
    FileNode->FileInfo.ChangeTime = MemfsGetSystemTime();

    MemfsFileNodeGetFileInfo(FileNode, FileInfo);

    return STATUS_SUCCESS;
}

static VOID Cleanup(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0, PWSTR FileName, ULONG Flags)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
#if defined(MEMFS_NAMED_STREAMS)
    MEMFS_FILE_NODE *MainFileNode = 0 != FileNode->MainFileNode ?
        FileNode->MainFileNode : FileNode;
#else
    MEMFS_FILE_NODE *MainFileNode = FileNode;
#endif

    assert(0 != Flags); /* FSP_FSCTL_VOLUME_PARAMS::PostCleanupWhenModifiedOnly ensures this */

    if (Flags & FspCleanupSetArchiveBit)
    {
        if (0 == (MainFileNode->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            MainFileNode->FileInfo.FileAttributes |= FILE_ATTRIBUTE_ARCHIVE;
    }

    if (Flags & (FspCleanupSetLastAccessTime | FspCleanupSetLastWriteTime | FspCleanupSetChangeTime))
    {
        UINT64 SystemTime = MemfsGetSystemTime();

        if (Flags & FspCleanupSetLastAccessTime)
            MainFileNode->FileInfo.LastAccessTime = SystemTime;
        if (Flags & FspCleanupSetLastWriteTime)
            MainFileNode->FileInfo.LastWriteTime = SystemTime;
        if (Flags & FspCleanupSetChangeTime)
            MainFileNode->FileInfo.ChangeTime = SystemTime;
    }

    if (Flags & FspCleanupSetAllocationSize)
    {
        UINT64 AllocationUnit = MEMFS_SECTOR_SIZE * MEMFS_SECTORS_PER_ALLOCATION_UNIT;
        UINT64 AllocationSize = (FileNode->FileInfo.FileSize + AllocationUnit - 1) /
            AllocationUnit * AllocationUnit;

        SetFileSizeInternal(FileSystem, FileNode, AllocationSize, TRUE);
    }

    if ((Flags & FspCleanupDelete) && !MemfsFileNodeMapHasChild(Memfs->FileNodeMap, FileNode))
    {
#if defined(MEMFS_NAMED_STREAMS)
        MEMFS_FILE_NODE_MAP_ENUM_CONTEXT Context = { FALSE };
        ULONG Index;

        MemfsFileNodeMapEnumerateNamedStreams(Memfs->FileNodeMap, FileNode,
            MemfsFileNodeMapEnumerateFn, &Context);
        for (Index = 0; Context.Count > Index; Index++)
            MemfsFileNodeMapRemove(Memfs->FileNodeMap, Context.FileNodes[Index]);
        MemfsFileNodeMapEnumerateFree(&Context);
#endif

        MemfsFileNodeMapRemove(Memfs->FileNodeMap, FileNode);
    }
}

static VOID Close(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

    MemfsFileNodeDereference(FileNode);
}

static NTSTATUS Read(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0, PVOID Buffer, UINT64 Offset, ULONG Length,
    PULONG PBytesTransferred)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    UINT64 EndOffset;

    if (Offset >= FileNode->FileInfo.FileSize)
        return STATUS_END_OF_FILE;

    EndOffset = Offset + Length;
    if (EndOffset > FileNode->FileInfo.FileSize)
        EndOffset = FileNode->FileInfo.FileSize;

#ifdef MEMFS_SLOWIO
    if (SlowioReturnPending(FileSystem))
    {
        MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
        try
        {
            InterlockedIncrement(&Memfs->SlowioThreadsRunning);
            std::thread(SlowioReadThread,
                FileSystem, FileNode, Buffer, Offset, EndOffset,
                FspFileSystemGetOperationContext()->Request->Hint).
                detach();
            return STATUS_PENDING;
        }
        catch (...)
        {
            InterlockedDecrement(&Memfs->SlowioThreadsRunning);
        }
    }
    SlowioSnooze(FileSystem);
#endif

    memcpy(Buffer, (PUINT8)FileNode->FileData + Offset, (size_t)(EndOffset - Offset));

    *PBytesTransferred = (ULONG)(EndOffset - Offset);

    return STATUS_SUCCESS;
}

static NTSTATUS Write(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0, PVOID Buffer, UINT64 Offset, ULONG Length,
    BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
    PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo)
{
#if defined(DEBUG_BUFFER_CHECK)
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    for (PUINT8 P = (PUINT8)Buffer, EndP = P + Length; EndP > P; P += SystemInfo.dwPageSize)
        __try
        {
            *P = *P | 0;
            assert(!IsWindows8OrGreater());
                /* only on Windows 8 we can make the buffer read-only! */
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            /* ignore! */
        }
#endif

    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    UINT64 EndOffset;
    NTSTATUS Result;

    if (ConstrainedIo)
    {
        if (Offset >= FileNode->FileInfo.FileSize)
            return STATUS_SUCCESS;
        EndOffset = Offset + Length;
        if (EndOffset > FileNode->FileInfo.FileSize)
            EndOffset = FileNode->FileInfo.FileSize;
    }
    else
    {
        if (WriteToEndOfFile)
            Offset = FileNode->FileInfo.FileSize;
        EndOffset = Offset + Length;
        if (EndOffset > FileNode->FileInfo.FileSize)
        {
            Result = SetFileSizeInternal(FileSystem, FileNode, EndOffset, FALSE);
            if (!NT_SUCCESS(Result))
                return Result;
        }
    }

#ifdef MEMFS_SLOWIO
    if (SlowioReturnPending(FileSystem))
    {
        MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
        try
        {
            InterlockedIncrement(&Memfs->SlowioThreadsRunning);
            std::thread(SlowioWriteThread,
                FileSystem, FileNode, Buffer, Offset, EndOffset,
                FspFileSystemGetOperationContext()->Request->Hint).
                detach();
            return STATUS_PENDING;
        }
        catch (...)
        {
            InterlockedDecrement(&Memfs->SlowioThreadsRunning);
        }
    }
    SlowioSnooze(FileSystem);
#endif

    memcpy((PUINT8)FileNode->FileData + Offset, Buffer, (size_t)(EndOffset - Offset));

    *PBytesTransferred = (ULONG)(EndOffset - Offset);
    MemfsFileNodeGetFileInfo(FileNode, FileInfo);

    return STATUS_SUCCESS;
}

NTSTATUS Flush(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

    /*  nothing to flush, since we do not cache anything */

    if (0 != FileNode)
    {
#if 0
#if defined(MEMFS_NAMED_STREAMS)
        if (0 != FileNode->MainFileNode)
            FileNode->MainFileNode->FileInfo.LastAccessTime =
            FileNode->MainFileNode->FileInfo.LastWriteTime =
            FileNode->MainFileNode->FileInfo.ChangeTime = MemfsGetSystemTime();
        else
#endif
        FileNode->FileInfo.LastAccessTime =
        FileNode->FileInfo.LastWriteTime =
        FileNode->FileInfo.ChangeTime = MemfsGetSystemTime();
#endif

        MemfsFileNodeGetFileInfo(FileNode, FileInfo);
    }

    return STATUS_SUCCESS;
}

static NTSTATUS GetFileInfo(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

    MemfsFileNodeGetFileInfo(FileNode, FileInfo);

    return STATUS_SUCCESS;
}

static NTSTATUS SetBasicInfo(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0, UINT32 FileAttributes,
    UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

#if defined(MEMFS_NAMED_STREAMS)
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
#endif

    if (INVALID_FILE_ATTRIBUTES != FileAttributes)
        FileNode->FileInfo.FileAttributes = FileAttributes;
    if (0 != CreationTime)
        FileNode->FileInfo.CreationTime = CreationTime;
    if (0 != LastAccessTime)
        FileNode->FileInfo.LastAccessTime = LastAccessTime;
    if (0 != LastWriteTime)
        FileNode->FileInfo.LastWriteTime = LastWriteTime;
    if (0 != ChangeTime)
        FileNode->FileInfo.ChangeTime = ChangeTime;

    MemfsFileNodeGetFileInfo(FileNode, FileInfo);

    return STATUS_SUCCESS;
}

static NTSTATUS SetFileSizeInternal(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0, UINT64 NewSize, BOOLEAN SetAllocationSize)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

    if (SetAllocationSize)
    {
        if (FileNode->FileInfo.AllocationSize != NewSize)
        {
            if (NewSize > Memfs->MaxFileSize)
                return STATUS_DISK_FULL;

            PVOID FileData = LargeHeapRealloc(FileNode->FileData, (size_t)NewSize);
            if (0 == FileData && 0 != NewSize)
                return STATUS_INSUFFICIENT_RESOURCES;

            FileNode->FileData = FileData;

            FileNode->FileInfo.AllocationSize = NewSize;
            if (FileNode->FileInfo.FileSize > NewSize)
                FileNode->FileInfo.FileSize = NewSize;
        }
    }
    else
    {
        if (FileNode->FileInfo.FileSize != NewSize)
        {
            if (FileNode->FileInfo.AllocationSize < NewSize)
            {
                UINT64 AllocationUnit = MEMFS_SECTOR_SIZE * MEMFS_SECTORS_PER_ALLOCATION_UNIT;
                UINT64 AllocationSize = (NewSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit;

                NTSTATUS Result = SetFileSizeInternal(FileSystem, FileNode, AllocationSize, TRUE);
                if (!NT_SUCCESS(Result))
                    return Result;
            }

            if (FileNode->FileInfo.FileSize < NewSize)
                memset((PUINT8)FileNode->FileData + FileNode->FileInfo.FileSize, 0,
                    (size_t)(NewSize - FileNode->FileInfo.FileSize));
            FileNode->FileInfo.FileSize = NewSize;
        }
    }

    return STATUS_SUCCESS;
}

static NTSTATUS SetFileSize(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0, UINT64 NewSize, BOOLEAN SetAllocationSize,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    NTSTATUS Result;

    Result = SetFileSizeInternal(FileSystem, FileNode0, NewSize, SetAllocationSize);
    if (!NT_SUCCESS(Result))
        return Result;

    MemfsFileNodeGetFileInfo(FileNode, FileInfo);

    return STATUS_SUCCESS;
}

static NTSTATUS CanDelete(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0, PWSTR FileName)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

    if (MemfsFileNodeMapHasChild(Memfs->FileNodeMap, FileNode))
        return STATUS_DIRECTORY_NOT_EMPTY;

    return STATUS_SUCCESS;
}

static NTSTATUS Rename(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0,
    PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    MEMFS_FILE_NODE *NewFileNode, *DescendantFileNode;
    MEMFS_FILE_NODE_MAP_ENUM_CONTEXT Context = { TRUE };
    ULONG Index, FileNameLen, NewFileNameLen;
    BOOLEAN Inserted;
    NTSTATUS Result;

    NewFileNode = MemfsFileNodeMapGet(Memfs->FileNodeMap, NewFileName);
    if (0 != NewFileNode && FileNode != NewFileNode)
    {
        if (!ReplaceIfExists)
        {
            Result = STATUS_OBJECT_NAME_COLLISION;
            goto exit;
        }

        if (NewFileNode->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            Result = STATUS_ACCESS_DENIED;
            goto exit;
        }
    }

    MemfsFileNodeMapEnumerateDescendants(Memfs->FileNodeMap, FileNode,
        MemfsFileNodeMapEnumerateFn, &Context);

    FileNameLen = (ULONG)wcslen(FileNode->FileName);
    NewFileNameLen = (ULONG)wcslen(NewFileName);
    for (Index = 0; Context.Count > Index; Index++)
    {
        DescendantFileNode = Context.FileNodes[Index];
        if (MEMFS_MAX_PATH <= wcslen(DescendantFileNode->FileName) - FileNameLen + NewFileNameLen)
        {
            Result = STATUS_OBJECT_NAME_INVALID;
            goto exit;
        }
    }

    if (0 != NewFileNode)
    {
        MemfsFileNodeReference(NewFileNode);
        MemfsFileNodeMapRemove(Memfs->FileNodeMap, NewFileNode);
        MemfsFileNodeDereference(NewFileNode);
    }

    for (Index = 0; Context.Count > Index; Index++)
    {
        DescendantFileNode = Context.FileNodes[Index];
        MemfsFileNodeMapRemove(Memfs->FileNodeMap, DescendantFileNode);
        memmove(DescendantFileNode->FileName + NewFileNameLen,
            DescendantFileNode->FileName + FileNameLen,
            (wcslen(DescendantFileNode->FileName) + 1 - FileNameLen) * sizeof(WCHAR));
        memcpy(DescendantFileNode->FileName, NewFileName, NewFileNameLen * sizeof(WCHAR));
        Result = MemfsFileNodeMapInsert(Memfs->FileNodeMap, DescendantFileNode, &Inserted);
        if (!NT_SUCCESS(Result))
        {
            FspDebugLog(__FUNCTION__ ": cannot insert into FileNodeMap; aborting\n");
            abort();
        }
        assert(Inserted);
    }

    Result = STATUS_SUCCESS;

exit:
    MemfsFileNodeMapEnumerateFree(&Context);

    return Result;
}

static NTSTATUS GetSecurity(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

#if defined(MEMFS_NAMED_STREAMS)
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
#endif

    if (FileNode->FileSecuritySize > *PSecurityDescriptorSize)
    {
        *PSecurityDescriptorSize = FileNode->FileSecuritySize;
        return STATUS_BUFFER_OVERFLOW;
    }

    *PSecurityDescriptorSize = FileNode->FileSecuritySize;
    if (0 != SecurityDescriptor)
        memcpy(SecurityDescriptor, FileNode->FileSecurity, FileNode->FileSecuritySize);

    return STATUS_SUCCESS;
}

static NTSTATUS SetSecurity(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0,
    SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    PSECURITY_DESCRIPTOR NewSecurityDescriptor, FileSecurity;
    SIZE_T FileSecuritySize;
    NTSTATUS Result;

#if defined(MEMFS_NAMED_STREAMS)
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
#endif

    Result = FspSetSecurityDescriptor(
        FileNode->FileSecurity,
        SecurityInformation,
        ModificationDescriptor,
        &NewSecurityDescriptor);
    if (!NT_SUCCESS(Result))
        return Result;

    FileSecuritySize = GetSecurityDescriptorLength(NewSecurityDescriptor);
    FileSecurity = (PSECURITY_DESCRIPTOR)malloc(FileSecuritySize);
    if (0 == FileSecurity)
    {
        FspDeleteSecurityDescriptor(NewSecurityDescriptor, (NTSTATUS (*)())FspSetSecurityDescriptor);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    memcpy(FileSecurity, NewSecurityDescriptor, FileSecuritySize);
    FspDeleteSecurityDescriptor(NewSecurityDescriptor, (NTSTATUS (*)())FspSetSecurityDescriptor);

    free(FileNode->FileSecurity);
    FileNode->FileSecuritySize = FileSecuritySize;
    FileNode->FileSecurity = FileSecurity;

    return STATUS_SUCCESS;
}

typedef struct _MEMFS_READ_DIRECTORY_CONTEXT
{
    PVOID Buffer;
    ULONG Length;
    PULONG PBytesTransferred;
} MEMFS_READ_DIRECTORY_CONTEXT;

static BOOLEAN AddDirInfo(MEMFS_FILE_NODE *FileNode, PWSTR FileName,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    UINT8 DirInfoBuf[sizeof(FSP_FSCTL_DIR_INFO) + sizeof FileNode->FileName];
    FSP_FSCTL_DIR_INFO *DirInfo = (FSP_FSCTL_DIR_INFO *)DirInfoBuf;
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;

    if (0 == FileName)
    {
        FspPathSuffix(FileNode->FileName, &Remain, &Suffix, Root);
        FileName = Suffix;
        FspPathCombine(FileNode->FileName, Suffix);
    }

    memset(DirInfo->Padding, 0, sizeof DirInfo->Padding);
    DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + wcslen(FileName) * sizeof(WCHAR));
    DirInfo->FileInfo = FileNode->FileInfo;
    memcpy(DirInfo->FileNameBuf, FileName, DirInfo->Size - sizeof(FSP_FSCTL_DIR_INFO));

    return FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred);
}

static BOOLEAN ReadDirectoryEnumFn(MEMFS_FILE_NODE *FileNode, PVOID Context0)
{
    MEMFS_READ_DIRECTORY_CONTEXT *Context = (MEMFS_READ_DIRECTORY_CONTEXT *)Context0;

    return AddDirInfo(FileNode, 0,
        Context->Buffer, Context->Length, Context->PBytesTransferred);
}

static NTSTATUS ReadDirectory(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0, PWSTR Pattern, PWSTR Marker,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    assert(0 == Pattern);

    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    MEMFS_FILE_NODE *ParentNode;
    MEMFS_READ_DIRECTORY_CONTEXT Context;
    NTSTATUS Result;

    Context.Buffer = Buffer;
    Context.Length = Length;
    Context.PBytesTransferred = PBytesTransferred;

    if (L'\0' != FileNode->FileName[1])
    {
        /* if this is not the root directory add the dot entries */

        ParentNode = MemfsFileNodeMapGetParent(Memfs->FileNodeMap, FileNode->FileName, &Result);
        if (0 == ParentNode)
            return Result;

        if (0 == Marker)
        {
            if (!AddDirInfo(FileNode, L".", Buffer, Length, PBytesTransferred))
                return STATUS_SUCCESS;
        }
        if (0 == Marker || (L'.' == Marker[0] && L'\0' == Marker[1]))
        {
            if (!AddDirInfo(ParentNode, L"..", Buffer, Length, PBytesTransferred))
                return STATUS_SUCCESS;
            Marker = 0;
        }
    }

    if (MemfsFileNodeMapEnumerateChildren(Memfs->FileNodeMap, FileNode, Marker,
        ReadDirectoryEnumFn, &Context))
        FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);

#ifdef MEMFS_SLOWIO
    if (SlowioReturnPending(FileSystem))
    {
        try
        {
            InterlockedIncrement(&Memfs->SlowioThreadsRunning);
            std::thread(SlowioReadDirectoryThread,
                FileSystem, *PBytesTransferred,
                FspFileSystemGetOperationContext()->Request->Hint).
                detach();
            return STATUS_PENDING;
        }
        catch (...)
        {
            InterlockedDecrement(&Memfs->SlowioThreadsRunning);
        }
    }
    SlowioSnooze(FileSystem);
#endif

    return STATUS_SUCCESS;
}

#if defined(MEMFS_DIRINFO_BY_NAME)
static NTSTATUS GetDirInfoByName(FSP_FILE_SYSTEM *FileSystem,
    PVOID ParentNode0, PWSTR FileName,
    FSP_FSCTL_DIR_INFO *DirInfo)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *ParentNode = (MEMFS_FILE_NODE *)ParentNode0;
    MEMFS_FILE_NODE *FileNode;
    WCHAR FileNameBuf[MEMFS_MAX_PATH];
    size_t ParentLength, BSlashLength, FileNameLength;
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;

    ParentLength = wcslen(ParentNode->FileName);
    BSlashLength = 1 < ParentLength;
    FileNameLength = wcslen(FileName);
    if (MEMFS_MAX_PATH <= ParentLength + BSlashLength + FileNameLength)
        return STATUS_OBJECT_NAME_NOT_FOUND; //STATUS_OBJECT_NAME_INVALID?

    memcpy(FileNameBuf, ParentNode->FileName, ParentLength * sizeof(WCHAR));
    memcpy(FileNameBuf + ParentLength, L"\\", BSlashLength * sizeof(WCHAR));
    memcpy(FileNameBuf + ParentLength + BSlashLength, FileName, (FileNameLength + 1) * sizeof(WCHAR));

    FileName = FileNameBuf;

    FileNode = MemfsFileNodeMapGet(Memfs->FileNodeMap, FileName);
    if (0 == FileNode)
        return STATUS_OBJECT_NAME_NOT_FOUND;

    FspPathSuffix(FileNode->FileName, &Remain, &Suffix, Root);
    FileName = Suffix;
    FspPathCombine(FileNode->FileName, Suffix);

    //memset(DirInfo->Padding, 0, sizeof DirInfo->Padding);
    DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + wcslen(FileName) * sizeof(WCHAR));
    DirInfo->FileInfo = FileNode->FileInfo;
    memcpy(DirInfo->FileNameBuf, FileName, DirInfo->Size - sizeof(FSP_FSCTL_DIR_INFO));

    return STATUS_SUCCESS;
}
#endif

#if defined(MEMFS_REPARSE_POINTS)
static NTSTATUS ResolveReparsePoints(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, UINT32 ReparsePointIndex, BOOLEAN ResolveLastPathComponent,
    PIO_STATUS_BLOCK PIoStatus, PVOID Buffer, PSIZE_T PSize)
{
    return FspFileSystemResolveReparsePoints(FileSystem, GetReparsePointByName, 0,
        FileName, ReparsePointIndex, ResolveLastPathComponent,
        PIoStatus, Buffer, PSize);
}

static NTSTATUS GetReparsePointByName(
    FSP_FILE_SYSTEM *FileSystem, PVOID Context,
    PWSTR FileName, BOOLEAN IsDirectory, PVOID Buffer, PSIZE_T PSize)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode;

#if defined(MEMFS_NAMED_STREAMS)
    /* GetReparsePointByName will never receive a named stream */
    assert(0 == wcschr(FileName, L':'));
#endif

    FileNode = MemfsFileNodeMapGet(Memfs->FileNodeMap, FileName);
    if (0 == FileNode)
        return STATUS_OBJECT_NAME_NOT_FOUND;

    if (0 == (FileNode->FileInfo.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return STATUS_NOT_A_REPARSE_POINT;

    if (0 != Buffer)
    {
        if (FileNode->ReparseDataSize > *PSize)
            return STATUS_BUFFER_TOO_SMALL;

        *PSize = FileNode->ReparseDataSize;
        memcpy(Buffer, FileNode->ReparseData, FileNode->ReparseDataSize);
    }

    return STATUS_SUCCESS;
}

static NTSTATUS GetReparsePoint(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0,
    PWSTR FileName, PVOID Buffer, PSIZE_T PSize)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

#if defined(MEMFS_NAMED_STREAMS)
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
#endif

    if (0 == (FileNode->FileInfo.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return STATUS_NOT_A_REPARSE_POINT;

    if (FileNode->ReparseDataSize > *PSize)
        return STATUS_BUFFER_TOO_SMALL;

    *PSize = FileNode->ReparseDataSize;
    memcpy(Buffer, FileNode->ReparseData, FileNode->ReparseDataSize);

    return STATUS_SUCCESS;
}

static NTSTATUS SetReparsePoint(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0,
    PWSTR FileName, PVOID Buffer, SIZE_T Size)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    PVOID ReparseData;
    NTSTATUS Result;

#if defined(MEMFS_NAMED_STREAMS)
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
#endif

    if (MemfsFileNodeMapHasChild(Memfs->FileNodeMap, FileNode))
        return STATUS_DIRECTORY_NOT_EMPTY;

    if (0 != FileNode->ReparseData)
    {
        Result = FspFileSystemCanReplaceReparsePoint(
            FileNode->ReparseData, FileNode->ReparseDataSize,
            Buffer, Size);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    ReparseData = realloc(FileNode->ReparseData, Size);
    if (0 == ReparseData && 0 != Size)
        return STATUS_INSUFFICIENT_RESOURCES;

    FileNode->FileInfo.FileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
    FileNode->FileInfo.ReparseTag = *(PULONG)Buffer;
        /* the first field in a reparse buffer is the reparse tag */
    FileNode->ReparseDataSize = Size;
    FileNode->ReparseData = ReparseData;
    memcpy(FileNode->ReparseData, Buffer, Size);

    return STATUS_SUCCESS;
}

static NTSTATUS DeleteReparsePoint(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0,
    PWSTR FileName, PVOID Buffer, SIZE_T Size)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    NTSTATUS Result;

#if defined(MEMFS_NAMED_STREAMS)
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
#endif

    if (0 != FileNode->ReparseData)
    {
        Result = FspFileSystemCanReplaceReparsePoint(
            FileNode->ReparseData, FileNode->ReparseDataSize,
            Buffer, Size);
        if (!NT_SUCCESS(Result))
            return Result;
    }
    else
        return STATUS_NOT_A_REPARSE_POINT;

    free(FileNode->ReparseData);

    FileNode->FileInfo.FileAttributes &= ~FILE_ATTRIBUTE_REPARSE_POINT;
    FileNode->FileInfo.ReparseTag = 0;
    FileNode->ReparseDataSize = 0;
    FileNode->ReparseData = 0;

    return STATUS_SUCCESS;
}
#endif

#if defined(MEMFS_NAMED_STREAMS)
typedef struct _MEMFS_GET_STREAM_INFO_CONTEXT
{
    PVOID Buffer;
    ULONG Length;
    PULONG PBytesTransferred;
} MEMFS_GET_STREAM_INFO_CONTEXT;

static BOOLEAN AddStreamInfo(MEMFS_FILE_NODE *FileNode,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    UINT8 StreamInfoBuf[sizeof(FSP_FSCTL_STREAM_INFO) + sizeof FileNode->FileName];
    FSP_FSCTL_STREAM_INFO *StreamInfo = (FSP_FSCTL_STREAM_INFO *)StreamInfoBuf;
    PWSTR StreamName;

    StreamName = wcschr(FileNode->FileName, L':');
    if (0 != StreamName)
        StreamName++;
    else
        StreamName = L"";

    StreamInfo->Size = (UINT16)(sizeof(FSP_FSCTL_STREAM_INFO) + wcslen(StreamName) * sizeof(WCHAR));
    StreamInfo->StreamSize = FileNode->FileInfo.FileSize;
    StreamInfo->StreamAllocationSize = FileNode->FileInfo.AllocationSize;
    memcpy(StreamInfo->StreamNameBuf, StreamName, StreamInfo->Size - sizeof(FSP_FSCTL_STREAM_INFO));

    return FspFileSystemAddStreamInfo(StreamInfo, Buffer, Length, PBytesTransferred);
}

static BOOLEAN GetStreamInfoEnumFn(MEMFS_FILE_NODE *FileNode, PVOID Context0)
{
    MEMFS_GET_STREAM_INFO_CONTEXT *Context = (MEMFS_GET_STREAM_INFO_CONTEXT *)Context0;

    return AddStreamInfo(FileNode,
        Context->Buffer, Context->Length, Context->PBytesTransferred);
}

static NTSTATUS GetStreamInfo(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0, PVOID Buffer, ULONG Length,
    PULONG PBytesTransferred)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    MEMFS_GET_STREAM_INFO_CONTEXT Context;

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    Context.Buffer = Buffer;
    Context.Length = Length;
    Context.PBytesTransferred = PBytesTransferred;

    if (0 == (FileNode->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        !AddStreamInfo(FileNode, Buffer, Length, PBytesTransferred))
        return STATUS_SUCCESS;

    if (MemfsFileNodeMapEnumerateNamedStreams(Memfs->FileNodeMap, FileNode, GetStreamInfoEnumFn, &Context))
        FspFileSystemAddStreamInfo(0, Buffer, Length, PBytesTransferred);

    /* ???: how to handle out of response buffer condition? */

    return STATUS_SUCCESS;
}
#endif

#if defined(MEMFS_CONTROL)
static NTSTATUS Control(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode, UINT32 ControlCode,
    PVOID InputBuffer, ULONG InputBufferLength,
    PVOID OutputBuffer, ULONG OutputBufferLength, PULONG PBytesTransferred)
{
    /* MEMFS also supports encryption! See below :) */
    if (CTL_CODE(0x8000 + 'M', 'R', METHOD_BUFFERED, FILE_ANY_ACCESS) == ControlCode)
    {
        if (OutputBufferLength != InputBufferLength)
            return STATUS_INVALID_PARAMETER;

        for (PUINT8 P = (PUINT8)InputBuffer, Q = (PUINT8)OutputBuffer, EndP = P + InputBufferLength;
            EndP > P; P++, Q++)
        {
            if (('A' <= *P && *P <= 'M') || ('a' <= *P && *P <= 'm'))
                *Q = *P + 13;
            else
            if (('N' <= *P && *P <= 'Z') || ('n' <= *P && *P <= 'z'))
                *Q = *P - 13;
            else
                *Q = *P;
        }

        *PBytesTransferred = InputBufferLength;
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_DEVICE_REQUEST;
}
#endif

#if defined(MEMFS_EA)
typedef struct _MEMFS_GET_EA_CONTEXT
{
    PFILE_FULL_EA_INFORMATION Ea;
    ULONG EaLength;
    PULONG PBytesTransferred;
} MEMFS_GET_EA_CONTEXT;

static BOOLEAN GetEaEnumFn(PFILE_FULL_EA_INFORMATION Ea, PVOID Context0)
{
    MEMFS_GET_EA_CONTEXT *Context = (MEMFS_GET_EA_CONTEXT *)Context0;

    return FspFileSystemAddEa(Ea,
        Context->Ea, Context->EaLength, Context->PBytesTransferred);
}

static NTSTATUS GetEa(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0,
    PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength, PULONG PBytesTransferred)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    MEMFS_GET_EA_CONTEXT Context;

    Context.Ea = Ea;
    Context.EaLength = EaLength;
    Context.PBytesTransferred = PBytesTransferred;

    if (MemfsFileNodeEnumerateEa(FileNode, GetEaEnumFn, &Context))
        FspFileSystemAddEa(0, Ea, EaLength, PBytesTransferred);

    return STATUS_SUCCESS;
}

static NTSTATUS SetEa(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileNode0,
    PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    NTSTATUS Result;

    Result = FspFileSystemEnumerateEa(FileSystem, MemfsFileNodeSetEa, FileNode, Ea, EaLength);
    if (!NT_SUCCESS(Result))
        return Result;

    MemfsFileNodeGetFileInfo(FileNode, FileInfo);

    return STATUS_SUCCESS;
}
#endif

static FSP_FILE_SYSTEM_INTERFACE MemfsInterface =
{
    GetVolumeInfo,
    SetVolumeLabel,
    GetSecurityByName,
#if defined(MEMFS_EA) || defined(MEMFS_WSL)
    0,
#else
    Create,
#endif
    Open,
#if defined(MEMFS_EA)
    0,
#else
    Overwrite,
#endif
    Cleanup,
    Close,
    Read,
    Write,
    Flush,
    GetFileInfo,
    SetBasicInfo,
    SetFileSize,
    CanDelete,
    Rename,
    GetSecurity,
    SetSecurity,
    ReadDirectory,
#if defined(MEMFS_REPARSE_POINTS)
    ResolveReparsePoints,
    GetReparsePoint,
    SetReparsePoint,
    DeleteReparsePoint,
#else
    0,
    0,
    0,
    0,
#endif
#if defined(MEMFS_NAMED_STREAMS)
    GetStreamInfo,
#else
    0,
#endif
#if defined(MEMFS_DIRINFO_BY_NAME)
    GetDirInfoByName,
#else
    0,
#endif
#if defined(MEMFS_CONTROL)
    Control,
#else
    0,
#endif
    0,
#if defined(MEMFS_EA) || defined(MEMFS_WSL)
    Create,
#endif
#if defined(MEMFS_EA)
    Overwrite,
    GetEa,
    SetEa,
#else
    0,
    0,
    0,
#endif
};

/*
 * Public API
 */

NTSTATUS MemfsCreateFunnel(
    ULONG Flags,
    ULONG FileInfoTimeout,
    ULONG MaxFileNodes,
    ULONG MaxFileSize,
    ULONG SlowioMaxDelay,
    ULONG SlowioPercentDelay,
    ULONG SlowioRarefyDelay,
    PWSTR FileSystemName,
    PWSTR VolumePrefix,
    PWSTR RootSddl,
    MEMFS **PMemfs)
{
    NTSTATUS Result;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    BOOLEAN CaseInsensitive = !!(Flags & MemfsCaseInsensitive);
    BOOLEAN FlushAndPurgeOnCleanup = !!(Flags & MemfsFlushAndPurgeOnCleanup);
    BOOLEAN SupportsPosixUnlinkRename = !(Flags & MemfsLegacyUnlinkRename);
    PWSTR DevicePath = MemfsNet == (Flags & MemfsDeviceMask) ?
        L"" FSP_FSCTL_NET_DEVICE_NAME : L"" FSP_FSCTL_DISK_DEVICE_NAME;
    UINT64 AllocationUnit;
    MEMFS *Memfs;
    MEMFS_FILE_NODE *RootNode;
    PSECURITY_DESCRIPTOR RootSecurity;
    ULONG RootSecuritySize;
    BOOLEAN Inserted;

    *PMemfs = 0;

    Result = MemfsHeapConfigure(0, 0, 0);
    if (!NT_SUCCESS(Result))
        return Result;

    if (0 == RootSddl)
        RootSddl = L"O:BAG:BAD:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;WD)";
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(RootSddl, SDDL_REVISION_1,
        &RootSecurity, &RootSecuritySize))
        return FspNtStatusFromWin32(GetLastError());

    Memfs = (MEMFS *)malloc(sizeof *Memfs);
    if (0 == Memfs)
    {
        LocalFree(RootSecurity);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    memset(Memfs, 0, sizeof *Memfs);
    Memfs->MaxFileNodes = MaxFileNodes;
    AllocationUnit = MEMFS_SECTOR_SIZE * MEMFS_SECTORS_PER_ALLOCATION_UNIT;
    Memfs->MaxFileSize = (ULONG)((MaxFileSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit);

#ifdef MEMFS_SLOWIO
    Memfs->SlowioMaxDelay = SlowioMaxDelay;
    Memfs->SlowioPercentDelay = SlowioPercentDelay;
    Memfs->SlowioRarefyDelay = SlowioRarefyDelay;
#endif

    Result = MemfsFileNodeMapCreate(CaseInsensitive, &Memfs->FileNodeMap);
    if (!NT_SUCCESS(Result))
    {
        free(Memfs);
        LocalFree(RootSecurity);
        return Result;
    }

    memset(&VolumeParams, 0, sizeof VolumeParams);
    VolumeParams.Version = sizeof FSP_FSCTL_VOLUME_PARAMS;
    VolumeParams.SectorSize = MEMFS_SECTOR_SIZE;
    VolumeParams.SectorsPerAllocationUnit = MEMFS_SECTORS_PER_ALLOCATION_UNIT;
    VolumeParams.VolumeCreationTime = MemfsGetSystemTime();
    VolumeParams.VolumeSerialNumber = (UINT32)(MemfsGetSystemTime() / (10000 * 1000));
    VolumeParams.FileInfoTimeout = FileInfoTimeout;
    VolumeParams.CaseSensitiveSearch = !CaseInsensitive;
    VolumeParams.CasePreservedNames = 1;
    VolumeParams.UnicodeOnDisk = 1;
    VolumeParams.PersistentAcls = 1;
    VolumeParams.ReparsePoints = 1;
    VolumeParams.ReparsePointsAccessCheck = 0;
#if defined(MEMFS_NAMED_STREAMS)
    VolumeParams.NamedStreams = 1;
#endif
    VolumeParams.PostCleanupWhenModifiedOnly = 1;
#if defined(MEMFS_DIRINFO_BY_NAME)
    VolumeParams.PassQueryDirectoryFileName = 1;
#endif
    VolumeParams.FlushAndPurgeOnCleanup = FlushAndPurgeOnCleanup;
#if defined(MEMFS_CONTROL)
    VolumeParams.DeviceControl = 1;
#endif
#if defined(MEMFS_EA)
    VolumeParams.ExtendedAttributes = 1;
#endif
#if defined(MEMFS_WSL)
    VolumeParams.WslFeatures = 1;
#endif
    VolumeParams.AllowOpenInKernelMode = 1;
#if defined(MEMFS_REJECT_EARLY_IRP)
    VolumeParams.RejectIrpPriorToTransact0 = 1;
#endif
    VolumeParams.SupportsPosixUnlinkRename = SupportsPosixUnlinkRename;
    if (0 != VolumePrefix)
        wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR), VolumePrefix);
    wcscpy_s(VolumeParams.FileSystemName, sizeof VolumeParams.FileSystemName / sizeof(WCHAR),
        0 != FileSystemName ? FileSystemName : L"-MEMFS");

    Result = FspFileSystemCreate(DevicePath, &VolumeParams, &MemfsInterface, &Memfs->FileSystem);
    if (!NT_SUCCESS(Result))
    {
        MemfsFileNodeMapDelete(Memfs->FileNodeMap);
        free(Memfs);
        LocalFree(RootSecurity);
        return Result;
    }

    Memfs->FileSystem->UserContext = Memfs;
    Memfs->VolumeLabelLength = sizeof L"MEMFS" - sizeof(WCHAR);
    memcpy(Memfs->VolumeLabel, L"MEMFS", Memfs->VolumeLabelLength);

#if 0
    FspFileSystemSetOperationGuardStrategy(Memfs->FileSystem,
        FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_COARSE);
#endif

    /*
     * Create root directory.
     */

    Result = MemfsFileNodeCreate(L"\\", &RootNode);
    if (!NT_SUCCESS(Result))
    {
        MemfsDelete(Memfs);
        LocalFree(RootSecurity);
        return Result;
    }

    RootNode->FileInfo.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;

    RootNode->FileSecurity = malloc(RootSecuritySize);
    if (0 == RootNode->FileSecurity)
    {
        MemfsFileNodeDelete(RootNode);
        MemfsDelete(Memfs);
        LocalFree(RootSecurity);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RootNode->FileSecuritySize = RootSecuritySize;
    memcpy(RootNode->FileSecurity, RootSecurity, RootSecuritySize);

    Result = MemfsFileNodeMapInsert(Memfs->FileNodeMap, RootNode, &Inserted);
    if (!NT_SUCCESS(Result))
    {
        MemfsFileNodeDelete(RootNode);
        MemfsDelete(Memfs);
        LocalFree(RootSecurity);
        return Result;
    }

    LocalFree(RootSecurity);

    *PMemfs = Memfs;

    return STATUS_SUCCESS;
}

VOID MemfsDelete(MEMFS *Memfs)
{
    FspFileSystemDelete(Memfs->FileSystem);

    MemfsFileNodeMapDelete(Memfs->FileNodeMap);

    free(Memfs);
}

NTSTATUS MemfsStart(MEMFS *Memfs)
{
#ifdef MEMFS_SLOWIO
    Memfs->SlowioThreadsRunning = 0;
#endif

    return FspFileSystemStartDispatcher(Memfs->FileSystem, 0);
}

VOID MemfsStop(MEMFS *Memfs)
{
    FspFileSystemStopDispatcher(Memfs->FileSystem);

#ifdef MEMFS_SLOWIO
    while (Memfs->SlowioThreadsRunning)
        Sleep(1);
#endif
}

FSP_FILE_SYSTEM *MemfsFileSystem(MEMFS *Memfs)
{
    return Memfs->FileSystem;
}

NTSTATUS MemfsHeapConfigure(SIZE_T InitialSize, SIZE_T MaximumSize, SIZE_T Alignment)
{
    return LargeHeapInitialize(0, InitialSize, MaximumSize, LargeHeapAlignment) ?
        STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
}
