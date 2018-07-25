/**
 * @file dll/fuse/fuse.c
 *
 * @copyright 2015-2018 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <dll/fuse/library.h>

#define FSP_FUSE_SECTORSIZE_MIN         512
#define FSP_FUSE_SECTORSIZE_MAX         4096

struct fuse_chan
{
    PWSTR MountPoint;
    UINT8 Buffer[];
};

#define FSP_FUSE_CORE_OPT(n, f, v)      { n, offsetof(struct fsp_fuse_core_opt_data, f), v }
#define FSP_FUSE_CORE_OPT_NOHELP_IDX    4

static struct fuse_opt fsp_fuse_core_opts[] =
{
    FUSE_OPT_KEY("-h", 'h'),
    FUSE_OPT_KEY("--help", 'h'),
    FUSE_OPT_KEY("-V", 'V'),
    FUSE_OPT_KEY("--version", 'V'),
    FUSE_OPT_KEY("-d", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("debug", FUSE_OPT_KEY_KEEP),
    FSP_FUSE_CORE_OPT("-d", debug, 1),
    FSP_FUSE_CORE_OPT("debug", debug, 1),

    FUSE_OPT_KEY("DebugLog=", 'D'),

    FUSE_OPT_KEY("hard_remove", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("use_ino", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("readdir_ino", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("direct_io", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("kernel_cache", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("auto_cache", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("noauto_cache", FUSE_OPT_KEY_DISCARD),
    FSP_FUSE_CORE_OPT("umask=", set_umask, 1),
    FSP_FUSE_CORE_OPT("umask=%o", umask, 0),
    FSP_FUSE_CORE_OPT("create_umask=", set_create_umask, 1),
    FSP_FUSE_CORE_OPT("create_umask=%o", create_umask, 0),
    FSP_FUSE_CORE_OPT("uid=", set_uid, 1),
    FSP_FUSE_CORE_OPT("uid=%d", uid, 0),
    FSP_FUSE_CORE_OPT("gid=", set_gid, 1),
    FSP_FUSE_CORE_OPT("gid=%d", gid, 0),
    FUSE_OPT_KEY("entry_timeout", FUSE_OPT_KEY_DISCARD),
    FSP_FUSE_CORE_OPT("attr_timeout=", set_attr_timeout, 1),
    FSP_FUSE_CORE_OPT("attr_timeout=%d", attr_timeout, 0),
    FUSE_OPT_KEY("ac_attr_timeout", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("negative_timeout", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("noforget", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("intr", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("intr_signal=", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("modules=", FUSE_OPT_KEY_DISCARD),

    FSP_FUSE_CORE_OPT("rellinks", rellinks, 1),
    FSP_FUSE_CORE_OPT("norellinks", rellinks, 0),

    FUSE_OPT_KEY("fstypename=", 'F'),
    FUSE_OPT_KEY("volname=", 'v'),

    FSP_FUSE_CORE_OPT("SectorSize=%hu", VolumeParams.SectorSize, 4096),
    FSP_FUSE_CORE_OPT("SectorsPerAllocationUnit=%hu", VolumeParams.SectorsPerAllocationUnit, 1),
    FSP_FUSE_CORE_OPT("MaxComponentLength=%hu", VolumeParams.MaxComponentLength, 0),
    FSP_FUSE_CORE_OPT("VolumeCreationTime=%lli", VolumeParams.VolumeCreationTime, 0),
    FSP_FUSE_CORE_OPT("VolumeSerialNumber=%lx", VolumeParams.VolumeSerialNumber, 0),
    FSP_FUSE_CORE_OPT("FileInfoTimeout=", set_FileInfoTimeout, 1),
    FSP_FUSE_CORE_OPT("FileInfoTimeout=%d", VolumeParams.FileInfoTimeout, 0),
    FSP_FUSE_CORE_OPT("DirInfoTimeout=", set_DirInfoTimeout, 1),
    FSP_FUSE_CORE_OPT("DirInfoTimeout=%d", VolumeParams.DirInfoTimeout, 0),
    FSP_FUSE_CORE_OPT("VolumeInfoTimeout=", set_VolumeInfoTimeout, 1),
    FSP_FUSE_CORE_OPT("VolumeInfoTimeout=%d", VolumeParams.VolumeInfoTimeout, 0),
    FSP_FUSE_CORE_OPT("KeepFileCache=", set_KeepFileCache, 1),
    FSP_FUSE_CORE_OPT("ThreadCount=%u", ThreadCount, 0),
    FUSE_OPT_KEY("UNC=", 'U'),
    FUSE_OPT_KEY("--UNC=", 'U'),
    FUSE_OPT_KEY("VolumePrefix=", 'U'),
    FUSE_OPT_KEY("--VolumePrefix=", 'U'),
    FUSE_OPT_KEY("FileSystemName=", 'F'),
    FUSE_OPT_KEY("--FileSystemName=", 'F'),

    FUSE_OPT_END,
};

static INIT_ONCE fsp_fuse_initonce = INIT_ONCE_STATIC_INIT;
static DWORD fsp_fuse_tlskey = TLS_OUT_OF_INDEXES;

static BOOL WINAPI fsp_fuse_initialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    fsp_fuse_tlskey = TlsAlloc();
    return TRUE;
}

VOID fsp_fuse_finalize(BOOLEAN Dynamic)
{
    /*
     * This function is called during DLL_PROCESS_DETACH. We must therefore keep
     * finalization tasks to a minimum.
     *
     * We must free our TLS key (if any). We only do so if the library
     * is being explicitly unloaded (rather than the process exiting).
     */

    if (Dynamic && TLS_OUT_OF_INDEXES != fsp_fuse_tlskey)
    {
        /* !!!:
         * We should also free all thread local contexts, which means putting them in a list,
         * protected with a critical section, etc. Arghhh!
         *
         * I am too lazy and I am not going to do that, unless people start using this
         * DLL dynamically (LoadLibrary/FreeLibrary).
         */
        TlsFree(fsp_fuse_tlskey);
    }
}

