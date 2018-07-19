/**
 * @file dll/fuse/library.h
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

#define ENOSYS_(env)                    ('C' == (env)->environment ? 88 : 40)

struct fuse
{
    struct fsp_fuse_env *env;
    int set_umask, umask;
    int set_create_umask, create_umask;
    int set_uid, uid;
    int set_gid, gid;
    int rellinks;
    unsigned ThreadCount;
    struct fuse_operations ops;
    void *data;
    unsigned conn_want;
    BOOLEAN fsinit;
    BOOLEAN has_symlinks;
    UINT32 DebugLog;
    FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY OpGuardStrategy;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    UINT16 VolumeLabelLength;
    WCHAR VolumeLabel[sizeof ((FSP_FSCTL_VOLUME_INFO *)0)->VolumeLabel / sizeof(WCHAR)];
    PWSTR MountPoint;
    FSP_FILE_SYSTEM *FileSystem;
    FSP_SERVICE *Service; /* weak */
    volatile int exited;
};

struct fsp_fuse_context_header
{
    char *PosixPath;
    struct fuse3 *fuse3;
    __declspec(align(MEMORY_ALLOCATION_ALIGNMENT)) UINT8 ContextBuf[];
};

struct fsp_fuse_file_desc
{
    char *PosixPath;
    BOOLEAN IsDirectory, IsReparsePoint;
    int OpenFlags;
    UINT64 FileHandle;
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

NTSTATUS fsp_fuse_op_enter(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_leave(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);

int fsp_fuse_intf_AddDirInfo(void *buf, const char *name,
    const struct fuse_stat *stbuf, fuse_off_t off);

extern FSP_FILE_SYSTEM_INTERFACE fsp_fuse_intf;

NTSTATUS fsp_fuse_get_token_uidgid(
    HANDLE Token,
    TOKEN_INFORMATION_CLASS UserOrOwnerClass, /* TokenUser|TokenOwner */
    PUINT32 PUid, PUINT32 PGid);

/* NFS reparse points */

#define NFS_SPECFILE_FIFO               0x000000004F464946
#define NFS_SPECFILE_CHR                0x0000000000524843
#define NFS_SPECFILE_BLK                0x00000000004b4c42
#define NFS_SPECFILE_LNK                0x00000000014b4e4c
#define NFS_SPECFILE_SOCK               0x000000004B434F53

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

struct fuse_context *fsp_fuse_get_context_internal(void);

struct fsp_fuse_core_opt_data
{
    struct fsp_fuse_env *env;
    int help, debug;
    HANDLE DebugLogHandle;
    int set_umask, umask,
        set_create_umask, create_umask,
        set_uid, uid,
        set_gid, gid,
        set_attr_timeout, attr_timeout,
        rellinks;
    int set_FileInfoTimeout,
        set_DirInfoTimeout,
        set_VolumeInfoTimeout,
        set_KeepFileCache;
    unsigned ThreadCount;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    UINT16 VolumeLabelLength;
    WCHAR VolumeLabel[sizeof ((FSP_FSCTL_VOLUME_INFO *)0)->VolumeLabel / sizeof(WCHAR)];
};
FSP_FSCTL_STATIC_ASSERT(
    sizeof ((struct fuse *)0)->VolumeLabel == sizeof ((struct fsp_fuse_core_opt_data *)0)->VolumeLabel,
    "fuse::VolumeLabel and fsp_fuse_core_opt_data::VolumeLabel: sizeof must be same.");

int fsp_fuse_core_opt_parse(struct fsp_fuse_env *env,
    struct fuse_args *args, struct fsp_fuse_core_opt_data *opt_data,
    int help);

#endif
