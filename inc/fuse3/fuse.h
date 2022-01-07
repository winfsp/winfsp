/**
 * @file fuse3/fuse.h
 * WinFsp FUSE3 compatible API.
 *
 * This file is derived from libfuse/include/fuse.h:
 *     FUSE: Filesystem in Userspace
 *     Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
 *
 * @copyright 2015-2022 Bill Zissimopoulos
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

#ifndef FUSE_H_
#define FUSE_H_

#include "fuse_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct fuse3;

enum fuse3_readdir_flags
{
    FUSE_READDIR_PLUS                   = (1 << 0),
};

enum fuse3_fill_dir_flags
{
    FUSE_FILL_DIR_PLUS                  = (1 << 1),
};

typedef int (*fuse3_fill_dir_t)(void *buf, const char *name,
    const struct fuse_stat *stbuf, fuse_off_t off,
    enum fuse3_fill_dir_flags flags);

struct fuse3_config
{
    int set_gid;
    unsigned int gid;
    int set_uid;
    unsigned int uid;
    int set_mode;
    unsigned int umask;
    double entry_timeout;
    double negative_timeout;
    double attr_timeout;
    int intr;
    int intr_signal;
    int remember;
    int hard_remove;
    int use_ino;
    int readdir_ino;
    int direct_io;
    int kernel_cache;
    int auto_cache;
    int ac_attr_timeout_set;
    double ac_attr_timeout;
    int nullpath_ok;
    /* private */
    int show_help;
    char *modules;
    int debug;
};

struct fuse3_operations
{
    /* S - supported by WinFsp */
    /* S */ int (*getattr)(const char *path, struct fuse_stat *stbuf,
        struct fuse3_file_info *fi);
    /* S */ int (*readlink)(const char *path, char *buf, size_t size);
    /* S */ int (*mknod)(const char *path, fuse_mode_t mode, fuse_dev_t dev);
    /* S */ int (*mkdir)(const char *path, fuse_mode_t mode);
    /* S */ int (*unlink)(const char *path);
    /* S */ int (*rmdir)(const char *path);
    /* S */ int (*symlink)(const char *dstpath, const char *srcpath);
    /* S */ int (*rename)(const char *oldpath, const char *newpath, unsigned int flags);
    /* _ */ int (*link)(const char *srcpath, const char *dstpath);
    /* S */ int (*chmod)(const char *path, fuse_mode_t mode,
        struct fuse3_file_info *fi);
    /* S */ int (*chown)(const char *path, fuse_uid_t uid, fuse_gid_t gid,
        struct fuse3_file_info *fi);
    /* S */ int (*truncate)(const char *path, fuse_off_t size,
        struct fuse3_file_info *fi);
    /* S */ int (*open)(const char *path, struct fuse3_file_info *fi);
    /* S */ int (*read)(const char *path, char *buf, size_t size, fuse_off_t off,
        struct fuse3_file_info *fi);
    /* S */ int (*write)(const char *path, const char *buf, size_t size, fuse_off_t off,
        struct fuse3_file_info *fi);
    /* S */ int (*statfs)(const char *path, struct fuse_statvfs *stbuf);
    /* S */ int (*flush)(const char *path, struct fuse3_file_info *fi);
    /* S */ int (*release)(const char *path, struct fuse3_file_info *fi);
    /* S */ int (*fsync)(const char *path, int datasync, struct fuse3_file_info *fi);
    /* S */ int (*setxattr)(const char *path, const char *name, const char *value, size_t size,
        int flags);
    /* S */ int (*getxattr)(const char *path, const char *name, char *value, size_t size);
    /* S */ int (*listxattr)(const char *path, char *namebuf, size_t size);
    /* S */ int (*removexattr)(const char *path, const char *name);
    /* S */ int (*opendir)(const char *path, struct fuse3_file_info *fi);
    /* S */ int (*readdir)(const char *path, void *buf, fuse3_fill_dir_t filler, fuse_off_t off,
        struct fuse3_file_info *fi, enum fuse3_readdir_flags);
    /* S */ int (*releasedir)(const char *path, struct fuse3_file_info *fi);
    /* S */ int (*fsyncdir)(const char *path, int datasync, struct fuse3_file_info *fi);
    /* S */ void *(*init)(struct fuse3_conn_info *conn,
        struct fuse3_config *conf);
    /* S */ void (*destroy)(void *data);
    /* _ */ int (*access)(const char *path, int mask);
    /* S */ int (*create)(const char *path, fuse_mode_t mode, struct fuse3_file_info *fi);
    /* _ */ int (*lock)(const char *path,
        struct fuse3_file_info *fi, int cmd, struct fuse_flock *lock);
    /* S */ int (*utimens)(const char *path, const struct fuse_timespec tv[2],
        struct fuse3_file_info *fi);
    /* _ */ int (*bmap)(const char *path, size_t blocksize, uint64_t *idx);
    /* S */ int (*ioctl)(const char *path, int cmd, void *arg, struct fuse3_file_info *fi,
        unsigned int flags, void *data);
    /* _ */ int (*poll)(const char *path, struct fuse3_file_info *fi,
        struct fuse3_pollhandle *ph, unsigned *reventsp);
    /* _ */ int (*write_buf)(const char *path,
        struct fuse3_bufvec *buf, fuse_off_t off, struct fuse3_file_info *fi);
    /* _ */ int (*read_buf)(const char *path,
        struct fuse3_bufvec **bufp, size_t size, fuse_off_t off, struct fuse3_file_info *fi);
    /* _ */ int (*flock)(const char *path, struct fuse3_file_info *, int op);
    /* _ */ int (*fallocate)(const char *path, int mode, fuse_off_t off, fuse_off_t len,
        struct fuse3_file_info *fi);
};

