/**
 * @file fuse/fuse_common.h
 * WinFsp FUSE compatible API.
 *
 * This file is derived from libfuse/include/fuse_common.h:
 *     FUSE: Filesystem in Userspace
 *     Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
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

#ifndef FUSE_COMMON_H_
#define FUSE_COMMON_H_

#include <stdint.h>
#include "fuse_opt.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(FSP_FUSE_API)
#if defined(WINFSP_DLL_INTERNAL)
#define FSP_FUSE_API                    __declspec(dllexport)
#else
#define FSP_FUSE_API                    __declspec(dllimport)
#endif
#endif

#if !defined(FSP_FUSE_MEMFN_P)
#define FSP_FUSE_MEMFN_P                void *(*memalloc)(size_t), void (*memfree)(void *)
#define FSP_FUSE_MEMFN_A                memalloc, memfree
#if defined(WINFSP_DLL_INTERNAL)
#define FSP_FUSE_MEMFN_V                MemAlloc, MemFree
#else
#define FSP_FUSE_MEMFN_V                malloc, free
#endif
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

#define FUSE_IOCTL_COMPAT               (1 << 0)
#define FUSE_IOCTL_UNRESTRICTED         (1 << 1)
#define FUSE_IOCTL_RETRY                (1 << 2)
#define FUSE_IOCTL_MAX_IOV              256

/*
 * FUSE uses a number of types (notably: struct stat) that are OS specific.
 * Furthermore there are sometimes multiple definitions of the same type even
 * within the same OS. This is certainly true on Windows, where these types
 * are not even native.
 *
 * For this reason we will define our own fuse_* types which represent the
 * types as the WinFsp DLL expects to see them. When the file is included
 * by FUSE clients in different environments we will translate between their
 * understanding of the types and ours.
 */
#if defined(_MSC_VER)
typedef uint32_t fuse_uid_t;
typedef uint32_t fuse_gid_t;
typedef int32_t fuse_pid_t;

typedef uint64_t fuse_dev_t;
typedef uint64_t fuse_ino_t;
typedef uint32_t fuse_mode_t;
typedef uint32_t fuse_nlink_t;
typedef int64_t fuse_off_t;

typedef uint64_t fuse_fsblkcnt_t;
typedef uint64_t fuse_fsfilcnt_t;
typedef int32_t fuse_blksize_t;
typedef int32_t fuse_blkcnt_t;

struct fuse_timespec
{
    time_t tv_sec;
    int tv_nsec;
};

struct fuse_stat
{
    fuse_dev_t st_dev;
    fuse_ino_t st_ino;
    fuse_mode_t st_mode;
    fuse_nlink_t st_nlink;
    fuse_uid_t st_uid;
    fuse_gid_t st_gid;
    fuse_dev_t st_rdev;
    fuse_off_t st_size;
    struct fuse_timespec st_atim;
    struct fuse_timespec st_mtim;
    struct fuse_timespec st_ctim;
    fuse_blksize_t st_blksize;
    fuse_blkcnt_t st_blocks;
    struct fuse_timespec st_birthtim;
};

struct fuse_statvfs
{
    unsigned int f_bsize;
    unsigned int f_frsize;
    fuse_fsblkcnt_t f_blocks;
    fuse_fsblkcnt_t f_bfree;
    fuse_fsblkcnt_t f_bavail;
    fuse_fsfilcnt_t f_files;
    fuse_fsfilcnt_t f_ffree;
    fuse_fsfilcnt_t f_favail;
    unsigned int f_fsid;
    unsigned int f_flag;
    unsigned int f_namemax;
};

#define FSP_FUSE_ENVIRONMENT            'W'

#elif defined(__CYGWIN__)

#include <errno.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <utime.h>

#define fuse_uid_t                      uid_t
#define fuse_gid_t                      gid_t
#define fuse_pid_t                      pid_t

#define fuse_dev_t                      dev_t
#define fuse_ino_t                      ino_t
#define fuse_mode_t                     mode_t
#define fuse_nlink_t                    nlink_t
#define fuse_off_t                      off_t

#define fuse_fsblkcnt_t                 fsblkcnt_t
#define fuse_fsfilcnt_t                 fsfilcnt_t
#define fuse_blksize_t                  blksize_t
#define fuse_blkcnt_t                   blkcnt_t

#define fuse_timespec                   timespec

#define fuse_stat                       stat
#define fuse_statvfs                    statvfs

#define FSP_FUSE_ENVIRONMENT            'C'

#else
#error unsupported environment
#endif

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

FSP_FUSE_API int fsp_fuse_version(void);
FSP_FUSE_API struct fuse_chan *fsp_fuse_mount(const char *mountpoint, struct fuse_args *args);
FSP_FUSE_API void fsp_fuse_unmount(const char *mountpoint, struct fuse_chan *ch);
FSP_FUSE_API int fsp_fuse_parse_cmdline(struct fuse_args *args, char **mountpoint,
    int *multithreaded, int *foreground,
    FSP_FUSE_MEMFN_P);

static inline int fuse_version(void)
{
    return fsp_fuse_version();
}

static inline struct fuse_chan *fuse_mount(const char *mountpoint, struct fuse_args *args)
{
    return fsp_fuse_mount(mountpoint, args);
}

static inline void fuse_unmount(const char *mountpoint, struct fuse_chan *ch)
{
    fsp_fuse_unmount(mountpoint, ch);
}

static inline int fuse_parse_cmdline(struct fuse_args *args, char **mountpoint,
    int *multithreaded, int *foreground)
{
    return fsp_fuse_parse_cmdline(args, mountpoint, multithreaded, foreground,
        FSP_FUSE_MEMFN_V);
}

static inline void fuse_pollhandle_destroy(struct fuse_pollhandle *ph)
{
    (void)ph;
}

static inline int fuse_daemonize(int foreground)
{
    (void)foreground;
    return 0;
}

static inline int fuse_set_signal_handlers(struct fuse_session *se)
{
    (void)se;
    return 0;
}

static inline void fuse_remove_signal_handlers(struct fuse_session *se)
{
    (void)se;
}

#ifdef __cplusplus
}
#endif

#endif
