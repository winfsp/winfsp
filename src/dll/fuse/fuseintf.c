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

    FspFileSystemOpLeave(FileSystem, Request, Response);

    context = fsp_fuse_get_context(0);
    context->fuse = 0;
    context->private_data = 0;
    context->uid = -1;
    context->gid = -1;

    contexthdr = (PVOID)((PUINT8)context - sizeof *contexthdr);
    if (0 != contexthdr->PosixPath)
        FspPosixDeletePath(contexthdr->PosixPath);
    memset(contexthdr, 0, sizeof *contexthdr);

    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_intf_GetFileInfoEx(FSP_FILE_SYSTEM *FileSystem,
    const char *PosixPath, struct fuse_file_info *fi,
    PUINT32 PUid, PUINT32 PGid, PUINT32 PMode,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    struct fuse *f = FileSystem->UserContext;
    UINT64 AllocationUnit;
    struct fuse_stat stbuf;
    int err;

    memset(&stbuf, 0, sizeof stbuf);

    if (0 != fi && 0 != f->ops.fgetattr)
        err = f->ops.fgetattr(PosixPath, (void *)&stbuf, fi);
    else if (0 == f->ops.getattr)
        err = f->ops.getattr(PosixPath, (void *)&stbuf);
    else
        return STATUS_INVALID_DEVICE_REQUEST;

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

static NTSTATUS fsp_fuse_intf_GetSecurityEx(FSP_FILE_SYSTEM *FileSystem,
    const char *PosixPath, struct fuse_file_info *fi,
    PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptorBuf, SIZE_T *PSecurityDescriptorSize)
{
    struct fuse *f = FileSystem->UserContext;
    UINT32 Uid, Gid, Mode;
    FSP_FSCTL_FILE_INFO FileInfo;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    SIZE_T SecurityDescriptorSize;
    NTSTATUS Result;

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, PosixPath, fi, &Uid, &Gid, &Mode, &FileInfo);
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

    return Result;
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
    NTSTATUS Result;

    Result = FspPosixMapWindowsToPosixPath(FileName, &PosixPath);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = fsp_fuse_intf_GetSecurityEx(FileSystem, PosixPath, 0,
        PFileAttributes, SecurityDescriptorBuf, PSecurityDescriptorSize);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = STATUS_SUCCESS;

exit:
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
    struct fuse *f = FileSystem->UserContext;
    struct fuse_context *context = fsp_fuse_get_context(0);
    struct fsp_fuse_context_header *contexthdr =
        (PVOID)((PUINT8)context - sizeof *contexthdr);
    UINT32 Uid, Gid, Mode;
    FSP_FSCTL_FILE_INFO FileInfoBuf;
    struct fsp_fuse_file_desc *filedesc = 0;
    struct fuse_file_info fi;
    BOOLEAN Opened = FALSE;
    int err;
    NTSTATUS Result;

    filedesc = MemAlloc(sizeof *filedesc);
    if (0 == filedesc)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    Uid = context->uid;
    Gid = context->gid;
    Mode = 0777;
    if (0 != SecurityDescriptor)
    {
        Result = FspPosixMapSecurityDescriptorToPermissions(SecurityDescriptor,
            &Uid, &Gid, &Mode);
        if (!NT_SUCCESS(Result))
            goto exit;
    }
    Mode &= ~context->umask;

    memset(&fi, 0, sizeof fi);

    if (CreateOptions & FILE_DIRECTORY_FILE)
    {
        if (0 != f->ops.mkdir)
        {
            err = f->ops.mkdir(contexthdr->PosixPath, Mode);
            if (0 != err)
            {
                Result = fsp_fuse_ntstatus_from_errno(f->env, err);
                goto exit;
            }

            if (0 != f->ops.opendir)
            {
                err = f->ops.opendir(contexthdr->PosixPath, &fi);
                Result = fsp_fuse_ntstatus_from_errno(f->env, err);
            }
        }
        else
            Result = STATUS_SUCCESS;
    }
    else
    {
        if (0 != f->ops.create)
        {
            err = f->ops.create(contexthdr->PosixPath, Mode, &fi);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
        else if (0 != f->ops.mknod && 0 != f->ops.open)
        {
            err = f->ops.mknod(contexthdr->PosixPath, Mode, 0);
            if (0 != err)
            {
                Result = fsp_fuse_ntstatus_from_errno(f->env, err);
                goto exit;
            }

            fi.flags = O_RDWR;
            err = f->ops.open(contexthdr->PosixPath, &fi);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
        else
            Result = STATUS_INVALID_DEVICE_REQUEST;
    }
    if (!NT_SUCCESS(Result))
        goto exit;

    Opened = TRUE;

    if (Uid != context->uid || Gid != context->gid)
        if (0 != f->ops.chown)
        {
            err = f->ops.chown(contexthdr->PosixPath, Uid, Gid);
            if (0 != err)
            {
                Result = fsp_fuse_ntstatus_from_errno(f->env, err);
                goto exit;
            }
        }

    /*
     * Ignore fuse_file_info::direct_io, fuse_file_info::keep_cache
     * WinFsp does not currently support disabling the cache manager
     * for an individual file although it should not be hard to add
     * if required.
     *
     * Ignore fuse_file_info::nonseekable.
     */

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, contexthdr->PosixPath, &fi,
        &Uid, &Gid, &Mode, &FileInfoBuf);
    if (!NT_SUCCESS(Result))
        goto exit;

    *PFileNode = 0;
    memcpy(FileInfo, &FileInfoBuf, sizeof FileInfoBuf);

    filedesc->PosixPath = contexthdr->PosixPath;
    filedesc->IsDirectory = !!(FileInfoBuf.FileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    filedesc->OpenFlags = fi.flags;
    filedesc->FileHandle = fi.fh;
    filedesc->DirBuffer = 0;
    filedesc->DirBufferSize = 0;
    contexthdr->PosixPath = 0;
    contexthdr->Response->Rsp.Create.Opened.UserContext2 = (UINT64)(UINT_PTR)filedesc;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        if (Opened)
        {
            if (CreateOptions & FILE_DIRECTORY_FILE)
            {
                if (0 != f->ops.releasedir)
                    f->ops.releasedir(filedesc->PosixPath, &fi);
            }
            else
            {
                if (0 != f->ops.release)
                    f->ops.release(filedesc->PosixPath, &fi);
            }
        }

        MemFree(filedesc);
    }

    return Result;
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

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, contexthdr->PosixPath, 0,
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
    filedesc->IsDirectory = !!(FileInfoBuf.FileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    filedesc->OpenFlags = fi.flags;
    filedesc->FileHandle = fi.fh;
    filedesc->DirBuffer = 0;
    filedesc->DirBufferSize = 0;
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
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.Cleanup.UserContext2;

    /*
     * In Windows a DeleteFile/RemoveDirectory is the sequence of the following:
     *     Create(FILE_OPEN)
     *     SetInformation(Disposition)
     *     Cleanup
     *     Close
     *
     * The FSD maintains a count of how many handles are currently open for a file. When the
     * last handle is closed *and* the disposition flag is set the FSD sends us a Cleanup with
     * the Delete flag set.
     *
     * Notice that when we receive a Cleanup with Delete set there can be no open handles other
     * than ours. [Even if there is a concurrent Open of this file, the FSD will fail it with
     * STATUS_DELETE_PENDING.] This means that we do not need to worry about the hard_remove
     * FUSE option and can safely remove the file at this time.
     */

    if (Delete)
        if (filedesc->IsDirectory)
        {
            if (0 != f->ops.rmdir)
                f->ops.rmdir(filedesc->PosixPath);
        }
        else
        {
            if (0 != f->ops.unlink)
                f->ops.rmdir(filedesc->PosixPath);
        }
}

static VOID fsp_fuse_intf_Close(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.Close.UserContext2;
    struct fuse_file_info fi;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    if (filedesc->IsDirectory)
    {
        if (0 != f->ops.releasedir)
            f->ops.releasedir(filedesc->PosixPath, &fi);
    }
    else
    {
        if (0 != f->ops.flush)
            f->ops.flush(filedesc->PosixPath, &fi);
        if (0 != f->ops.release)
            f->ops.release(filedesc->PosixPath, &fi);
    }

    MemFree(filedesc->DirBuffer);
    MemFree(filedesc->PosixPath);
    MemFree(filedesc);
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
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.QueryInformation.UserContext2;
    UINT32 Uid, Gid, Mode;
    struct fuse_file_info fi;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    return fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath, &fi,
        &Uid, &Gid, &Mode, FileInfo);
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
    PSECURITY_DESCRIPTOR SecurityDescriptorBuf, SIZE_T *PSecurityDescriptorSize)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.QuerySecurity.UserContext2;
    struct fuse_file_info fi;
    UINT32 FileAttributes;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    return fsp_fuse_intf_GetSecurityEx(FileSystem, filedesc->PosixPath, &fi,
        &FileAttributes, SecurityDescriptorBuf, PSecurityDescriptorSize);
}