VOID fsp_fuse_finalize_thread(VOID)
{
    struct fuse_context *context;

    if (TLS_OUT_OF_INDEXES != fsp_fuse_tlskey)
    {
        context = TlsGetValue(fsp_fuse_tlskey);
        if (0 != context)
        {
            fsp_fuse_obj_free(FSP_FUSE_HDR_FROM_CONTEXT(context));
            TlsSetValue(fsp_fuse_tlskey, 0);
        }
    }
}

FSP_FUSE_API int fsp_fuse_version(struct fsp_fuse_env *env)
{
    return FUSE_VERSION;
}

FSP_FUSE_API struct fuse_chan *fsp_fuse_mount(struct fsp_fuse_env *env,
    const char *mountpoint, struct fuse_args *args)
{
    struct fuse_chan *ch = 0;
    WCHAR TempMountPointBuf[MAX_PATH], MountPointBuf[MAX_PATH];
    int Size;

    if (0 == mountpoint || '\0' == mountpoint[0] ||
        ('*' == mountpoint[0] && '\0' == mountpoint[1]))
    {
        MountPointBuf[0] = L'*';
        MountPointBuf[1] = L'\0';
        Size = 2 * sizeof(WCHAR);
    }
    else if (
        (
            ('A' <= mountpoint[0] && mountpoint[0] <= 'Z') ||
            ('a' <= mountpoint[0] && mountpoint[0] <= 'z')
        ) &&
        ':' == mountpoint[1] && '\0' == mountpoint[2])
    {
        MountPointBuf[0] = mountpoint[0];
        MountPointBuf[1] = ':';
        MountPointBuf[2] = '\0';
        Size = 3 * sizeof(WCHAR);
    }
    else
    {
        char *win_mountpoint = 0;

        if (0 != env->conv_to_win_path)
            mountpoint = win_mountpoint = env->conv_to_win_path(mountpoint);

        Size = 0;
        if (0 != mountpoint &&
            0 != MultiByteToWideChar(CP_UTF8, 0, mountpoint, -1, TempMountPointBuf, MAX_PATH))
            Size = GetFullPathNameW(TempMountPointBuf, MAX_PATH, MountPointBuf, 0);

        env->memfree(win_mountpoint);

        if (0 == Size || MAX_PATH <= Size)
            goto fail;

        mountpoint = 0;
        Size = (Size + 1) * sizeof(WCHAR);
    }

    ch = fsp_fuse_obj_alloc(env, sizeof *ch + Size);
    if (0 == ch)
        goto fail;

    ch->MountPoint = (PVOID)ch->Buffer;
    memcpy(ch->MountPoint, MountPointBuf, Size);

    return ch;

fail:
    fsp_fuse_obj_free(ch);

    return 0;
}

