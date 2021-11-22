/**
 * @file dll/fuse/fuse_intf.c
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

#include <dll/fuse/library.h>

static NTSTATUS fsp_fuse_intf_GetReparsePointByName(
    FSP_FILE_SYSTEM *FileSystem, PVOID Context,
    PWSTR FileName, BOOLEAN IsDirectory, PVOID Buffer, PSIZE_T PSize);
static NTSTATUS fsp_fuse_intf_SetEaEntry(
    FSP_FILE_SYSTEM *FileSystem, PVOID Context,
    PFILE_FULL_EA_INFORMATION SingleEa);

static inline
VOID fsp_fuse_op_enter_lock(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    switch (FileSystem->OpGuardStrategy)
    {
    case FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE:
        if ((FspFsctlTransactCreateKind == Request->Kind &&
                FILE_OPEN != ((Request->Req.Create.CreateOptions >> 24) & 0xff)) ||
            FspFsctlTransactOverwriteKind == Request->Kind ||
            (FspFsctlTransactCleanupKind == Request->Kind &&
                Request->Req.Cleanup.Delete) ||
            (FspFsctlTransactSetInformationKind == Request->Kind &&
                (10/*FileRenameInformation*/ == Request->Req.SetInformation.FileInformationClass ||
                65/*FileRenameInformationEx*/ == Request->Req.SetInformation.FileInformationClass)) ||
            FspFsctlTransactSetVolumeInformationKind == Request->Kind ||
            (FspFsctlTransactFlushBuffersKind == Request->Kind &&
                0 == Request->Req.FlushBuffers.UserContext &&
                0 == Request->Req.FlushBuffers.UserContext2) ||
            /* FSCTL_SET_REPARSE_POINT manipulates namespace */
            (FspFsctlTransactFileSystemControlKind == Request->Kind &&
                FSCTL_SET_REPARSE_POINT == Request->Req.FileSystemControl.FsControlCode))
        {
            AcquireSRWLockExclusive(&FileSystem->OpGuardLock);
        }
        else
        if (FspFsctlTransactCreateKind == Request->Kind ||
            (FspFsctlTransactSetInformationKind == Request->Kind &&
                (13/*FileDispositionInformation*/ == Request->Req.SetInformation.FileInformationClass ||
                64/*FileDispositionInformationEx*/ == Request->Req.SetInformation.FileInformationClass)) ||
            FspFsctlTransactQueryDirectoryKind == Request->Kind ||
            FspFsctlTransactQueryVolumeInformationKind == Request->Kind ||
            /* FSCTL_GET_REPARSE_POINT may access namespace */
            (FspFsctlTransactFileSystemControlKind == Request->Kind &&
                FSCTL_GET_REPARSE_POINT == Request->Req.FileSystemControl.FsControlCode))
        {
            AcquireSRWLockShared(&FileSystem->OpGuardLock);
        }
        break;

    case FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_COARSE:
        AcquireSRWLockExclusive(&FileSystem->OpGuardLock);
        break;
    }
}

static inline
VOID fsp_fuse_op_leave_unlock(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    switch (FileSystem->OpGuardStrategy)
    {
    case FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE:
        if ((FspFsctlTransactCreateKind == Request->Kind &&
                FILE_OPEN != ((Request->Req.Create.CreateOptions >> 24) & 0xff)) ||
            FspFsctlTransactOverwriteKind == Request->Kind ||
            (FspFsctlTransactCleanupKind == Request->Kind &&
                Request->Req.Cleanup.Delete) ||
            (FspFsctlTransactSetInformationKind == Request->Kind &&
                (10/*FileRenameInformation*/ == Request->Req.SetInformation.FileInformationClass ||
                65/*FileRenameInformationEx*/ == Request->Req.SetInformation.FileInformationClass)) ||
            FspFsctlTransactSetVolumeInformationKind == Request->Kind ||
            (FspFsctlTransactFlushBuffersKind == Request->Kind &&
                0 == Request->Req.FlushBuffers.UserContext &&
                0 == Request->Req.FlushBuffers.UserContext2) ||
            /* FSCTL_SET_REPARSE_POINT manipulates namespace */
            (FspFsctlTransactFileSystemControlKind == Request->Kind &&
                FSCTL_SET_REPARSE_POINT == Request->Req.FileSystemControl.FsControlCode))
        {
            ReleaseSRWLockExclusive(&FileSystem->OpGuardLock);
        }
        else
        if (FspFsctlTransactCreateKind == Request->Kind ||
            (FspFsctlTransactSetInformationKind == Request->Kind &&
                (13/*FileDispositionInformation*/ == Request->Req.SetInformation.FileInformationClass ||
                64/*FileDispositionInformationEx*/ == Request->Req.SetInformation.FileInformationClass)) ||
            FspFsctlTransactQueryDirectoryKind == Request->Kind ||
            FspFsctlTransactQueryVolumeInformationKind == Request->Kind ||
            /* FSCTL_GET_REPARSE_POINT may access namespace */
            (FspFsctlTransactFileSystemControlKind == Request->Kind &&
                FSCTL_GET_REPARSE_POINT == Request->Req.FileSystemControl.FsControlCode))
        {
            ReleaseSRWLockShared(&FileSystem->OpGuardLock);
        }
        break;

    case FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_COARSE:
        ReleaseSRWLockExclusive(&FileSystem->OpGuardLock);
        break;
    }
}

