/**
 * @file dll/fuse/fuse_intf.c
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

/*
 * FSP_FUSE_REPARSE_OPTHACK
 *
 * Define this macro to enable the ResolveReparsePoints optimization hack.
 * See fsp_fuse_intf_GetSecurityByName for details.
 */
#define FSP_FUSE_REPARSE_OPTHACK

NTSTATUS fsp_fuse_op_enter(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    struct fuse *f = FileSystem->UserContext;
    struct fuse_context *context;
    struct fsp_fuse_context_header *contexthdr;
    char *PosixPath = 0;
    UINT32 Uid = -1, Gid = -1;
    PWSTR FileName = 0, Suffix;
    WCHAR Root[2] = L"\\";
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
        if (Request->Req.Create.OpenTargetDirectory)
            FspPathSuffix((PWSTR)Request->Buffer, &FileName, &Suffix, Root);
        else
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
        if (FspFsctlTransactCreateKind == Request->Kind && Request->Req.Create.OpenTargetDirectory)
            FspPathCombine((PWSTR)Request->Buffer, Suffix);
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

    context = fsp_fuse_get_context(f->env);
    if (0 == context)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    Result = FspFileSystemOpEnter(FileSystem, Request, Response);
    if (!NT_SUCCESS(Result))
        goto exit;

    context->fuse = f;
    context->private_data = f->data;
    context->uid = Uid;
    context->gid = Gid;

    contexthdr = FSP_FUSE_HDR_FROM_CONTEXT(context);
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
    struct fuse *f = FileSystem->UserContext;
    struct fuse_context *context;
    struct fsp_fuse_context_header *contexthdr;

    FspFileSystemOpLeave(FileSystem, Request, Response);

    context = fsp_fuse_get_context(f->env);
    context->fuse = 0;
    context->private_data = 0;
    context->uid = -1;
    context->gid = -1;

    contexthdr = FSP_FUSE_HDR_FROM_CONTEXT(context);
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

    if (0 != f->ops.fgetattr && 0 != fi && -1 != fi->fh)
        err = f->ops.fgetattr(PosixPath, (void *)&stbuf, fi);
    else if (0 != f->ops.getattr)
        err = f->ops.getattr(PosixPath, (void *)&stbuf);
    else
        return STATUS_INVALID_DEVICE_REQUEST;

    if (0 != err)
        return fsp_fuse_ntstatus_from_errno(f->env, err);

    if (f->set_umask)
        stbuf.st_mode = (stbuf.st_mode & 0170000) | (0777 & ~f->umask);
    if (f->set_uid)
        stbuf.st_uid = f->uid;
    if (f->set_gid)
        stbuf.st_gid = f->gid;

    *PUid = stbuf.st_uid;
    *PGid = stbuf.st_gid;
    *PMode = stbuf.st_mode;

    AllocationUnit = (UINT64)f->VolumeParams.SectorSize *
        (UINT64)f->VolumeParams.SectorsPerAllocationUnit;
    switch (stbuf.st_mode & 0170000)
    {
    case 0040000:
        FileInfo->FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        FileInfo->ReparseTag = 0;
        break;
    case 0120000:
        if (FSP_FUSE_HAS_SYMLINKS(f))
        {
            FileInfo->FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
            FileInfo->ReparseTag = IO_REPARSE_TAG_SYMLINK;
            break;
        }
        /* fall through */
    default:
        FileInfo->FileAttributes = 0;
        FileInfo->ReparseTag = 0;
        break;
    }
    FileInfo->FileSize = stbuf.st_size;
    FileInfo->AllocationSize =
        (FileInfo->FileSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit;
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

static char *fsp_fuse_intf_GetSymlinkPointer(FSP_FILE_SYSTEM *FileSystem,
    char *PosixPath)
{
    struct fuse *f = FileSystem->UserContext;
    char *p, *lastp;
    struct fuse_stat stbuf;
    int err;

    if (0 == f->ops.getattr)
        return 0;

    p = PosixPath;
    for (;;)
    {
        while ('/' == *p)
            p++;
        lastp = p;
        while ('/' != *p)
        {
            if ('\0' == *p)
                return 0;
            p++;
        }

        *p = '\0';
        memset(&stbuf, 0, sizeof stbuf);
        err = f->ops.getattr(PosixPath, (void *)&stbuf);
        *p = '/';

        if (0 != err)
            return 0;
        else if (0120000 == (stbuf.st_mode & 0170000))
            return lastp;
    }

    return 0;
}

static NTSTATUS fsp_fuse_intf_GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    struct fuse *f = FileSystem->UserContext;
    struct fuse_statvfs stbuf;
    int err;

    if (0 == f->ops.statfs)
        return STATUS_INVALID_DEVICE_REQUEST;

    memset(&stbuf, 0, sizeof stbuf);
    err = f->ops.statfs("/", &stbuf);
    if (0 != err)
        return fsp_fuse_ntstatus_from_errno(f->env, err);

    VolumeInfo->TotalSize = (UINT64)stbuf.f_blocks * (UINT64)stbuf.f_frsize;
    VolumeInfo->FreeSize = (UINT64)stbuf.f_bfree * (UINT64)stbuf.f_frsize;
    VolumeInfo->VolumeLabelLength = 0;
    VolumeInfo->VolumeLabel[0] = L'\0';

    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_intf_SetVolumeLabel(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PWSTR VolumeLabel,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    /*
     * Implemented: there is no volume label concept in FUSE.
     * Perhaps we can emulate it with a xattr on "/" one day.
     */
    return STATUS_INVALID_PARAMETER;
}

static NTSTATUS fsp_fuse_intf_GetSecurityByName(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptorBuf, SIZE_T *PSecurityDescriptorSize)
{
    struct fuse *f = FileSystem->UserContext;
    char *PosixPath = 0, *PosixSymlinkPointer;
    NTSTATUS Result;

    Result = FspPosixMapWindowsToPosixPath(FileName, &PosixPath);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = fsp_fuse_intf_GetSecurityEx(FileSystem, PosixPath, 0,
        PFileAttributes, SecurityDescriptorBuf, PSecurityDescriptorSize);
    if (!NT_SUCCESS(Result))
        goto exit;

    if (FSP_FUSE_HAS_SYMLINKS(f) &&
        0 != (PosixSymlinkPointer = fsp_fuse_intf_GetSymlinkPointer(FileSystem, PosixPath)))
    {
#if defined(FSP_FUSE_REPARSE_OPTHACK)
        /* OPTHACK: This is a rather gross hack for optimization purposes only.
         *
         * If the FileName in this GetSecurityByName call and the PosixPath we computed
         * matches the one in the FSD request, then remember where the first symlink is
         * for use in ResolveReparsePoints later on. This avoids going through every path
         * component twice (doing both a getattr and readlink).
         */

        struct fuse_context *context = fsp_fuse_get_context(f->env);
        struct fsp_fuse_context_header *contexthdr = FSP_FUSE_HDR_FROM_CONTEXT(context);

        if (FspFsctlTransactCreateKind == contexthdr->Request->Kind &&
            (PUINT8)FileName == contexthdr->Request->Buffer &&
            0 == lstrcmpA(PosixPath, contexthdr->PosixPath))
            contexthdr->SymlinkIndex = PosixSymlinkPointer - PosixPath;
#endif

        Result = STATUS_REPARSE;
    }
    else
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
    struct fuse_context *context = fsp_fuse_get_context(f->env);
    struct fsp_fuse_context_header *contexthdr = FSP_FUSE_HDR_FROM_CONTEXT(context);
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
    if ('C' == f->env->environment) /* Cygwin */
        fi.flags = 0x0200 | 2 /*O_CREAT|O_RDWR*/;
    else
        fi.flags = 0x0100 | 2 /*O_CREAT|O_RDWR*/;

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
            else
            {
                fi.fh = -1;
                Result = STATUS_SUCCESS;
            }
        }
        else
            Result = STATUS_INVALID_DEVICE_REQUEST;
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
    struct fuse_context *context = fsp_fuse_get_context(f->env);
    struct fsp_fuse_context_header *contexthdr = FSP_FUSE_HDR_FROM_CONTEXT(context);
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
    default:
    case FILE_READ_DATA:
        fi.flags = 0/*O_RDONLY*/;
        break;
    case FILE_WRITE_DATA:
        fi.flags = 1/*O_WRONLY*/;
        break;
    case FILE_READ_DATA | FILE_WRITE_DATA:
        fi.flags = 2/*O_RDWR*/;
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
        {
            fi.fh = -1;
            Result = STATUS_SUCCESS;
        }
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
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.Overwrite.UserContext2;
    UINT32 Uid, Gid, Mode;
    struct fuse_file_info fi;
    int err;
    NTSTATUS Result;

    if (0 != f->ops.ftruncate)
    {
        memset(&fi, 0, sizeof fi);
        fi.flags = filedesc->OpenFlags;
        fi.fh = filedesc->FileHandle;

        err = f->ops.ftruncate(filedesc->PosixPath, 0, &fi);
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
    }
    else if (0 != f->ops.truncate)
    {
        err = f->ops.truncate(filedesc->PosixPath, 0);
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
    }
    else
        Result = STATUS_INVALID_DEVICE_REQUEST;
    if (!NT_SUCCESS(Result))
        return Result;

    return fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath, &fi,
        &Uid, &Gid, &Mode, FileInfo);
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
                f->ops.unlink(filedesc->PosixPath);
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
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.Read.UserContext2;
    struct fuse_file_info fi;
    int bytes;
    NTSTATUS Result;

    if (0 == f->ops.read)
        return STATUS_INVALID_DEVICE_REQUEST;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    bytes = f->ops.read(filedesc->PosixPath, Buffer, Length, Offset, &fi);
    if (0 < bytes)
    {
        *PBytesTransferred = bytes;
        Result = STATUS_SUCCESS;
    }
    else if (0 == bytes)
        Result = STATUS_END_OF_FILE;
    else
        Result = fsp_fuse_ntstatus_from_errno(f->env, bytes);

    return Result;
}