FSP_FUSE_API void fsp_fuse_unmount(struct fsp_fuse_env *env,
    const char *mountpoint, struct fuse_chan *ch)
{
    fsp_fuse_obj_free(ch);
}

FSP_FUSE_API int fsp_fuse_is_lib_option(struct fsp_fuse_env *env,
    const char *opt)
{
    return fsp_fuse_opt_match(env, fsp_fuse_core_opts, opt);
}

static void fsp_fuse_cleanup(struct fuse *f);

static NTSTATUS fsp_fuse_svcstart(FSP_SERVICE *Service, ULONG argc, PWSTR *argv)
{
    struct fuse *f = Service->UserContext;
    struct fuse_context *context;
    struct fuse_conn_info conn;
    NTSTATUS Result;

    f->Service = Service;

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
    }

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
    fsp_fuse_cleanup(f);

    return Result;
}

static NTSTATUS fsp_fuse_svcstop(FSP_SERVICE *Service)
{
    struct fuse *f = Service->UserContext;

    FspFileSystemStopDispatcher(f->FileSystem);

    fsp_fuse_cleanup(f);

    return STATUS_SUCCESS;
}

static void fsp_fuse_cleanup(struct fuse *f)
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

    f->Service = 0;
}

static int fsp_fuse_core_opt_proc(void *opt_data0, const char *arg, int key,
    struct fuse_args *outargs)
{
    struct fsp_fuse_core_opt_data *opt_data = opt_data0;