NTSTATUS fsp_fuse_op_enter(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    struct fuse *f = FileSystem->UserContext;
    struct fuse_context *context;
    struct fsp_fuse_context_header *contexthdr;
    char *PosixPath = 0;
    UINT32 Uid = -1, Gid = -1, Pid = -1;
    PWSTR FileName = 0, Suffix;
    WCHAR Root[2] = L"\\";
    UINT64 AccessToken = 0;
    NTSTATUS Result;

    if (FspFsctlTransactCreateKind == Request->Kind)
    {
        if (Request->Req.Create.OpenTargetDirectory)
            FspPathSuffix((PWSTR)Request->Buffer, &FileName, &Suffix, Root);
        else
            FileName = (PWSTR)Request->Buffer;
        AccessToken = Request->Req.Create.AccessToken;
    }
    else if (FspFsctlTransactSetInformationKind == Request->Kind &&
        (10/*FileRenameInformation*/ == Request->Req.SetInformation.FileInformationClass ||
        65/*FileRenameInformationEx*/ == Request->Req.SetInformation.FileInformationClass))
    {
        FileName = (PWSTR)(Request->Buffer + Request->Req.SetInformation.Info.Rename.NewFileName.Offset);
        AccessToken = Request->Req.SetInformation.Info.Rename.AccessToken;
    }

    if (0 != FileName)
    {
        Result = FspPosixMapWindowsToPosixPath(FileName, &PosixPath);
        if (FspFsctlTransactCreateKind == Request->Kind && Request->Req.Create.OpenTargetDirectory)
            FspPathCombine((PWSTR)Request->Buffer, Suffix);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    if (0 != AccessToken)
    {
        Result = fsp_fuse_get_token_uidgid(
            FSP_FSCTL_TRANSACT_REQ_TOKEN_HANDLE(AccessToken),
            TokenUser,
            &Uid, &Gid);
        if (!NT_SUCCESS(Result))
            goto exit;

        Pid = FSP_FSCTL_TRANSACT_REQ_TOKEN_PID(AccessToken);
    }

    context = fsp_fuse_get_context(f->env);
    if (0 == context)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    fsp_fuse_op_enter_lock(FileSystem, Request, Response);

    context->fuse = f;
    context->private_data = f->data;
    context->uid = Uid;
    context->gid = Gid;
    context->pid = 0 != f->env->winpid_to_pid ? f->env->winpid_to_pid(Pid) : Pid;

    contexthdr = FSP_FUSE_HDR_FROM_CONTEXT(context);
    contexthdr->PosixPath = PosixPath;

    Result = STATUS_SUCCESS;

exit:
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

    fsp_fuse_op_leave_unlock(FileSystem, Request, Response);

    context = fsp_fuse_get_context(f->env);
    context->fuse = 0;
    context->private_data = 0;
    context->uid = -1;
    context->gid = -1;
    context->pid = -1;

    contexthdr = FSP_FUSE_HDR_FROM_CONTEXT(context);
    if (0 != contexthdr->PosixPath)
        FspPosixDeletePath(contexthdr->PosixPath);
    memset(contexthdr, 0, sizeof *contexthdr);

    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_intf_NewHiddenName(FSP_FILE_SYSTEM *FileSystem,
    char *PosixPath, char **PPosixHiddenPath)
{
    struct fuse *f = FileSystem->UserContext;
    NTSTATUS Result;
    char *PosixHiddenPath = 0;
    char *p, *lastp;
    size_t Size;
    RPC_STATUS RpcStatus;
    union
    {
        struct { UINT32 V[4]; } Values;
        UUID Uuid;
    } UuidBuf;
    struct fuse_stat_ex stbuf;
    int err, maxtries = 3;

    *PPosixHiddenPath = 0;

    p = PosixPath;
    for (;;)
    {
        while ('/' == *p)
            p++;
        lastp = p;
        while ('/' != *p)
        {
            if ('\0' == *p)
                goto loopend;
            p++;
        }
    }
loopend:;

    Size = lastp - PosixPath + sizeof ".fuse_hidden0123456789abcdef";
    PosixHiddenPath = MemAlloc(Size);
    if (0 == PosixHiddenPath)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    memcpy(PosixHiddenPath, PosixPath, lastp - PosixPath);

    do
    {
        RpcStatus = UuidCreate(&UuidBuf.Uuid);
        if (S_OK != RpcStatus && RPC_S_UUID_LOCAL_ONLY != RpcStatus)
        {
            Result = STATUS_INTERNAL_ERROR;
            goto exit;
        }

        wsprintfA(PosixHiddenPath + (lastp - PosixPath),
            ".fuse_hidden%08lx%08lx",
            UuidBuf.Values.V[0] ^ UuidBuf.Values.V[2],
            UuidBuf.Values.V[1] ^ UuidBuf.Values.V[3]);

        memset(&stbuf, 0, sizeof stbuf);
        if (0 != f->ops.getattr)
            err = f->ops.getattr(PosixHiddenPath, (void *)&stbuf);
        else
            err = -ENOSYS_(f->env);
    } while (0 == err && 0 < --maxtries);

    if (0 == err)
    {
        Result = STATUS_DEVICE_BUSY; // current EBUSY mapping
        goto exit;
    }

    *PPosixHiddenPath = PosixHiddenPath;
    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
        MemFree(PosixHiddenPath);

    return Result;
}

static BOOLEAN fsp_fuse_intf_CheckSymlinkDirectory(FSP_FILE_SYSTEM *FileSystem,
    const char *PosixPath)
{
    struct fuse *f = FileSystem->UserContext;

    if (FSP_FUSE_HAS_SLASHDOT(f))
    {
        char *PosixDotPath = 0;
        size_t Length;
        struct fuse_stat_ex stbuf;
        int err;
        BOOLEAN Result = FALSE;

        Length = lstrlenA(PosixPath);
        PosixDotPath = MemAlloc(Length + 3);
        if (0 != PosixDotPath)
        {
            memcpy(PosixDotPath, PosixPath, Length);
            PosixDotPath[Length + 0] = '/';
            PosixDotPath[Length + 1] = '.';
            PosixDotPath[Length + 2] = '\0';

            memset(&stbuf, 0, sizeof stbuf);
            if (0 != f->ops.getattr)
                err = f->ops.getattr(PosixDotPath, (void *)&stbuf);
            else
                err = -ENOSYS_(f->env);

            MemFree(PosixDotPath);

            Result = 0 == err && 0040000 == (stbuf.st_mode & 0170000);
        }

        return Result;
    }
    else
    {
        PWSTR WindowsPath = 0, P;
        char *PosixResolvedPath = 0;
        UINT32 ReparsePointIndex;
        UINT32 ResolveFileAttributes[2] = { FILE_ATTRIBUTE_REPARSE_POINT, -1 };
        IO_STATUS_BLOCK IoStatus;
        union
        {
            REPARSE_DATA_BUFFER V;
            UINT8 B[FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) +
                FSP_FSCTL_TRANSACT_PATH_SIZEMAX + sizeof(WCHAR)/* add space for term-0 */];
        } ReparseDataBuf;
        SIZE_T ReparseDataSize;
        struct fuse_stat_ex stbuf;
        int err;
        NTSTATUS Result;

        Result = FspPosixMapPosixToWindowsPath(PosixPath, &WindowsPath);
        if (!NT_SUCCESS(Result))
            goto exit;

        ReparsePointIndex = 0;
        for (P = WindowsPath; '\0' != *P; P++)
            if (L'\\' == *P)
                ReparsePointIndex = (UINT32)(P + 1 - WindowsPath);

        ReparseDataSize = sizeof ReparseDataBuf - sizeof(WCHAR)/* leave space for term-0 */;
        Result = FspFileSystemResolveReparsePoints(FileSystem,
            fsp_fuse_intf_GetReparsePointByName, ResolveFileAttributes,
            WindowsPath, ReparsePointIndex, TRUE,
            &IoStatus, &ReparseDataBuf,
            &ReparseDataSize);
        if (!NT_SUCCESS(Result))
            goto exit;

        if (IO_REPARSE_TAG_SYMLINK != ReparseDataBuf.V.ReparseTag)
        {
            Result = STATUS_UNSUCCESSFUL;
            goto exit;
        }

        if (-1 != ResolveFileAttributes[1])
        {
            Result = (FILE_ATTRIBUTE_DIRECTORY & ResolveFileAttributes[1]) ?
                STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
            goto exit;
        }

        P = (PWSTR)(ReparseDataBuf.V.SymbolicLinkReparseBuffer.PathBuffer +
            ReparseDataBuf.V.SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR));
        P[ReparseDataBuf.V.SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(WCHAR)] = L'\0';

        Result = FspPosixMapWindowsToPosixPath(P, &PosixResolvedPath);
        if (!NT_SUCCESS(Result))
            goto exit;

        memset(&stbuf, 0, sizeof stbuf);
        if (0 != f->ops.getattr)
            err = f->ops.getattr(PosixResolvedPath, (void *)&stbuf);
        else
            err = -ENOSYS_(f->env);

        Result = 0 == err && 0040000 == (stbuf.st_mode & 0170000) ?
            STATUS_SUCCESS : STATUS_UNSUCCESSFUL;

    exit:
        if (0 != PosixResolvedPath)
            FspPosixDeletePath(PosixResolvedPath);
        if (0 != WindowsPath)
            FspPosixDeletePath(WindowsPath);

        return NT_SUCCESS(Result);
    }
}

static inline uint32_t fsp_fuse_intf_MapFileAttributesToFlags(UINT32 FileAttributes)
{
    uint32_t flags = 0;

    if (FileAttributes & FILE_ATTRIBUTE_READONLY)
        flags |= FSP_FUSE_UF_READONLY;
    if (FileAttributes & FILE_ATTRIBUTE_HIDDEN)
        flags |= FSP_FUSE_UF_HIDDEN;
    if (FileAttributes & FILE_ATTRIBUTE_SYSTEM)
        flags |= FSP_FUSE_UF_SYSTEM;
    if (FileAttributes & FILE_ATTRIBUTE_ARCHIVE)
        flags |= FSP_FUSE_UF_ARCHIVE;

    return flags;
}

static inline UINT32 fsp_fuse_intf_MapFlagsToFileAttributes(uint32_t flags)
{
    UINT32 FileAttributes = 0;

    if (flags & FSP_FUSE_UF_READONLY)
        FileAttributes |= FILE_ATTRIBUTE_READONLY;
    if (flags & FSP_FUSE_UF_HIDDEN)
        FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
    if (flags & FSP_FUSE_UF_SYSTEM)
        FileAttributes |= FILE_ATTRIBUTE_SYSTEM;
    if (flags & FSP_FUSE_UF_ARCHIVE)
        FileAttributes |= FILE_ATTRIBUTE_ARCHIVE;

    return FileAttributes;
}

#define FUSE_FILE_INFO(IsDirectory, fi) ((IsDirectory) ? 0 : (fi))
#define fsp_fuse_intf_GetFileInfoEx(FileSystem, PosixPath, fi, PUid, PGid, PMode, FileInfo)\
    fsp_fuse_intf_GetFileInfoFunnel(FileSystem, PosixPath, fi, 0, PUid, PGid, PMode, 0, TRUE, FileInfo)
static NTSTATUS fsp_fuse_intf_GetFileInfoFunnel(FSP_FILE_SYSTEM *FileSystem,
    const char *PosixPath, struct fuse_file_info *fi, const void *stbufp,
    PUINT32 PUid, PUINT32 PGid, PUINT32 PMode, PUINT32 PDev,
    BOOLEAN CheckSymlinkDirectory,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    struct fuse *f = FileSystem->UserContext;
    UINT64 AllocationUnit;
    struct fuse_stat_ex stbuf;
    BOOLEAN StatEx = 0 != (f->conn_want & FSP_FUSE_CAP_STAT_EX);

    memset(&stbuf, 0, sizeof stbuf);
    if (0 != stbufp)
        memcpy(&stbuf, stbufp, StatEx ? sizeof(struct fuse_stat_ex) : sizeof(struct fuse_stat));
    else
    {
        int err;

        if (0 != f->ops.fgetattr && 0 != fi && -1 != fi->fh)
            err = f->ops.fgetattr(PosixPath, (void *)&stbuf, fi);
        else if (0 != f->ops.getattr)
            err = f->ops.getattr(PosixPath, (void *)&stbuf);
        else
            return STATUS_INVALID_DEVICE_REQUEST;

        if (0 != err)
            return fsp_fuse_ntstatus_from_errno(f->env, err);
    }

    if (f->set_umask)
        stbuf.st_mode = (stbuf.st_mode & 0170000) | (0777 & ~f->umask);
    if (f->set_uid)
        stbuf.st_uid = f->uid;
    if (f->set_gid)
        stbuf.st_gid = f->gid;

    *PUid = stbuf.st_uid;
    *PGid = stbuf.st_gid;
    *PMode = stbuf.st_mode;
    if (0 != PDev)
        *PDev = stbuf.st_rdev;

    AllocationUnit = (UINT64)f->VolumeParams.SectorSize *
        (UINT64)f->VolumeParams.SectorsPerAllocationUnit;
    switch (stbuf.st_mode & 0170000)
    {
    case 0040000: /* S_IFDIR */
        FileInfo->FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        FileInfo->ReparseTag = 0;
        break;
    case 0010000: /* S_IFIFO */
    case 0020000: /* S_IFCHR */
    case 0060000: /* S_IFBLK */
    case 0140000: /* S_IFSOCK */
        FileInfo->FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
        FileInfo->ReparseTag = IO_REPARSE_TAG_NFS;
        break;
    case 0120000: /* S_IFLNK */
        if (FSP_FUSE_HAS_SYMLINKS(f))
        {
            FileInfo->FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
            FileInfo->ReparseTag = IO_REPARSE_TAG_SYMLINK;
            if (CheckSymlinkDirectory && fsp_fuse_intf_CheckSymlinkDirectory(FileSystem, PosixPath))
                FileInfo->FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
            break;
        }
        /* fall through */
    default:
        FileInfo->FileAttributes = 0;
        FileInfo->ReparseTag = 0;
        break;
    }
    if (StatEx)
        FileInfo->FileAttributes |= fsp_fuse_intf_MapFlagsToFileAttributes(stbuf.st_flags);
    if (f->dothidden)
    {
        const char *basename = PosixPath;
        for (const char *p = PosixPath; '\0' != *p; p++)
            if ('/' == *p)
                basename = p + 1;
        if ('.' == basename[0])
            FileInfo->FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
    }
    FileInfo->FileSize = stbuf.st_size;
    FileInfo->AllocationSize =
        (FileInfo->FileSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit;
    FspPosixUnixTimeToFileTime((void *)&stbuf.st_birthtim, &FileInfo->CreationTime);
    FspPosixUnixTimeToFileTime((void *)&stbuf.st_atim, &FileInfo->LastAccessTime);
    FspPosixUnixTimeToFileTime((void *)&stbuf.st_mtim, &FileInfo->LastWriteTime);
    FspPosixUnixTimeToFileTime((void *)&stbuf.st_ctim, &FileInfo->ChangeTime);
    FileInfo->IndexNumber = stbuf.st_ino;

    FileInfo->HardLinks = 0;
    FileInfo->EaSize = 0;

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
        Result = FspPosixMergePermissionsToSecurityDescriptor(Uid, Gid, Mode, f->FileSecurity,
            &SecurityDescriptor);
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
            FspPosixMergePermissionsToSecurityDescriptor);

    return Result;
}

