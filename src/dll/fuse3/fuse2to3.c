/**
 * @file dll/fuse3/fuse2to3.c
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

#include <dll/fuse3/library.h>

static inline struct fuse3 *fuse2to3_getfuse3(void)
{
    return FSP_FUSE_HDR_FROM_CONTEXT(fsp_fuse_get_context_internal())->fuse3;
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
    conn3->capable = conn->capable;
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
    int res = f3->ops.readdir(path, buf, &fuse2to3_filldir, off, &fi3,
        dh->ReaddirPlus ? FUSE_READDIR_PLUS : 0);
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
    struct fuse3 *f3 = FSP_FUSE_HDR_FROM_CONTEXT(context)->fuse3;

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
    conn->want = conn3.want;

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

static struct fuse_operations fuse2to3_ops =
{
    .getattr = fuse2to3_getattr,
    .readlink = fuse2to3_readlink,
    .mknod = fuse2to3_mknod,
    .mkdir = fuse2to3_mkdir,
    .unlink = fuse2to3_unlink,
    .rmdir = fuse2to3_rmdir,
    .symlink = fuse2to3_symlink,
    .rename = fuse2to3_rename,
    .link = fuse2to3_link,
    .chmod = fuse2to3_chmod,
    .chown = fuse2to3_chown,
    .truncate = fuse2to3_truncate,
    .open = fuse2to3_open,
    .read = fuse2to3_read,
    .write = fuse2to3_write,
    .statfs = fuse2to3_statfs,
    .flush = fuse2to3_flush,
    .release = fuse2to3_release,
    .fsync = fuse2to3_fsync,
    .setxattr = fuse2to3_setxattr,
    .getxattr = fuse2to3_getxattr,
    .listxattr = fuse2to3_listxattr,
    .removexattr = fuse2to3_removexattr,
    .opendir = fuse2to3_opendir,
    .readdir = fuse2to3_readdir,
    .releasedir = fuse2to3_releasedir,
    .fsyncdir = fuse2to3_fsyncdir,
    .init = fuse2to3_init,
    .destroy = fuse2to3_destroy,
    .access = fuse2to3_access,
    .create = fuse2to3_create,
    .ftruncate = fuse2to3_ftruncate,
    .fgetattr = fuse2to3_fgetattr,
    .lock = fuse2to3_lock,
    .utimens = fuse2to3_utimens,
    .bmap = fuse2to3_bmap,
    .ioctl = fuse2to3_ioctl,
    .poll = fuse2to3_poll,
    .write_buf = fuse2to3_write_buf,
    .read_buf = fuse2to3_read_buf,
    .flock = fuse2to3_flock,
    .fallocate = fuse2to3_fallocate,
};
