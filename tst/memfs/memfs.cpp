/**
 * @file memfs.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
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

static inline
BOOLEAN MemfsFileNameHasPrefix(PWSTR a, PWSTR b)
{
    return 0 == wcsncmp(a, b, wcslen(b));
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
    UINT16 VolumeLabelLength;
    WCHAR VolumeLabel[32];
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
MEMFS_FILE_NODE *MemfsFileNodeMapGetParent(MEMFS_FILE_NODE_MAP *FileNodeMap, PWSTR FileName0,
    PNTSTATUS PResult)
{
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;
    WCHAR FileName[MAX_PATH];
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

static inline
BOOLEAN MemfsFileNodeMapEnumerateChildren(MEMFS_FILE_NODE_MAP *FileNodeMap, MEMFS_FILE_NODE *FileNode,
    BOOLEAN (*EnumFn)(MEMFS_FILE_NODE *, PVOID), PVOID Context)
{
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;
    MEMFS_FILE_NODE_MAP::iterator iter = FileNodeMap->upper_bound(FileNode->FileName);
    BOOLEAN Equal;
    for (; FileNodeMap->end() != iter; ++iter)
    {
        if (!MemfsFileNameHasPrefix(iter->second->FileName, FileNode->FileName))
            break;
        FspPathSuffix(iter->second->FileName, &Remain, &Suffix, Root);
        Equal = 0 == MemfsFileNameCompare(Remain, FileNode->FileName);
        FspPathCombine(iter->second->FileName, Suffix);
        if (Equal)
        {
            if (!EnumFn(iter->second, Context))
                return FALSE;
        }
    }
    return TRUE;
}

static NTSTATUS SetFileSize(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0, UINT64 FileSize,
    FSP_FSCTL_FILE_INFO *FileInfo);

static NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;

    VolumeInfo->TotalSize = Memfs->MaxFileNodes * Memfs->MaxFileSize;
    VolumeInfo->FreeSize =
        (Memfs->MaxFileNodes - MemfsFileNodeMapCount(Memfs->FileNodeMap)) * Memfs->MaxFileSize;
    VolumeInfo->VolumeLabelLength = Memfs->VolumeLabelLength;
    memcpy(VolumeInfo->VolumeLabel, Memfs->VolumeLabel, Memfs->VolumeLabelLength);

    return STATUS_SUCCESS;
}

static NTSTATUS SetVolumeLabel(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
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

static NTSTATUS Read(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0, PVOID Buffer, UINT64 Offset, ULONG Length,
    PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    UINT64 EndOffset;

    if (Offset >= FileNode->FileInfo.FileSize)
        return STATUS_END_OF_FILE;

    EndOffset = Offset + Length;
    if (EndOffset > FileNode->FileInfo.FileSize)
        EndOffset = FileNode->FileInfo.FileSize;

    memcpy(Buffer, (PUINT8)FileNode->FileData + Offset, EndOffset - Offset);

    *PBytesTransferred = (ULONG)(EndOffset - Offset);
    *FileInfo = FileNode->FileInfo;

    return STATUS_SUCCESS;
}

static NTSTATUS Write(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0, PVOID Buffer, UINT64 Offset, ULONG Length, BOOLEAN WriteToEndOfFile,
    PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    UINT64 EndOffset;

    if (WriteToEndOfFile)
        Offset = FileNode->FileInfo.FileSize;
    EndOffset = Offset + Length;
    if (EndOffset > FileNode->FileInfo.FileSize)
        SetFileSize(FileSystem, Request, FileNode, EndOffset, FileInfo);

    memcpy((PUINT8)FileNode->FileData + Offset, Buffer, EndOffset - Offset);

    *PBytesTransferred = (ULONG)(EndOffset - Offset);
    *FileInfo = FileNode->FileInfo;

    return STATUS_SUCCESS;
}

NTSTATUS Flush(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode)
{
    /* nothing to do, since we do not cache anything */
    return STATUS_SUCCESS;
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