static NTSTATUS fsp_fuse_intf_GetReparsePointSymlink(FSP_FILE_SYSTEM *FileSystem,
    const char *PosixPath, PVOID Buffer, PSIZE_T PSize)
{
    struct fuse *f = FileSystem->UserContext;
    char PosixTargetPath[FSP_FSCTL_TRANSACT_PATH_SIZEMAX / sizeof(WCHAR)];
    PWSTR TargetPath = 0;
    ULONG TargetPathLength;
    int err;
    NTSTATUS Result;

    err = f->ops.readlink(PosixPath, PosixTargetPath, sizeof PosixTargetPath);
    if (-EINVAL/* same on MSVC and Cygwin */ == err)
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
    }

    Result = FspPosixMapPosixToWindowsPath(PosixTargetPath, &TargetPath);
    if (!NT_SUCCESS(Result))
        goto exit;

    TargetPathLength = lstrlenW(TargetPath) * sizeof(WCHAR);
    if (TargetPathLength > *PSize)
    {
        Result = STATUS_BUFFER_TOO_SMALL;
        goto exit;
    }
    *PSize = TargetPathLength;
    memcpy(Buffer, TargetPath, TargetPathLength);

    Result = STATUS_SUCCESS;

exit:
    if (0 != TargetPath)
        FspPosixDeletePath(TargetPath);

    return Result;
}

static NTSTATUS fsp_fuse_intf_GetReparsePointEx(FSP_FILE_SYSTEM *FileSystem,
    const char *PosixPath, struct fuse_file_info *fi,
    PVOID Buffer, PSIZE_T PSize, PUINT32 PResolveFileAttributes)
{
    struct fuse *f = FileSystem->UserContext;
    UINT32 Uid, Gid, Mode, Dev;
    FSP_FSCTL_FILE_INFO FileInfo;
    PREPARSE_DATA_BUFFER ReparseData;
    USHORT ReparseDataLength;
    SIZE_T Size;
    NTSTATUS Result;

    if (0 != PResolveFileAttributes && FILE_ATTRIBUTE_REPARSE_POINT == PResolveFileAttributes[0])
    {
        Mode = 0120000;
        memset(&FileInfo, 0, sizeof FileInfo);
        FileInfo.FileAttributes = PResolveFileAttributes[0];
        FileInfo.ReparseTag = IO_REPARSE_TAG_SYMLINK;
        PResolveFileAttributes[0] = 0;
        goto skip_getattr;
    }

    Result = fsp_fuse_intf_GetFileInfoFunnel(FileSystem, PosixPath, fi, 0,
        &Uid, &Gid, &Mode, &Dev, FALSE, &FileInfo);
    if (!NT_SUCCESS(Result))
        return Result;

    if (0 == (FILE_ATTRIBUTE_REPARSE_POINT & FileInfo.FileAttributes))
    {
        if (0 != PResolveFileAttributes)
            PResolveFileAttributes[1] = FileInfo.FileAttributes;
        return STATUS_NOT_A_REPARSE_POINT;
    }

skip_getattr:
    if (0 == Buffer)
        return STATUS_SUCCESS;

    switch (Mode & 0170000)
    {
    case 0010000: /* S_IFIFO */
        ReparseDataLength = (USHORT)(
            FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer) -
            FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer) +
            8);
        break;

    case 0020000: /* S_IFCHR */
        ReparseDataLength = (USHORT)(
            FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer) -
            FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer) +
            16);
        break;

    case 0060000: /* S_IFBLK */
        ReparseDataLength = (USHORT)(
            FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer) -
            FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer) +
            16);
        break;

    case 0140000: /* S_IFSOCK */
        ReparseDataLength = (USHORT)(
            FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer) -
            FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer) +
            8);
        break;

    case 0120000: /* S_IFLNK */
        ReparseDataLength = (USHORT)(
            FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) -
            FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer));
        break;

    default:
        /* cannot happen! */
        return STATUS_NOT_A_REPARSE_POINT;
    }

    if ((SIZE_T)FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer) + ReparseDataLength > *PSize)
        return STATUS_BUFFER_TOO_SMALL;

    ReparseData = (PREPARSE_DATA_BUFFER)Buffer;
    ReparseData->ReparseTag = FileInfo.ReparseTag;
    ReparseData->ReparseDataLength = ReparseDataLength;

    switch (Mode & 0170000)
    {
    case 0010000: /* S_IFIFO */
        *(PUINT64)(ReparseData->GenericReparseBuffer.DataBuffer +  0) = NFS_SPECFILE_FIFO;
        break;

    case 0020000: /* S_IFCHR */
        *(PUINT64)(ReparseData->GenericReparseBuffer.DataBuffer +  0) = NFS_SPECFILE_CHR;
        *(PUINT32)(ReparseData->GenericReparseBuffer.DataBuffer +  8) = (Dev >> 16) & 0xffff;
        *(PUINT32)(ReparseData->GenericReparseBuffer.DataBuffer + 12) = Dev & 0xffff;
        break;

    case 0060000: /* S_IFBLK */
        *(PUINT64)(ReparseData->GenericReparseBuffer.DataBuffer +  0) = NFS_SPECFILE_BLK;
        *(PUINT32)(ReparseData->GenericReparseBuffer.DataBuffer +  8) = (Dev >> 16) & 0xffff;
        *(PUINT32)(ReparseData->GenericReparseBuffer.DataBuffer + 12) = Dev & 0xffff;
        break;

    case 0140000: /* S_IFSOCK */
        *(PUINT64)(ReparseData->GenericReparseBuffer.DataBuffer +  0) = NFS_SPECFILE_SOCK;
        break;

    case 0120000: /* S_IFLNK */
        Size = *PSize -
            FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer);
        Result = fsp_fuse_intf_GetReparsePointSymlink(FileSystem, PosixPath,
            ReparseData->SymbolicLinkReparseBuffer.PathBuffer, &Size);
        if (!NT_SUCCESS(Result))
            return Result;

        ReparseData->ReparseDataLength += (USHORT)Size;
        ReparseData->SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;
        ReparseData->SymbolicLinkReparseBuffer.SubstituteNameLength = (USHORT)Size;
        ReparseData->SymbolicLinkReparseBuffer.PrintNameOffset = 0;
        ReparseData->SymbolicLinkReparseBuffer.PrintNameLength = (USHORT)Size;
        ReparseData->SymbolicLinkReparseBuffer.Flags = SYMLINK_FLAG_RELATIVE;
        break;

    default:
        /* cannot happen! */
        return STATUS_NOT_A_REPARSE_POINT;
    }

    *PSize = FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer) + ReparseData->ReparseDataLength;
    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_intf_GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    struct fuse *f = FileSystem->UserContext;
    struct fuse_statvfs stbuf;
    int err;

    memset(&stbuf, 0, sizeof stbuf);
    if (0 != f->ops.statfs)
    {
        err = f->ops.statfs("/", &stbuf);
        if (0 != err)
            return fsp_fuse_ntstatus_from_errno(f->env, err);
    }

    VolumeInfo->TotalSize = (UINT64)stbuf.f_blocks * (UINT64)stbuf.f_frsize;
    VolumeInfo->FreeSize = (UINT64)stbuf.f_bfree * (UINT64)stbuf.f_frsize;
    VolumeInfo->VolumeLabelLength = f->VolumeLabelLength;
    memcpy(&VolumeInfo->VolumeLabel, &f->VolumeLabel, f->VolumeLabelLength);

    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_intf_SetVolumeLabel(FSP_FILE_SYSTEM *FileSystem,
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
    char *PosixPath = 0;
    NTSTATUS Result;

    Result = FspPosixMapWindowsToPosixPath(FileName, &PosixPath);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = fsp_fuse_intf_GetSecurityEx(FileSystem, PosixPath, 0,
        PFileAttributes, SecurityDescriptorBuf, PSecurityDescriptorSize);
    if (!NT_SUCCESS(Result) &&
        STATUS_OBJECT_NAME_NOT_FOUND != Result &&
        STATUS_OBJECT_PATH_NOT_FOUND != Result)
        goto exit;

    if (FSP_FUSE_HAS_SYMLINKS(f) &&
        FspFileSystemFindReparsePoint(FileSystem, fsp_fuse_intf_GetReparsePointByName, 0,
            FileName, PFileAttributes))
        Result = STATUS_REPARSE;
    else if (NT_SUCCESS(Result))
        Result = STATUS_SUCCESS;

exit:
    if (0 != PosixPath)
        FspPosixDeletePath(PosixPath);

    return Result;
}

