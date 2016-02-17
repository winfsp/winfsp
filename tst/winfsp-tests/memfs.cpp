/**
 * @file memfs.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#undef _DEBUG
#include "memfs.h"
#include <map>
#include <cassert>

#define MEMFS_SECTOR_SIZE               512
#define MEMFS_SECTORS_PER_ALLOCATION_UNIT 1

static inline
UINT64 MemfsGetSystemTime(VOID)
{
    FILETIME FileTime;
    GetSystemTimeAsFileTime(&FileTime);
    return ((PLARGE_INTEGER)&FileTime)->QuadPart;
}

static inline
int MemfsFileNameCompare(PWSTR a, PWSTR b)
{
    return wcscmp(a, b);
}

typedef struct _MEMFS_FILE_NODE
{
    WCHAR FileName[MAX_PATH];
    FSP_FSCTL_FILE_INFO FileInfo;
    SIZE_T FileSecuritySize;
    PVOID FileSecurity;
    PVOID FileData;
    ULONG RefCount;
} MEMFS_FILE_NODE;

struct MEMFS_FILE_NODE_LESS
{
    bool operator()(PWSTR a, PWSTR b) const
    {
        return 0 > MemfsFileNameCompare(a, b);
    }
};
typedef std::map<PWSTR, MEMFS_FILE_NODE *, MEMFS_FILE_NODE_LESS> MEMFS_FILE_NODE_MAP;

typedef struct _MEMFS
{
    FSP_FILE_SYSTEM *FileSystem;
    MEMFS_FILE_NODE_MAP *FileNodeMap;
    ULONG MaxFileNodes;
    ULONG MaxFileSize;
    CRITICAL_SECTION Lock;
} MEMFS;

static inline
NTSTATUS MemfsFileNodeCreate(PWSTR FileName, MEMFS_FILE_NODE **PFileNode)
{
    static UINT64 IndexNumber = 1;
    MEMFS_FILE_NODE *FileNode;

    *PFileNode = 0;

    if (MAX_PATH <= wcslen(FileName))
        return STATUS_OBJECT_NAME_INVALID;

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

static inline
VOID MemfsFileNodeDelete(MEMFS_FILE_NODE *FileNode)
{
    free(FileNode->FileData);
    free(FileNode->FileSecurity);
    free(FileNode);
}

static inline
NTSTATUS MemfsFileNodeMapCreate(MEMFS_FILE_NODE_MAP **PFileNodeMap)
{
    *PFileNodeMap = 0;
    try
    {
        *PFileNodeMap = new MEMFS_FILE_NODE_MAP;
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

static inline
MEMFS_FILE_NODE *MemfsFileNodeMapGetParent(MEMFS_FILE_NODE_MAP *FileNodeMap, PWSTR FileName,
    PNTSTATUS PResult)
{
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;
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
NTSTATUS MemfsFileNodeMapInsert(MEMFS_FILE_NODE_MAP *FileNodeMap, MEMFS_FILE_NODE *FileNode,
    PBOOLEAN PInserted)
{
    *PInserted = 0;
    try
    {
        *PInserted = FileNodeMap->insert(MEMFS_FILE_NODE_MAP::value_type(FileNode->FileName, FileNode)).second;
        if (*PInserted)
            FileNode->RefCount++;
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
    --FileNode->RefCount;
    FileNodeMap->erase(FileNode->FileName);
}

static inline
BOOLEAN MemfsFileNodeMapHasChild(MEMFS_FILE_NODE_MAP *FileNodeMap, MEMFS_FILE_NODE *FileNode)
{
    BOOLEAN Result;
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;
    MEMFS_FILE_NODE_MAP::iterator iter = FileNodeMap->upper_bound(FileNode->FileName);
    if (iter == FileNodeMap->end())
        return FALSE;
    FspPathSuffix(iter->second->FileName, &Remain, &Suffix, Root);
    Result = 0 == MemfsFileNameCompare(Remain, FileNode->FileName);
    FspPathCombine(iter->second->FileName, Suffix);
    return Result;
}

static NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *RootNode;

    RootNode = MemfsFileNodeMapGet(Memfs->FileNodeMap, L"\\");
    if (0 == RootNode)
        return STATUS_DISK_CORRUPT_ERROR;

    VolumeInfo->TotalSize = Memfs->MaxFileNodes * Memfs->MaxFileSize;
    VolumeInfo->FreeSize =
        (Memfs->MaxFileNodes - MemfsFileNodeMapCount(Memfs->FileNodeMap)) * Memfs->MaxFileSize;
    VolumeInfo->VolumeCreationTime = RootNode->FileInfo.CreationTime;
    VolumeInfo->VolumeLabelLength = sizeof L"MEMFS" - sizeof(WCHAR);
    memcpy(VolumeInfo->VolumeLabel, L"MEMFS", VolumeInfo->VolumeLabelLength);

    return STATUS_SUCCESS;
}

static NTSTATUS GetSecurity(FSP_FILE_SYSTEM *FileSystem,
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
        MemfsFileNodeMapGetParent(Memfs->FileNodeMap, FileName, &Result);
        return Result;
    }

    if (0 != PFileAttributes)
        *PFileAttributes = FileNode->FileInfo.FileAttributes;

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
    FSP_FSCTL_TRANSACT_REQ *Request,
    PWSTR FileName, BOOLEAN CaseSensitive, UINT32 CreateOptions,
    UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
    PVOID *PFileNode, FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode;
    NTSTATUS Result;
    BOOLEAN Inserted;

    if (CreateOptions & FILE_DIRECTORY_FILE)
        AllocationSize = 0;

    FileNode = MemfsFileNodeMapGet(Memfs->FileNodeMap, FileName);
    if (0 != FileNode)
        return STATUS_OBJECT_NAME_COLLISION;

    if (!MemfsFileNodeMapGetParent(Memfs->FileNodeMap, FileName, &Result))
        return Result;

    if (MemfsFileNodeMapCount(Memfs->FileNodeMap) >= Memfs->MaxFileNodes)
        return STATUS_CANNOT_MAKE;

    if (AllocationSize > Memfs->MaxFileSize)
        return STATUS_DISK_FULL;

    Result = MemfsFileNodeCreate(FileName, &FileNode);
    if (!NT_SUCCESS(Result))
        return Result;

    FileNode->FileInfo.FileAttributes = FileAttributes;

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

    FileNode->FileInfo.AllocationSize = AllocationSize;
    if (0 != FileNode->FileInfo.AllocationSize)
    {
        FileNode->FileData = malloc(FileNode->FileInfo.AllocationSize);
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

    FileNode->RefCount++;
    *PFileNode = FileNode;
    *FileInfo = FileNode->FileInfo;

    return STATUS_SUCCESS;
}

static NTSTATUS Open(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PWSTR FileName, BOOLEAN CaseSensitive, UINT32 CreateOptions,
    PVOID *PFileNode, FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode;
    NTSTATUS Result;

    FileNode = MemfsFileNodeMapGet(Memfs->FileNodeMap, FileName);
    if (0 == FileNode)
    {
        Result = STATUS_OBJECT_NAME_NOT_FOUND;
        MemfsFileNodeMapGetParent(Memfs->FileNodeMap, FileName, &Result);
        return Result;
    }

    FileNode->FileInfo.LastAccessTime = MemfsGetSystemTime();

    FileNode->RefCount++;
    *PFileNode = FileNode;
    *FileInfo = FileNode->FileInfo;

    return STATUS_SUCCESS;
}

NTSTATUS Overwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

    if (ReplaceFileAttributes)
        FileNode->FileInfo.FileAttributes = FileAttributes;
    else
        FileNode->FileInfo.FileAttributes |= FileAttributes;

    FileNode->FileInfo.FileSize = 0;
    FileNode->FileInfo.LastWriteTime =
    FileNode->FileInfo.LastAccessTime = MemfsGetSystemTime();

    *FileInfo = FileNode->FileInfo;

    return STATUS_SUCCESS;
}

static VOID Cleanup(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0, PWSTR FileName, BOOLEAN Delete)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

    assert(0 == FileName || 0 == wcscmp(FileNode->FileName, FileName));

    if (Delete && !MemfsFileNodeMapHasChild(Memfs->FileNodeMap, FileNode))
        MemfsFileNodeMapRemove(Memfs->FileNodeMap, FileNode);
}

static VOID Close(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

    if (0 == --FileNode->RefCount)
        MemfsFileNodeDelete(FileNode);
}

static NTSTATUS GetFileInfo(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

    *FileInfo = FileNode->FileInfo;

    return STATUS_SUCCESS;
}

NTSTATUS SetBasicInfo(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0, UINT32 FileAttributes,
    UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

    if (INVALID_FILE_ATTRIBUTES != FileAttributes)
        FileNode->FileInfo.FileAttributes = FileAttributes;
    if (0 != CreationTime)
        FileNode->FileInfo.CreationTime = CreationTime;
    if (0 != LastAccessTime)
        FileNode->FileInfo.LastAccessTime = LastAccessTime;
    if (0 != LastWriteTime)
        FileNode->FileInfo.LastWriteTime = LastWriteTime;

    *FileInfo = FileNode->FileInfo;

    return STATUS_SUCCESS;
}

NTSTATUS SetAllocationSize(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0, UINT64 AllocationSize,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    PVOID FileData;

    if (FileNode->FileInfo.AllocationSize != AllocationSize)
    {
        FileData = realloc(FileNode->FileData, AllocationSize);
        if (0 == FileData)
            return STATUS_INSUFFICIENT_RESOURCES;

        FileNode->FileData = FileData;

        FileNode->FileInfo.AllocationSize = AllocationSize;
        if (FileNode->FileInfo.FileSize > AllocationSize)
            FileNode->FileInfo.FileSize = AllocationSize;
    }

    *FileInfo = FileNode->FileInfo;

    return STATUS_SUCCESS;
}

NTSTATUS SetFileSize(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0, UINT64 FileSize, BOOLEAN AdvanceOnly,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

    if (FileNode->FileInfo.FileSize != FileSize)
    {
        if (FileNode->FileInfo.AllocationSize < FileSize)
        {
            UINT64 AllocationUnit = MEMFS_SECTOR_SIZE * MEMFS_SECTORS_PER_ALLOCATION_UNIT;
            UINT64 AllocationSize = (FileSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit;

            NTSTATUS Result = SetAllocationSize(FileSystem, Request, FileNode, AllocationSize, FileInfo);
            if (!NT_SUCCESS(Result))
                return Result;
        }

        FileNode->FileInfo.FileSize = FileSize;
    }

    *FileInfo = FileNode->FileInfo;

    return STATUS_SUCCESS;
}

NTSTATUS CanDelete(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0, PWSTR FileName)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

    assert(0 == FileName || 0 == wcscmp(FileNode->FileName, FileName));

    if (MemfsFileNodeMapHasChild(Memfs->FileNodeMap, FileNode))
        return STATUS_DIRECTORY_NOT_EMPTY;

    return STATUS_SUCCESS;
}

NTSTATUS Rename(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0,
    PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    MEMFS_FILE_NODE *NewFileNode;
    BOOLEAN Inserted;
    NTSTATUS Result;

    assert(0 == FileName || 0 == wcscmp(FileNode->FileName, FileName));

    if (MAX_PATH <= wcslen(NewFileName))
        return STATUS_OBJECT_NAME_INVALID;

    NewFileNode = MemfsFileNodeMapGet(Memfs->FileNodeMap, NewFileName);
    if (0 != NewFileNode)
    {
        if (!ReplaceIfExists)
            return STATUS_OBJECT_NAME_COLLISION;

        if (NewFileNode->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            return STATUS_ACCESS_DENIED;

        NewFileNode->RefCount++;
        MemfsFileNodeMapRemove(Memfs->FileNodeMap, NewFileNode);
        if (0 == --NewFileNode->RefCount)
            MemfsFileNodeDelete(NewFileNode);
    }

    MemfsFileNodeMapRemove(Memfs->FileNodeMap, FileNode);
    wcscpy_s(FileNode->FileName, sizeof FileNode->FileName / sizeof(WCHAR), NewFileName);
    Result = MemfsFileNodeMapInsert(Memfs->FileNodeMap, FileNode, &Inserted);
    if (!NT_SUCCESS(Result))
    {
        FspDebugLog(__FUNCTION__ ": cannot insert into FileNodeMap; aborting\n");
        abort();
    }

    return STATUS_SUCCESS;
}

static FSP_FILE_SYSTEM_INTERFACE MemfsInterface =
{
    GetVolumeInfo,
    GetSecurity,
    Create,
    Open,
    Overwrite,
    Cleanup,
    Close,
    GetFileInfo,
    SetBasicInfo,
    SetAllocationSize,
    SetFileSize,
    CanDelete,
    Rename,
};

static VOID MemfsEnterOperation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    EnterCriticalSection(&Memfs->Lock);
}

static VOID MemfsLeaveOperation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    LeaveCriticalSection(&Memfs->Lock);
}

NTSTATUS MemfsCreate(ULONG Flags, ULONG FileInfoTimeout,
    ULONG MaxFileNodes, ULONG MaxFileSize,
    MEMFS **PMemfs)
{
    NTSTATUS Result;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    PWSTR DevicePath = (Flags & MemfsNet) ?
        L"" FSP_FSCTL_NET_DEVICE_NAME : L"" FSP_FSCTL_DISK_DEVICE_NAME;
    UINT64 AllocationUnit;
    MEMFS *Memfs;
    MEMFS_FILE_NODE *RootNode;
    BOOLEAN Inserted;

    *PMemfs = 0;

    Memfs = (MEMFS *)malloc(sizeof *Memfs);
    if (0 == Memfs)
        return STATUS_INSUFFICIENT_RESOURCES;

    memset(Memfs, 0, sizeof *Memfs);
    Memfs->MaxFileNodes = MaxFileNodes;
    AllocationUnit = MEMFS_SECTOR_SIZE * MEMFS_SECTORS_PER_ALLOCATION_UNIT;
    Memfs->MaxFileSize = (ULONG)((MaxFileSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit);

    Result = MemfsFileNodeMapCreate(&Memfs->FileNodeMap);
    if (!NT_SUCCESS(Result))
    {
        free(Memfs);
        return Result;
    }

    memset(&VolumeParams, 0, sizeof VolumeParams);
    VolumeParams.SectorSize = MEMFS_SECTOR_SIZE;
    VolumeParams.SectorsPerAllocationUnit = MEMFS_SECTORS_PER_ALLOCATION_UNIT;
    VolumeParams.VolumeSerialNumber = (UINT32)(MemfsGetSystemTime() / (10000 * 1000));
    VolumeParams.FileInfoTimeout = FileInfoTimeout;
    VolumeParams.CaseSensitiveSearch = 1;
    VolumeParams.CasePreservedNames = 1;
    VolumeParams.UnicodeOnDisk = 1;
    VolumeParams.PersistentAcls = 1;
    wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR), L"\\memfs\\share");

    Result = FspFileSystemCreate(DevicePath, &VolumeParams, &MemfsInterface, &Memfs->FileSystem);
    if (!NT_SUCCESS(Result))
    {
        MemfsFileNodeMapDelete(Memfs->FileNodeMap);
        free(Memfs);
        return Result;
    }
    Memfs->FileSystem->UserContext = Memfs;

    InitializeCriticalSection(&Memfs->Lock);

    FspFileSystemSetOperationGuard(Memfs->FileSystem,
        MemfsEnterOperation,
        MemfsLeaveOperation);

    /*
     * Create root directory.
     */

    Result = MemfsFileNodeCreate(L"\\", &RootNode);
    if (!NT_SUCCESS(Result))
    {
        MemfsDelete(Memfs);
        return Result;
    }

    RootNode->FileInfo.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;

    Result = MemfsFileNodeMapInsert(Memfs->FileNodeMap, RootNode, &Inserted);
    if (!NT_SUCCESS(Result))
    {
        MemfsFileNodeDelete(RootNode);
        MemfsDelete(Memfs);
        return Result;
    }

    *PMemfs = Memfs;

    return STATUS_SUCCESS;
}

VOID MemfsDelete(MEMFS *Memfs)
{
    DeleteCriticalSection(&Memfs->Lock);

    FspFileSystemDelete(Memfs->FileSystem);

    MemfsFileNodeMapDelete(Memfs->FileNodeMap);

    free(Memfs);
}

NTSTATUS MemfsStart(MEMFS *Memfs)
{
    return FspFileSystemStartDispatcher(Memfs->FileSystem, 0);
}

VOID MemfsStop(MEMFS *Memfs)
{
    FspFileSystemStopDispatcher(Memfs->FileSystem);
}

FSP_FILE_SYSTEM *MemfsFileSystem(MEMFS *Memfs)
{
    return Memfs->FileSystem;
}
