/**
 * @file dll/fuse3/fuse2to3.c
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

#include <dll/fuse3/library.h>

static inline struct fuse3 *fuse2to3_getfuse3(void)
{
    return fsp_fuse_get_context_internal()->fuse->fuse3;
}

static inline void fuse2to3_fi2from3(struct fuse_file_info *fi, struct fuse3_file_info *fi3)
{
    memset(fi, 0, sizeof *fi);
    fi->flags = fi3->flags;
    fi->writepage = fi3->writepage;
    fi->direct_io = fi3->direct_io;
    fi->keep_cache = fi3->keep_cache;
    fi->flush = fi3->flush;
    fi->nonseekable = fi3->nonseekable;
    fi->fh = fi3->fh;
    fi->lock_owner = fi3->lock_owner;
}

static inline void fuse2to3_fi3from2(struct fuse3_file_info *fi3, struct fuse_file_info *fi)
{
    memset(fi3, 0, sizeof *fi3);
    fi3->flags = fi->flags;
    fi3->writepage = fi->writepage;
    fi3->direct_io = fi->direct_io;
    fi3->keep_cache = fi->keep_cache;
    fi3->flush = fi->flush;
    fi3->nonseekable = fi->nonseekable;
    fi3->fh = fi->fh;
    fi3->lock_owner = fi->lock_owner;
}

static inline void fuse2to3_conn3from2(struct fuse3_conn_info *conn3, struct fuse_conn_info *conn)
{
    memset(conn3, 0, sizeof *conn3);
    conn3->proto_major = 7;             /* pretend that we are FUSE kernel protocol 7.26 */
    conn3->proto_minor = 26;            /*     which was current at the time of FUSE 3.2 */
    conn3->max_write = conn->max_write;
    conn3->max_read = conn->max_write;
    conn3->max_readahead = conn->max_readahead;
    conn3->capable = (conn->capable & ~FSP_FUSE_CAP_READDIR_PLUS) | FUSE_CAP_READDIRPLUS;
    conn3->want = conn->want;
}

static int fuse2to3_getattr(const char *path, struct fuse_stat *stbuf)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.getattr(path, stbuf, 0);
}

static int fuse2to3_readlink(const char *path, char *buf, size_t size)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.readlink(path, buf, size);
}

static int fuse2to3_mknod(const char *path, fuse_mode_t mode, fuse_dev_t dev)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.mknod(path, mode, dev);
}

static int fuse2to3_mkdir(const char *path, fuse_mode_t mode)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.mkdir(path, mode);
}

static int fuse2to3_unlink(const char *path)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.unlink(path);
}

static int fuse2to3_rmdir(const char *path)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.rmdir(path);
}

static int fuse2to3_symlink(const char *dstpath, const char *srcpath)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.symlink(dstpath, srcpath);
}

static int fuse2to3_rename(const char *oldpath, const char *newpath)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.rename(oldpath, newpath, 0);
}

static int fuse2to3_link(const char *srcpath, const char *dstpath)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.link(srcpath, dstpath);
}

static int fuse2to3_chmod(const char *path, fuse_mode_t mode)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.chmod(path, mode, 0);
}

static int fuse2to3_chown(const char *path, fuse_uid_t uid, fuse_gid_t gid)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.chown(path, uid, gid, 0);
}

static int fuse2to3_truncate(const char *path, fuse_off_t size)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.truncate(path, size, 0);
}