static NTSTATUS fsp_fuse_intf_Create(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
    UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
    PVOID ExtraBuffer, ULONG ExtraLength, BOOLEAN ExtraBufferIsReparsePoint,
    PVOID *PFileDesc, FSP_FSCTL_FILE_INFO *FileInfo)
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

    if (0 != ExtraBuffer)
    {
        if (!ExtraBufferIsReparsePoint)
        {
            if (0 == f->ops.listxattr || 0 == f->ops.getxattr ||
                0 == f->ops.setxattr || 0 == f->ops.removexattr)
            {
                Result = STATUS_EAS_NOT_SUPPORTED;
                goto exit;
            }
        }
        else
        {
            /* !!!: revisit */
            Result = STATUS_INVALID_PARAMETER;
            goto exit;
        }
    }

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
    if (CreateOptions & FILE_DIRECTORY_FILE)
    {
        if (f->set_create_dir_umask)
            Mode = 0777 & ~f->create_dir_umask;
        else
        if (f->set_create_umask)
            Mode = 0777 & ~f->create_umask;
    }
    else
    {
        if (f->set_create_file_umask)
            Mode = 0777 & ~f->create_file_umask;
        else
        if (f->set_create_umask)
            Mode = 0777 & ~f->create_umask;
    }

    memset(&fi, 0, sizeof fi);
    if ('C' == f->env->environment) /* Cygwin */
        fi.flags = 0x0200 | 0x0800 | 2 /*O_CREAT|O_EXCL|O_RDWR*/;
    else
        fi.flags = 0x0100 | 0x0400 | 2 /*O_CREAT|O_EXCL|O_RDWR*/;

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

    if (0 != FileAttributes &&
        0 != (f->conn_want & FSP_FUSE_CAP_STAT_EX) && 0 != f->ops.chflags)
    {
        err = f->ops.chflags(contexthdr->PosixPath,
            fsp_fuse_intf_MapFileAttributesToFlags(CreateOptions & FILE_DIRECTORY_FILE ?
                FileAttributes : FileAttributes | FILE_ATTRIBUTE_ARCHIVE));
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        if (!NT_SUCCESS(Result) && STATUS_INVALID_DEVICE_REQUEST != Result)
            goto exit;
    }

    if ((Uid != context->uid || Gid != context->gid) &&
        0 != f->ops.chown)
    {
        err = f->ops.chown(contexthdr->PosixPath, Uid, Gid);
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        if (!NT_SUCCESS(Result) && STATUS_INVALID_DEVICE_REQUEST != Result)
            goto exit;
    }

    if (0 != ExtraBuffer)
    {
        if (!ExtraBufferIsReparsePoint)
        {
            Result = FspFileSystemEnumerateEa(FileSystem,
                fsp_fuse_intf_SetEaEntry, contexthdr->PosixPath, ExtraBuffer, ExtraLength);
            if (!NT_SUCCESS(Result))
                goto exit;
        }
        else
        {
            /* !!!: revisit: WslFeatures, GetFileInfoFunnel, GetReparsePointEx, SetReparsePoint */
            Result = STATUS_INVALID_PARAMETER;
            goto exit;
        }
    }
    /*
     * Ignore fuse_file_info::direct_io, fuse_file_info::keep_cache.
     * NOTE: Originally WinFsp dit not support disabling the cache manager
     * for an individual file. This is now possible and we should revisit.
     *
     * Ignore fuse_file_info::nonseekable.
     */

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, contexthdr->PosixPath,
        FUSE_FILE_INFO(CreateOptions & FILE_DIRECTORY_FILE, &fi),
        &Uid, &Gid, &Mode, &FileInfoBuf);
    if (!NT_SUCCESS(Result))
        goto exit;

    *PFileDesc = filedesc;
    memcpy(FileInfo, &FileInfoBuf, sizeof FileInfoBuf);

    filedesc->PosixPath = contexthdr->PosixPath;
    filedesc->IsDirectory = !!(FileInfoBuf.FileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    filedesc->IsReparsePoint = FALSE;
    filedesc->OpenFlags = fi.flags;
    filedesc->FileHandle = fi.fh;
    filedesc->DirBuffer = 0;
    contexthdr->PosixPath = 0;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        if (Opened)
        {
            if (CreateOptions & FILE_DIRECTORY_FILE)
            {
                if (0 != f->ops.releasedir)
                    f->ops.releasedir(contexthdr->PosixPath, &fi);
            }
            else
            {
                if (0 != f->ops.release)
                    f->ops.release(contexthdr->PosixPath, &fi);
            }
        }

        MemFree(filedesc);
    }

    return Result;
}

static NTSTATUS fsp_fuse_intf_Open(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
    PVOID *PFileDesc, FSP_FSCTL_FILE_INFO *FileInfo)
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

    if (0 != (CreateOptions & FILE_DELETE_ON_CLOSE) &&
        0 != (f->conn_want & FSP_FUSE_CAP_DELETE_ACCESS) && 0 != f->ops.access)
    {
        err = f->ops.access(contexthdr->PosixPath, FSP_FUSE_DELETE_OK);
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        if (!NT_SUCCESS(Result) && STATUS_INVALID_DEVICE_REQUEST != Result)
        {
            if (STATUS_ACCESS_DENIED == Result)
                Result = STATUS_CANNOT_DELETE;
            goto exit;
        }
    }

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
    switch (GrantedAccess & (FILE_READ_DATA | FILE_WRITE_DATA))
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

    if (FileInfoBuf.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
    {
        fi.fh = -1;
        Result = STATUS_SUCCESS;
    }
    else if (FileInfoBuf.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
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
        /*
         * Some Windows applications (notably Go programs) specify FILE_APPEND_DATA without
         * FILE_WRITE_DATA when opening files for appending. This caused the WinFsp-FUSE layer
         * to erroneously pass O_RDONLY to the FUSE file system in such cases. We add a test
         * for FILE_APPEND_DATA to ensure that either O_WRONLY or O_RDWR is specified.
         */
        if (GrantedAccess & FILE_APPEND_DATA)
        {
            if (fi.flags == 0)
                fi.flags = 1; /* need O_WRONLY as a bare minimum in order to append */
        }

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

    *PFileDesc = filedesc;
    memcpy(FileInfo, &FileInfoBuf, sizeof FileInfoBuf);

    filedesc->PosixPath = contexthdr->PosixPath;
    filedesc->IsDirectory = !!(FileInfoBuf.FileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    filedesc->IsReparsePoint = !!(FileInfoBuf.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
    filedesc->OpenFlags = fi.flags;
    filedesc->FileHandle = fi.fh;
    filedesc->DirBuffer = 0;
    contexthdr->PosixPath = 0;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
        MemFree(filedesc);

    return Result;
}

static NTSTATUS fsp_fuse_intf_Overwrite(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize,
    PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    UINT32 Uid, Gid, Mode;
    struct fuse_file_info fi;
    int err;
    NTSTATUS Result;

    if (filedesc->IsDirectory || filedesc->IsReparsePoint)
        return STATUS_ACCESS_DENIED;

    if (0 != Ea)
    {
        char names[3 * 1024];
        int namesize;

        if (0 == f->ops.listxattr || 0 == f->ops.getxattr ||
            0 == f->ops.setxattr || 0 == f->ops.removexattr)
            return STATUS_EAS_NOT_SUPPORTED;

        namesize = f->ops.listxattr(filedesc->PosixPath, names, sizeof names);
        if (0 < namesize)
            for (char *p = names, *endp = p + namesize; endp > p; p += namesize)
            {
                namesize = lstrlenA(p) + 1;
                f->ops.removexattr(filedesc->PosixPath, p);
            }

        Result = FspFileSystemEnumerateEa(FileSystem,
            fsp_fuse_intf_SetEaEntry, filedesc->PosixPath, Ea, EaLength);
        if (!NT_SUCCESS(Result))
            return Result;
    }

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

    if (0 != FileAttributes &&
        0 != (f->conn_want & FSP_FUSE_CAP_STAT_EX) && 0 != f->ops.chflags)
    {
        /*
         * The code below is not strictly correct. File attributes should be
         * replaced when ReplaceFileAttributes is TRUE and merged (or'ed) when
         * ReplaceFileAttributes is FALSE. I am punting on this detail for now.
         */

        err = f->ops.chflags(filedesc->PosixPath,
            fsp_fuse_intf_MapFileAttributesToFlags(FileAttributes | FILE_ATTRIBUTE_ARCHIVE));
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        if (!NT_SUCCESS(Result) && STATUS_INVALID_DEVICE_REQUEST != Result)
            return Result;
    }

    return fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath,
        FUSE_FILE_INFO(filedesc->IsDirectory, &fi),
        &Uid, &Gid, &Mode, FileInfo);
}

static VOID fsp_fuse_intf_Cleanup(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc, PWSTR FileName, ULONG Flags)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;

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
     *
     *
     * NOTE:
     *
     * Since WinFsp 2022 Beta4 (v1.10B4) it is possible to handle handles open other than ours
     * because of the new POSIX unlink semantics. Although we still do not provide the hard_remove
     * option, file systems that would need the hard_remove option can instead use the
     * LegacyUnlinkRename option to opt out of the POSIX unlink semantics.
     */

    if (Flags & FspCleanupDelete)
        if (filedesc->IsDirectory && !filedesc->IsReparsePoint)
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
    PVOID FileDesc)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    struct fuse_file_info fi;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    if (filedesc->IsReparsePoint)
    {
        /* reparse points are not opened, nothing to do! */
    }
    else if (filedesc->IsDirectory)
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

    FspFileSystemDeleteDirectoryBuffer(&filedesc->DirBuffer);
    MemFree(filedesc->PosixPath);
    MemFree(filedesc);
}

