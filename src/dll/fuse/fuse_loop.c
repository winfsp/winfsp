/**
 * @file dll/fuse/fuse_loop.c
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

#define FSP_FUSE_SECTORSIZE_MIN         512
#define FSP_FUSE_SECTORSIZE_MAX         4096

static INIT_ONCE fsp_fuse_svconce = INIT_ONCE_STATIC_INIT;
static HANDLE fsp_fuse_svcthread;

static NTSTATUS fsp_fuse_svcstart(FSP_SERVICE *Service, ULONG argc, PWSTR *argv)
{
    return STATUS_SUCCESS;
}

static NTSTATUS fsp_fuse_svcstop(FSP_SERVICE *Service)
{
    return STATUS_SUCCESS;
}

static DWORD WINAPI fsp_fuse_svcmain(PVOID Context)
{
    return FspServiceRun(FspDiagIdent(), fsp_fuse_svcstart, fsp_fuse_svcstop, 0);
}

static BOOL WINAPI fsp_fuse_svcinit(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    fsp_fuse_svcthread = CreateThread(0, 0, fsp_fuse_svcmain, 0, 0, 0);
    return TRUE;
}

static void fsp_fuse_loop_cleanup(struct fuse *f);

static NTSTATUS fsp_fuse_loop_start(struct fuse *f)
{
    struct fuse_context *context;
    struct fuse_conn_info conn;
    NTSTATUS Result;

    f->LoopEvent = CreateEventW(0, TRUE, FALSE, 0);
    if (0 == f->LoopEvent)
        goto fail;

    context = fsp_fuse_get_context(f->env);
    if (0 == context)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto fail;
    }
    context->fuse = f;
    context->private_data = f->data;
    context->uid = -1;
    context->gid = -1;
    context->pid = -1;

    memset(&conn, 0, sizeof conn);
    conn.proto_major = 7;               /* pretend that we are FUSE kernel protocol 7.12 */
    conn.proto_minor = 12;              /*     which was current at the time of FUSE 2.8 */
    conn.async_read = 1;
    conn.max_write = UINT_MAX;
    conn.capable =
        FUSE_CAP_ASYNC_READ |
        //FUSE_CAP_POSIX_LOCKS |        /* WinFsp handles locking in the FSD currently */
        //FUSE_CAP_ATOMIC_O_TRUNC |     /* due to Windows/WinFsp design, no support */
        //FUSE_CAP_EXPORT_SUPPORT |     /* not needed in Windows/WinFsp */
        FUSE_CAP_BIG_WRITES |
        FUSE_CAP_DONT_MASK |
        FSP_FUSE_CAP_READDIR_PLUS |
        FSP_FUSE_CAP_READ_ONLY |
        FSP_FUSE_CAP_STAT_EX |
        FSP_FUSE_CAP_DELETE_ACCESS |
        FSP_FUSE_CAP_CASE_INSENSITIVE;
    if (0 != f->ops.init)
    {
        context->private_data = f->data = f->ops.init(&conn);
        f->VolumeParams.ReadOnlyVolume = 0 != (conn.want & FSP_FUSE_CAP_READ_ONLY);
        f->VolumeParams.CaseSensitiveSearch = 0 == (conn.want & FSP_FUSE_CAP_CASE_INSENSITIVE);
        if (!f->VolumeParams.CaseSensitiveSearch)
            /*
             * Disable GetDirInfoByName when file system is case-insensitive.
             * The reason is that Windows always sends us queries with uppercase
             * file names in GetDirInfoByName and we have no way in FUSE to normalize
             * those file names when embedding them in FSP_FSCTL_DIR_INFO.
             */
            f->VolumeParams.PassQueryDirectoryFileName = FALSE;
        f->conn_want = conn.want;
    }
    f->fsinit = TRUE;
    if (0 != f->ops.statfs)
    {
        struct fuse_statvfs stbuf;
        int err;

        memset(&stbuf, 0, sizeof stbuf);
        err = f->ops.statfs("/", &stbuf);
        if (0 != err)
        {
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
            goto fail;
        }

        if (0 == f->VolumeParams.SectorSize && 0 != stbuf.f_frsize)
            f->VolumeParams.SectorSize = (UINT16)stbuf.f_frsize;
#if 0
        if (0 == f->VolumeParams.SectorsPerAllocationUnit && 0 != stbuf.f_frsize)
            f->VolumeParams.SectorsPerAllocationUnit = (UINT16)(stbuf.f_bsize / stbuf.f_frsize);
#endif
        if (0 == f->VolumeParams.MaxComponentLength)
            f->VolumeParams.MaxComponentLength = (UINT16)stbuf.f_namemax;
    }
    if (0 != f->ops.getattr)
    {
        struct fuse_stat_ex stbuf;
        int err;

        memset(&stbuf, 0, sizeof stbuf);
        err = f->ops.getattr("/", (void *)&stbuf);
        if (0 != err)
        {
            Result = fsp_fuse_ntstatus_from_errno(f->env, err);
            goto fail;
        }

        if (0 == f->VolumeParams.VolumeCreationTime)
        {
            if (0 != stbuf.st_birthtim.tv_sec)
                FspPosixUnixTimeToFileTime((void *)&stbuf.st_birthtim,
                    &f->VolumeParams.VolumeCreationTime);
            else
            if (0 != stbuf.st_ctim.tv_sec)
                FspPosixUnixTimeToFileTime((void *)&stbuf.st_ctim,
                    &f->VolumeParams.VolumeCreationTime);
        }
    }
    if (0 != f->ops.readlink)
    {
        char buf[FSP_FSCTL_TRANSACT_PATH_SIZEMAX / sizeof(WCHAR)];
        int err;

        /* this should always fail with ENOSYS or EINVAL */
        err = f->ops.readlink("/", buf, sizeof buf);
        f->has_symlinks = -ENOSYS_(f->env) != err;

        if (f->has_symlinks)
        {
            /*
             * Determine if the file system supports "/." queries.
             *
             * Symlinks on Windows are differentiated as "file" symlinks or "directory" symlinks.
             * When we need to make the distinction we can follow one of two techniques:
             *
             * - Slashdot technique: We issue a getattr(path + "/.") and check the stat result.
             * In general this is not a getattr() query that FUSE file systems are expected
             * to handle. For this reason we issue a getattr("/.") below to determine
             * if the file system handles this kind of query against the root directory.
             *
             * - Resolve technique: If the file system cannot handle slashdot queries, we resolve
             * the path using readlink on each path component, then issue getattr on the resolved
             * path and check the stat result.
             */
            struct fuse_stat_ex stbuf;
            memset(&stbuf, 0, sizeof stbuf);
            err = f->ops.getattr("/.", (void *)&stbuf);
            f->has_slashdot = 0 == err && 0040000 == (stbuf.st_mode & 0170000);
        }
    }
    if (0 != f->ops.listxattr && 0 != f->ops.getxattr &&
        0 != f->ops.setxattr && 0 != f->ops.removexattr)
        f->VolumeParams.ExtendedAttributes = 1;

    /* the FSD does not currently limit these VolumeParams fields; do so here! */
    if (f->VolumeParams.SectorSize < FSP_FUSE_SECTORSIZE_MIN ||
        f->VolumeParams.SectorSize > FSP_FUSE_SECTORSIZE_MAX)
        f->VolumeParams.SectorSize = FSP_FUSE_SECTORSIZE_MAX;
    if (f->VolumeParams.SectorsPerAllocationUnit == 0)
        f->VolumeParams.SectorsPerAllocationUnit = 1;
    if (f->VolumeParams.MaxComponentLength > 255)
        f->VolumeParams.MaxComponentLength = 255;

    if (0 == f->VolumeParams.VolumeCreationTime)
    {
        FILETIME FileTime;
        GetSystemTimeAsFileTime(&FileTime);
        f->VolumeParams.VolumeCreationTime = *(PUINT64)&FileTime;
    }
    if (0 == f->VolumeParams.VolumeSerialNumber)
        f->VolumeParams.VolumeSerialNumber =
            ((PLARGE_INTEGER)&f->VolumeParams.VolumeCreationTime)->HighPart ^
            ((PLARGE_INTEGER)&f->VolumeParams.VolumeCreationTime)->LowPart;

    Result = FspFileSystemCreate(
        f->VolumeParams.Prefix[0] ?
            L"" FSP_FSCTL_NET_DEVICE_NAME : L"" FSP_FSCTL_DISK_DEVICE_NAME,
        &f->VolumeParams, &fsp_fuse_intf,
        &f->FileSystem);
    if (!NT_SUCCESS(Result))
    {
        FspServiceLog(EVENTLOG_ERROR_TYPE,
            L"Cannot create " FSP_FUSE_LIBRARY_NAME " file system.");
        goto fail;
    }

    f->FileSystem->UserContext = f;
    FspFileSystemSetOperationGuard(f->FileSystem, fsp_fuse_op_enter, fsp_fuse_op_leave);
    FspFileSystemSetOperationGuardStrategy(f->FileSystem, f->OpGuardStrategy);
    FspFileSystemSetDebugLog(f->FileSystem, f->DebugLog);

    if (0 != f->MountPoint)
    {
        Result = FspFileSystemSetMountPoint(f->FileSystem,
            L'*' == f->MountPoint[0] && L'\0' == f->MountPoint[1] ? 0 : f->MountPoint);
        if (!NT_SUCCESS(Result))
        {
            FspServiceLog(EVENTLOG_ERROR_TYPE,
                L"Cannot set " FSP_FUSE_LIBRARY_NAME " file system mount point.");
            goto fail;
        }
    }

    Result = FspFileSystemStartDispatcher(f->FileSystem, f->ThreadCount);
    if (!NT_SUCCESS(Result))
    {
        FspServiceLog(EVENTLOG_ERROR_TYPE,
            L"Cannot start " FSP_FUSE_LIBRARY_NAME " file system dispatcher.");
        goto fail;
    }

    return STATUS_SUCCESS;