    switch (key)
    {
    default:
        return 1;
    case 'h':
        /* Note: The limit on FspServiceLog messages is 1024 bytes. */
        FspServiceLog(EVENTLOG_ERROR_TYPE, L""
            FSP_FUSE_LIBRARY_NAME " options:\n"
            "    -o umask=MASK              set file permissions (octal)\n"
            "    -o create_umask=MASK       set newly created file permissions (octal)\n"
            "    -o uid=N                   set file owner (-1 for mounting user id)\n"
            "    -o gid=N                   set file group (-1 for mounting user group)\n"
            "    -o rellinks                interpret absolute symlinks as volume relative\n"
            "    -o volname=NAME            set volume label\n"
            "    -o VolumePrefix=UNC        set UNC prefix (/Server/Share)\n"
            "        --VolumePrefix=UNC     set UNC prefix (\\Server\\Share)\n"
            "    -o FileSystemName=NAME     set file system name\n"
            "    -o DebugLog=FILE           debug log file (requires -d)\n"
            );
        FspServiceLog(EVENTLOG_ERROR_TYPE, L""
            FSP_FUSE_LIBRARY_NAME " advanced options:\n"
            "    -o FileInfoTimeout=N       metadata timeout (millis, -1 for data caching)\n"
            "    -o DirInfoTimeout=N        directory info timeout (millis)\n"
            "    -o VolumeInfoTimeout=N     volume info timeout (millis)\n"
            "    -o KeepFileCache           do not discard cache when files are closed\n"
            "    -o ThreadCount             number of file system dispatcher threads\n"
            );
        opt_data->help = 1;
        return 1;
    case 'V':
        FspServiceLog(EVENTLOG_ERROR_TYPE, L""
            FSP_FUSE_LIBRARY_NAME " version %d.%d",
            FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
        opt_data->help = 1;
        return 1;
    case 'D':
        arg += sizeof "DebugLog=" - 1;
        opt_data->DebugLogHandle = CreateFileA(
            arg,
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            0,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            0);
        return 0;
    case 'U':
        if ('U' == arg[0])
            arg += sizeof "UNC=" - 1;
        else if ('U' == arg[2])
            arg += sizeof "--UNC=" - 1;
        else if ('V' == arg[0])
            arg += sizeof "VolumePrefix=" - 1;
        else if ('V' == arg[2])
            arg += sizeof "--VolumePrefix=" - 1;
        if (0 == MultiByteToWideChar(CP_UTF8, 0, arg, -1,
            opt_data->VolumeParams.Prefix, sizeof opt_data->VolumeParams.Prefix / sizeof(WCHAR)))
            return -1;
        opt_data->VolumeParams.Prefix
            [sizeof opt_data->VolumeParams.Prefix / sizeof(WCHAR) - 1] = L'\0';
        for (PWSTR P = opt_data->VolumeParams.Prefix; *P; P++)
            if (L'/' == *P)
                *P = '\\';
        return 0;
    case 'F':
        if ('f' == arg[0])
            arg += sizeof "fstypename=" - 1;
        else if ('F' == arg[0])
            arg += sizeof "FileSystemName=" - 1;
        else if ('F' == arg[2])
            arg += sizeof "--FileSystemName=" - 1;
        if (0 == MultiByteToWideChar(CP_UTF8, 0, arg, -1,
            opt_data->VolumeParams.FileSystemName + 5,
            sizeof opt_data->VolumeParams.FileSystemName / sizeof(WCHAR) - 5))
            return -1;
        opt_data->VolumeParams.FileSystemName
            [sizeof opt_data->VolumeParams.FileSystemName / sizeof(WCHAR) - 1] = L'\0';
        memcpy(opt_data->VolumeParams.FileSystemName, L"FUSE-", 5 * sizeof(WCHAR));
        return 0;
    case 'v':
        arg += sizeof "volname=" - 1;
        opt_data->VolumeLabelLength = (UINT16)(sizeof(WCHAR) *
            MultiByteToWideChar(CP_UTF8, 0, arg, lstrlenA(arg),
            opt_data->VolumeLabel, sizeof opt_data->VolumeLabel / sizeof(WCHAR)));
        if (0 == opt_data->VolumeLabelLength)
            return -1;
        return 0;
    }
}

int fsp_fuse_core_opt_parse(struct fsp_fuse_env *env,
    struct fuse_args *args, struct fsp_fuse_core_opt_data *opt_data,
    int help)
{
    if (help)
        return fsp_fuse_opt_parse(env, args, opt_data,
            fsp_fuse_core_opts, fsp_fuse_core_opt_proc);
    else
        return fsp_fuse_opt_parse(env, args, opt_data,
            fsp_fuse_core_opts + FSP_FUSE_CORE_OPT_NOHELP_IDX, fsp_fuse_core_opt_proc);
}

FSP_FUSE_API struct fuse *fsp_fuse_new(struct fsp_fuse_env *env,
    struct fuse_chan *ch, struct fuse_args *args,
    const struct fuse_operations *ops, size_t opsize, void *data)
{
    struct fuse *f = 0;
    struct fsp_fuse_core_opt_data opt_data;
    ULONG Size;
    PWSTR ErrorMessage = L".";
    NTSTATUS Result;

    if (opsize > sizeof(struct fuse_operations))
        opsize = sizeof(struct fuse_operations);

    memset(&opt_data, 0, sizeof opt_data);
    opt_data.env = env;
    opt_data.DebugLogHandle = GetStdHandle(STD_ERROR_HANDLE);
    opt_data.VolumeParams.Version = sizeof(FSP_FSCTL_VOLUME_PARAMS);
    opt_data.VolumeParams.FileInfoTimeout = 1000;
    opt_data.VolumeParams.FlushAndPurgeOnCleanup = TRUE;

    if (-1 == fsp_fuse_core_opt_parse(env, args, &opt_data, /*help=*/1))
        return 0;
    if (opt_data.help)
        return 0;

    if (opt_data.debug)
    {
        if (INVALID_HANDLE_VALUE == opt_data.DebugLogHandle)
        {
            ErrorMessage = L": cannot open debug log file.";
            goto fail;
        }
        FspDebugLogSetHandle(opt_data.DebugLogHandle);
    }

    if ((opt_data.set_uid && -1 == opt_data.uid) ||
        (opt_data.set_gid && -1 == opt_data.gid))
    {
        HANDLE Token;

        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token))
        {
            fsp_fuse_get_token_uidgid(Token, TokenUser,
                opt_data.set_uid && -1 == opt_data.uid ? &opt_data.uid : 0,
                opt_data.set_gid && -1 == opt_data.gid ? &opt_data.gid : 0);

            CloseHandle(Token);
        }

