/**
 * @file fuse/fuse_common.h
 * WinFsp FUSE compatible API.
 *
 * This file is derived from libfuse/include/fuse_common.h:
 *     FUSE: Filesystem in Userspace
 *     Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
 *
 * @copyright 2015-2017 Bill Zissimopoulos
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

#ifndef FUSE_COMMON_H_
#define FUSE_COMMON_H_

#include "winfsp_fuse.h"
#include "fuse_opt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FUSE_MAJOR_VERSION              2
#define FUSE_MINOR_VERSION              8
#define FUSE_MAKE_VERSION(maj, min)     ((maj) * 10 + (min))
#define FUSE_VERSION                    FUSE_MAKE_VERSION(FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION)

#define FUSE_CAP_ASYNC_READ             (1 << 0)
#define FUSE_CAP_POSIX_LOCKS            (1 << 1)
#define FUSE_CAP_ATOMIC_O_TRUNC         (1 << 3)
#define FUSE_CAP_EXPORT_SUPPORT         (1 << 4)
#define FUSE_CAP_BIG_WRITES             (1 << 5)
#define FUSE_CAP_DONT_MASK              (1 << 6)
#define FUSE_CAP_ALLOCATE               (1 << 27)   /* reserved (OSXFUSE) */
#define FUSE_CAP_EXCHANGE_DATA          (1 << 28)   /* reserved (OSXFUSE) */
#define FUSE_CAP_CASE_INSENSITIVE       (1 << 29)   /* file system is case insensitive */
#define FUSE_CAP_VOL_RENAME             (1 << 30)   /* reserved (OSXFUSE) */
#define FUSE_CAP_XTIMES                 (1 << 31)   /* reserved (OSXFUSE) */

#define FSP_FUSE_CAP_READDIR_PLUS       (1 << 21)   /* file system supports enhanced readdir */
#define FSP_FUSE_CAP_READ_ONLY          (1 << 22)   /* file system is marked read-only */
#define FSP_FUSE_CAP_CASE_INSENSITIVE   FUSE_CAP_CASE_INSENSITIVE

#define FUSE_IOCTL_COMPAT               (1 << 0)
#define FUSE_IOCTL_UNRESTRICTED         (1 << 1)
#define FUSE_IOCTL_RETRY                (1 << 2)
#define FUSE_IOCTL_MAX_IOV              256

struct fuse_file_info
{
    int flags;
    unsigned int fh_old;
    int writepage;
    unsigned int direct_io:1;
    unsigned int keep_cache:1;
    unsigned int flush:1;
    unsigned int nonseekable:1;
    unsigned int padding:28;
    uint64_t fh;
    uint64_t lock_owner;
};

struct fuse_conn_info
{
    unsigned proto_major;
    unsigned proto_minor;
    unsigned async_read;
    unsigned max_write;
    unsigned max_readahead;
    unsigned capable;
    unsigned want;
    unsigned reserved[25];
};

struct fuse_session;
struct fuse_chan;
struct fuse_pollhandle;

FSP_FUSE_API int FSP_FUSE_API_NAME(fsp_fuse_version)(struct fsp_fuse_env *env);
FSP_FUSE_API struct fuse_chan *FSP_FUSE_API_NAME(fsp_fuse_mount)(struct fsp_fuse_env *env,
    const char *mountpoint, struct fuse_args *args);
FSP_FUSE_API void FSP_FUSE_API_NAME(fsp_fuse_unmount)(struct fsp_fuse_env *env,
    const char *mountpoint, struct fuse_chan *ch);
FSP_FUSE_API int FSP_FUSE_API_NAME(fsp_fuse_parse_cmdline)(struct fsp_fuse_env *env,
    struct fuse_args *args,
    char **mountpoint, int *multithreaded, int *foreground);
FSP_FUSE_API int32_t FSP_FUSE_API_NAME(fsp_fuse_ntstatus_from_errno)(struct fsp_fuse_env *env,
    int err);

FSP_FUSE_SYM(
int fuse_version(void),
{
    return FSP_FUSE_API_CALL(fsp_fuse_version)
        (fsp_fuse_env());
})

FSP_FUSE_SYM(
struct fuse_chan *fuse_mount(const char *mountpoint, struct fuse_args *args),
{
    return FSP_FUSE_API_CALL(fsp_fuse_mount)
        (fsp_fuse_env(), mountpoint, args);
})

FSP_FUSE_SYM(
void fuse_unmount(const char *mountpoint, struct fuse_chan *ch),
{
    FSP_FUSE_API_CALL(fsp_fuse_unmount)
        (fsp_fuse_env(), mountpoint, ch);
})

FSP_FUSE_SYM(
int fuse_parse_cmdline(struct fuse_args *args,
    char **mountpoint, int *multithreaded, int *foreground),
{
    return FSP_FUSE_API_CALL(fsp_fuse_parse_cmdline)
        (fsp_fuse_env(), args, mountpoint, multithreaded, foreground);
})

FSP_FUSE_SYM(
void fuse_pollhandle_destroy(struct fuse_pollhandle *ph),
{
    (void)ph;
})

FSP_FUSE_SYM(
int fuse_daemonize(int foreground),
{
    return fsp_fuse_daemonize(foreground);
})

FSP_FUSE_SYM(
int fuse_set_signal_handlers(struct fuse_session *se),
{
    return fsp_fuse_set_signal_handlers(se);
})

FSP_FUSE_SYM(
void fuse_remove_signal_handlers(struct fuse_session *se),
{
    (void)se;
    fsp_fuse_set_signal_handlers(0);
})

#ifdef __cplusplus
}
#endif

#endif
