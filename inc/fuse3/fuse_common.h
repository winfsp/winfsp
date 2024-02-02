/**
 * @file fuse3/fuse_common.h
 * WinFsp FUSE3 compatible API.
 *
 * This file is derived from libfuse/include/fuse_common.h:
 *     FUSE: Filesystem in Userspace
 *     Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
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

#ifndef FUSE_COMMON_H_
#define FUSE_COMMON_H_

#include "winfsp_fuse.h"
#if !defined(WINFSP_DLL_INTERNAL)
#include "fuse_opt.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define FUSE_MAJOR_VERSION              3
#define FUSE_MINOR_VERSION              2
#define FUSE_MAKE_VERSION(maj, min)     ((maj) * 10 + (min))
#define FUSE_VERSION                    FUSE_MAKE_VERSION(FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION)

#define FUSE_CAP_ASYNC_READ             (1 << 0)
#define FUSE_CAP_POSIX_LOCKS            (1 << 1)
#define FUSE_CAP_ATOMIC_O_TRUNC         (1 << 3)
#define FUSE_CAP_EXPORT_SUPPORT         (1 << 4)
#define FUSE_CAP_DONT_MASK              (1 << 6)
#define FUSE_CAP_SPLICE_WRITE           (1 << 7)
#define FUSE_CAP_SPLICE_MOVE            (1 << 8)
#define FUSE_CAP_SPLICE_READ            (1 << 9)
#define FUSE_CAP_FLOCK_LOCKS            (1 << 10)
#define FUSE_CAP_IOCTL_DIR              (1 << 11)
#define FUSE_CAP_AUTO_INVAL_DATA        (1 << 12)
#define FUSE_CAP_READDIRPLUS            (1 << 13)
#define FUSE_CAP_READDIRPLUS_AUTO       (1 << 14)
#define FUSE_CAP_ASYNC_DIO              (1 << 15)
#define FUSE_CAP_WRITEBACK_CACHE        (1 << 16)
#define FUSE_CAP_NO_OPEN_SUPPORT        (1 << 17)
#define FUSE_CAP_PARALLEL_DIROPS        (1 << 18)
#define FUSE_CAP_POSIX_ACL              (1 << 19)
#define FUSE_CAP_HANDLE_KILLPRIV        (1 << 20)
#define FUSE_CAP_ALLOCATE               (1 << 27)   /* reserved (OSXFUSE) */
#define FUSE_CAP_EXCHANGE_DATA          (1 << 28)   /* reserved (OSXFUSE) */
#define FUSE_CAP_CASE_INSENSITIVE       (1 << 29)   /* file system is case insensitive */
#define FUSE_CAP_VOL_RENAME             (1 << 30)   /* reserved (OSXFUSE) */
#define FUSE_CAP_XTIMES                 (1 << 31)   /* reserved (OSXFUSE) */

#define FSP_FUSE_CAP_CASE_INSENSITIVE   FUSE_CAP_CASE_INSENSITIVE

#define FUSE_IOCTL_COMPAT               (1 << 0)
#define FUSE_IOCTL_UNRESTRICTED         (1 << 1)
#define FUSE_IOCTL_RETRY                (1 << 2)
#define FUSE_IOCTL_DIR                  (1 << 4)
#define FUSE_IOCTL_MAX_IOV              256

#define FUSE_BUFVEC_INIT(s)             \
    ((struct fuse3_bufvec){ 1, 0, 0, { {s, (enum fuse3_buf_flags)0, 0, -1, 0} } })

struct fuse3_file_info
{
    int flags;
    unsigned int writepage:1;
    unsigned int direct_io:1;
    unsigned int keep_cache:1;
    unsigned int flush:1;
    unsigned int nonseekable:1;
    unsigned int flock_release:1;
    unsigned int padding:27;
    uintptr_t fh;
    uint64_t lock_owner;
    uint32_t poll_events;
};

struct fuse3_loop_config
{
    int clone_fd;
    unsigned int max_idle_threads;
};

struct fuse3_conn_info
{
    unsigned proto_major;
    unsigned proto_minor;
    unsigned max_write;
    unsigned max_read;
    unsigned max_readahead;
    unsigned capable;
    unsigned want;
    unsigned max_background;
    unsigned congestion_threshold;
    unsigned time_gran;
    unsigned reserved[22];
};