static NTSTATUS fsp_fuse_intf_Write(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, PVOID Buffer, UINT64 Offset, ULONG Length,
    BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
    PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.Write.UserContext2;
    UINT32 Uid, Gid, Mode;
    struct fuse_file_info fi;
    FSP_FSCTL_FILE_INFO FileInfoBuf;
    UINT64 EndOffset, AllocationUnit;
    int bytes;
    NTSTATUS Result;

    if (0 == f->ops.write)
        return STATUS_INVALID_DEVICE_REQUEST;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath, &fi,
        &Uid, &Gid, &Mode, &FileInfoBuf);
    if (!NT_SUCCESS(Result))
        return Result;

    if (ConstrainedIo)
    {
        if (Offset >= FileInfoBuf.FileSize)
            goto success;
        EndOffset = Offset + Length;
        if (EndOffset > FileInfoBuf.FileSize)
            EndOffset = FileInfoBuf.FileSize;
    }
    else
    {
        if (WriteToEndOfFile)
            Offset = FileInfoBuf.FileSize;
        EndOffset = Offset + Length;
    }

    bytes = f->ops.write(filedesc->PosixPath, Buffer, (size_t)(EndOffset - Offset), Offset, &fi);
    if (0 > bytes)
        return fsp_fuse_ntstatus_from_errno(f->env, bytes);

    *PBytesTransferred = bytes;

    AllocationUnit = (UINT64)f->VolumeParams.SectorSize *
        (UINT64)f->VolumeParams.SectorsPerAllocationUnit;
    FileInfoBuf.FileSize = Offset + bytes;
    FileInfoBuf.AllocationSize =
        (FileInfoBuf.FileSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit;

success:
    memcpy(FileInfo, &FileInfoBuf, sizeof FileInfoBuf);

    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_intf_Flush(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.FlushBuffers.UserContext2;
    struct fuse_file_info fi;
    int err;
    NTSTATUS Result;

    if (0 == filedesc)
        return STATUS_SUCCESS; /* FUSE cannot flush volumes */

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    Result = STATUS_SUCCESS; /* just say success, if fs does not support fsync */
    if (filedesc->IsDirectory)
    {
        if (0 != f->ops.fsyncdir)
        {
            err = f->ops.fsyncdir(filedesc->PosixPath, 0, &fi);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
    }
    else
    {
        if (0 != f->ops.fsync)
        {
            err = f->ops.fsync(filedesc->PosixPath, 0, &fi);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
    }

    return Result;
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
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.SetInformation.UserContext2;
    UINT32 Uid, Gid, Mode;
    struct fuse_file_info fi;
    FSP_FSCTL_FILE_INFO FileInfoBuf;
    struct fuse_timespec tv[2];
    struct fuse_utimbuf timbuf;
    int err;
    NTSTATUS Result;

    if (0 == f->ops.utimens && 0 == f->ops.utime)
        return STATUS_SUCCESS; /* liar! */

    /* no way to set FileAttributes, CreationTime! */
    if (0 == LastAccessTime && 0 == LastWriteTime)
        return STATUS_SUCCESS;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath, &fi,
        &Uid, &Gid, &Mode, &FileInfoBuf);
    if (!NT_SUCCESS(Result))
        return Result;

    if (0 != LastAccessTime)
        FileInfoBuf.LastAccessTime = LastAccessTime;
    if (0 != LastWriteTime)
        FileInfoBuf.LastWriteTime = LastWriteTime;

    /* UNIX epoch in 100-ns intervals */
    LastAccessTime = FileInfoBuf.LastAccessTime - 116444736000000000;
    LastWriteTime = FileInfoBuf.LastWriteTime - 116444736000000000;

    if (0 != f->ops.utimens)
    {
#if defined(_WIN64)
        tv[0].tv_sec = (int64_t)(LastAccessTime / 10000000);
        tv[0].tv_nsec = (int64_t)(LastAccessTime % 10000000) * 100;
        tv[1].tv_sec = (int64_t)(LastWriteTime / 10000000);
        tv[1].tv_nsec = (int64_t)(LastWriteTime % 10000000) * 100;
#else
        tv[0].tv_sec = (int32_t)(LastAccessTime / 10000000);
        tv[0].tv_nsec = (int32_t)(LastAccessTime % 10000000) * 100;
        tv[1].tv_sec = (int32_t)(LastWriteTime / 10000000);
        tv[1].tv_nsec = (int32_t)(LastWriteTime % 10000000) * 100;
#endif

        err = f->ops.utimens(filedesc->PosixPath, tv);
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
    }
    else
    {
#if defined(_WIN64)
        timbuf.actime = (int64_t)(LastAccessTime / 10000000);
        timbuf.modtime = (int64_t)(LastWriteTime / 10000000);
#else
        timbuf.actime = (int32_t)(LastAccessTime / 10000000);
        timbuf.modtime = (int32_t)(LastWriteTime / 10000000);
#endif

        err = f->ops.utime(filedesc->PosixPath, &timbuf);
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
    }
    if (!NT_SUCCESS(Result))
        return Result;

    memcpy(FileInfo, &FileInfoBuf, sizeof FileInfoBuf);

    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_intf_SetFileSize(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, UINT64 NewSize, BOOLEAN SetAllocationSize,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.SetInformation.UserContext2;
    UINT32 Uid, Gid, Mode;
    struct fuse_file_info fi;
    FSP_FSCTL_FILE_INFO FileInfoBuf;
    UINT64 AllocationUnit;
    int err;
    NTSTATUS Result;

    if (0 == f->ops.ftruncate && 0 == f->ops.truncate)
        return STATUS_INVALID_DEVICE_REQUEST;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath, &fi,
        &Uid, &Gid, &Mode, &FileInfoBuf);
    if (!NT_SUCCESS(Result))
        return Result;

    if (!SetAllocationSize || FileInfoBuf.FileSize > NewSize)
    {
        /*
         * "FileInfoBuf.FileSize > NewSize" explanation:
         * FUSE 2.8 does not support allocation size. However if the new AllocationSize
         * is less than the current FileSize we must truncate the file.
         */
        if (0 != f->ops.ftruncate)
        {
            err = f->ops.ftruncate(filedesc->PosixPath, NewSize, &fi);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
        else
        {
            err = f->ops.truncate(filedesc->PosixPath, NewSize);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
        if (!NT_SUCCESS(Result))
            return Result;

        AllocationUnit = (UINT64)f->VolumeParams.SectorSize *
            (UINT64)f->VolumeParams.SectorsPerAllocationUnit;
        FileInfoBuf.FileSize = NewSize;
        FileInfoBuf.AllocationSize =
            (FileInfoBuf.FileSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit;
    }

    memcpy(FileInfo, &FileInfoBuf, sizeof FileInfoBuf);

    return STATUS_SUCCESS;
}

static int fsp_fuse_intf_CanDeleteAddDirInfo(void *buf, const char *name,
    const struct fuse_stat *stbuf, fuse_off_t off)
{
    struct fuse_dirhandle *dh = buf;

    if ('.' == name[0] && ('\0' == name[1] || ('.' == name[1] && '\0' == name[2])))
    {
        dh->DotFiles = TRUE;
        return 0;
    }
    else
    {
        dh->HasChild = TRUE;
        return 1;
    }
}

static int fsp_fuse_intf_CanDeleteAddDirInfoOld(fuse_dirh_t dh, const char *name,
    int type, fuse_ino_t ino)
{
    return fsp_fuse_intf_CanDeleteAddDirInfo(dh, name, 0, 0) ? -ENOMEM : 0;
}

static NTSTATUS fsp_fuse_intf_CanDelete(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode, PWSTR FileName)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.SetInformation.UserContext2;
    struct fuse_file_info fi;
    struct fuse_dirhandle dh;
    int err;

    if (filedesc->IsDirectory)
    {
        /* check that directory is empty! */

        memset(&dh, 0, sizeof dh);

        if (0 != f->ops.readdir)
            err = f->ops.readdir(filedesc->PosixPath, &dh, fsp_fuse_intf_CanDeleteAddDirInfo, 0, &fi);
        else if (0 != f->ops.getdir)
            err = f->ops.getdir(filedesc->PosixPath, &dh, fsp_fuse_intf_CanDeleteAddDirInfoOld);
        else
            err = 0;

        if (dh.HasChild)
            return STATUS_DIRECTORY_NOT_EMPTY;
        else if (dh.DotFiles)
            return STATUS_SUCCESS;
        else
            return fsp_fuse_ntstatus_from_errno(f->env, err);
    }

    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_intf_Rename(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode,
    PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists)
{
    struct fuse *f = FileSystem->UserContext;
    struct fuse_context *context = fsp_fuse_get_context(f->env);
    struct fsp_fuse_context_header *contexthdr = FSP_FUSE_HDR_FROM_CONTEXT(context);
    UINT32 Uid, Gid, Mode;
    FSP_FSCTL_FILE_INFO FileInfoBuf;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.SetInformation.UserContext2;
    int err;
    NTSTATUS Result;

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, contexthdr->PosixPath, 0,
        &Uid, &Gid, &Mode, &FileInfoBuf);
    if (!NT_SUCCESS(Result) &&
        STATUS_OBJECT_NAME_NOT_FOUND != Result &&
        STATUS_OBJECT_PATH_NOT_FOUND != Result)
        return Result;

    if (NT_SUCCESS(Result))
    {
        if (!ReplaceIfExists)
            return STATUS_OBJECT_NAME_COLLISION;

        if (FileInfoBuf.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            return STATUS_ACCESS_DENIED;
    }

    err = f->ops.rename(filedesc->PosixPath, contexthdr->PosixPath);
    return fsp_fuse_ntstatus_from_errno(f->env, err);
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
        FspDeleteSecurityDescriptor(NewSecurityDescriptor,
            FspSetSecurityDescriptor);

    if (0 != SecurityDescriptor)
        FspDeleteSecurityDescriptor(SecurityDescriptor,
            FspPosixMapPermissionsToSecurityDescriptor);

    return Result;
}

int fsp_fuse_intf_AddDirInfo(void *buf, const char *name,
    const struct fuse_stat *stbuf, fuse_off_t off)
{
    struct fuse_dirhandle *dh = buf;
    struct fsp_fuse_dirinfo *di;
    ULONG len, xfersize;

    len = lstrlenA(name);
    if (len > 255)
        len = 255;

    di = (PVOID)((PUINT8)dh->Buffer + dh->BytesTransferred);
    xfersize = FSP_FSCTL_DEFAULT_ALIGN_UP(sizeof(struct fsp_fuse_dirinfo) + len + 1);

    if ((PUINT8)di + xfersize > (PUINT8)dh->Buffer + dh->Length)
    {
        PVOID Buffer;
        ULONG Length = dh->Length;

        if (0 == Length)
            Length = 16 * 1024;
        else if (Length < 16 * 1024 * 1024)
            Length *= 2;
        else
            return 1;

        Buffer = MemAlloc(Length);
        if (0 == Buffer)
            return 1;

        memcpy(Buffer, dh->Buffer, dh->BytesTransferred);
        MemFree(dh->Buffer);

        dh->Buffer = Buffer;
        dh->Length = Length;

        di = (PVOID)((PUINT8)dh->Buffer + dh->BytesTransferred);
    }

    dh->BytesTransferred += xfersize;
    dh->NonZeroOffset = dh->NonZeroOffset || 0 != off;

    di->Size = (UINT16)(sizeof(struct fsp_fuse_dirinfo) + len + 1);
    di->FileInfoValid = FALSE;
    di->NextOffset = 0 != off ? off : dh->BytesTransferred;
    memcpy(di->PosixNameBuf, name, len);
    di->PosixNameBuf[len] = '\0';

    return 0;
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
    struct fsp_fuse_dirinfo *di;
    PUINT8 diend;
    union
    {
        FSP_FSCTL_DIR_INFO V;
        UINT8 B[sizeof(FSP_FSCTL_DIR_INFO) + 255 * sizeof(WCHAR)];
    } DirInfoBuf;
    FSP_FSCTL_DIR_INFO *DirInfo = &DirInfoBuf.V;
    UINT32 Uid, Gid, Mode;
    char *PosixPath = 0, *PosixName, *PosixPathEnd, SavedPathChar;
    PWSTR FileName = 0;
    ULONG Size;
    int err;
    NTSTATUS Result;

    memset(&dh, 0, sizeof dh);

    if (0 == filedesc->DirBuffer)
    {
        if (0 != f->ops.readdir)
        {
            memset(&fi, 0, sizeof fi);
            fi.flags = filedesc->OpenFlags;
            fi.fh = filedesc->FileHandle;

            err = f->ops.readdir(filedesc->PosixPath, &dh, fsp_fuse_intf_AddDirInfo, Offset, &fi);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
        else if (0 != f->ops.getdir)
        {
            err = f->ops.getdir(filedesc->PosixPath, &dh, fsp_fuse_intf_AddDirInfoOld);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
        else
            Result = STATUS_INVALID_DEVICE_REQUEST;

        if (!NT_SUCCESS(Result))
            goto exit;

        if (0 == dh.BytesTransferred)
        {
            /* EOF */
            *PBytesTransferred = 0;
            FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);
            goto success;
        }
        else if (dh.NonZeroOffset)
        {
            di = (PVOID)((PUINT8)dh.Buffer + 0);
            diend = (PUINT8)dh.Buffer + dh.BytesTransferred;
        }
        else
        {
            di = (PVOID)((PUINT8)dh.Buffer + Offset);
            diend = (PUINT8)dh.Buffer + dh.BytesTransferred;
            filedesc->DirBuffer = dh.Buffer;
            filedesc->DirBufferSize = dh.BytesTransferred;
            dh.Buffer = 0;
        }
    }
    else
    {
        di = (PVOID)((PUINT8)filedesc->DirBuffer + Offset);
        diend = (PUINT8)filedesc->DirBuffer + filedesc->DirBufferSize;
    }

    for (;
        (PUINT8)di + sizeof(di->Size) <= diend;
        di = (PVOID)((PUINT8)di + FSP_FSCTL_DEFAULT_ALIGN_UP(di->Size)))
    {
        if (sizeof(struct fsp_fuse_dirinfo) > di->Size)
            break;

        if (!di->FileInfoValid)
        {
            if (0 == PosixPath)
            {
                Size = lstrlenA(filedesc->PosixPath);
                PosixPath = MemAlloc(Size + 1 + 255 + 1);
                if (0 == PosixPath)
                {
                    Result = STATUS_INSUFFICIENT_RESOURCES;
                    goto exit;
                }

                memcpy(PosixPath, filedesc->PosixPath, Size);
                if (1 < Size)
                    /* if not root */
                    PosixPath[Size++] = '/';
                PosixPath[Size] = '\0';
                PosixName = PosixPath + Size;
            }

            if ('.' == di->PosixNameBuf[0] && '\0' == di->PosixNameBuf[1])
            {
                PosixPathEnd = 1 < PosixName - PosixPath ? PosixName - 1 : PosixName;
                SavedPathChar = *PosixPathEnd;
                *PosixPathEnd = '\0';
            }
            else
            if ('.' == di->PosixNameBuf[0] && '.' == di->PosixNameBuf[1] && '\0' == di->PosixNameBuf[2])
            {
                PosixPathEnd = 1 < PosixName - PosixPath ? PosixName - 2 : PosixName;
                while (PosixPath < PosixPathEnd && '/' != *PosixPathEnd)
                    PosixPathEnd--;
                if (PosixPath == PosixPathEnd)
                    PosixPathEnd++;
                SavedPathChar = *PosixPathEnd;
                *PosixPathEnd = '\0';
            }
            else
            {
                PosixPathEnd = 0;
                Size = lstrlenA(di->PosixNameBuf);
                if (Size > 255)
                    Size = 255;
                memcpy(PosixName, di->PosixNameBuf, Size);
                PosixName[Size] = '\0';
            }

            Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, PosixPath, 0,
                &Uid, &Gid, &Mode, &di->FileInfo);
            if (!NT_SUCCESS(Result))
                goto exit;

            if (0 != PosixPathEnd)
                *PosixPathEnd = SavedPathChar;

            di->FileInfoValid = TRUE;
        }
        memcpy(&DirInfo->FileInfo, &di->FileInfo, sizeof di->FileInfo);

        Result = FspPosixMapPosixToWindowsPath(di->PosixNameBuf, &FileName);
        if (!NT_SUCCESS(Result))
            goto exit;

        Size = lstrlenW(FileName);
        if (Size > 255)
            Size = 255;
        Size *= sizeof(WCHAR);
        memcpy(DirInfo->FileNameBuf, FileName, Size);

        FspPosixDeletePath(FileName);

        memset(DirInfo->Padding, 0, sizeof DirInfo->Padding);
        DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + Size);
        DirInfo->NextOffset = di->NextOffset;

        if (!FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred))
            break;
    }

    if ((PUINT8)di + sizeof(di->Size) > diend)
        FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);

success:
    Result = STATUS_SUCCESS;

exit:
    MemFree(PosixPath);
    MemFree(dh.Buffer);

    return Result;
}

static NTSTATUS fsp_fuse_intf_ResolveReparsePoints(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, BOOLEAN OpenReparsePoint,
    PIO_STATUS_BLOCK PIoStatus, PVOID Buffer, PSIZE_T PSize)
{
    struct fuse *f = FileSystem->UserContext;
    struct fuse_context *context = fsp_fuse_get_context(f->env);
    struct fsp_fuse_context_header *contexthdr = FSP_FUSE_HDR_FROM_CONTEXT(context);
    char c, *p, *lastp;
    char PosixTargetPath[FSP_FSCTL_TRANSACT_PATH_SIZEMAX / sizeof(WCHAR)];
    char PosixTargetLink[FSP_FSCTL_TRANSACT_PATH_SIZEMAX / sizeof(WCHAR)];
    PWSTR TargetPath = 0;
    ULONG Length, MaxTries = 32;
    int err;
    NTSTATUS Result;

    if (0 == f->ops.readlink)
        return STATUS_INVALID_DEVICE_REQUEST;

    Length = lstrlenA(contexthdr->PosixPath);
    if (Length > sizeof PosixTargetPath - 1)
        return STATUS_OBJECT_NAME_INVALID;
    memcpy(PosixTargetPath, contexthdr->PosixPath, Length + 1);

    p = PosixTargetPath +
        contexthdr->SymlinkIndex; /* NOTE: if FSP_FUSE_REPARSE_OPTHACK is undefined, this will be 0 */
    c = *p;
    for (;;)
    {
        while ('/' == *p)
            p++;
        lastp = p;
        while ('/' != *p)
        {
            if ('\0' == *p)
            {
                if (!OpenReparsePoint)
                    goto end;
                OpenReparsePoint = FALSE;
                break;
            }
            p++;
        }

        /* handle dot and dotdot! */
        if ('.' == lastp[0])
        {
            if (p == lastp + 1)
                /* dot */
                continue;

            if ('.' == lastp[1] && p == lastp + 1)
            {
                /* dotdot */
                p = lastp;
                while (PosixTargetPath < p)
                {
                    p--;
                    if ('/' != *p)
                        break;
                }
                while (PosixTargetPath < p)
                {
                    p--;
                    if ('/' == *p)
                        break;
                }
                continue;
            }
        }

        c = *p;
        *p = '\0';
        err = f->ops.readlink(contexthdr->PosixPath, PosixTargetLink, sizeof PosixTargetLink);
        *p = c;

        if (EINVAL/* same on MSVC and Cygwin */ == err)
            /* it was not a symlink; continue */
            continue;
        else if (0 != err)
            return fsp_fuse_ntstatus_from_errno(f->env, err);

        /* found a symlink */
        if (0 == --MaxTries)
            return STATUS_REPARSE_POINT_NOT_RESOLVED;
        Length = lstrlenA(PosixTargetLink);
        if ('/' == PosixTargetLink[0])
        {
            /* we do not support absolute paths without the rellinks option */
            if (!f->rellinks)
            {
                Result = STATUS_OBJECT_NAME_NOT_FOUND;
                goto exit;
            }

            /* absolute symlink; replace whole path */
            memcpy(PosixTargetPath, PosixTargetLink, Length + 1);
            p = PosixTargetPath;
        }
        else
        {
            /* relative symlink; replace last path component seen */
            if (Length + 1 > (p + sizeof PosixTargetPath) - lastp)
                return STATUS_OBJECT_NAME_INVALID;
            memcpy(lastp, PosixTargetLink, Length + 1);
            p = lastp;
        }
    }

end:
    Result = FspPosixMapPosixToWindowsPath(PosixTargetPath, &TargetPath);
    if (!NT_SUCCESS(Result))
        return Result;

    Length = lstrlenW(TargetPath);
    if (Length > *PSize)
    {
        Result = STATUS_OBJECT_NAME_INVALID;
        goto exit;
    }
    *PSize = Length;
    memcpy(Buffer, TargetPath, Length * sizeof(WCHAR));

    PIoStatus->Status = STATUS_REPARSE;
    PIoStatus->Information = 0/*IO_REPARSE*/;
    Result = STATUS_REPARSE;

exit:
    if (0 != TargetPath)
        FspPosixDeletePath(TargetPath);

    return Result;
}

static NTSTATUS fsp_fuse_intf_GetReparsePoint(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode,
    PWSTR FileName, PVOID Buffer, PSIZE_T PSize)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.FileSystemControl.UserContext2;
    char PosixTargetPath[FSP_FSCTL_TRANSACT_PATH_SIZEMAX / sizeof(WCHAR)];
    PWSTR TargetPath = 0;
    ULONG Length;
    int err;
    NTSTATUS Result;

    if (0 == f->ops.readlink)
        return STATUS_INVALID_DEVICE_REQUEST;

    err = f->ops.readlink(filedesc->PosixPath, PosixTargetPath, sizeof PosixTargetPath);
    if (EINVAL/* same on MSVC and Cygwin */ == err)
    {
        Result = STATUS_NOT_A_REPARSE_POINT;
        goto exit;
    }
    else if (0 != err)
    {
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        goto exit;
    }

    /* is this an absolute path? */
    if ('/' == PosixTargetPath[0])
    {
        /* we do not support absolute paths without the rellinks option */
        if (!f->rellinks)
        {
            Result = STATUS_ACCESS_DENIED;
            goto exit;
        }

        /*
         * Transform absolute path to relative.
         */

        char *p, *t, *pstem, *tstem;
        unsigned index, count;

        p = pstem = filedesc->PosixPath;
        t = tstem = PosixTargetPath;

        /* skip common prefix */
        for (;;)
        {
            while ('/' == *p)
                p++;
            while ('/' == *t)
                t++;
            pstem = p, tstem = t;
            while ('/' != *p)
            {
                if (*p != *t)
                    goto common_prefix_end;
                else if ('\0' == *p)
                {
                    while ('/' == *t)
                        t++;
                    pstem = p, tstem = t;
                    goto common_prefix_end;
                }
                p++, t++;
            }
        }
    common_prefix_end:
        p = pstem;
        t = tstem;

        /* count path components */
        for (count = 0; '\0' != *p; count++)
        {
            while ('/' == *p)
                p++;
            while ('/' != *p && '\0' != *p)
                p++;
        }

        /* make relative path */
        if (0 == count)
        {
            /* special case symlink loop: a -> a/stem */
            while (PosixTargetPath < tstem)
            {
                tstem--;
                if ('/' != *tstem)
                    break;
            }
            while (PosixTargetPath < tstem)
            {
                tstem--;
                if ('/' == *tstem)
                {
                    tstem++;
                    break;
                }
            }
        }
        Length = lstrlenA(tstem);
        Length += !!Length; /* add tstem term-0 */
        if (3 * count + Length > sizeof PosixTargetPath)
        {
            Result = STATUS_IO_REPARSE_DATA_INVALID;
            goto exit;
        }
        memmove(PosixTargetPath + 3 * count, tstem, Length);
        for (index = 0; count > index; index++)
        {
            PosixTargetPath[index * 3 + 0] = '.';
            PosixTargetPath[index * 3 + 1] = '.';
            PosixTargetPath[index * 3 + 2] = '/';
        }
        if (0 == Length)
            PosixTargetPath[(count - 1) * 3 + 2] = '\0';
    }

    Result = FspPosixMapPosixToWindowsPath(PosixTargetPath, &TargetPath);
    if (!NT_SUCCESS(Result))
        goto exit;

    Length = lstrlenW(TargetPath);
    if (Length > *PSize)
    {
        Result = STATUS_BUFFER_TOO_SMALL;
        goto exit;
    }
    *PSize = Length;
    memcpy(Buffer, TargetPath, Length * sizeof(WCHAR));

    Result = STATUS_SUCCESS;

exit:
    if (0 != TargetPath)
        FspPosixDeletePath(TargetPath);

    return Result;
}

static NTSTATUS fsp_fuse_intf_SetReparsePoint(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PVOID FileNode,
    PWSTR FileName, PVOID Buffer, SIZE_T Size)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc =
        (PVOID)(UINT_PTR)Request->Req.FileSystemControl.UserContext2;
    WCHAR TargetPath[FSP_FSCTL_TRANSACT_PATH_SIZEMAX / sizeof(WCHAR)];
    char *PosixTargetPath = 0;
    int err;
    NTSTATUS Result;

    if (0 == f->ops.symlink)
        return STATUS_INVALID_DEVICE_REQUEST;

    /* were we asked to delete the reparse point? no can do! */
    if (0 == Buffer)
        return STATUS_ACCESS_DENIED;

    /* is this an absolute path? */
    if (Size > 4 * sizeof(WCHAR) &&
        '\\' == ((PWSTR)Buffer)[0] &&
        '?'  == ((PWSTR)Buffer)[1] &&
        '?'  == ((PWSTR)Buffer)[2] &&
        '\\' == ((PWSTR)Buffer)[3])
    {
        /* we do not support absolute paths that point outside this file system */
        if (!Request->Req.FileSystemControl.TargetOnFileSystem)
        {
            Result = STATUS_ACCESS_DENIED;
            goto exit;
        }
    }

    if (Size > sizeof TargetPath - sizeof(WCHAR))
    {
        Result = STATUS_IO_REPARSE_DATA_INVALID;
        goto exit;
    }
    memcpy(TargetPath, Buffer, Size);
    TargetPath[Size / sizeof(WCHAR)] = L'\0';

    Result = FspPosixMapWindowsToPosixPath(TargetPath, &PosixTargetPath);
    if (!NT_SUCCESS(Result))
        goto exit;

    err = f->ops.symlink(PosixTargetPath, filedesc->PosixPath);
    if (0 != err)
    {
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        goto exit;
    }

    Result = STATUS_SUCCESS;

exit:
    if (0 != PosixTargetPath)
        FspPosixDeletePath(PosixTargetPath);

    return Result;
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
    fsp_fuse_intf_SetFileSize,
    fsp_fuse_intf_CanDelete,
    fsp_fuse_intf_Rename,
    fsp_fuse_intf_GetSecurity,
    fsp_fuse_intf_SetSecurity,
    fsp_fuse_intf_ReadDirectory,
    fsp_fuse_intf_ResolveReparsePoints,
    fsp_fuse_intf_GetReparsePoint,
    fsp_fuse_intf_SetReparsePoint,
};