static NTSTATUS fsp_fuse_intf_Read(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc, PVOID Buffer, UINT64 Offset, ULONG Length,
    PULONG PBytesTransferred)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    struct fuse_file_info fi;
    int bytes;
    NTSTATUS Result;

    if (filedesc->IsDirectory || filedesc->IsReparsePoint)
        return STATUS_ACCESS_DENIED;

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
    PVOID FileDesc, PVOID Buffer, UINT64 Offset, ULONG Length,
    BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
    PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    UINT32 Uid, Gid, Mode;
    struct fuse_file_info fi;
    FSP_FSCTL_FILE_INFO FileInfoBuf;
    UINT64 EndOffset, AllocationUnit;
    int bytes;
    NTSTATUS Result;

    if (filedesc->IsDirectory || filedesc->IsReparsePoint)
        return STATUS_ACCESS_DENIED;

    if (0 == f->ops.write)
        return STATUS_INVALID_DEVICE_REQUEST;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath,
        FUSE_FILE_INFO(filedesc->IsDirectory, &fi),
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
    if (Offset + bytes > FileInfoBuf.FileSize)
        FileInfoBuf.FileSize = Offset + bytes;
    FileInfoBuf.AllocationSize =
        (FileInfoBuf.FileSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit;

success:
    memcpy(FileInfo, &FileInfoBuf, sizeof FileInfoBuf);

    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_intf_Flush(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    UINT32 Uid, Gid, Mode;
    struct fuse_file_info fi;
    FSP_FSCTL_FILE_INFO FileInfoBuf;
    int err;
    NTSTATUS Result;

    if (0 == filedesc)
        return STATUS_SUCCESS; /* FUSE cannot flush volumes */

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    Result = STATUS_SUCCESS; /* just say success, if fs does not support fsync */
    if (filedesc->IsReparsePoint)
        Result = STATUS_ACCESS_DENIED;
    else if (filedesc->IsDirectory)
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
    if (!NT_SUCCESS(Result))
        return Result;

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath,
        FUSE_FILE_INFO(filedesc->IsDirectory, &fi),
        &Uid, &Gid, &Mode, &FileInfoBuf);
    if (!NT_SUCCESS(Result))
        return Result;

    memcpy(FileInfo, &FileInfoBuf, sizeof FileInfoBuf);

    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_intf_GetFileInfo(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    UINT32 Uid, Gid, Mode;
    struct fuse_file_info fi;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    return fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath,
        FUSE_FILE_INFO(filedesc->IsDirectory, &fi),
        &Uid, &Gid, &Mode, FileInfo);
}

static NTSTATUS fsp_fuse_intf_SetBasicInfo(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc, UINT32 FileAttributes,
    UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    UINT32 Uid, Gid, Mode;
    struct fuse_file_info fi;
    FSP_FSCTL_FILE_INFO FileInfoBuf;
    struct fuse_timespec tv[2];
    struct fuse_utimbuf timbuf;
    int err;
    NTSTATUS Result;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    if (INVALID_FILE_ATTRIBUTES != FileAttributes &&
        0 != (f->conn_want & FSP_FUSE_CAP_STAT_EX) && 0 != f->ops.chflags)
    {
        err = f->ops.chflags(filedesc->PosixPath,
            fsp_fuse_intf_MapFileAttributesToFlags(FileAttributes));
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    if ((0 != LastAccessTime || 0 != LastWriteTime) &&
        (0 != f->ops.utimens || 0 != f->ops.utime))
    {
        if (0 == LastAccessTime || 0 == LastWriteTime)
        {
            Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath,
                FUSE_FILE_INFO(filedesc->IsDirectory, &fi),
                &Uid, &Gid, &Mode, &FileInfoBuf);
            if (!NT_SUCCESS(Result))
                return Result;

            if (0 == LastAccessTime)
                LastAccessTime = FileInfoBuf.LastAccessTime;
            if (0 == LastWriteTime)
                LastWriteTime = FileInfoBuf.LastWriteTime;
        }

        FspPosixFileTimeToUnixTime(LastAccessTime, (void *)&tv[0]);
        FspPosixFileTimeToUnixTime(LastWriteTime, (void *)&tv[1]);
        if (0 != f->ops.utimens)
        {
            err = f->ops.utimens(filedesc->PosixPath, tv);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
        else
        {
            timbuf.actime = tv[0].tv_sec;
            timbuf.modtime = tv[1].tv_sec;
            err = f->ops.utime(filedesc->PosixPath, &timbuf);
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        }
        if (!NT_SUCCESS(Result))
            return Result;
    }

    if (0 != CreationTime && 0 != f->ops.setcrtime)
    {
        FspPosixFileTimeToUnixTime(CreationTime, (void *)&tv[0]);
        err = f->ops.setcrtime(filedesc->PosixPath, &tv[0]);
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    if (0 != ChangeTime && 0 != f->ops.setchgtime)
    {
        FspPosixFileTimeToUnixTime(ChangeTime, (void *)&tv[0]);
        err = f->ops.setchgtime(filedesc->PosixPath, &tv[0]);
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    return fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath,
        FUSE_FILE_INFO(filedesc->IsDirectory, &fi),
        &Uid, &Gid, &Mode, FileInfo);
}

static NTSTATUS fsp_fuse_intf_SetFileSize(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc, UINT64 NewSize, BOOLEAN SetAllocationSize,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    UINT32 Uid, Gid, Mode;
    struct fuse_file_info fi;
    FSP_FSCTL_FILE_INFO FileInfoBuf;
    UINT64 AllocationUnit;
    int err;
    NTSTATUS Result;

    if (filedesc->IsDirectory || filedesc->IsReparsePoint)
        return STATUS_ACCESS_DENIED;

    if (0 == f->ops.ftruncate && 0 == f->ops.truncate)
        return STATUS_INVALID_DEVICE_REQUEST;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath,
        FUSE_FILE_INFO(filedesc->IsDirectory, &fi),
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

/* !static: used by fuse2to3 */
int fsp_fuse_intf_CanDeleteAddDirInfo(void *buf, const char *name,
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
    PVOID FileDesc, PWSTR FileName)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    struct fuse_file_info fi;
    struct fuse_dirhandle dh;
    int err;

    if (0 != (f->conn_want & FSP_FUSE_CAP_DELETE_ACCESS) && 0 != f->ops.access)
    {
        NTSTATUS Result;
        err = f->ops.access(filedesc->PosixPath, FSP_FUSE_DELETE_OK);
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        if (!NT_SUCCESS(Result) && STATUS_INVALID_DEVICE_REQUEST != Result)
        {
            if (STATUS_ACCESS_DENIED == Result)
                Result = STATUS_CANNOT_DELETE;
            return Result;
        }
    }

    if (filedesc->IsDirectory && !filedesc->IsReparsePoint)
    {
        /* check that directory is empty! */

        memset(&dh, 0, sizeof dh);

        if (0 != f->ops.readdir)
        {
            memset(&fi, 0, sizeof fi);
            fi.flags = filedesc->OpenFlags;
            fi.fh = filedesc->FileHandle;

            err = f->ops.readdir(filedesc->PosixPath, &dh, fsp_fuse_intf_CanDeleteAddDirInfo, 0, &fi);
        }
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
    PVOID FileDesc,
    PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists)
{
    struct fuse *f = FileSystem->UserContext;
    struct fuse_context *context = fsp_fuse_get_context(f->env);
    struct fsp_fuse_context_header *contexthdr = FSP_FUSE_HDR_FROM_CONTEXT(context);
    UINT32 Uid, Gid, Mode;
    FSP_FSCTL_FILE_INFO FileInfoBuf;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    int err;
    NTSTATUS Result;

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, contexthdr->PosixPath, 0,
        &Uid, &Gid, &Mode, &FileInfoBuf);
    if (!NT_SUCCESS(Result) &&
        STATUS_OBJECT_NAME_NOT_FOUND != Result &&
        STATUS_OBJECT_PATH_NOT_FOUND != Result)
        return Result;

    if (NT_SUCCESS(Result) &&
        (f->VolumeParams.CaseSensitiveSearch || 0 != invariant_wcsicmp(FileName, NewFileName)))
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
    PVOID FileDesc,
    PSECURITY_DESCRIPTOR SecurityDescriptorBuf, SIZE_T *PSecurityDescriptorSize)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    struct fuse_file_info fi;
    UINT32 FileAttributes;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    return fsp_fuse_intf_GetSecurityEx(FileSystem, filedesc->PosixPath,
        FUSE_FILE_INFO(filedesc->IsDirectory, &fi),
        &FileAttributes, SecurityDescriptorBuf, PSecurityDescriptorSize);
}

static NTSTATUS fsp_fuse_intf_SetSecurity(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc,
    SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
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

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath,
        FUSE_FILE_INFO(filedesc->IsDirectory, &fi),
        &Uid, &Gid, &Mode, &FileInfo);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = FspPosixMergePermissionsToSecurityDescriptor(Uid, Gid, Mode, f->FileSecurity,
        &SecurityDescriptor);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = FspSetSecurityDescriptor(
        SecurityDescriptor,
        SecurityInformation,
        ModificationDescriptor,
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
            FspPosixMergePermissionsToSecurityDescriptor);

    return Result;
}