struct fuse3_context
{
    struct fuse3 *fuse;
    fuse_uid_t uid;
    fuse_gid_t gid;
    fuse_pid_t pid;
    void *private_data;
    fuse_mode_t umask;
};

#define fuse_main(argc, argv, ops, data)\
    fuse3_main_real(argc, argv, ops, sizeof *(ops), data)

FSP_FUSE_API int FSP_FUSE_API_NAME(fsp_fuse3_main_real)(struct fsp_fuse_env *env,
    int argc, char *argv[],
    const struct fuse3_operations *ops, size_t opsize, void *data);
FSP_FUSE_API void FSP_FUSE_API_NAME(fsp_fuse3_lib_help)(struct fsp_fuse_env *env,
    struct fuse_args *args);
FSP_FUSE_API struct fuse3 *FSP_FUSE_API_NAME(fsp_fuse3_new_30)(struct fsp_fuse_env *env,
    struct fuse_args *args,
    const struct fuse3_operations *ops, size_t opsize, void *data);
FSP_FUSE_API struct fuse3 *FSP_FUSE_API_NAME(fsp_fuse3_new)(struct fsp_fuse_env *env,
    struct fuse_args *args,
    const struct fuse3_operations *ops, size_t opsize, void *data);
FSP_FUSE_API void FSP_FUSE_API_NAME(fsp_fuse3_destroy)(struct fsp_fuse_env *env,
    struct fuse3 *f);
FSP_FUSE_API int FSP_FUSE_API_NAME(fsp_fuse3_mount)(struct fsp_fuse_env *env,
    struct fuse3 *f, const char *mountpoint);
FSP_FUSE_API void FSP_FUSE_API_NAME(fsp_fuse3_unmount)(struct fsp_fuse_env *env,
    struct fuse3 *f);
FSP_FUSE_API int FSP_FUSE_API_NAME(fsp_fuse3_loop)(struct fsp_fuse_env *env,
    struct fuse3 *f);
FSP_FUSE_API int FSP_FUSE_API_NAME(fsp_fuse3_loop_mt_31)(struct fsp_fuse_env *env,
    struct fuse3 *f, int clone_fd);
FSP_FUSE_API int FSP_FUSE_API_NAME(fsp_fuse3_loop_mt)(struct fsp_fuse_env *env,
    struct fuse3 *f, struct fuse3_loop_config *config);
FSP_FUSE_API void FSP_FUSE_API_NAME(fsp_fuse3_exit)(struct fsp_fuse_env *env,
    struct fuse3 *f);
FSP_FUSE_API struct fuse3_context *FSP_FUSE_API_NAME(fsp_fuse3_get_context)(struct fsp_fuse_env *env);

FSP_FUSE_SYM(
int fuse3_main_real(int argc, char *argv[],
    const struct fuse3_operations *ops, size_t opsize, void *data),
{
    return FSP_FUSE_API_CALL(fsp_fuse3_main_real)
        (fsp_fuse_env(), argc, argv, ops, opsize, data);
})

