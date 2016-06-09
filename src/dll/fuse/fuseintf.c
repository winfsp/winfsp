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

NTSTATUS fsp_fuse_op_enter(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    struct fuse_context *context;
    struct fsp_fuse_context_header *context_header;
    UINT32 Uid = -1, Gid = -1;
    HANDLE Token;
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
        Token = (HANDLE)Request->Req.Create.AccessToken;
    else if (FspFsctlTransactSetInformationKind == Request->Kind &&
        10/*FileRenameInformation*/ == Request->Req.SetInformation.FileInformationClass)
        Token = (HANDLE)Request->Req.SetInformation.Info.Rename.AccessToken;
    else if (FspFsctlTransactSetSecurityKind == Request->Kind)
        Token = (HANDLE)Request->Req.SetSecurity.AccessToken;
    else
        Token = 0;

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

    context->fuse = FileSystem->UserContext;
    context->uid = Uid;
    context->gid = Gid;

    context_header = (PVOID)((PUINT8)context - sizeof *context_header);
    context_header->Request = Request;
    context_header->Response = Response;

    Result = FspFileSystemOpEnter(FileSystem, Request, Response);

exit:
    if (UserInfo != &UserInfoBuf.V)
        MemFree(UserInfo);

    if (GroupInfo != &GroupInfoBuf.V)
        MemFree(GroupInfo);

    return Result;
}

NTSTATUS fsp_fuse_op_leave(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    struct fuse_context *context;
    struct fsp_fuse_context_header *context_header;

    context = fsp_fuse_get_context(0);
    context->fuse = 0;
    context->uid = -1;
    context->gid = -1;

    context_header = (PVOID)((PUINT8)context - sizeof *context_header);
    context_header->Request = 0;
    context_header->Response = 0;

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

static NTSTATUS fsp_fuse_intf_GetSecurityByName(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptorBuf, SIZE_T *PSecurityDescriptorSize)
{
    struct fuse *f = FileSystem->UserContext;
    char *PosixPath = 0;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    SIZE_T SecurityDescriptorSize;
    struct fuse_stat stbuf;
    int err;
    NTSTATUS Result;

    if (0 == f->ops.getattr)
        return STATUS_INVALID_DEVICE_REQUEST;

    Result = FspPosixMapWindowsToPosixPath(FileName, &PosixPath);
    if (!NT_SUCCESS(Result))
        goto exit;

    memset(&stbuf, 0, sizeof stbuf);
    err = f->ops.getattr(PosixPath, (void *)&stbuf);
    if (0 != err)
    {
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        goto exit;
    }

    if (0 != PFileAttributes)
        *PFileAttributes = (stbuf.st_mode & 0040000) ?
            FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

    if (0 != PSecurityDescriptorSize)
    {
        Result = FspPosixMapPermissionsToSecurityDescriptor(
            stbuf.st_uid, stbuf.st_gid, stbuf.st_mode, &SecurityDescriptor);
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
    return STATUS_INVALID_DEVICE_REQUEST;
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
