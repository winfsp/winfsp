/**
 * @file memfs.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include "memfs.h"

static NTSTATUS GetSecurity(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, PDWORD PFileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS Create(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    FSP_FILE_NODE_INFO *Info)
{
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS Open(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    FSP_FILE_NODE_INFO *Info)
{
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS Overwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, DWORD FileAttributes, BOOLEAN ReplaceFileAttributes,
    FSP_FILE_SIZE_INFO *Info)
{
    return STATUS_NOT_IMPLEMENTED;
}

static VOID Cleanup(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, BOOLEAN Delete)
{
}

static VOID Close(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode)
{
}

static FSP_FILE_SYSTEM_INTERFACE MemfsInterface =
{
    .GetSecurity = GetSecurity,
    .Create = Create,
    .Open = Open,
    .Overwrite = Overwrite,
    .Cleanup = Cleanup,
    .Close = Close,
};

NTSTATUS MemfsCreate(PWSTR DevicePath, ULONG MaxFileNodes, ULONG MaxFileSize,
    MEMFS **PMemfs)
{
    NTSTATUS Result;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    MEMFS *Memfs;

    *PMemfs = 0;

    Memfs = malloc(sizeof *Memfs);
    if (0 == Memfs)
        return STATUS_INSUFFICIENT_RESOURCES;

    memset(Memfs, 0, sizeof *Memfs);
    Memfs->MaxFileNodes = MaxFileNodes;
    Memfs->MaxFileSize = MaxFileSize;

    memset(&VolumeParams, 0, sizeof VolumeParams);
    VolumeParams.FileNameRequired = TRUE;
    wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR), L"\\memfs\\share");

    Result = FspFileSystemCreate(DevicePath, &VolumeParams, &MemfsInterface, &Memfs->FileSystem);
    if (!NT_SUCCESS(Result))
    {
        free(Memfs);
        return Result;
    }

    *PMemfs = Memfs;

    return STATUS_SUCCESS;
}

VOID MemfsDelete(MEMFS *Memfs)
{
    FspFileSystemDelete(Memfs->FileSystem);
    free(Memfs);
}
