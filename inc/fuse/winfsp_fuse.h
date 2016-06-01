/**
 * @file fuse/winfsp_fuse.h
 * WinFsp FUSE compatible API.
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

#ifndef FUSE_WINFSP_FUSE_H_INCLUDED
#define FUSE_WINFSP_FUSE_H_INCLUDED

#include <stdint.h>
#if !defined(WINFSP_DLL_INTERNAL)
#include <stdlib.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(WINFSP_DLL_INTERNAL)
#define FSP_FUSE_API                    __declspec(dllexport)
#else
#define FSP_FUSE_API                    __declspec(dllimport)
#endif

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

#if defined(_WIN64) || defined(_WIN32)

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

#if defined(WINFSP_DLL_INTERNAL)
#define FSP_FUSE_ENV_INIT               \
    {                                   \
        'W',                            \
        MemAlloc, MemFree,              \
        fsp_fuse_daemonize,             \
        fsp_fuse_set_signal_handlers,   \
        fsp_fuse_remove_signal_handlers,\
    }
#else
#define FSP_FUSE_ENV_INIT               \
    {                                   \
        'W',                            \
        malloc, free,                   \
        fsp_fuse_daemonize,             \
        fsp_fuse_set_signal_handlers,   \
        fsp_fuse_remove_signal_handlers,\
    }
#endif

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

#define FSP_FUSE_ENV_INIT               \
    {                                   \
        'C',                            \
        malloc, free,                   \
        fsp_fuse_daemonize,             \
        fsp_fuse_set_signal_handlers,   \
        fsp_fuse_remove_signal_handlers,\
    }

/*
 * Note that long is 8 bytes long in Cygwin64 and 4 bytes long in Win64.
 * For this reason we avoid using long anywhere in these headers.
 */

#else
#error unsupported environment
#endif

struct fsp_fuse_env
{
    unsigned environment;
    void *(*memalloc)(size_t);
    void (*memfree)(void *);
    int (*daemonize)(int);
    int (*set_signal_handlers)(void *);
    void (*remove_signal_handlers)(void *);
};

static inline int fsp_fuse_daemonize(int foreground)
{
#if defined(_WIN64) || defined(_WIN32)
    (void)foreground;
    return 0;
#elif defined(__CYGWIN__)
    int daemon(int nochdir, int noclose);
    int chdir(const char *path);
    if (!foreground)
    {
        if (-1 == daemon(0, 0))
            return -1;
    }
    else
        chdir("/");
    return 0;
#endif
}

static inline int fsp_fuse_set_signal_handlers(void *se)
{
    (void)se;
    return 0;
}

static inline void fsp_fuse_remove_signal_handlers(void *se)
{
    (void)se;
}

static inline struct fsp_fuse_env *fsp_fuse_env(void)
{
    static struct fsp_fuse_env env = FSP_FUSE_ENV_INIT;
    return &env;
}

#ifdef __cplusplus
}
#endif

#endif