static NTSTATUS fsp_fuse_intf_SetSecurity(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode,
    SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR Ignored)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.SetSecurity.UserContext2;
    struct fuse_file_info fi;
    UINT32 Uid, Gid, Mode, NewUid, NewGid, NewMode;
    FSP_FSCTL_FILE_INFO FileInfo;
    PSECURITY_DESCRIPTOR SecurityDescriptor, NewSecurityDescriptor;
    int err;
    NTSTATUS Result;

    if (0 == f->ops.chmod)
        return STATUS_INVALID_DEVICE_REQUEST;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath, &fi, &Uid, &Gid, &Mode,
        &FileInfo);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = FspPosixMapPermissionsToSecurityDescriptor(Uid, Gid, Mode, &SecurityDescriptor);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = FspSetSecurityDescriptor(FileSystem, Request, SecurityDescriptor,
        &NewSecurityDescriptor);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = FspPosixMapSecurityDescriptorToPermissions(NewSecurityDescriptor,
        &NewUid, &NewGid, &NewMode);
    if (!NT_SUCCESS(Result))
        goto exit;

    if (NewMode != Mode)
    {
        err = f->ops.chmod(filedesc->PosixPath, NewMode);
        if (0 != err)
        {
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
            goto exit;
        }
    }

    if (NewUid != Uid || NewGid != Gid)
        if (0 != f->ops.chown)
        {
            err = f->ops.chown(filedesc->PosixPath, NewUid, NewGid);
            if (0 != err)
            {
                Result = fsp_fuse_ntstatus_from_errno(f->env, err);
                goto exit;
            }
        }

    Result = STATUS_SUCCESS;

