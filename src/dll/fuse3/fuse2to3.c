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

static int fuse2to3_getattr(const char *path, struct fuse_stat *stbuf)
{
    return -ENOSYS;
}

static int fuse2to3_readlink(const char *path, char *buf, size_t size)
{
    return -ENOSYS;
}

static int fuse2to3_mknod(const char *path, fuse_mode_t mode, fuse_dev_t dev)
{
    return -ENOSYS;
}

static int fuse2to3_mkdir(const char *path, fuse_mode_t mode)
{
    return -ENOSYS;
}

static int fuse2to3_unlink(const char *path)
{
    return -ENOSYS;
}

static int fuse2to3_rmdir(const char *path)
{
    return -ENOSYS;
}

static int fuse2to3_symlink(const char *dstpath, const char *srcpath)
{
    return -ENOSYS;
}

static int fuse2to3_rename(const char *oldpath, const char *newpath)
{
    return -ENOSYS;
}

static int fuse2to3_link(const char *srcpath, const char *dstpath)
{
    return -ENOSYS;
}

static int fuse2to3_chmod(const char *path, fuse_mode_t mode)
{
    return -ENOSYS;
}

static int fuse2to3_chown(const char *path, fuse_uid_t uid, fuse_gid_t gid)
{
    return -ENOSYS;
}

static int fuse2to3_truncate(const char *path, fuse_off_t size)
{
    return -ENOSYS;
}

static int fuse2to3_open(const char *path, struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static int fuse2to3_read(const char *path, char *buf, size_t size, fuse_off_t off,
    struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static int fuse2to3_write(const char *path, const char *buf, size_t size, fuse_off_t off,
    struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static int fuse2to3_statfs(const char *path, struct fuse_statvfs *stbuf)
{
    return -ENOSYS;
}

static int fuse2to3_flush(const char *path, struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static int fuse2to3_release(const char *path, struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static int fuse2to3_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static int fuse2to3_setxattr(const char *path, const char *name, const char *value, size_t size,
    int flags)
{
    return -ENOSYS;
}

static int fuse2to3_getxattr(const char *path, const char *name, char *value, size_t size)
{
    return -ENOSYS;
}

static int fuse2to3_listxattr(const char *path, char *namebuf, size_t size)
{
    return -ENOSYS;
}

static int fuse2to3_removexattr(const char *path, const char *name)
{
    return -ENOSYS;
}

static int fuse2to3_opendir(const char *path, struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static int fuse2to3_readdir(const char *path, void *buf, fuse_fill_dir_t filler, fuse_off_t off,
    struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static int fuse2to3_releasedir(const char *path, struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static int fuse2to3_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static void *fuse2to3_init(struct fuse_conn_info *conn)
{
    return 0;
}

static void fuse2to3_destroy(void *data)
{
}

static int fuse2to3_access(const char *path, int mask)
{
    return -ENOSYS;
}

static int fuse2to3_create(const char *path, fuse_mode_t mode, struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static int fuse2to3_ftruncate(const char *path, fuse_off_t off, struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static int fuse2to3_fgetattr(const char *path, struct fuse_stat *stbuf, struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static int fuse2to3_lock(const char *path,
    struct fuse_file_info *fi, int cmd, struct fuse_flock *lock)
{
    return -ENOSYS;
}

static int fuse2to3_utimens(const char *path, const struct fuse_timespec tv[2])
{
    return -ENOSYS;
}

static int fuse2to3_bmap(const char *path, size_t blocksize, uint64_t *idx)
{
    return -ENOSYS;
}

static int fuse2to3_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi,
    unsigned int flags, void *data)
{
    return -ENOSYS;
}

static int fuse2to3_poll(const char *path, struct fuse_file_info *fi,
    struct fuse_pollhandle *ph, unsigned *reventsp)
{
    return -ENOSYS;
}

static int fuse2to3_write_buf(const char *path,
    struct fuse_bufvec *buf, fuse_off_t off, struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static int fuse2to3_read_buf(const char *path,
    struct fuse_bufvec **bufp, size_t size, fuse_off_t off, struct fuse_file_info *fi)
{
    return -ENOSYS;
}

static int fuse2to3_flock(const char *path, struct fuse_file_info *fi, int op)
{
    return -ENOSYS;
}

static int fuse2to3_fallocate(const char *path, int mode, fuse_off_t off, fuse_off_t len,
    struct fuse_file_info *fi)
{
    return -ENOSYS;
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