static VOID fsp_fuse_intf_LogBadDirInfo(
    const char *PosixPath, const char *PosixName, const char *Message)
{
    static LONG Count = 0;
    ULONG NewCount;

    NewCount = (ULONG)InterlockedIncrement(&Count);

    /* log only the first 5 such warnings to avoid warning overload */
    if (5 >= NewCount)
        FspDebugLog("%S[TID=%04lx]: WARN: readdir(\"%s\"): name=\"%s\": %s\n",
            FspDiagIdent(), GetCurrentThreadId(),
            PosixPath, PosixName, Message);
}

/* !static: used by fuse2to3 */
int fsp_fuse_intf_AddDirInfo(void *buf, const char *name,
    const struct fuse_stat *stbuf, fuse_off_t off)
{
    struct fuse_dirhandle *dh = buf;
    struct fsp_fuse_file_desc *filedesc = dh->filedesc;
    union
    {
        FSP_FSCTL_DIR_INFO V;
        UINT8 B[sizeof(FSP_FSCTL_DIR_INFO) + 255 * sizeof(WCHAR)];
    } DirInfoBuf;
    FSP_FSCTL_DIR_INFO *DirInfo = &DirInfoBuf.V;
    ULONG SizeA, SizeW;

    if ('/' == filedesc->PosixPath[0] && '\0' == filedesc->PosixPath[1])
    {
        /* if this is the root directory do not add the dot entries */

        if ('.' == name[0] && ('\0' == name[1] ||
            ('.' == name[1] && '\0' == name[2])))
            return 0;
    }

    SizeA = lstrlenA(name);
    if (SizeA > 255)
    {
        fsp_fuse_intf_LogBadDirInfo(filedesc->PosixPath, name,
            "too long");
        return 0;
    }

    SizeW = MultiByteToWideChar(CP_UTF8, 0, name, SizeA, DirInfo->FileNameBuf, 255);
    if (0 == SizeW)
    {
        fsp_fuse_intf_LogBadDirInfo(filedesc->PosixPath, name,
            "MultiByteToWideChar failed");
        return 0;
    }

    memset(DirInfo, 0, sizeof *DirInfo);
    DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + SizeW * sizeof(WCHAR));

    if (dh->ReaddirPlus && 0 != stbuf &&
        0120000/* S_IFLNK */ != (stbuf->st_mode & 0170000))
    {
        UINT32 Uid, Gid, Mode;
        NTSTATUS Result0;

        Result0 = fsp_fuse_intf_GetFileInfoFunnel(dh->FileSystem, name, 0, stbuf,
            &Uid, &Gid, &Mode, 0, TRUE, &DirInfo->FileInfo);
        if (NT_SUCCESS(Result0))
            DirInfo->Padding[0] = 1; /* HACK: remember that the FileInfo is valid */
    }

    return !FspFileSystemFillDirectoryBuffer(&filedesc->DirBuffer, DirInfo, &dh->Result);
}

static int fsp_fuse_intf_AddDirInfoOld(fuse_dirh_t dh, const char *name,
    int type, fuse_ino_t ino)
{
    return fsp_fuse_intf_AddDirInfo(dh, name, 0, 0) ? -ENOMEM : 0;
}

static NTSTATUS fsp_fuse_intf_FixDirInfo(FSP_FILE_SYSTEM *FileSystem,
    struct fsp_fuse_file_desc *filedesc)
{
    char *PosixPath = 0, *PosixName, *PosixPathEnd, SavedPathChar;
    ULONG SizeA, SizeW;
    PUINT8 Buffer;
    PULONG Index, IndexEnd;
    ULONG Count;
    FSP_FSCTL_DIR_INFO *DirInfo;
    UINT32 Uid, Gid, Mode;
    NTSTATUS Result;

    SizeA = lstrlenA(filedesc->PosixPath);
    PosixPath = MemAlloc(SizeA + 1 + 255 + 1);
    if (0 == PosixPath)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    memcpy(PosixPath, filedesc->PosixPath, SizeA);
    if (1 < SizeA)
        /* if not root */
        PosixPath[SizeA++] = '/';
    PosixPath[SizeA] = '\0';
    PosixName = PosixPath + SizeA;

    FspFileSystemPeekInDirectoryBuffer(&filedesc->DirBuffer, &Buffer, &Index, &Count);

    for (IndexEnd = Index + Count; IndexEnd > Index; Index++)
    {
        DirInfo = (FSP_FSCTL_DIR_INFO *)(Buffer + *Index);
        SizeW = (DirInfo->Size - sizeof *DirInfo) / sizeof(WCHAR);

        if (DirInfo->Padding[0])
        {
            /* DirInfo has been filled already! */

            DirInfo->Padding[0] = 0;
        }
        else
        {
            if (1 == SizeW && L'.' == DirInfo->FileNameBuf[0])
            {
                PosixPathEnd = 1 < PosixName - PosixPath ? PosixName - 1 : PosixName;
                SavedPathChar = *PosixPathEnd;
                *PosixPathEnd = '\0';
            }
            else
            if (2 == SizeW && L'.' == DirInfo->FileNameBuf[0] && L'.' == DirInfo->FileNameBuf[1])
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
                SizeA = WideCharToMultiByte(CP_UTF8, 0, DirInfo->FileNameBuf, SizeW, PosixName, 255, 0, 0);
                if (0 == SizeA)
                {
                    /* this should never happen because we just converted using MultiByteToWideChar */
                    Result = STATUS_OBJECT_NAME_INVALID;
                    goto exit;
                }
                PosixName[SizeA] = '\0';
            }

            Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, PosixPath, 0,
                &Uid, &Gid, &Mode, &DirInfo->FileInfo);
            if (!NT_SUCCESS(Result))
            {
                /* mark the directory buffer entry as invalid */
                *Index = FspFileSystemDirectoryBufferEntryInvalid;
                fsp_fuse_intf_LogBadDirInfo(filedesc->PosixPath, PosixName,
                    "getattr failed");
            }

            if (0 != PosixPathEnd)
                *PosixPathEnd = SavedPathChar;
        }

        FspPosixDecodeWindowsPath(DirInfo->FileNameBuf, SizeW);
    }

    Result = STATUS_SUCCESS;

exit:
    MemFree(PosixPath);

    return Result;
}

static NTSTATUS fsp_fuse_intf_ReadDirectory(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc, PWSTR Pattern, PWSTR Marker,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    struct fuse_dirhandle dh;
    struct fuse_file_info fi;
    int err;
    NTSTATUS Result;

    if (!filedesc->IsDirectory || filedesc->IsReparsePoint)
        return STATUS_ACCESS_DENIED;

    if (FspFileSystemAcquireDirectoryBuffer(&filedesc->DirBuffer, 0 == Marker, &Result))
    {
        memset(&dh, 0, sizeof dh);
        dh.filedesc = filedesc;
        dh.FileSystem = FileSystem;
        dh.ReaddirPlus = 0 != (f->conn_want & FSP_FUSE_CAP_READDIR_PLUS);
        dh.Result = STATUS_SUCCESS;

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

        if (NT_SUCCESS(Result))
        {
            Result = dh.Result;
            if (NT_SUCCESS(Result))
                Result = fsp_fuse_intf_FixDirInfo(FileSystem, filedesc);
        }

        FspFileSystemReleaseDirectoryBuffer(&filedesc->DirBuffer);
    }

    if (!NT_SUCCESS(Result))
        return Result;

    FspFileSystemReadDirectoryBuffer(&filedesc->DirBuffer,
        Marker, Buffer, Length, PBytesTransferred);

    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_intf_GetDirInfoByName(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc, PWSTR FileName,
    FSP_FSCTL_DIR_INFO *DirInfo)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    char *PosixName = 0;
    char PosixPath[FSP_FSCTL_TRANSACT_PATH_SIZEMAX / sizeof(WCHAR)];
    int ParentLength, FSlashLength, PosixNameLength;
    UINT32 Uid, Gid, Mode;
    NTSTATUS Result;

    if (!filedesc->IsDirectory || filedesc->IsReparsePoint)
        return STATUS_ACCESS_DENIED;

    Result = FspPosixMapWindowsToPosixPath(FileName, &PosixName);
    if (!NT_SUCCESS(Result))
    {
        Result = STATUS_OBJECT_NAME_NOT_FOUND; //Result?
        goto exit;
    }

    ParentLength = lstrlenA(filedesc->PosixPath);
    FSlashLength = 1 < ParentLength;
    PosixNameLength = lstrlenA(PosixName);
    if (FSP_FSCTL_TRANSACT_PATH_SIZEMAX <= (ParentLength + FSlashLength + PosixNameLength) * sizeof(WCHAR))
    {
        Result = STATUS_OBJECT_NAME_NOT_FOUND; //STATUS_OBJECT_NAME_INVALID?
        goto exit;
    }

    memcpy(PosixPath, filedesc->PosixPath, ParentLength);
    memcpy(PosixPath + ParentLength, "/", FSlashLength);
    memcpy(PosixPath + ParentLength + FSlashLength, PosixName, PosixNameLength + 1);

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, PosixPath, 0,
        &Uid, &Gid, &Mode, &DirInfo->FileInfo);
    if (!NT_SUCCESS(Result))
    {
        Result = STATUS_OBJECT_NAME_NOT_FOUND; //Result?
        goto exit;
    }

    /*
     * FUSE does not do FileName normalization; so just return the FileName as given to us!
     */

    //memset(DirInfo->Padding, 0, sizeof DirInfo->Padding);
    DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + lstrlenW(FileName) * sizeof(WCHAR));
    memcpy(DirInfo->FileNameBuf, FileName, DirInfo->Size - sizeof(FSP_FSCTL_DIR_INFO));

    Result = STATUS_SUCCESS;