exit:
    if (0 != NewSecurityDescriptor)
        FspDeleteSecurityDescriptor(SecurityDescriptor,
            FspSetSecurityDescriptor);

    if (0 != SecurityDescriptor)
        FspDeleteSecurityDescriptor(SecurityDescriptor,
            FspPosixMapPermissionsToSecurityDescriptor);

    return Result;
}

struct fuse_dirhandle
{
    FSP_FILE_SYSTEM *FileSystem;
    char *PosixPath, *PosixName;
    PVOID OriginalBuffer;
    ULONG OriginalLength;
    PVOID Buffer;
    ULONG Length;
    ULONG BytesTransferred;
};

int fsp_fuse_intf_AddDirInfo(void *buf, const char *name,
    const struct fuse_stat *stbuf, fuse_off_t off)
{
    struct fuse_dirhandle *dh = buf;
    UINT32 Uid, Gid, Mode;
    union
    {
        FSP_FSCTL_DIR_INFO V;
        UINT8 B[sizeof(FSP_FSCTL_DIR_INFO) + 255 * sizeof(WCHAR)];
    } DirInfoBuf;
    FSP_FSCTL_DIR_INFO *DirInfo = &DirInfoBuf.V;
    char *PosixPathEnd = 0, SavedPathChar;
    PWSTR FileName = 0;
    ULONG Size;
    NTSTATUS Result;

    if ('.' == name[0] && '\0' == name[1])
    {
        PosixPathEnd = 1 < dh->PosixName - dh->PosixPath ? dh->PosixName - 1 : dh->PosixName;
        SavedPathChar = *PosixPathEnd;
        *PosixPathEnd = '\0';
    }
    else
    if ('.' == name[0] && '.' == name[1] && '\0' == name[2])
    {
        PosixPathEnd = 1 < dh->PosixName - dh->PosixPath ? dh->PosixName - 2 : dh->PosixName;
        while (dh->PosixPath < PosixPathEnd && '/' != *PosixPathEnd)
            PosixPathEnd--;
        if (dh->PosixPath == PosixPathEnd)
            PosixPathEnd++;
        SavedPathChar = *PosixPathEnd;
        *PosixPathEnd = '\0';
    }
    else
    {
        Size = lstrlenA(name);
        if (Size > 255)
            Size = 255;
        memcpy(dh->PosixName, name, Size);
        dh->PosixName[Size] = '\0';
    }

    Result = fsp_fuse_intf_GetFileInfoEx(dh->FileSystem, dh->PosixPath, 0,
        &Uid, &Gid, &Mode, &DirInfo->FileInfo);
    if (!NT_SUCCESS(Result))
        return 1;

    if (0 != PosixPathEnd)
        *PosixPathEnd = SavedPathChar;

    Result = FspPosixMapPosixToWindowsPath(name, &FileName);
    if (!NT_SUCCESS(Result))
        return 1;

    Size = lstrlenW(FileName);
    if (Size > 255)
        Size = 255;
    Size *= sizeof(WCHAR);
    memcpy(DirInfo->FileNameBuf, FileName, Size);

    FspPosixDeletePath(FileName);

    memset(DirInfo->Padding, 0, sizeof DirInfo->Padding);
    DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + Size);
    DirInfo->NextOffset = off;

	/*
	 * FUSE readdir documentation quote:
     *
	 * The filesystem may choose between two modes of operation:
	 *
	 * 1) The readdir implementation ignores the offset parameter, and
	 * passes zero to the filler function's offset.  The filler
	 * function will not return '1' (unless an error happens), so the
	 * whole directory is read in a single readdir operation.  This
	 * works just like the old getdir() method.
	 *
	 * 2) The readdir implementation keeps track of the offsets of the
	 * directory entries.  It uses the offset parameter and always
	 * passes non-zero offset to the filler function.  When the buffer
	 * is full (or an error happens) the filler function will return
	 * '1'.
	 */

    if (0 != off)
    {
        if (0 == dh->Buffer)
        {
            dh->Buffer = dh->OriginalBuffer;
            dh->Length = dh->OriginalLength;
        }
    }
    else if (dh->OriginalBuffer != dh->Buffer)
    {
        DirInfo->NextOffset = dh->BytesTransferred + FSP_FSCTL_DEFAULT_ALIGN_UP(DirInfo->Size);

        if (0 != dh->Buffer &&
            FspFileSystemAddDirInfo(DirInfo, dh->Buffer, dh->Length, &dh->BytesTransferred))
            return 0;

        if (0 == dh->Length)
            dh->Length = 8 * 1024; /* initial alloc: 16 == 8 * 2; see below */
        else if (dh->Length >= 2 * 1024 * 1024)
            return 1;

        PVOID Buffer = MemAlloc(dh->Length * 2);
        if (0 == Buffer)
            return 1;

        memcpy(Buffer, dh->Buffer, dh->BytesTransferred);
        MemFree(dh->Buffer);

        dh->Buffer = Buffer;
        dh->Length *= 2;
    }

    return ! FspFileSystemAddDirInfo(DirInfo, dh->Buffer, dh->Length, &dh->BytesTransferred);
}

