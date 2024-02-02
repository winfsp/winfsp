/**
 * @file dll/fuse/library.h
 *
 * @copyright 2015-2024 Bill Zissimopoulos
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

#ifndef WINFSP_DLL_FUSE_LIBRARY_H_INCLUDED
#define WINFSP_DLL_FUSE_LIBRARY_H_INCLUDED

#include <dll/library.h>
#include <fuse/fuse.h>
#include <fuse/fuse_opt.h>

#define FSP_FUSE_LIBRARY_NAME           LIBRARY_NAME "-FUSE"

#define FSP_FUSE_HDR_FROM_CONTEXT(c)    \
    ((struct fsp_fuse_context_header *)((PUINT8)(c) - sizeof(struct fsp_fuse_context_header)))
#define FSP_FUSE_CONTEXT_FROM_HDR(h)    \
    ((struct fuse_context *)((PUINT8)(h) + sizeof(struct fsp_fuse_context_header)))

#define FSP_FUSE_HAS_SYMLINKS(f)        ((f)->has_symlinks)
#define FSP_FUSE_HAS_SLASHDOT(f)        ((f)->has_slashdot)

#define ENOSYS_(env)                    ('C' == (env)->environment ? 88 : 40)

/* NFS reparse points */
#define NFS_SPECFILE_FIFO               0x000000004F464946
#define NFS_SPECFILE_CHR                0x0000000000524843
#define NFS_SPECFILE_BLK                0x00000000004b4c42
#define NFS_SPECFILE_LNK                0x00000000014b4e4c
#define NFS_SPECFILE_SOCK               0x000000004B434F53

/* FUSE internal struct's */
struct fuse
{
    struct fsp_fuse_env *env;
    int set_umask, umask;
    int set_create_umask, create_umask;
    int set_create_file_umask, create_file_umask;
    int set_create_dir_umask, create_dir_umask;
    int set_uid, uid;
    int set_gid, gid;
    int rellinks;
    int dothidden;
    unsigned ThreadCount;
    struct fuse_operations ops;
    void *data;
    unsigned conn_want;
    BOOLEAN fsinit;
    BOOLEAN has_symlinks, has_slashdot;
    UINT32 DebugLog;
    FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY OpGuardStrategy;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    UINT16 VolumeLabelLength;
    WCHAR VolumeLabel[sizeof ((FSP_FSCTL_VOLUME_INFO *)0)->VolumeLabel / sizeof(WCHAR)];
    PWSTR MountPoint;
    HANDLE LoopEvent;
    FSP_FILE_SYSTEM *FileSystem;
    volatile int exited;
    struct fuse3 *fuse3;
    PSECURITY_DESCRIPTOR FileSecurity;
    FSP_FSCTL_DECLSPEC_ALIGN UINT8 FileSecurityBuf[];
};
struct fsp_fuse_context_header
{
    char *PosixPath;
    __declspec(align(MEMORY_ALLOCATION_ALIGNMENT)) UINT8 ContextBuf[];
};
struct fsp_fuse_file_desc
{
    char *PosixPath;
    BOOLEAN IsDirectory, IsReparsePoint;
    int OpenFlags;
    UINT_PTR FileHandle;
    PVOID DirBuffer;
};
struct fuse_dirhandle
{
    /* ReadDirectory */
    struct fsp_fuse_file_desc *filedesc;
    FSP_FILE_SYSTEM *FileSystem;
    BOOLEAN ReaddirPlus;
    NTSTATUS Result;
    /* CanDelete */
    BOOLEAN DotFiles, HasChild;
};

/* FUSE obj alloc/free */
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
    if (0 == obj)
        return;

    struct fsp_fuse_obj_hdr *hdr = (PVOID)((PUINT8)obj - sizeof(struct fsp_fuse_obj_hdr));

    hdr->dtor(hdr);
}

/* fsp_fuse_get_context_internal */
extern DWORD fsp_fuse_tlskey;
static inline struct fuse_context *fsp_fuse_get_context_internal(void)
{
    return TlsGetValue(fsp_fuse_tlskey);
}

/* fsp_fuse_core_opt_parse */
struct fsp_fuse_core_opt_data
{
    struct fsp_fuse_env *env;
    int help, debug;
    HANDLE DebugLogHandle;
    int set_umask, umask,
        set_create_umask, create_umask,
        set_create_file_umask, create_file_umask,
        set_create_dir_umask, create_dir_umask,
        set_uid, uid, username_to_uid_result,
        set_gid, gid,
        set_uidmap,
        set_attr_timeout, attr_timeout,
        rellinks,
        dothidden;
    int set_FileInfoTimeout,
        set_DirInfoTimeout,
        set_EaTimeout,
        set_VolumeInfoTimeout,
        set_KeepFileCache,
        set_LegacyUnlinkRename;
    unsigned ThreadCount;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    UINT16 VolumeLabelLength;
    WCHAR VolumeLabel[sizeof ((FSP_FSCTL_VOLUME_INFO *)0)->VolumeLabel / sizeof(WCHAR)];
    ULONG FileSecuritySize;
    FSP_FSCTL_DECLSPEC_ALIGN UINT8 FileSecurityBuf[1024];
};
FSP_FSCTL_STATIC_ASSERT(
    sizeof ((struct fuse *)0)->VolumeLabel == sizeof ((struct fsp_fuse_core_opt_data *)0)->VolumeLabel,
    "fuse::VolumeLabel and fsp_fuse_core_opt_data::VolumeLabel: sizeof must be same.");
int fsp_fuse_core_opt_parse(struct fsp_fuse_env *env,
    struct fuse_args *args, struct fsp_fuse_core_opt_data *opt_data,
    int help);

/* misc public symbols */
NTSTATUS fsp_fuse_op_enter(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_leave(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
int fsp_fuse_intf_CanDeleteAddDirInfo(void *buf, const char *name,
    const struct fuse_stat *stbuf, fuse_off_t off);
int fsp_fuse_intf_AddDirInfo(void *buf, const char *name,
    const struct fuse_stat *stbuf, fuse_off_t off);
NTSTATUS fsp_fuse_get_token_uidgid(
    HANDLE Token,
    TOKEN_INFORMATION_CLASS UserOrOwnerClass, /* TokenUser|TokenOwner */
    PUINT32 PUid, PUINT32 PGid);
extern FSP_FILE_SYSTEM_INTERFACE fsp_fuse_intf;

#endif
