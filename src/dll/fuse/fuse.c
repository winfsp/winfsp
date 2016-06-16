/**
 * @file dll/fuse/fuse.c
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

#define FSP_FUSE_SECTORSIZE_MIN         512
#define FSP_FUSE_SECTORSIZE_MAX         4096

struct fuse_chan
{
    PWSTR MountPoint;
    UINT8 Buffer[];
};

#define FSP_FUSE_CORE_OPT(n, f, v)      { n, offsetof(struct fsp_fuse_core_opt_data, f), v }

struct fsp_fuse_core_opt_data
{
    struct fsp_fuse_env *env;
    int help, debug;
    int hard_remove,
        use_ino, readdir_ino,
        set_umask, umask,
        set_uid, uid,
        set_gid, gid,
        set_attr_timeout, attr_timeout;
    int set_FileInfoTimeout;
    int CaseInsensitiveSearch, ReparsePoints,
        NamedStreams, ReadOnlyVolume;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
};

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

    FSP_FUSE_CORE_OPT("hard_remove", hard_remove, 1),
    FSP_FUSE_CORE_OPT("use_ino", use_ino, 1),
    FSP_FUSE_CORE_OPT("readdir_ino", readdir_ino, 1),
    FUSE_OPT_KEY("direct_io", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("kernel_cache", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("auto_cache", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("noauto_cache", FUSE_OPT_KEY_DISCARD),
    FSP_FUSE_CORE_OPT("umask=", set_umask, 1),
    FSP_FUSE_CORE_OPT("umask=%o", umask, 0),
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

    FSP_FUSE_CORE_OPT("SectorSize=%hu", VolumeParams.SectorSize, 4096),
    FSP_FUSE_CORE_OPT("SectorsPerAllocationUnit=%hu", VolumeParams.SectorsPerAllocationUnit, 1),
    FSP_FUSE_CORE_OPT("MaxComponentLength=%hu", VolumeParams.MaxComponentLength, 0),
    FSP_FUSE_CORE_OPT("VolumeCreationTime=%lli", VolumeParams.VolumeCreationTime, 0),
    FSP_FUSE_CORE_OPT("VolumeSerialNumber=%lx", VolumeParams.VolumeSerialNumber, 0),
    FSP_FUSE_CORE_OPT("TransactTimeout=%u", VolumeParams.TransactTimeout, 0),
    FSP_FUSE_CORE_OPT("IrpTimeout=%u", VolumeParams.IrpTimeout, 0),
    FSP_FUSE_CORE_OPT("IrpCapacity=%u", VolumeParams.IrpCapacity, 0),
    FSP_FUSE_CORE_OPT("FileInfoTimeout=", set_FileInfoTimeout, 1),
    FSP_FUSE_CORE_OPT("FileInfoTimeout=%d", VolumeParams.FileInfoTimeout, 0),
    FSP_FUSE_CORE_OPT("CaseInsensitiveSearch", CaseInsensitiveSearch, 1),
    FSP_FUSE_CORE_OPT("ReparsePoints", ReparsePoints, 1),
    FSP_FUSE_CORE_OPT("NamedStreams", NamedStreams, 1),
    FUSE_OPT_KEY("HardLinks", FUSE_OPT_KEY_DISCARD),
    FUSE_OPT_KEY("ExtendedAttributes", FUSE_OPT_KEY_DISCARD),
    FSP_FUSE_CORE_OPT("ReadOnlyVolume", ReadOnlyVolume, 1),
    FUSE_OPT_KEY("--UNC=", 'U'),
    FUSE_OPT_KEY("--VolumePrefix=", 'U'),

    FUSE_OPT_END,
};

static INIT_ONCE fsp_fuse_initonce = INIT_ONCE_STATIC_INIT;
static DWORD fsp_fuse_tlskey = TLS_OUT_OF_INDEXES;

struct fsp_fuse_obj_hdr
{
    void (*dtor)(void *);
    __declspec(align(MEMORY_ALLOCATION_ALIGNMENT)) UINT8 ObjectBuf[];
};

static inline void *fsp_fuse_obj_alloc(struct fsp_fuse_env *env, size_t size)
{
    struct fsp_fuse_obj_hdr *hdr;

    hdr = env->memalloc(sizeof(struct fsp_fuse_obj_hdr) + size);
    if (0 == hdr)
        return 0;

    hdr->dtor = env->memfree;
    memset(hdr->ObjectBuf, 0, size);
    return hdr->ObjectBuf;
}

static inline void fsp_fuse_obj_free(void *obj)
{
    struct fsp_fuse_obj_hdr *hdr = (PVOID)((PUINT8)obj - sizeof(struct fsp_fuse_obj_hdr));

    hdr->dtor(hdr);
}

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
    int Size;

    if (0 == mountpoint)
        mountpoint = "";

    Size = MultiByteToWideChar(CP_UTF8, 0, mountpoint, -1, 0, 0);
    if (0 == Size)
        goto fail;

    ch = fsp_fuse_obj_alloc(env, sizeof *ch + Size * sizeof(WCHAR));
    if (0 == ch)
        goto fail;

    ch->MountPoint = (PVOID)ch->Buffer;
    Size = MultiByteToWideChar(CP_UTF8, 0, mountpoint, -1, ch->MountPoint, Size);
    if (0 == Size)
        goto fail;

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

static NTSTATUS fsp_fuse_preflight(struct fuse *f)
{
    NTSTATUS Result;

    Result = FspFsctlPreflight(f->VolumeParams.Prefix[0] ?
        L"" FSP_FSCTL_NET_DEVICE_NAME : L"" FSP_FSCTL_DISK_DEVICE_NAME);
    if (!NT_SUCCESS(Result))
        return Result;

    if (L'\0' != f->MountPoint)
    {
        if ((
                (L'A' <= f->MountPoint[0] && f->MountPoint[0] <= L'Z') ||
                (L'a' <= f->MountPoint[0] && f->MountPoint[0] <= L'z')
            ) &&
            L':' == f->MountPoint[1] || L'\0' == f->MountPoint[2])
        {
            if (GetLogicalDrives() & (1 << ((f->MountPoint[0] & ~0x20) - 'a')))
                return STATUS_OBJECT_NAME_COLLISION;
        }
        else
        if (L'*' == f->MountPoint[0] && L'\0' == f->MountPoint[1])
            ;
        else
            return STATUS_OBJECT_NAME_INVALID;
    }

    return STATUS_SUCCESS;
}

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
        FUSE_CAP_DONT_MASK;
    if (0 != f->ops.init)
        context->private_data = f->data = f->ops.init(&conn);
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

        if (stbuf.f_frsize > FSP_FUSE_SECTORSIZE_MAX)
            stbuf.f_frsize = FSP_FUSE_SECTORSIZE_MAX;
        if (0 == f->VolumeParams.SectorSize)
            f->VolumeParams.SectorSize = (UINT16)stbuf.f_frsize;
        if (0 == f->VolumeParams.MaxComponentLength)
            f->VolumeParams.MaxComponentLength = (UINT16)stbuf.f_namemax;
    }
    if (0 != f->ops.getattr)
    {
        struct fuse_stat stbuf;
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
                f->VolumeParams.VolumeCreationTime =
                    Int32x32To64(stbuf.st_birthtim.tv_sec, 10000000) + 116444736000000000 +
                    stbuf.st_birthtim.tv_nsec / 100;
            else
            if (0 != stbuf.st_ctim.tv_sec)
                f->VolumeParams.VolumeCreationTime =
                    Int32x32To64(stbuf.st_ctim.tv_sec, 10000000) + 116444736000000000 +
                    stbuf.st_ctim.tv_nsec / 100;
        }
    }

    /* the FSD does not currently limit these VolumeParams fields; do so here! */
    if (f->VolumeParams.SectorSize < FSP_FUSE_SECTORSIZE_MIN)
        f->VolumeParams.SectorSize = FSP_FUSE_SECTORSIZE_MIN;
    if (f->VolumeParams.SectorSize > FSP_FUSE_SECTORSIZE_MAX)
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

    if (L'\0' != f->MountPoint)
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

    Result = FspFileSystemStartDispatcher(f->FileSystem, 0);
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
        FspServiceLog(EVENTLOG_ERROR_TYPE, L""
            FSP_FUSE_LIBRARY_NAME " options:\n"
            "    -o SectorSize=N        sector size for Windows (512-4096, deflt: 512)\n"
            "    -o SectorsPerAllocationUnit=N  allocation unit size (deflt: 1*SectorSize)\n"
            "    -o MaxComponentLength=N    max file name component length (deflt: 255)\n"
            "    -o VolumeCreationTime=T    volume creation time (FILETIME hex format)\n"
            "    -o VolumeSerialNumber=N    32-bit wide\n"
            "    -o FileInfoTimeout=N       FileInfo/Security/VolumeInfo timeout (millisec)\n"
            "    -o CaseInsensitiveSearch   file system supports case-insensitive file names\n"
            //"    -o ReparsePoints           file system supports reparse points\n"
            //"    -o NamedStreams            file system supports named streams\n"
            //"    -o ReadOnlyVolume          file system is read only\n"
            "    --UNC=U --VolumePrefix=U   UNC prefix (\\Server\\Share)\n");
        opt_data->help = 1;
        return 1;
    case 'V':
        FspServiceLog(EVENTLOG_ERROR_TYPE, L""
            FSP_FUSE_LIBRARY_NAME " version %d.%d",
            FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
        opt_data->help = 1;
        return 1;
    case 'U':
        if ('U' == arg[2])
            arg += sizeof "--UNC" - 1;
        else if ('V' == arg[2])
            arg += sizeof "--VolumePrefix" - 1;
        if (0 == MultiByteToWideChar(CP_UTF8, 0, arg, -1,
            opt_data->VolumeParams.Prefix, sizeof opt_data->VolumeParams.Prefix / sizeof(WCHAR)))
            return -1;
        return 0;
    }
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

    if (-1 == fsp_fuse_opt_parse(env, args, &opt_data, fsp_fuse_core_opts, fsp_fuse_core_opt_proc))
        return 0;
    if (opt_data.help)
        return 0;

    if (!opt_data.set_FileInfoTimeout && opt_data.set_attr_timeout)
        opt_data.VolumeParams.FileInfoTimeout = opt_data.set_attr_timeout * 1000;
    opt_data.VolumeParams.CaseSensitiveSearch = !opt_data.CaseInsensitiveSearch;
    opt_data.VolumeParams.PersistentAcls = TRUE;
    opt_data.VolumeParams.ReparsePoints = !!opt_data.ReparsePoints;
    opt_data.VolumeParams.NamedStreams = !!opt_data.NamedStreams;
    opt_data.VolumeParams.ReadOnlyVolume = !!opt_data.ReadOnlyVolume;

    f = fsp_fuse_obj_alloc(env, sizeof *f);
    if (0 == f)
        goto fail;

    f->env = env;
    memcpy(&f->ops, ops, opsize);
    f->data = data;
    f->DebugLog = opt_data.debug ? -1 : 0;
    memcpy(&f->VolumeParams, &opt_data.VolumeParams, sizeof opt_data.VolumeParams);

    Size = (lstrlenW(ch->MountPoint) + 1) * sizeof(WCHAR);
    f->MountPoint = fsp_fuse_obj_alloc(env, Size);
    if (0 == f->MountPoint)
        goto fail;
    memcpy(f->MountPoint, ch->MountPoint, Size);

    Result = fsp_fuse_preflight(f);
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
        context->pid = -1;

        TlsSetValue(fsp_fuse_tlskey, context);
    }

    return context;
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