enum fuse3_buf_flags
{
    FUSE_BUF_IS_FD                      = (1 << 1),
    FUSE_BUF_FD_SEEK                    = (1 << 2),
    FUSE_BUF_FD_RETRY                   = (1 << 3),
};

enum fuse3_buf_copy_flags
{
    FUSE_BUF_NO_SPLICE                  = (1 << 1),
    FUSE_BUF_FORCE_SPLICE               = (1 << 2),
    FUSE_BUF_SPLICE_MOVE                = (1 << 3),
    FUSE_BUF_SPLICE_NONBLOCK            = (1 << 4),
};

struct fuse3_buf
{
    size_t size;
    enum fuse3_buf_flags flags;
    void *mem;
    int fd;
    fuse_off_t pos;
};

struct fuse3_bufvec
{
    size_t count;
    size_t idx;
    size_t off;
    struct fuse3_buf buf[1];
};

struct fuse3_session;
struct fuse3_pollhandle;
struct fuse3_conn_info_opts;

FSP_FUSE_API struct fuse3_conn_info_opts *FSP_FUSE_API_NAME(fsp_fuse3_parse_conn_info_opts)(
    struct fsp_fuse_env *env,
    struct fuse_args *args);
FSP_FUSE_API void FSP_FUSE_API_NAME(fsp_fuse3_apply_conn_info_opts)(struct fsp_fuse_env *env,
    struct fuse3_conn_info_opts *opts, struct fuse3_conn_info *conn);
FSP_FUSE_API int FSP_FUSE_API_NAME(fsp_fuse3_version)(struct fsp_fuse_env *env);
FSP_FUSE_API const char *FSP_FUSE_API_NAME(fsp_fuse3_pkgversion)(struct fsp_fuse_env *env);
FSP_FUSE_API int32_t FSP_FUSE_API_NAME(fsp_fuse_ntstatus_from_errno)(struct fsp_fuse_env *env,
    int err);

FSP_FUSE_SYM(
struct fuse3_conn_info_opts* fuse3_parse_conn_info_opts(
    struct fuse_args *args),
{
    return FSP_FUSE_API_CALL(fsp_fuse3_parse_conn_info_opts)
        (fsp_fuse_env(), args);
})

FSP_FUSE_SYM(
void fuse3_apply_conn_info_opts(
    struct fuse3_conn_info_opts *opts, struct fuse3_conn_info *conn),
{
    FSP_FUSE_API_CALL(fsp_fuse3_apply_conn_info_opts)
        (fsp_fuse_env(), opts, conn);
})

FSP_FUSE_SYM(
int fuse3_version(void),
{
    return FSP_FUSE_API_CALL(fsp_fuse3_version)
        (fsp_fuse_env());
})

FSP_FUSE_SYM(
const char *fuse3_pkgversion(void),
{
    return FSP_FUSE_API_CALL(fsp_fuse3_pkgversion)
        (fsp_fuse_env());
})

FSP_FUSE_SYM(
void fuse3_pollhandle_destroy(struct fuse3_pollhandle *ph),
{
    (void)ph;
})

FSP_FUSE_SYM(
size_t fuse3_buf_size(const struct fuse3_bufvec *bufv),
{
    (void)bufv;
    return 0;
})

FSP_FUSE_SYM(
ssize_t fuse3_buf_copy(struct fuse3_bufvec *dst, struct fuse3_bufvec *src,
    enum fuse3_buf_copy_flags flags),
{
    (void)dst;
    (void)src;
    (void)flags;
    return 0;
})

FSP_FUSE_SYM(
int fuse3_daemonize(int foreground),
{
    return fsp_fuse_daemonize(foreground);
})

FSP_FUSE_SYM(
int fuse3_set_signal_handlers(struct fuse3_session *se),
{
    return fsp_fuse_set_signal_handlers(se);
})

FSP_FUSE_SYM(
void fuse3_remove_signal_handlers(struct fuse3_session *se),
{
    (void)se;
    fsp_fuse_set_signal_handlers(0);
})

#ifdef __cplusplus
}
#endif

#endif