fail:
    fsp_fuse_loop_cleanup(f);

    return Result;
}

static void fsp_fuse_loop_stop(struct fuse *f)
{
    FspFileSystemStopDispatcher(f->FileSystem);

    fsp_fuse_loop_cleanup(f);
}

static void fsp_fuse_loop_cleanup(struct fuse *f)
{
    if (0 != f->FileSystem)
    {
        FspFileSystemDelete(f->FileSystem);
        f->FileSystem = 0;
    }

    if (f->fsinit)
    {
        if (f->ops.destroy)
            f->ops.destroy(f->data);
        f->fsinit = FALSE;
    }

    if (0 != f->LoopEvent)
    {
        CloseHandle(f->LoopEvent);
        f->LoopEvent = 0;
    }
}

static NTSTATUS fsp_fuse_loop_internal(struct fuse *f)
{
    HANDLE WaitObjects[2];
    DWORD WaitResult;
    NTSTATUS Result;

    Result = fsp_fuse_loop_start(f);
    if (!NT_SUCCESS(Result))
    {
        /* emulate WinFsp-FUSE v1.3 behavior! */
        FspServiceLog(EVENTLOG_ERROR_TYPE,
            L"The service %s has failed to start (Status=%lx).", FspDiagIdent(), Result);
        return Result;
    }

    InitOnceExecuteOnce(&fsp_fuse_svconce, fsp_fuse_svcinit, 0, 0);
    if (0 == fsp_fuse_svcthread)
    {
        fsp_fuse_loop_stop(f);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* if either the service thread dies or our event gets signaled, stop the loop */
    Result = STATUS_SUCCESS;
    if (!f->exited)
    {
        WaitObjects[0] = fsp_fuse_svcthread;
        WaitObjects[1] = f->LoopEvent;
        WaitResult = WaitForMultipleObjects(2, WaitObjects, FALSE, INFINITE);
        if (WAIT_OBJECT_0 != WaitResult && WAIT_OBJECT_0 + 1 != WaitResult)
            Result = FspNtStatusFromWin32(GetLastError());
    }

    fsp_fuse_loop_stop(f);

    return Result;
}

FSP_FUSE_API int fsp_fuse_loop(struct fsp_fuse_env *env,
    struct fuse *f)
{
    f->OpGuardStrategy = FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_COARSE;
    return NT_SUCCESS(fsp_fuse_loop_internal(f)) ? 0 : -1;
}

FSP_FUSE_API int fsp_fuse_loop_mt(struct fsp_fuse_env *env,
    struct fuse *f)
{
    f->OpGuardStrategy = FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE;
    return NT_SUCCESS(fsp_fuse_loop_internal(f)) ? 0 : -1;
}

/* Cygwin signal support */

FSP_FUSE_API void fsp_fuse_signal_handler(int sig)
{
    FspServiceConsoleCtrlHandler(CTRL_BREAK_EVENT);
}
