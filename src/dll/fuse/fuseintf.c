/**
 * @file dll/fuse/fuseintf.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the
 * GNU Affero General Public License version 3 as published by the
 * Free Software Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <dll/fuse/library.h>
#include <fcntl.h>

NTSTATUS fsp_fuse_op_enter(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    struct fuse_context *context;
    struct fsp_fuse_context_header *contexthdr;
    char *PosixPath = 0;
    UINT32 Uid = -1, Gid = -1;
    PWSTR FileName = 0;
    HANDLE Token = 0;
    union
    {
        TOKEN_USER V;
        UINT8 B[128];
    } UserInfoBuf;
    PTOKEN_USER UserInfo = &UserInfoBuf.V;
    union
    {
        TOKEN_PRIMARY_GROUP V;
        UINT8 B[128];
    } GroupInfoBuf;
    PTOKEN_PRIMARY_GROUP GroupInfo = &GroupInfoBuf.V;
    DWORD Size;
    NTSTATUS Result;

    if (FspFsctlTransactCreateKind == Request->Kind)
    {
        FileName = (PWSTR)Request->Buffer;
        Token = (HANDLE)Request->Req.Create.AccessToken;
    }
    else if (FspFsctlTransactSetInformationKind == Request->Kind &&
        10/*FileRenameInformation*/ == Request->Req.SetInformation.FileInformationClass)
    {
        FileName = (PWSTR)(Request->Buffer + Request->Req.SetInformation.Info.Rename.NewFileName.Offset);
        Token = (HANDLE)Request->Req.SetInformation.Info.Rename.AccessToken;
    }
    else if (FspFsctlTransactSetSecurityKind == Request->Kind)
        Token = (HANDLE)Request->Req.SetSecurity.AccessToken;

    if (0 != FileName)
    {
        Result = FspPosixMapWindowsToPosixPath(FileName, &PosixPath);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    if (0 != Token)
    {
        if (!GetTokenInformation(Token, TokenUser, UserInfo, sizeof UserInfoBuf, &Size))
        {
            if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
            {
                Result = FspNtStatusFromWin32(GetLastError());
                goto exit;
            }

            UserInfo = MemAlloc(Size);
            if (0 == UserInfo)
            {
                Result = STATUS_INSUFFICIENT_RESOURCES;
                goto exit;
            }

            if (!GetTokenInformation(Token, TokenUser, UserInfo, Size, &Size))
            {
                Result = FspNtStatusFromWin32(GetLastError());
                goto exit;
            }
        }

        if (!GetTokenInformation(Token, TokenPrimaryGroup, GroupInfo, sizeof GroupInfoBuf, &Size))
        {
            if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
            {
                Result = FspNtStatusFromWin32(GetLastError());
                goto exit;
            }

            GroupInfo = MemAlloc(Size);
            if (0 == UserInfo)
            {
                Result = STATUS_INSUFFICIENT_RESOURCES;
                goto exit;
            }

            if (!GetTokenInformation(Token, TokenPrimaryGroup, GroupInfo, Size, &Size))
            {
                Result = FspNtStatusFromWin32(GetLastError());
                goto exit;
            }
        }

        Result = FspPosixMapSidToUid(UserInfo->User.Sid, &Uid);
        if (!NT_SUCCESS(Result))
            goto exit;

        Result = FspPosixMapSidToUid(GroupInfo->PrimaryGroup, &Gid);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    context = fsp_fuse_get_context(0);
    if (0 == context)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    Result = FspFileSystemOpEnter(FileSystem, Request, Response);
    if (!NT_SUCCESS(Result))
        goto exit;

    context->fuse = FileSystem->UserContext;
    context->private_data = context->fuse->data;
    context->uid = Uid;
    context->gid = Gid;

    contexthdr = (PVOID)((PUINT8)context - sizeof *contexthdr);
    contexthdr->Request = Request;
    contexthdr->Response = Response;
    contexthdr->PosixPath = PosixPath;

    Result = STATUS_SUCCESS;

exit:
    if (UserInfo != &UserInfoBuf.V)
        MemFree(UserInfo);

    if (GroupInfo != &GroupInfoBuf.V)
        MemFree(GroupInfo);

    if (!NT_SUCCESS(Result) && 0 != PosixPath)
        FspPosixDeletePath(PosixPath);

    return Result;
}

NTSTATUS fsp_fuse_op_leave(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    struct fuse_context *context;
    struct fsp_fuse_context_header *contexthdr;

    context = fsp_fuse_get_context(0);
    context->fuse = 0;
    context->private_data = 0;
    context->uid = -1;
    context->gid = -1;

    contexthdr = (PVOID)((PUINT8)context - sizeof *contexthdr);
    if (0 != contexthdr->PosixPath)
        FspPosixDeletePath(contexthdr->PosixPath);
    memset(contexthdr, 0, sizeof *contexthdr);

    FspFileSystemOpLeave(FileSystem, Request, Response);

    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_intf_GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS fsp_fuse_intf_SetVolumeLabel(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PWSTR VolumeLabel,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS fsp_fuse_intf_GetFileInfoByPath(FSP_FILE_SYSTEM *FileSystem,
    const char *PosixPath,
    PUINT32 PUid, PUINT32 PGid, PUINT32 PMode,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    struct fuse *f = FileSystem->UserContext;
    UINT64 AllocationUnit;
    struct fuse_stat stbuf;
    int err;

    if (0 == f->ops.getattr)
        return STATUS_INVALID_DEVICE_REQUEST;

    memset(&stbuf, 0, sizeof stbuf);
    err = f->ops.getattr(PosixPath, (void *)&stbuf);
    if (0 != err)
        return fsp_fuse_ntstatus_from_errno(f->env, err);

    *PUid = stbuf.st_uid;
    *PGid = stbuf.st_gid;
    *PMode = stbuf.st_mode;

    AllocationUnit = f->VolumeParams.SectorSize * f->VolumeParams.SectorsPerAllocationUnit;
    FileInfo->FileAttributes = (stbuf.st_mode & 0040000) ? FILE_ATTRIBUTE_DIRECTORY : 0;
    FileInfo->ReparseTag = 0;
    FileInfo->AllocationSize =
        (stbuf.st_size + AllocationUnit - 1) / AllocationUnit * AllocationUnit;
    FileInfo->FileSize = stbuf.st_size;
    FileInfo->CreationTime =
        Int32x32To64(stbuf.st_birthtim.tv_sec, 10000000) + 116444736000000000 +
        stbuf.st_birthtim.tv_nsec / 100;
    FileInfo->LastAccessTime =
        Int32x32To64(stbuf.st_atim.tv_sec, 10000000) + 116444736000000000 +
        stbuf.st_atim.tv_nsec / 100;
    FileInfo->LastWriteTime =
        Int32x32To64(stbuf.st_mtim.tv_sec, 10000000) + 116444736000000000 +
        stbuf.st_mtim.tv_nsec / 100;
    FileInfo->ChangeTime =
        Int32x32To64(stbuf.st_ctim.tv_sec, 10000000) + 116444736000000000 +
        stbuf.st_ctim.tv_nsec / 100;
    FileInfo->IndexNumber = stbuf.st_ino;

    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_intf_GetSecurityByName(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptorBuf, SIZE_T *PSecurityDescriptorSize)
{
    struct fuse *f = FileSystem->UserContext;
    char *PosixPath = 0;
    UINT32 Uid, Gid, Mode;
    FSP_FSCTL_FILE_INFO FileInfo;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    SIZE_T SecurityDescriptorSize;
    NTSTATUS Result;

    Result = FspPosixMapWindowsToPosixPath(FileName, &PosixPath);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = fsp_fuse_intf_GetFileInfoByPath(FileSystem, PosixPath, &Uid, &Gid, &Mode, &FileInfo);
    if (!NT_SUCCESS(Result))
        goto exit;

    if (0 != PSecurityDescriptorSize)
    {
        Result = FspPosixMapPermissionsToSecurityDescriptor(Uid, Gid, Mode, &SecurityDescriptor);
        if (!NT_SUCCESS(Result))
            goto exit;

        SecurityDescriptorSize = GetSecurityDescriptorLength(SecurityDescriptor);

        if (SecurityDescriptorSize > *PSecurityDescriptorSize)
        {
            *PSecurityDescriptorSize = SecurityDescriptorSize;
            Result = STATUS_BUFFER_OVERFLOW;
            goto exit;
        }

        *PSecurityDescriptorSize = SecurityDescriptorSize;
        if (0 != SecurityDescriptorBuf)
            memcpy(SecurityDescriptorBuf, SecurityDescriptor, SecurityDescriptorSize);
    }

    if (0 != PFileAttributes)
        *PFileAttributes = FileInfo.FileAttributes;

    Result = STATUS_SUCCESS;

exit:
    if (0 != SecurityDescriptor)
        FspDeleteSecurityDescriptor(SecurityDescriptor,
            FspPosixMapPermissionsToSecurityDescriptor);

    if (0 != PosixPath)
        FspPosixDeletePath(PosixPath);

    return Result;
}

static NTSTATUS fsp_fuse_intf_Create(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PWSTR FileName, BOOLEAN CaseSensitive, UINT32 CreateOptions,
    UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
    PVOID *PFileNode, FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS fsp_fuse_intf_Open(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PWSTR FileName, BOOLEAN CaseSensitive, UINT32 CreateOptions,
    PVOID *PFileNode, FSP_FSCTL_FILE_INFO *FileInfo)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_context_header *contexthdr = fsp_fuse_context_header();
    UINT32 Uid, Gid, Mode;
    FSP_FSCTL_FILE_INFO FileInfoBuf;
    struct fsp_fuse_file_desc *filedesc = 0;
    struct fuse_file_info fi;
    int err;
    NTSTATUS Result;

    Result = fsp_fuse_intf_GetFileInfoByPath(FileSystem, contexthdr->PosixPath,
        &Uid, &Gid, &Mode, &FileInfoBuf);
    if (!NT_SUCCESS(Result))
        goto exit;

    filedesc = MemAlloc(sizeof *filedesc);
    if (0 == filedesc)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    memset(&fi, 0, sizeof fi);
    switch (Request->Req.Create.DesiredAccess & (FILE_READ_DATA | FILE_WRITE_DATA))
    {
    case FILE_READ_DATA:
        fi.flags = _O_RDONLY;
        break;
    case FILE_WRITE_DATA:
        fi.flags = _O_WRONLY;
        break;
    case FILE_READ_DATA | FILE_WRITE_DATA:
        fi.flags = _O_RDWR;
        break;
    }

    if (FileInfoBuf.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        if (0 != f->ops.opendir)
        {
            err = f->ops.opendir(contexthdr->PosixPath, &fi);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
        else
            Result = STATUS_SUCCESS;
    }
    else
    {
        if (0 != f->ops.open)
        {
            err = f->ops.open(contexthdr->PosixPath, &fi);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
        else
            Result = STATUS_INVALID_DEVICE_REQUEST;
    }
    if (!NT_SUCCESS(Result))
        goto exit;

    /*
     * Ignore fuse_file_info::direct_io, fuse_file_info::keep_cache
     * WinFsp does not currently support disabling the cache manager
     * for an individual file although it should not be hard to add
     * if required.
     *
     * Ignore fuse_file_info::nonseekable.
     */

    *PFileNode = 0;
    memcpy(FileInfo, &FileInfoBuf, sizeof FileInfoBuf);

    filedesc->PosixPath = contexthdr->PosixPath;
    filedesc->OpenFlags = fi.flags;
    filedesc->FileHandle = fi.fh;
    contexthdr->PosixPath = 0;
    contexthdr->Response->Rsp.Create.Opened.UserContext2 = (UINT64)(UINT_PTR)filedesc;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
        MemFree(filedesc);

    return Result;
}

static NTSTATUS fsp_fuse_intf_Overwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static VOID fsp_fuse_intf_Cleanup(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, PWSTR FileName, BOOLEAN Delete)
{
}

static VOID fsp_fuse_intf_Close(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode)
{
}

static NTSTATUS fsp_fuse_intf_Read(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, PVOID Buffer, UINT64 Offset, ULONG Length,
    PULONG PBytesTransferred)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS fsp_fuse_intf_Write(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, PVOID Buffer, UINT64 Offset, ULONG Length,
    BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
    PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS fsp_fuse_intf_Flush(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS fsp_fuse_intf_GetFileInfo(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS fsp_fuse_intf_SetBasicInfo(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, UINT32 FileAttributes,
    UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS fsp_fuse_intf_SetAllocationSize(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, UINT64 AllocationSize,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS fsp_fuse_intf_SetFileSize(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, UINT64 FileSize,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS fsp_fuse_intf_CanDelete(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, PWSTR FileName)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS fsp_fuse_intf_Rename(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode,
    PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS fsp_fuse_intf_GetSecurity(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS fsp_fuse_intf_SetSecurity(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode,
    SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS fsp_fuse_intf_ReadDirectory(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, PVOID Buffer, UINT64 Offset, ULONG Length,
    PWSTR Pattern,
    PULONG PBytesTransferred)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

FSP_FILE_SYSTEM_INTERFACE fsp_fuse_intf =
{
    fsp_fuse_intf_GetVolumeInfo,
    fsp_fuse_intf_SetVolumeLabel,
    fsp_fuse_intf_GetSecurityByName,
    fsp_fuse_intf_Create,
    fsp_fuse_intf_Open,
    fsp_fuse_intf_Overwrite,
    fsp_fuse_intf_Cleanup,
    fsp_fuse_intf_Close,
    fsp_fuse_intf_Read,
    fsp_fuse_intf_Write,
    fsp_fuse_intf_Flush,
    fsp_fuse_intf_GetFileInfo,
    fsp_fuse_intf_SetBasicInfo,
    fsp_fuse_intf_SetAllocationSize,
    fsp_fuse_intf_SetFileSize,
    fsp_fuse_intf_CanDelete,
    fsp_fuse_intf_Rename,
    fsp_fuse_intf_GetSecurity,
    fsp_fuse_intf_SetSecurity,
    fsp_fuse_intf_ReadDirectory,
};