        if ((opt_data.set_uid && -1 == opt_data.uid) ||
            (opt_data.set_gid && -1 == opt_data.gid))
        {
            ErrorMessage = L": unknown user/group.";
            goto fail;
        }
    }

    if (!opt_data.set_FileInfoTimeout && opt_data.set_attr_timeout)
        opt_data.VolumeParams.FileInfoTimeout = opt_data.attr_timeout * 1000;
    if (opt_data.set_DirInfoTimeout)
        opt_data.VolumeParams.DirInfoTimeoutValid = 1;
    if (opt_data.set_VolumeInfoTimeout)
        opt_data.VolumeParams.VolumeInfoTimeoutValid = 1;
    if (opt_data.set_KeepFileCache)
        opt_data.VolumeParams.FlushAndPurgeOnCleanup = FALSE;
    opt_data.VolumeParams.CaseSensitiveSearch = TRUE;
    opt_data.VolumeParams.CasePreservedNames = TRUE;
    opt_data.VolumeParams.PersistentAcls = TRUE;
    opt_data.VolumeParams.ReparsePoints = TRUE;
    opt_data.VolumeParams.ReparsePointsAccessCheck = FALSE;
    opt_data.VolumeParams.NamedStreams = FALSE;
    opt_data.VolumeParams.ReadOnlyVolume = FALSE;
    opt_data.VolumeParams.PostCleanupWhenModifiedOnly = TRUE;
    opt_data.VolumeParams.PassQueryDirectoryFileName = TRUE;
    opt_data.VolumeParams.DeviceControl = TRUE;
    opt_data.VolumeParams.UmFileContextIsUserContext2 = TRUE;
    if (L'\0' == opt_data.VolumeParams.FileSystemName[0])
        memcpy(opt_data.VolumeParams.FileSystemName, L"FUSE", 5 * sizeof(WCHAR));

    f = fsp_fuse_obj_alloc(env, sizeof *f);
    if (0 == f)
        goto fail;

    f->env = env;
    f->set_umask = opt_data.set_umask; f->umask = opt_data.umask;
    f->set_create_umask = opt_data.set_create_umask; f->create_umask = opt_data.create_umask;
    f->set_uid = opt_data.set_uid; f->uid = opt_data.uid;
    f->set_gid = opt_data.set_gid; f->gid = opt_data.gid;
    f->rellinks = opt_data.rellinks;
    f->ThreadCount = opt_data.ThreadCount;
    memcpy(&f->ops, ops, opsize);
    f->data = data;
    f->DebugLog = opt_data.debug ? -1 : 0;
    memcpy(&f->VolumeParams, &opt_data.VolumeParams, sizeof opt_data.VolumeParams);
    f->VolumeLabelLength = opt_data.VolumeLabelLength;
    memcpy(&f->VolumeLabel, &opt_data.VolumeLabel, opt_data.VolumeLabelLength);

    Size = (lstrlenW(ch->MountPoint) + 1) * sizeof(WCHAR);
    f->MountPoint = fsp_fuse_obj_alloc(env, Size);
    if (0 == f->MountPoint)
        goto fail;
    memcpy(f->MountPoint, ch->MountPoint, Size);

    Result = FspFileSystemPreflight(
        f->VolumeParams.Prefix[0] ? L"" FSP_FSCTL_NET_DEVICE_NAME : L"" FSP_FSCTL_DISK_DEVICE_NAME,
        '*' != f->MountPoint[0] || '\0' != f->MountPoint[1] ? f->MountPoint : 0);
    if (!NT_SUCCESS(Result))
    {
        switch (Result)
        {
        case STATUS_ACCESS_DENIED:
            ErrorMessage = L": access denied.";
            break;

        case STATUS_NO_SUCH_DEVICE:
            ErrorMessage = L": FSD not found.";
            break;

        case STATUS_OBJECT_NAME_INVALID:
            ErrorMessage = L": invalid mount point.";
            break;

        case STATUS_OBJECT_NAME_COLLISION:
            ErrorMessage = L": mount point in use.";
            break;

        default:
            ErrorMessage = L": unspecified error.";
            break;
        }

        goto fail;
    }

    return f;