int fsp_fuse_intf_AddDirInfoOld(fuse_dirh_t dh, const char *name,
    int type, fuse_ino_t ino)
{
    return fsp_fuse_intf_AddDirInfo(dh, name, 0, 0) ? -ENOMEM : 0;
}

static NTSTATUS fsp_fuse_intf_ReadDirectory(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, PVOID Buffer, UINT64 Offset, ULONG Length,
    PWSTR Pattern,
    PULONG PBytesTransferred)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.QueryDirectory.UserContext2;
    struct fuse_file_info fi;
    struct fuse_dirhandle dh;
    FSP_FSCTL_DIR_INFO *DirInfo;
    PUINT8 DirInfoEnd;
    ULONG Size;
    int err;
    NTSTATUS Result;

    if (0 == filedesc->DirBuffer)
    {
        memset(&dh, 0, sizeof dh);

        Size = lstrlenA(filedesc->PosixPath);
        dh.PosixPath = MemAlloc(Size + 1 + 255 + 1);
        if (0 == dh.PosixPath)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        memcpy(dh.PosixPath, filedesc->PosixPath, Size);
        if (1 < Size)
            /* if not root */
            dh.PosixPath[Size++] = '/';
        dh.PosixName = dh.PosixPath + Size;

        dh.OriginalBuffer = Buffer;
        dh.OriginalLength = Length;

        if (0 != f->ops.readdir)
        {
            memset(&fi, 0, sizeof fi);
            fi.flags = filedesc->OpenFlags;
            fi.fh = filedesc->FileHandle;

            err = f->ops.readdir(filedesc->PosixPath, &dh, fsp_fuse_intf_AddDirInfo, 0, &fi);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
        else if (0 != f->ops.getdir)
        {
            err = f->ops.getdir(filedesc->PosixPath, &dh, fsp_fuse_intf_AddDirInfoOld);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
        else
            Result = STATUS_INVALID_DEVICE_REQUEST;

    exit:
        MemFree(dh.PosixPath);

        if (NT_SUCCESS(Result))
        {
            if (dh.OriginalBuffer == dh.Buffer)
            {
                *PBytesTransferred = dh.BytesTransferred;
                return STATUS_SUCCESS;
            }

            if (0 == dh.Buffer)
            {
                *PBytesTransferred = dh.BytesTransferred;

                /* EOF */
                FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);

                return STATUS_SUCCESS;
            }

            filedesc->DirBuffer = dh.Buffer;
            filedesc->DirBufferSize = dh.BytesTransferred;
            /* fall through! */
        }
        else
        {
            MemFree(dh.Buffer);
            return Result;
        }
    }

    DirInfo = (PVOID)((PUINT8)filedesc->DirBuffer + Offset);
    DirInfoEnd = (PUINT8)filedesc->DirBuffer + filedesc->DirBufferSize;
    for (;
        (PUINT8)DirInfo + sizeof(DirInfo->Size) <= DirInfoEnd;
        DirInfo = (PVOID)((PUINT8)DirInfo + FSP_FSCTL_DEFAULT_ALIGN_UP(DirInfo->Size)))
    {
        if (sizeof(FSP_FSCTL_DIR_INFO) > DirInfo->Size)
            break;

        if (!FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred))
            break;
    }

    if ((PUINT8)DirInfo + sizeof(DirInfo->Size) > DirInfoEnd)
        FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);

    return STATUS_SUCCESS;
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