FSP_FUSE_SYM(
void fuse3_lib_help(struct fuse_args *args),
{
    FSP_FUSE_API_CALL(fsp_fuse3_lib_help)
        (fsp_fuse_env(), args);
})

#if FUSE_USE_VERSION == 30
FSP_FUSE_SYM(
struct fuse3 *fuse3_new_30(struct fuse_args *args,
    const struct fuse3_operations *ops, size_t opsize, void *data),
{
    return FSP_FUSE_API_CALL(fsp_fuse3_new_30)
        (fsp_fuse_env(), args, ops, opsize, data);
})
#define fuse_new(args, op, size, data)\
    fuse3_new_30(args, op, size, data)

#else
FSP_FUSE_SYM(
struct fuse3 *fuse3_new(struct fuse_args *args,
    const struct fuse3_operations *ops, size_t opsize, void *data),
{
    return FSP_FUSE_API_CALL(fsp_fuse3_new)
        (fsp_fuse_env(), args, ops, opsize, data);
})
#endif

FSP_FUSE_SYM(
void fuse3_destroy(struct fuse3 *f),
{
    FSP_FUSE_API_CALL(fsp_fuse3_destroy)
        (fsp_fuse_env(), f);
})

FSP_FUSE_SYM(
int fuse3_mount(struct fuse3 *f, const char *mountpoint),
{
    return FSP_FUSE_API_CALL(fsp_fuse3_mount)
        (fsp_fuse_env(), f, mountpoint);
})

FSP_FUSE_SYM(
void fuse3_unmount(struct fuse3 *f),
{
    FSP_FUSE_API_CALL(fsp_fuse3_unmount)
        (fsp_fuse_env(), f);
})

FSP_FUSE_SYM(
int fuse3_loop(struct fuse3 *f),
{
    return FSP_FUSE_API_CALL(fsp_fuse3_loop)
        (fsp_fuse_env(), f);
})

#if FUSE_USE_VERSION < 32
FSP_FUSE_SYM(
int fuse3_loop_mt_31(struct fuse3 *f, int clone_fd),
{
    return FSP_FUSE_API_CALL(fsp_fuse3_loop_mt_31)
        (fsp_fuse_env(), f, clone_fd);
})
#define fuse_loop_mt(f, clone_fd)\
    fuse3_loop_mt_31(f, clone_fd)

#else
FSP_FUSE_SYM(
int fuse3_loop_mt(struct fuse3 *f, struct fuse3_loop_config *config),
{
    return FSP_FUSE_API_CALL(fsp_fuse3_loop_mt)
        (fsp_fuse_env(), f, config);
})
#endif

FSP_FUSE_SYM(
void fuse3_exit(struct fuse3 *f),
{
    FSP_FUSE_API_CALL(fsp_fuse3_exit)
        (fsp_fuse_env(), f);
})

FSP_FUSE_SYM(
struct fuse3_context *fuse3_get_context(void),
{
    return FSP_FUSE_API_CALL(fsp_fuse3_get_context)
        (fsp_fuse_env());
})

FSP_FUSE_SYM(
int fuse3_getgroups(int size, fuse_gid_t list[]),
{
    (void)size;
    (void)list;
    return -ENOSYS;
})

FSP_FUSE_SYM(
int fuse3_interrupted(void),
{
    return 0;
})

FSP_FUSE_SYM(
int fuse3_invalidate_path(struct fuse3 *f, const char *path),
{
    (void)f;
    (void)path;
    return -ENOENT;
})

FSP_FUSE_SYM(
int fuse3_notify_poll(struct fuse3_pollhandle *ph),
{
    (void)ph;
    return 0;
})

FSP_FUSE_SYM(
int fuse3_start_cleanup_thread(struct fuse3 *f),
{
    (void)f;
    return 0;
})

FSP_FUSE_SYM(
void fuse3_stop_cleanup_thread(struct fuse3 *f),
{
    (void)f;
})

FSP_FUSE_SYM(
int fuse3_clean_cache(struct fuse3 *f),
{
    (void)f;
    return 600;
})

FSP_FUSE_SYM(
struct fuse3_session *fuse3_get_session(struct fuse3 *f),
{
    return (struct fuse3_session *)f;
})

#ifdef __cplusplus
}
#endif

#endif