static int fuse2to3_open(const char *path, struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.open(path, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_read(const char *path, char *buf, size_t size, fuse_off_t off,
    struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.read(path, buf, size, off, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_write(const char *path, const char *buf, size_t size, fuse_off_t off,
    struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.write(path, buf, size, off, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_statfs(const char *path, struct fuse_statvfs *stbuf)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.statfs(path, stbuf);
}

static int fuse2to3_flush(const char *path, struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.flush(path, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_release(const char *path, struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.release(path, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.fsync(path, datasync, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_setxattr(const char *path, const char *name, const char *value, size_t size,
    int flags)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.setxattr(path, name, value, size, flags);
}

static int fuse2to3_getxattr(const char *path, const char *name, char *value, size_t size)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.getxattr(path, name, value, size);
}

static int fuse2to3_listxattr(const char *path, char *namebuf, size_t size)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.listxattr(path, namebuf, size);
}

static int fuse2to3_removexattr(const char *path, const char *name)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.removexattr(path, name);
}

static int fuse2to3_opendir(const char *path, struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.opendir(path, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_candel_filldir(void *buf, const char *name,
    const struct fuse_stat *stbuf, fuse_off_t off,
    enum fuse3_fill_dir_flags flags)
{
    return fsp_fuse_intf_CanDeleteAddDirInfo(buf, name, 0, off);
}

static int fuse2to3_filldir(void *buf, const char *name,
    const struct fuse_stat *stbuf, fuse_off_t off,
    enum fuse3_fill_dir_flags flags)
{
    return 0 != (flags & FUSE_FILL_DIR_PLUS) ?
        fsp_fuse_intf_AddDirInfo(buf, name, stbuf, off) :
        fsp_fuse_intf_AddDirInfo(buf, name, 0, off);
}

static int fuse2to3_readdir(const char *path, void *buf, fuse_fill_dir_t filler, fuse_off_t off,
    struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse_dirhandle *dh = buf;
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res;
    if (fsp_fuse_intf_CanDeleteAddDirInfo == filler)
        res = f3->ops.readdir(path, buf, &fuse2to3_candel_filldir, off, &fi3, 0);
    else if (fsp_fuse_intf_AddDirInfo == filler)
        res = f3->ops.readdir(path, buf, &fuse2to3_filldir, off, &fi3,
            dh->ReaddirPlus ? FUSE_READDIR_PLUS : 0);
    else
    {
        FspDebugLog("fuse2to3_readdir = -ENOSYS (internal error: unknown filler)\n");
        res = -ENOSYS_(f3->fuse->env);
    }
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_releasedir(const char *path, struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.releasedir(path, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.fsyncdir(path, datasync, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static void *fuse2to3_init(struct fuse_conn_info *conn)
{
    struct fuse_context *context = fsp_fuse_get_context_internal();
    struct fuse *f = context->fuse;
    struct fuse3 *f3 = f->fuse3;

    struct fuse3_conn_info conn3;
    fuse2to3_conn3from2(&conn3, conn);

    struct fuse3_config conf3;
    memset(&conf3, 0, sizeof conf3);
    conf3.set_gid = f->set_gid;
    conf3.gid = f->gid;
    conf3.set_uid = f->set_uid;
    conf3.uid = f->uid;
    conf3.set_mode = f->set_umask;
    conf3.umask = f->umask;
#if 0
    /*
     * Cannot set timeouts because of lack of floating point support.
     *
     * FUSE uses the `double` type for timeouts. This DLL does not use the standard library
     * for a variety of reasons. This means that we cannot easily perform the computations
     * below.
     *
     * If this becomes important (double) floating point values could perhaps be calculated
     * using bit tricks. See below:
     * - http://locklessinc.com/articles/i2f/
     * - https://stackoverflow.com/a/20308114
     */
    conf3.entry_timeout = f->VolumeParams.DirInfoTimeoutValid ?
        f->VolumeParams.DirInfoTimeout / 1000 : f->VolumeParams.FileInfoTimeout / 1000;
    conf3.negative_timeout = 0;
    conf3.attr_timeout = f->VolumeParams.FileInfoTimeout / 1000;
    conf3.ac_attr_timeout = conf3.attr_timeout;
#endif

    void *res = f3->ops.init(&conn3, &conf3);

    conn->max_write = conn3.max_write;
    conn->max_readahead = conn3.max_readahead;
    conn->want = 0 != (conn3.want & FUSE_CAP_READDIRPLUS) ? FSP_FUSE_CAP_READDIR_PLUS : 0;
    conn->want |= conn3.want & ~FUSE_CAP_READDIRPLUS;

    return res;
}

static void fuse2to3_destroy(void *data)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    f3->ops.destroy(data);
}

static int fuse2to3_access(const char *path, int mask)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.access(path, mask);
}

static int fuse2to3_create(const char *path, fuse_mode_t mode, struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.create(path, mode, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_ftruncate(const char *path, fuse_off_t off, struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.truncate(path, off, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_fgetattr(const char *path, struct fuse_stat *stbuf, struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.getattr(path, stbuf, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_lock(const char *path,
    struct fuse_file_info *fi, int cmd, struct fuse_flock *lock)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.lock(path, &fi3, cmd, lock);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_utimens(const char *path, const struct fuse_timespec tv[2])
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.utimens(path, tv, 0);
}

static int fuse2to3_bmap(const char *path, size_t blocksize, uint64_t *idx)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    return f3->ops.bmap(path, blocksize, idx);
}

static int fuse2to3_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi,
    unsigned int flags, void *data)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.ioctl(path, cmd, arg, &fi3, flags, data);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_poll(const char *path, struct fuse_file_info *fi,
    struct fuse_pollhandle *ph, unsigned *reventsp)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.poll(path, &fi3, (struct fuse3_pollhandle *)ph, reventsp);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_write_buf(const char *path,
    struct fuse_bufvec *buf, fuse_off_t off, struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.write_buf(path,
        (struct fuse3_bufvec *)buf, /* revisit if we implement bufvec's */
        off, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_read_buf(const char *path,
    struct fuse_bufvec **bufp, size_t size, fuse_off_t off, struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.read_buf(path,
        (struct fuse3_bufvec **)bufp, /* revisit if we implement bufvec's */
        size, off, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_flock(const char *path, struct fuse_file_info *fi, int op)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.flock(path, &fi3, op);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fuse2to3_fallocate(const char *path, int mode, fuse_off_t off, fuse_off_t len,
    struct fuse_file_info *fi)
{
    struct fuse3 *f3 = fuse2to3_getfuse3();
    struct fuse3_file_info fi3;
    fuse2to3_fi3from2(&fi3, fi);
    int res = f3->ops.fallocate(path, mode, off, len, &fi3);
    fuse2to3_fi2from3(fi, &fi3);
    return res;
}

static int fsp_fuse3_copy_args(struct fsp_fuse_env *env,
    const struct fuse_args *args,
    struct fuse_args *outargs)
{
    outargs->argc = 0;
    outargs->argv = 0;
    outargs->allocated = 0;

    for (int argi = 0; args->argc > argi; argi++)
        if (-1 == fsp_fuse_opt_add_arg(env, outargs, args->argv[argi]))
            goto fail;

    return 0;

fail:
    fsp_fuse_opt_free_args(env, outargs);

    return -1;
}

static struct fuse3 *fsp_fuse3_new_common(struct fsp_fuse_env *env,
    struct fuse_args *args,
    const struct fuse3_operations *ops, size_t opsize, void *data,
    int help)
{
    /* preflight args */
    struct fsp_fuse_core_opt_data opt_data;
    struct fuse_args pfargs;
    memset(&opt_data, 0, sizeof opt_data);
    if (-1 == fsp_fuse3_copy_args(env, args, &pfargs))
        return 0;
    int optres = fsp_fuse_core_opt_parse(env, &pfargs, &opt_data, /*help=*/1);
    fsp_fuse_opt_free_args(env, &pfargs);
    if (-1 == optres)
        return 0;
    if (opt_data.help)
        return 0;

    struct fuse3 *f3 = 0;

    if (opsize > sizeof(struct fuse3_operations))
        opsize = sizeof(struct fuse3_operations);

    f3 = fsp_fuse_obj_alloc(env, sizeof *f3);
    if (0 == f3)
        goto fail;

    if (-1 == fsp_fuse3_copy_args(env, args, &f3->args))
        goto fail;
    memcpy(&f3->ops, ops, opsize);
    f3->data = data;

    return f3;

fail:
    if (0 != f3)
        fsp_fuse3_destroy(env, f3);

    return 0;
}

FSP_FUSE_API struct fuse3 *fsp_fuse3_new_30(struct fsp_fuse_env *env,
    struct fuse_args *args,
    const struct fuse3_operations *ops, size_t opsize, void *data)
{
    return fsp_fuse3_new_common(env, args, ops, opsize, data, /*help=*/1);
}

FSP_FUSE_API struct fuse3 *fsp_fuse3_new(struct fsp_fuse_env *env,
    struct fuse_args *args,
    const struct fuse3_operations *ops, size_t opsize, void *data)
{
    return fsp_fuse3_new_common(env, args, ops, opsize, data, /*help=*/0);
}

FSP_FUSE_API void fsp_fuse3_destroy(struct fsp_fuse_env *env,
    struct fuse3 *f3)
{
    if (0 != f3->fuse)
        fsp_fuse_destroy(env, f3->fuse);

    fsp_fuse_opt_free_args(env, &f3->args);

    fsp_fuse_obj_free(f3);
}

FSP_FUSE_API int fsp_fuse3_mount(struct fsp_fuse_env *env,
    struct fuse3 *f3, const char *mountpoint)
{
    struct fuse_chan *ch = 0;
    struct fuse *f = 0;
    struct fuse_operations fuse2to3_ops =
    {
        .getattr = 0 != f3->ops.getattr ? fuse2to3_getattr : 0,
        .readlink = 0 != f3->ops.readlink ? fuse2to3_readlink : 0,
        .mknod = 0 != f3->ops.mknod ? fuse2to3_mknod : 0,
        .mkdir = 0 != f3->ops.mkdir ? fuse2to3_mkdir : 0,
        .unlink = 0 != f3->ops.unlink ? fuse2to3_unlink : 0,
        .rmdir = 0 != f3->ops.rmdir ? fuse2to3_rmdir : 0,
        .symlink = 0 != f3->ops.symlink ? fuse2to3_symlink : 0,
        .rename = 0 != f3->ops.rename ? fuse2to3_rename : 0,
        .link = 0 != f3->ops.link ? fuse2to3_link : 0,
        .chmod = 0 != f3->ops.chmod ? fuse2to3_chmod : 0,
        .chown = 0 != f3->ops.chown ? fuse2to3_chown : 0,
        .truncate = 0 != f3->ops.truncate ? fuse2to3_truncate : 0,
        .open = 0 != f3->ops.open ? fuse2to3_open : 0,
        .read = 0 != f3->ops.read ? fuse2to3_read : 0,
        .write = 0 != f3->ops.write ? fuse2to3_write : 0,
        .statfs = 0 != f3->ops.statfs ? fuse2to3_statfs : 0,
        .flush = 0 != f3->ops.flush ? fuse2to3_flush : 0,
        .release = 0 != f3->ops.release ? fuse2to3_release : 0,
        .fsync = 0 != f3->ops.fsync ? fuse2to3_fsync : 0,
        .setxattr = 0 != f3->ops.setxattr ? fuse2to3_setxattr : 0,
        .getxattr = 0 != f3->ops.getxattr ? fuse2to3_getxattr : 0,
        .listxattr = 0 != f3->ops.listxattr ? fuse2to3_listxattr : 0,
        .removexattr = 0 != f3->ops.removexattr ? fuse2to3_removexattr : 0,
        .opendir = 0 != f3->ops.opendir ? fuse2to3_opendir : 0,
        .readdir = 0 != f3->ops.readdir ? fuse2to3_readdir : 0,
        .releasedir = 0 != f3->ops.releasedir ? fuse2to3_releasedir : 0,
        .fsyncdir = 0 != f3->ops.fsyncdir ? fuse2to3_fsyncdir : 0,
        .init = 0 != f3->ops.init ? fuse2to3_init : 0,
        .destroy = 0 != f3->ops.destroy ? fuse2to3_destroy : 0,
        .access = 0 != f3->ops.access ? fuse2to3_access : 0,
        .create = 0 != f3->ops.create ? fuse2to3_create : 0,
        .ftruncate = 0 != f3->ops.truncate ? fuse2to3_ftruncate : 0,
        .fgetattr = 0 != f3->ops.getattr ? fuse2to3_fgetattr : 0,
        .lock = 0 != f3->ops.lock ? fuse2to3_lock : 0,
        .utimens = 0 != f3->ops.utimens ? fuse2to3_utimens : 0,
        .bmap = 0 != f3->ops.bmap ? fuse2to3_bmap : 0,
        .ioctl = 0 != f3->ops.ioctl ? fuse2to3_ioctl : 0,
        .poll = 0 != f3->ops.poll ? fuse2to3_poll : 0,
        .write_buf = 0 != f3->ops.write_buf ? fuse2to3_write_buf : 0,
        .read_buf = 0 != f3->ops.read_buf ? fuse2to3_read_buf : 0,
        .flock = 0 != f3->ops.flock ? fuse2to3_flock : 0,
        .fallocate = 0 != f3->ops.fallocate ? fuse2to3_fallocate : 0,
    };

    ch = fsp_fuse_mount(env, mountpoint, &f3->args);
    if (0 == ch)
        goto fail;

    f = fsp_fuse_new(env, ch, &f3->args, &fuse2to3_ops, sizeof fuse2to3_ops, f3->data);
    if (0 == f)
        goto fail;

    /*
     * Free the fuse_chan which is no longer needed. Note that this behavior is WinFsp-FUSE
     * specific, because WinFsp-FUSE only allocates/frees fuse_chan memory during fuse_mount/
     * fuse_unmount and does not perform any actual mounting/unmounting. This would not work
     * on a different FUSE implementation.
     *
     * (Store mountpoint and ch inside struct fuse3 so that they can be freed during fuse_destroy
     * in that case.)
     */
    fsp_fuse_unmount(env, mountpoint, ch);

    /* Free the args which are no longer needed. */
    fsp_fuse_opt_free_args(env, &f3->args);

    f->fuse3 = f3;
    f3->fuse = f;

    return 0;

fail:
    if (0 != f)
        fsp_fuse_destroy(env, f);

    if (0 != ch)
        fsp_fuse_unmount(env, mountpoint, ch);

    return -1;
}

FSP_FUSE_API void fsp_fuse3_unmount(struct fsp_fuse_env *env,
    struct fuse3 *f3)
{
    fsp_fuse_destroy(env, f3->fuse);
    f3->fuse = 0;
}
