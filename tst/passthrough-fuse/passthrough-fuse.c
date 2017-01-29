/**
 * @file passthrough-fuse.c
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

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <fuse.h>

#if defined(_WIN64) || defined(_WIN32)
#include "winposix.h"
#else
#include <dirent.h>
#include <unistd.h>
#endif

#define FSNAME                          "passthrough"
#define PROGNAME                        "passthrough-fuse"

#define concat_path(ptfs, fn, fp)       (sizeof fp > (unsigned)snprintf(fp, sizeof fp, "%s/%s", ptfs->rootdir, fn))
#define ptfs_impl_fullpath(n)           \
    char full ## n[PATH_MAX];           \
    if (!concat_path(((PTFS *)fuse_get_context()->private_data), n, full ## n))\
        return -ENAMETOOLONG;           \
    n = full ## n

typedef struct
{
    const char *rootdir;
} PTFS;

static int ptfs_getattr(const char *path, struct fuse_stat *stbuf)
{
    ptfs_impl_fullpath(path);

    return -1 != lstat(path, stbuf) ? 0 : -errno;
}

static int ptfs_mkdir(const char *path, fuse_mode_t mode)
{
    ptfs_impl_fullpath(path);

    return -1 != mkdir(path, mode) ? 0 : -errno;
}

static int ptfs_unlink(const char *path)
{
    ptfs_impl_fullpath(path);

    return -1 != unlink(path) ? 0 : -errno;
}

static int ptfs_rmdir(const char *path)
{
    ptfs_impl_fullpath(path);

    return -1 != rmdir(path) ? 0 : -errno;
}

static int ptfs_rename(const char *oldpath, const char *newpath)
{
    ptfs_impl_fullpath(newpath);
    ptfs_impl_fullpath(oldpath);

    return -1 != rename(oldpath, newpath) ? 0 : -errno;
}

static int ptfs_chmod(const char *path, fuse_mode_t mode)
{
    ptfs_impl_fullpath(path);

    return -1 != chmod(path, mode) ? 0 : -errno;
}

static int ptfs_chown(const char *path, fuse_uid_t uid, fuse_gid_t gid)
{
    ptfs_impl_fullpath(path);

    return -1 != lchown(path, uid, gid) ? 0 : -errno;
}

static int ptfs_truncate(const char *path, fuse_off_t size)
{
    ptfs_impl_fullpath(path);

    return -1 != truncate(path, size) ? 0 : -errno;
}

static int ptfs_utime(const char *path, struct fuse_utimbuf *timbuf)
{
    ptfs_impl_fullpath(path);

    return -1 != utime(path, timbuf) ? 0 : -errno;
}

static int ptfs_open(const char *path, struct fuse_file_info *fi)
{
    ptfs_impl_fullpath(path);

    int fd;
    return -1 != (fd = open(path, fi->flags)) ? (fi->fh = fd, 0) : -errno;
}

static int ptfs_read(const char *path, char *buf, size_t size, fuse_off_t off,
    struct fuse_file_info *fi)
{
    int fd = (int)fi->fh;

    int by;
    return -1 != (by = pread(fd, buf, size, off)) ? by : -errno;
}

static int ptfs_write(const char *path, const char *buf, size_t size, fuse_off_t off,
    struct fuse_file_info *fi)
{
    int fd = (int)fi->fh;

    int by;
    return -1 != (by = pwrite(fd, buf, size, off)) ? by : -errno;
}

static int ptfs_statfs(const char *path, struct fuse_statvfs *stbuf)
{
    ptfs_impl_fullpath(path);

    return -1 != statvfs(path, stbuf) ? 0 : -errno;
}

static int ptfs_release(const char *path, struct fuse_file_info *fi)
{
    int fd = (int)fi->fh;

    close(fd);
    return 0;
}

static int ptfs_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    int fd = (int)fi->fh;

    return -1 != fsync(fd) ? 0 : -errno;
}

static int ptfs_opendir(const char *path, struct fuse_file_info *fi)
{
    ptfs_impl_fullpath(path);

    DIR *dp;
    return 0 != (dp = opendir(path)) ? (fi->fh = (intptr_t)dp, 0) : -errno;
}

static int ptfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, fuse_off_t off,
    struct fuse_file_info *fi)
{
    DIR *dp = (DIR *)fi->fh;
    struct dirent *de;

    for (;;)
    {
        errno = 0;
        if (0 == (de = readdir(dp)))
            break;
        if (0 != filler(buf, de->d_name, 0, 0))
            return -ENOMEM;
    }

    return -errno;
}

static int ptfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp = (DIR *)fi->fh;

    closedir(dp);
    return 0;
}

static int ptfs_create(const char *path, fuse_mode_t mode, struct fuse_file_info *fi)
{
    ptfs_impl_fullpath(path);

    int fd;
    return -1 != (fd = creat(path, mode)) ? (fi->fh = fd, 0) : -errno;
}

static int ptfs_ftruncate(const char *path, fuse_off_t off, struct fuse_file_info *fi)
{
    int fd = (int)fi->fh;

    return -1 != ftruncate(fd, off) ? 0 : -errno;
}

static int ptfs_fgetattr(const char *path, struct fuse_stat *stbuf, struct fuse_file_info *fi)
{
    int fd = (int)fi->fh;

    return -1 != fstat(fd, stbuf) ? 0 : -errno;
}

static struct fuse_operations ptfs_ops =
{
    ptfs_getattr,
    0, //getdir
    0, //readlink
    0, //mknod
    ptfs_mkdir,
    ptfs_unlink,
    ptfs_rmdir,
    0, //symlink
    ptfs_rename,
    0, //link
    ptfs_chmod,
    ptfs_chown,
    ptfs_truncate,
    ptfs_utime,
    ptfs_open,
    ptfs_read,
    ptfs_write,
    ptfs_statfs,
    0, //flush
    ptfs_release,
    ptfs_fsync,
    0, //setxattr
    0, //getxattr
    0, //listxattr
    0, //removexattr
    ptfs_opendir,
    ptfs_readdir,
    ptfs_releasedir,
    0, //fsyncdir
    0, //init
    0, //destroy
    0, //access
    ptfs_create,
    ptfs_ftruncate,
    ptfs_fgetattr,
};

static void usage(void)
{
    fprintf(stderr, "usage: " PROGNAME " [FUSE options] rootdir mountpoint\n");
    exit(2);
}

int main(int argc, char *argv[])
{
    PTFS ptfs = { 0 };

    if (3 <= argc && '-' != argv[argc - 2][0] && '-' != argv[argc - 1][0])
    {
        ptfs.rootdir = realpath(argv[argc - 2], 0); /* memory freed at process end */
        argv[argc - 2] = argv[argc - 1];
        argc--;
    }

    if (0 == ptfs.rootdir)
        usage();

    return fuse_main(argc, argv, &ptfs_ops, &ptfs);
}
