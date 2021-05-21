/**
 * @file passthrough-fuse.c
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

#include <errno.h>
#include <fcntl.h>
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

#define PTFS_UTIMENS

#define FSNAME                          "passthrough"
#define PROGNAME                        "passthrough-fuse"

#define concat_path(ptfs, fn, fp)       (sizeof fp > (unsigned)snprintf(fp, sizeof fp, "%s%s", ptfs->rootdir, fn))

#define fi_dirbit                       (0x8000000000000000ULL)
#define fi_fh(fi, MASK)                 ((fi)->fh & (MASK))
#define fi_setfh(fi, FH, MASK)          ((fi)->fh = (intptr_t)(FH) | (MASK))
#define fi_fd(fi)                       (fi_fh(fi, fi_dirbit) ? \
    dirfd((DIR *)(intptr_t)fi_fh(fi, ~fi_dirbit)) : (int)fi_fh(fi, ~fi_dirbit))
#define fi_dirp(fi)                     ((DIR *)(intptr_t)fi_fh(fi, ~fi_dirbit))
#define fi_setfd(fi, fd)                (fi_setfh(fi, fd, 0))
#define fi_setdirp(fi, dirp)            (fi_setfh(fi, dirp, fi_dirbit))

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

#if !defined(PTFS_UTIMENS)
static int ptfs_utime(const char *path, struct fuse_utimbuf *timbuf)
{
    ptfs_impl_fullpath(path);

    return -1 != utime(path, timbuf) ? 0 : -errno;
}
#endif

static int ptfs_open(const char *path, struct fuse_file_info *fi)
{
    ptfs_impl_fullpath(path);

    int fd;
    return -1 != (fd = open(path, fi->flags)) ? (fi_setfd(fi, fd), 0) : -errno;
}

static int ptfs_read(const char *path, char *buf, size_t size, fuse_off_t off,
    struct fuse_file_info *fi)
{
    int fd = fi_fd(fi);

    int nb;
    return -1 != (nb = pread(fd, buf, size, off)) ? nb : -errno;
}

static int ptfs_write(const char *path, const char *buf, size_t size, fuse_off_t off,
    struct fuse_file_info *fi)
{
    int fd = fi_fd(fi);

    int nb;
    return -1 != (nb = pwrite(fd, buf, size, off)) ? nb : -errno;
}

static int ptfs_statfs(const char *path, struct fuse_statvfs *stbuf)
{
    ptfs_impl_fullpath(path);

    return -1 != statvfs(path, stbuf) ? 0 : -errno;
}

static int ptfs_release(const char *path, struct fuse_file_info *fi)
{
    int fd = fi_fd(fi);

    close(fd);
    return 0;
}

static int ptfs_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    int fd = fi_fd(fi);

    return -1 != fsync(fd) ? 0 : -errno;
}

static int ptfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    ptfs_impl_fullpath(path);

    return -1 != lsetxattr(path, name, value, size, flags) ? 0 : -errno;
}

static int ptfs_getxattr(const char *path, const char *name, char *value, size_t size)
{
    ptfs_impl_fullpath(path);

    int nb;
    return -1 != (nb = lgetxattr(path, name, value, size)) ? nb : -errno;
}

static int ptfs_listxattr(const char *path, char *namebuf, size_t size)
{
    ptfs_impl_fullpath(path);

    int nb;
    return -1 != (nb = llistxattr(path, namebuf, size)) ? nb : -errno;
}

static int ptfs_removexattr(const char *path, const char *name)
{
    ptfs_impl_fullpath(path);

    return -1 != lremovexattr(path, name) ? 0 : -errno;
}

static int ptfs_opendir(const char *path, struct fuse_file_info *fi)
{
    ptfs_impl_fullpath(path);

    DIR *dirp;
    return 0 != (dirp = opendir(path)) ? (fi_setdirp(fi, dirp), 0) : -errno;
}

static int ptfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, fuse_off_t off,
    struct fuse_file_info *fi)
{
    DIR *dirp = fi_dirp(fi);
    struct dirent *de;

    rewinddir(dirp);
    for (;;)
    {
        errno = 0;
        if (0 == (de = readdir(dirp)))
            break;
#if defined(_WIN64) || defined(_WIN32)
        if (0 != filler(buf, de->d_name, &de->d_stat, 0))
#else
        if (0 != filler(buf, de->d_name, 0, 0))
#endif
            return -ENOMEM;
    }

    return -errno;
}

static int ptfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    DIR *dirp = fi_dirp(fi);

    return -1 != closedir(dirp) ? 0 : -errno;
}

static void *ptfs_init(struct fuse_conn_info *conn)
{
#if defined(_WIN64) || defined(_WIN32)
#if defined(FSP_FUSE_CAP_READDIR_PLUS)
    conn->want |= (conn->capable & FSP_FUSE_CAP_READDIR_PLUS);
#endif

#if defined(FSP_FUSE_USE_STAT_EX) && defined(FSP_FUSE_CAP_STAT_EX)
    conn->want |= (conn->capable & FSP_FUSE_CAP_STAT_EX);
#endif

#if defined(FSP_FUSE_CAP_CASE_INSENSITIVE)
    conn->want |= (conn->capable & FSP_FUSE_CAP_CASE_INSENSITIVE);
#endif
#endif

    return fuse_get_context()->private_data;
}

static int ptfs_create(const char *path, fuse_mode_t mode, struct fuse_file_info *fi)
{
    ptfs_impl_fullpath(path);

    int fd;
    return -1 != (fd = open(path, fi->flags, mode)) ? (fi_setfd(fi, fd), 0) : -errno;
}

static int ptfs_ftruncate(const char *path, fuse_off_t off, struct fuse_file_info *fi)
{
    int fd = fi_fd(fi);

    return -1 != ftruncate(fd, off) ? 0 : -errno;
}

static int ptfs_fgetattr(const char *path, struct fuse_stat *stbuf, struct fuse_file_info *fi)
{
    int fd = fi_fd(fi);

    return -1 != fstat(fd, stbuf) ? 0 : -errno;
}

#if defined(PTFS_UTIMENS)
static int ptfs_utimens(const char *path, const struct fuse_timespec tv[2])
{
    ptfs_impl_fullpath(path);

    return -1 != utimensat(AT_FDCWD, path, tv, AT_SYMLINK_NOFOLLOW) ? 0 : -errno;
}
#endif

#if defined(_WIN64) || defined(_WIN32)
static int ptfs_setcrtime(const char *path, const struct fuse_timespec *tv)
{
    ptfs_impl_fullpath(path);

    return -1 != setcrtime(path, tv) ? 0 : -errno;
}
#endif

#if defined(FSP_FUSE_USE_STAT_EX)
static int ptfs_chflags(const char *path, uint32_t flags)
{
    ptfs_impl_fullpath(path);

    return -1 != lchflags(path, flags) ? 0 : -errno;
}
#endif

static struct fuse_operations ptfs_ops =
{
    .getattr = ptfs_getattr,
    .mkdir = ptfs_mkdir,
    .unlink = ptfs_unlink,
    .rmdir = ptfs_rmdir,
    .rename = ptfs_rename,
    .chmod = ptfs_chmod,
    .chown = ptfs_chown,
    .truncate = ptfs_truncate,
#if !defined(PTFS_UTIMENS)
    .utime = ptfs_utime,
#endif
    .open = ptfs_open,
    .read = ptfs_read,
    .write = ptfs_write,
    .statfs = ptfs_statfs,
    .release = ptfs_release,
    .fsync = ptfs_fsync,
    .setxattr = ptfs_setxattr,
    .getxattr = ptfs_getxattr,
    .listxattr = ptfs_listxattr,
    .removexattr = ptfs_removexattr,
    .opendir = ptfs_opendir,
    .readdir = ptfs_readdir,
    .releasedir = ptfs_releasedir,
    .init = ptfs_init,
    .create = ptfs_create,
    .ftruncate = ptfs_ftruncate,
    .fgetattr = ptfs_fgetattr,
#if defined(PTFS_UTIMENS)
    .utimens = ptfs_utimens,
#endif
#if defined(_WIN64) || defined(_WIN32)
    .setcrtime = ptfs_setcrtime,
#endif
#if defined(FSP_FUSE_USE_STAT_EX)
    .chflags = ptfs_chflags,
#endif
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

#if defined(_WIN64) || defined(_WIN32)
    /*
     * When building for Windows (rather than Cygwin or POSIX OS)
     * allow the path to be specified using the --VolumePrefix
     * switch using the syntax \\passthrough-fuse\C$\Path. This
     * allows us to run the file system under the WinFsp.Launcher
     * and start it using commands like:
     *
     *     net use z: \\passthrough-fuse\C$\Path
     */
    if (0 == ptfs.rootdir)
        for (int argi = 1; argc > argi; argi++)
        {
            int strncmp(const char *a, const char *b, size_t length);
            char *strchr(const char *s, int c);
            char *p = 0;

            if (0 == strncmp("--UNC=", argv[argi], sizeof "--UNC=" - 1))
                p = argv[argi] + sizeof "--UNC=" - 1;
            else if (0 == strncmp("--VolumePrefix=", argv[argi], sizeof "--VolumePrefix=" - 1))
                p = argv[argi] + sizeof "--VolumePrefix=" - 1;

            if (0 != p && '\\' != p[1])
            {
                p = strchr(p + 1, '\\');
                if (0 != p &&
                    (
                    ('A' <= p[1] && p[1] <= 'Z') ||
                    ('a' <= p[1] && p[1] <= 'z')
                    ) &&
                    '$' == p[2])
                {
                    p[2] = ':';
                    ptfs.rootdir = realpath(p + 1, 0); /* memory freed at process end */
                    p[2] = '$';
                    break;
                }
            }
        }
#endif

    if (0 == ptfs.rootdir)
        usage();

    return fuse_main(argc, argv, &ptfs_ops, &ptfs);
}