exit:
    if (0 != PosixName)
        FspPosixDeletePath(PosixName);

    return Result;
}

static NTSTATUS fsp_fuse_intf_ResolveReparsePoints(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, UINT32 ReparsePointIndex, BOOLEAN ResolveLastPathComponent,
    PIO_STATUS_BLOCK PIoStatus, PVOID Buffer, PSIZE_T PSize)
{
    return FspFileSystemResolveReparsePoints(FileSystem, fsp_fuse_intf_GetReparsePointByName, 0,
        FileName, ReparsePointIndex, ResolveLastPathComponent,
        PIoStatus, Buffer, PSize);
}

static NTSTATUS fsp_fuse_intf_GetReparsePointByName(
    FSP_FILE_SYSTEM *FileSystem, PVOID Context,
    PWSTR FileName, BOOLEAN IsDirectory, PVOID Buffer, PSIZE_T PSize)
{
    struct fuse *f = FileSystem->UserContext;
    char *PosixPath = 0;
    NTSTATUS Result;

    Result = FspPosixMapWindowsToPosixPath(FileName, &PosixPath);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = fsp_fuse_intf_GetReparsePointEx(FileSystem, PosixPath, 0, Buffer, PSize, Context);

exit:
    if (0 != PosixPath)
        FspPosixDeletePath(PosixPath);

    return Result;
}

static NTSTATUS fsp_fuse_intf_GetReparsePoint(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc,
    PWSTR FileName, PVOID Buffer, PSIZE_T PSize)
{
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    struct fuse_file_info fi;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    return fsp_fuse_intf_GetReparsePointEx(FileSystem, filedesc->PosixPath,
        FUSE_FILE_INFO(filedesc->IsDirectory, &fi),
        Buffer, PSize, 0);
}

static NTSTATUS fsp_fuse_intf_SetReparsePoint(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc,
    PWSTR FileName, PVOID Buffer, SIZE_T Size)
{
    struct fuse *f = FileSystem->UserContext;
    struct fuse_context *context = fsp_fuse_get_context(f->env);
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    struct fuse_file_info fi;
    UINT32 Uid, Gid, Mode, Dev;
    FSP_FSCTL_FILE_INFO FileInfo;
    PREPARSE_DATA_BUFFER ReparseData;
    PWSTR ReparseTargetPath;
    SIZE_T ReparseTargetPathLength;
    WCHAR TargetPath[FSP_FSCTL_TRANSACT_PATH_SIZEMAX / sizeof(WCHAR)];
    char *PosixTargetPath = 0, *PosixHiddenPath = 0;
    BOOLEAN IsSymlink;
    int err;
    NTSTATUS Result;

    /*
     * How we implement this is probably one of the worst aspects of this FUSE implementation.
     *
     * In Windows a CreateSymbolicLink is the sequence of the following:
     *     Create
     *     DeviceIoControl(FSCTL_SET_REPARSE_POINT)
     *     Cleanup
     *     Close
     *
     * The Create call creates the new file and the DeviceIoControl(FSCTL_SET_REPARSE_POINT)
     * call is supposed to convert it into a reparse point. However FUSE mknod/symlink will
     * fail with -EEXIST in this case.
     *
     * We must therefore find a solution using rename, which is unreliable and error-prone.
     * Note that this will also result in a change of the inode number for the reparse point!
     */

    ReparseData = (PREPARSE_DATA_BUFFER)Buffer;

    if (IO_REPARSE_TAG_SYMLINK == ReparseData->ReparseTag || (
        IO_REPARSE_TAG_NFS == ReparseData->ReparseTag &&
        NFS_SPECFILE_LNK == *(PUINT64)ReparseData->GenericReparseBuffer.DataBuffer))
    {
        if (filedesc->IsDirectory && 0 == f->ops.rmdir)
            return STATUS_INVALID_DEVICE_REQUEST;
        if (0 == f->ops.symlink || 0 == f->ops.rename || 0 == f->ops.unlink)
            return STATUS_INVALID_DEVICE_REQUEST;

        IsSymlink = TRUE;
    }
    else if (IO_REPARSE_TAG_NFS == ReparseData->ReparseTag)
    {
        /* FUSE cannot make a directory into an NFS reparse point */
        if (filedesc->IsDirectory)
            return STATUS_ACCESS_DENIED;
        if (0 == f->ops.mknod || 0 == f->ops.rename || 0 == f->ops.unlink)
            return STATUS_INVALID_DEVICE_REQUEST;

        IsSymlink = FALSE;
    }
    else
        return STATUS_IO_REPARSE_TAG_MISMATCH;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    Result = fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath,
        FUSE_FILE_INFO(filedesc->IsDirectory, &fi),
        &Uid, &Gid, &Mode, &FileInfo);
    if (!NT_SUCCESS(Result))
        return Result;

    if (IsSymlink)
    {
        if (IO_REPARSE_TAG_SYMLINK == ReparseData->ReparseTag)
        {
            ReparseTargetPath = ReparseData->SymbolicLinkReparseBuffer.PathBuffer +
                ReparseData->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR);
            ReparseTargetPathLength = ReparseData->SymbolicLinkReparseBuffer.SubstituteNameLength;

            /* is this an absolute path? */
            if (0 == (ReparseData->SymbolicLinkReparseBuffer.Flags & SYMLINK_FLAG_RELATIVE) &&
                ReparseTargetPathLength >= sizeof(WCHAR) && L'\\' == ReparseTargetPath[0])
            {
                FSP_FSCTL_TRANSACT_REQ *Request = FspFileSystemGetOperationContext()->Request;

                /* we do not support absolute paths that point outside this file system */
                if (0 == Request->Req.FileSystemControl.TargetOnFileSystem)
                    return STATUS_ACCESS_DENIED;

                ReparseTargetPath += Request->Req.FileSystemControl.TargetOnFileSystem / sizeof(WCHAR);
                ReparseTargetPathLength -= Request->Req.FileSystemControl.TargetOnFileSystem;
            }
        }
        else
        {
            /* the PATH is in POSIX format (UTF-16 encoding) */
            ReparseTargetPath = (PVOID)(ReparseData->GenericReparseBuffer.DataBuffer + 8);
            ReparseTargetPathLength = ReparseData->ReparseDataLength - 8;
        }

        memcpy(TargetPath, ReparseTargetPath, ReparseTargetPathLength);
        TargetPath[ReparseTargetPathLength / sizeof(WCHAR)] = L'\0';

        /*
         * From this point forward we must jump to the EXIT label on failure.
         */

        Result = FspPosixMapWindowsToPosixPathEx(TargetPath, &PosixTargetPath,
            IO_REPARSE_TAG_SYMLINK == ReparseData->ReparseTag);
        if (!NT_SUCCESS(Result))
            goto exit;

        Result = fsp_fuse_intf_NewHiddenName(FileSystem, filedesc->PosixPath, &PosixHiddenPath);
        if (!NT_SUCCESS(Result))
            goto exit;

        context->uid = Uid, context->gid = Gid;
        err = f->ops.symlink(PosixTargetPath, PosixHiddenPath);
        context->uid = -1, context->gid = -1;
        if (0 != err)
        {
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
            goto exit;
        }
    }
    else
    {
        switch (*(PUINT64)ReparseData->GenericReparseBuffer.DataBuffer)
        {
        case NFS_SPECFILE_FIFO:
            Mode = (Mode & ~0170000) | 0010000;
            Dev = 0;
            break;
        case NFS_SPECFILE_SOCK:
            Mode = (Mode & ~0170000) | 0140000;
            Dev = 0;
            break;
        case NFS_SPECFILE_CHR:
            Mode = (Mode & ~0170000) | 0020000;
            Dev =
                (*(PUINT32)(ReparseData->GenericReparseBuffer.DataBuffer +  8) << 16) |
                (*(PUINT32)(ReparseData->GenericReparseBuffer.DataBuffer + 12));
            break;
        case NFS_SPECFILE_BLK:
            Mode = (Mode & ~0170000) | 0060000;
            Dev =
                (*(PUINT32)(ReparseData->GenericReparseBuffer.DataBuffer +  8) << 16) |
                (*(PUINT32)(ReparseData->GenericReparseBuffer.DataBuffer + 12));
            break;
        default:
            return STATUS_IO_REPARSE_DATA_INVALID;
        }

        /*
         * From this point forward we must jump to the EXIT label on failure.
         */

        Result = fsp_fuse_intf_NewHiddenName(FileSystem, filedesc->PosixPath, &PosixHiddenPath);
        if (!NT_SUCCESS(Result))
            goto exit;

        context->uid = Uid, context->gid = Gid;
        err = f->ops.mknod(PosixHiddenPath, Mode, Dev);
        context->uid = -1, context->gid = -1;
        if (0 != err)
        {
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
            goto exit;
        }
    }

    if (filedesc->IsDirectory)
    {
        err = f->ops.rmdir(filedesc->PosixPath);
        if (0 == err)
            err = f->ops.rename(PosixHiddenPath, filedesc->PosixPath);
    }
    else
        err = f->ops.rename(PosixHiddenPath, filedesc->PosixPath);
    if (0 != err)
    {
        f->ops.unlink(PosixHiddenPath);
        Result = fsp_fuse_ntstatus_from_errno(f->env, err);
        goto exit;
    }

    if (filedesc->IsDirectory)
    {
        if (0 != f->ops.releasedir)
            f->ops.releasedir(filedesc->PosixPath, &fi);
    }
    else
    {
        if (0 != f->ops.release)
            f->ops.release(filedesc->PosixPath, &fi);
    }
    filedesc->IsReparsePoint = TRUE;
    filedesc->FileHandle = -1;

    Result = STATUS_SUCCESS;