fail:
    FspServiceLog(EVENTLOG_ERROR_TYPE,
        L"Cannot create " FSP_FUSE_LIBRARY_NAME " file system%s",
        ErrorMessage);

    if (0 != f)
        fsp_fuse_destroy(env, f);

    return 0;
}

FSP_FUSE_API void fsp_fuse_destroy(struct fsp_fuse_env *env,
    struct fuse *f)
{
    fsp_fuse_cleanup(f);

    fsp_fuse_obj_free(f->MountPoint);

    fsp_fuse_obj_free(f);
}

FSP_FUSE_API int fsp_fuse_loop(struct fsp_fuse_env *env,
    struct fuse *f)
{
    f->OpGuardStrategy = FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_COARSE;
    return 0 == FspServiceRunEx(FspDiagIdent(), fsp_fuse_svcstart, fsp_fuse_svcstop, 0, f) ?
        0 : -1;
}

FSP_FUSE_API int fsp_fuse_loop_mt(struct fsp_fuse_env *env,
    struct fuse *f)
{
    f->OpGuardStrategy = FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE;
    return 0 == FspServiceRunEx(FspDiagIdent(), fsp_fuse_svcstart, fsp_fuse_svcstop, 0, f) ?
        0 : -1;
}

FSP_FUSE_API void fsp_fuse_exit(struct fsp_fuse_env *env,
    struct fuse *f)
{
    if (0 != f->Service)
        FspServiceStop(f->Service);
    f->exited = 1;
}

FSP_FUSE_API int FSP_FUSE_API_NAME(fsp_fuse_exited)(struct fsp_fuse_env *env,
    struct fuse *f)
{
    return f->exited;
}

FSP_FUSE_API struct fuse_context *fsp_fuse_get_context(struct fsp_fuse_env *env)
{
    struct fuse_context *context;

    InitOnceExecuteOnce(&fsp_fuse_initonce, fsp_fuse_initialize, 0, 0);
    if (TLS_OUT_OF_INDEXES == fsp_fuse_tlskey)
        return 0;

    context = TlsGetValue(fsp_fuse_tlskey);
    if (0 == context)
    {
        struct fsp_fuse_context_header *contexthdr;

        contexthdr = fsp_fuse_obj_alloc(env,
            sizeof(struct fsp_fuse_context_header) + sizeof(struct fuse_context));
        if (0 == contexthdr)
            return 0;

        context = FSP_FUSE_CONTEXT_FROM_HDR(contexthdr);

        TlsSetValue(fsp_fuse_tlskey, context);
    }

    return context;
}

struct fuse_context *fsp_fuse_get_context_internal(void)
{
    return TlsGetValue(fsp_fuse_tlskey);
}

FSP_FUSE_API int32_t fsp_fuse_ntstatus_from_errno(struct fsp_fuse_env *env,
    int err)
{
    if (0 > err)
        err = -err;

    if ('C' == env->environment)
        switch (err)
        {
        #undef FSP_FUSE_ERRNO
        #define FSP_FUSE_ERRNO 67
        #include "errno.i"
        default:
            return STATUS_ACCESS_DENIED;
        }
    else
        switch (err)
        {
        #undef FSP_FUSE_ERRNO
        #define FSP_FUSE_ERRNO 87
        #include "errno.i"
        default:
            return STATUS_ACCESS_DENIED;
        }
}

/* Cygwin signal support */

FSP_FUSE_API void fsp_fuse_signal_handler(int sig)
{
    FspServiceConsoleCtrlHandler(CTRL_BREAK_EVENT);
}