static NTSTATUS SetBasicInfo(FSP_FILE_SYSTEM *FileSystem,
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

static NTSTATUS SetAllocationSize(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0, UINT64 AllocationSize,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    PVOID FileData;

    if (FileNode->FileInfo.AllocationSize != AllocationSize)
    {
        if (AllocationSize > Memfs->MaxFileSize)
            return STATUS_DISK_FULL;

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

static NTSTATUS SetFileSize(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0, UINT64 FileSize,
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

        if (FileNode->FileInfo.FileSize < FileSize)
            memset((PUINT8)FileNode->FileData + FileNode->FileInfo.FileSize, 0,
                FileSize - FileNode->FileInfo.FileSize);
        FileNode->FileInfo.FileSize = FileSize;
    }

    *FileInfo = FileNode->FileInfo;

    return STATUS_SUCCESS;
}

static NTSTATUS CanDelete(FSP_FILE_SYSTEM *FileSystem,
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

static NTSTATUS Rename(FSP_FILE_SYSTEM *FileSystem,
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

static NTSTATUS GetSecurity(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;

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
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0,
    SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor)
{
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    PSECURITY_DESCRIPTOR NewSecurityDescriptor, FileSecurity;
    SIZE_T FileSecuritySize;
    NTSTATUS Result;

    Result = FspSetSecurityDescriptor(FileSystem, Request, FileNode->FileSecurity,
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
    UINT64 Offset;
    ULONG Length;
    PULONG PBytesTransferred;
    BOOLEAN OffsetFound;
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
    DirInfo->NextOffset = FileNode->FileInfo.IndexNumber;
    memcpy(DirInfo->FileNameBuf, FileName, DirInfo->Size - sizeof(FSP_FSCTL_DIR_INFO));

    return FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred);
}

static BOOLEAN ReadDirectoryEnumFn(MEMFS_FILE_NODE *FileNode, PVOID Context0)
{
    MEMFS_READ_DIRECTORY_CONTEXT *Context = (MEMFS_READ_DIRECTORY_CONTEXT *)Context0;

    if (0 != Context->Offset && !Context->OffsetFound)
    {
        Context->OffsetFound = FileNode->FileInfo.IndexNumber == Context->Offset;
        return TRUE;
    }

    return AddDirInfo(FileNode, 0,
        Context->Buffer, Context->Length, Context->PBytesTransferred);
}

static NTSTATUS ReadDirectory(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode0, PVOID Buffer, UINT64 Offset, ULONG Length,
    PULONG PBytesTransferred)
{
    MEMFS *Memfs = (MEMFS *)FileSystem->UserContext;
    MEMFS_FILE_NODE *FileNode = (MEMFS_FILE_NODE *)FileNode0;
    MEMFS_FILE_NODE *ParentNode;
    MEMFS_READ_DIRECTORY_CONTEXT Context;
    NTSTATUS Result;

    ParentNode = MemfsFileNodeMapGetParent(Memfs->FileNodeMap, FileNode->FileName, &Result);
    if (0 == ParentNode)
        return Result;

    Context.Buffer = Buffer;
    Context.Offset = Offset;
    Context.Length = Length;
    Context.PBytesTransferred = PBytesTransferred;
    Context.OffsetFound = FALSE;

    if (0 == Offset)
        if (!AddDirInfo(FileNode, L".", Buffer, Length, PBytesTransferred))
            return STATUS_SUCCESS;
    if (0 == Offset || FileNode->FileInfo.IndexNumber == Offset)
    {
        Context.OffsetFound = FileNode->FileInfo.IndexNumber == Context.Offset;

        if (!AddDirInfo(ParentNode, L"..", Buffer, Length, PBytesTransferred))
            return STATUS_SUCCESS;
    }

    if (MemfsFileNodeMapEnumerateChildren(Memfs->FileNodeMap, FileNode, ReadDirectoryEnumFn, &Context))
        FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);

    return STATUS_SUCCESS;
}

static FSP_FILE_SYSTEM_INTERFACE MemfsInterface =
{
    GetVolumeInfo,
    SetVolumeLabel,
    GetSecurityByName,
    Create,
    Open,
    Overwrite,
    Cleanup,
    Close,
    Read,
    Write,
    Flush,
    GetFileInfo,
    SetBasicInfo,
    SetAllocationSize,
    SetFileSize,
    CanDelete,
    Rename,
    GetSecurity,
    SetSecurity,
    ReadDirectory,
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
    PWSTR VolumePrefix,
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
    VolumeParams.VolumeCreationTime = MemfsGetSystemTime();
    VolumeParams.VolumeSerialNumber = (UINT32)(MemfsGetSystemTime() / (10000 * 1000));
    VolumeParams.FileInfoTimeout = FileInfoTimeout;
    VolumeParams.CaseSensitiveSearch = 1;
    VolumeParams.CasePreservedNames = 1;
    VolumeParams.UnicodeOnDisk = 1;
    VolumeParams.PersistentAcls = 1;
    if (0 != VolumePrefix)
        wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR), VolumePrefix);

    Result = FspFileSystemCreate(DevicePath, &VolumeParams, &MemfsInterface, &Memfs->FileSystem);
    if (!NT_SUCCESS(Result))
    {
        MemfsFileNodeMapDelete(Memfs->FileNodeMap);
        free(Memfs);
        return Result;
    }

    Memfs->FileSystem->UserContext = Memfs;
    Memfs->VolumeLabelLength = sizeof L"MEMFS" - sizeof(WCHAR);
    memcpy(Memfs->VolumeLabel, L"MEMFS", Memfs->VolumeLabelLength);

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