exit:
    MemFree(PosixHiddenPath);

    if (0 != PosixTargetPath)
        FspPosixDeletePath(PosixTargetPath);

    return Result;
}

static NTSTATUS fsp_fuse_intf_DeleteReparsePoint(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc,
    PWSTR FileName, PVOID Buffer, SIZE_T Size)
{
    /* we were asked to delete the reparse point? no can do! */
    return STATUS_ACCESS_DENIED;
}

static NTSTATUS fsp_fuse_intf_Control(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc, UINT32 ControlCode,
    PVOID InputBuffer, ULONG InputBufferLength,
    PVOID OutputBuffer, ULONG OutputBufferLength, PULONG PBytesTransferred)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    struct fuse_file_info fi;
    int cmd;
    int err;

    if (0 == f->ops.ioctl)
        return STATUS_INVALID_DEVICE_REQUEST;

    if (FSP_FUSE_DEVICE_TYPE != DEVICE_TYPE_FROM_CTL_CODE(ControlCode))
        return STATUS_INVALID_DEVICE_REQUEST;

    if (0 != InputBufferLength && 0 != OutputBufferLength &&
        InputBufferLength != OutputBufferLength)
        return STATUS_INVALID_DEVICE_REQUEST;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    /* construct a Linux compatible ioctl code */
    cmd = FSP_FUSE_IOCTL((ControlCode >> 2) & 0xfff, InputBufferLength, OutputBufferLength);

    if (0 == OutputBufferLength)
        err = f->ops.ioctl(filedesc->PosixPath, cmd, 0, &fi, 0, InputBuffer);
    else
    {
        if (0 != InputBufferLength)
            // OutputBuffer points to Response->Buffer which is FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX long
            memcpy(OutputBuffer, InputBuffer, InputBufferLength);
        err = f->ops.ioctl(filedesc->PosixPath, cmd, 0, &fi, 0, OutputBuffer);
    }
    *PBytesTransferred = OutputBufferLength;

    return fsp_fuse_ntstatus_from_errno(f->env, err);
}

static NTSTATUS fsp_fuse_intf_GetEa(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc,
    PFILE_FULL_EA_INFORMATION Ea0, ULONG EaLength, PULONG PBytesTransferred)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    char names[3 * 1024];
    int namesize, valuesize;
    PFILE_FULL_EA_INFORMATION Ea = Ea0, PrevEa = 0;
    PUINT8 EaEnd = (PUINT8)Ea + EaLength, EaValue;

    if (0 == f->ops.listxattr || 0 == f->ops.getxattr)
        return STATUS_INVALID_DEVICE_REQUEST;

    namesize = f->ops.listxattr(filedesc->PosixPath, names, sizeof names);
    if (0 >= namesize)
    {
        *PBytesTransferred = 0;
        return fsp_fuse_ntstatus_from_errno(f->env, namesize);
    }

    for (char *p = names, *endp = p + namesize; endp > p; p += namesize)
    {
        namesize = lstrlenA(p) + 1;

        EaValue = (PUINT8)Ea + FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + namesize;
        if (EaValue >= EaEnd)
            /* if there is no space (at least 1 byte) for a value bail out */
            break;

        valuesize = f->ops.getxattr(filedesc->PosixPath, p, EaValue, EaEnd - EaValue);
        if (0 >= valuesize)
            continue;

        Ea->NextEntryOffset = 0;
        Ea->Flags = 0;
        Ea->EaNameLength = namesize - 1;
        Ea->EaValueLength = valuesize;
        memcpy(Ea->EaName, p, namesize);

        if (0 != PrevEa)
            PrevEa->NextEntryOffset = (ULONG)((PUINT8)Ea - (PUINT8)PrevEa);
        PrevEa = Ea;

        *PBytesTransferred = (ULONG)((PUINT8)EaValue - (PUINT8)Ea0 + valuesize);
        Ea = (PVOID)((PUINT8)Ea +
            FSP_FSCTL_ALIGN_UP(
                FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + namesize + valuesize,
                sizeof(ULONG)));
    }

    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_intf_SetEaEntry(
    FSP_FILE_SYSTEM *FileSystem, PVOID Context,
    PFILE_FULL_EA_INFORMATION SingleEa)
{
    struct fuse *f = FileSystem->UserContext;
    const char *PosixPath = Context;
    int err;

    if (0 != SingleEa->EaValueLength)
    {
        err = f->ops.setxattr(PosixPath, SingleEa->EaName,
            SingleEa->EaName + SingleEa->EaNameLength + 1, SingleEa->EaValueLength, 0);
        return fsp_fuse_ntstatus_from_errno(f->env, err);
    }
    else
    {
        err = f->ops.removexattr(PosixPath, SingleEa->EaName);
            /* ignore errors */
        return STATUS_SUCCESS;
    }
}

static NTSTATUS fsp_fuse_intf_SetEa(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileDesc,
    PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    struct fuse *f = FileSystem->UserContext;
    struct fsp_fuse_file_desc *filedesc = FileDesc;
    UINT32 Uid, Gid, Mode;
    struct fuse_file_info fi;
    NTSTATUS Result;

    if (0 == f->ops.setxattr || 0 == f->ops.removexattr)
        return STATUS_INVALID_DEVICE_REQUEST;

    Result = FspFileSystemEnumerateEa(FileSystem,
        fsp_fuse_intf_SetEaEntry, filedesc->PosixPath, Ea, EaLength);
    if (!NT_SUCCESS(Result))
        return Result;

    memset(&fi, 0, sizeof fi);
    fi.flags = filedesc->OpenFlags;
    fi.fh = filedesc->FileHandle;

    return fsp_fuse_intf_GetFileInfoEx(FileSystem, filedesc->PosixPath,
        FUSE_FILE_INFO(filedesc->IsDirectory, &fi),
        &Uid, &Gid, &Mode, FileInfo);
}

FSP_FILE_SYSTEM_INTERFACE fsp_fuse_intf =
{
    fsp_fuse_intf_GetVolumeInfo,
    fsp_fuse_intf_SetVolumeLabel,
    fsp_fuse_intf_GetSecurityByName,
    0,
    fsp_fuse_intf_Open,
    0,
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
    fsp_fuse_intf_DeleteReparsePoint,
    0,
    fsp_fuse_intf_GetDirInfoByName,
    fsp_fuse_intf_Control,
    0,
    fsp_fuse_intf_Create,
    fsp_fuse_intf_Overwrite,
    fsp_fuse_intf_GetEa,
    fsp_fuse_intf_SetEa,
};

/*
 * Utility
 */
NTSTATUS fsp_fuse_get_token_uidgid(
    HANDLE Token,
    TOKEN_INFORMATION_CLASS UserOrOwnerClass, /* TokenUser|TokenOwner */
    PUINT32 PUid, PUINT32 PGid)
{
    UINT32 Uid, Gid;
    union
    {
        TOKEN_USER V;
        UINT8 B[128];
    } UserInfoBuf;
    PTOKEN_USER UserInfo = &UserInfoBuf.V;
    union
    {
        TOKEN_OWNER V;
        UINT8 B[128];
    } OwnerInfoBuf;
    PTOKEN_OWNER OwnerInfo = &OwnerInfoBuf.V;
    union
    {
        TOKEN_PRIMARY_GROUP V;
        UINT8 B[128];
    } GroupInfoBuf;
    PTOKEN_PRIMARY_GROUP GroupInfo = &GroupInfoBuf.V;
    DWORD Size;
    NTSTATUS Result;

    if (0 != PUid && TokenUser == UserOrOwnerClass)
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

        Result = FspPosixMapSidToUid(UserInfo->User.Sid, &Uid);
        if (!NT_SUCCESS(Result))
            goto exit;
    }
    else if (0 != PUid && TokenOwner == UserOrOwnerClass)
    {
        if (!GetTokenInformation(Token, TokenOwner, OwnerInfo, sizeof OwnerInfoBuf, &Size))
        {
            if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
            {
                Result = FspNtStatusFromWin32(GetLastError());
                goto exit;
            }

            OwnerInfo = MemAlloc(Size);
            if (0 == OwnerInfo)
            {
                Result = STATUS_INSUFFICIENT_RESOURCES;
                goto exit;
            }

            if (!GetTokenInformation(Token, TokenOwner, OwnerInfo, Size, &Size))
            {
                Result = FspNtStatusFromWin32(GetLastError());
                goto exit;
            }
        }

        Result = FspPosixMapSidToUid(OwnerInfo->Owner, &Uid);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    if (0 != PGid)
    {
        if (!GetTokenInformation(Token, TokenPrimaryGroup, GroupInfo, sizeof GroupInfoBuf, &Size))
        {
            if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
            {
                Result = FspNtStatusFromWin32(GetLastError());
                goto exit;
            }

            GroupInfo = MemAlloc(Size);
            if (0 == GroupInfo)
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

        Result = FspPosixMapSidToUid(GroupInfo->PrimaryGroup, &Gid);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    if (0 != PUid)
        *PUid = Uid;

    if (0 != PGid)
        *PGid = Gid;

    Result = STATUS_SUCCESS;

exit:
    if (UserInfo != &UserInfoBuf.V)
        MemFree(UserInfo);

    if (OwnerInfo != &OwnerInfoBuf.V)
        MemFree(OwnerInfo);

    if (GroupInfo != &GroupInfoBuf.V)
        MemFree(GroupInfo);

    return Result;
}
