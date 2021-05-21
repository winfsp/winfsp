/**
 * @file winposix.h
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

#ifndef WINPOSIX_H_INCLUDED
#define WINPOSIX_H_INCLUDED

#define O_RDONLY                        _O_RDONLY
#define O_WRONLY                        _O_WRONLY
#define O_RDWR                          _O_RDWR
#define O_APPEND                        _O_APPEND
#define O_CREAT                         _O_CREAT
#define O_EXCL                          _O_EXCL
#define O_TRUNC                         _O_TRUNC

#define PATH_MAX                        1024
#define AT_FDCWD                        -2
#define AT_SYMLINK_NOFOLLOW             2

typedef struct _DIR DIR;
struct dirent
{
    struct fuse_stat d_stat;
    char d_name[255];
};

char *realpath(const char *path, char *resolved);

int statvfs(const char *path, struct fuse_statvfs *stbuf);

int open(const char *path, int oflag, ...);
int fstat(int fd, struct fuse_stat *stbuf);
int ftruncate(int fd, fuse_off_t size);
int pread(int fd, void *buf, size_t nbyte, fuse_off_t offset);
int pwrite(int fd, const void *buf, size_t nbyte, fuse_off_t offset);
int fsync(int fd);
int close(int fd);

int lstat(const char *path, struct fuse_stat *stbuf);
int chmod(const char *path, fuse_mode_t mode);
int lchown(const char *path, fuse_uid_t uid, fuse_gid_t gid);
int lchflags(const char *path, uint32_t flags);
int truncate(const char *path, fuse_off_t size);
int utime(const char *path, const struct fuse_utimbuf *timbuf);
int utimensat(int dirfd, const char *path, const struct fuse_timespec times[2], int flag);
int setcrtime(const char *path, const struct fuse_timespec *tv);
int unlink(const char *path);
int rename(const char *oldpath, const char *newpath);

int lsetxattr(const char *path, const char *name, const void *value, size_t size, int flags);
int lgetxattr(const char *path, const char *name, void *value, size_t size);
int llistxattr(const char *path, char *namebuf, size_t size);
int lremovexattr(const char *path, const char *name);

int mkdir(const char *path, fuse_mode_t mode);
int rmdir(const char *path);

DIR *opendir(const char *path);
int dirfd(DIR *dirp);
void rewinddir(DIR *dirp);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);

long WinFspLoad(void);
#undef fuse_main
#define fuse_main(argc, argv, ops, data)\
    (WinFspLoad(), fuse_main_real(argc, argv, ops, sizeof *(ops), data))

#endif
