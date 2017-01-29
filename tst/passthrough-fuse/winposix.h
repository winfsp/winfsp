/**
 * @file winposix.h
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

#ifndef WINPOSIX_H_INCLUDED
#define WINPOSIX_H_INCLUDED

#define PATH_MAX                        1024

typedef struct _DIR DIR;
struct dirent
{
    char d_name[255];
};

char *realpath(const char *path, char *resolved_path);

int statvfs(const char *path, struct fuse_statvfs *buf);

int mkdir(const char *path, fuse_mode_t mode);
int rmdir(const char *path);

int unlink(const char *path);
int lstat(const char *path, struct fuse_stat *stbuf);
int chmod(const char *path, fuse_mode_t mode);
int lchown(const char *path, fuse_uid_t uid, fuse_gid_t gid);
int truncate(const char *path, fuse_off_t size);
int utime(const char *path, const struct fuse_utimbuf *timbuf);

int creat(const char *path, fuse_mode_t mode);
int open(const char *path, int oflag);
int fstat(int fd, struct fuse_stat *stbuf);
int ftruncate(int fd, fuse_off_t size);;
int pread(int fd, void *buf, size_t nbyte, fuse_off_t offset);
int pwrite(int fd, const void *buf, size_t nbyte, fuse_off_t offset);
int fsync(int fd);
int close(int fd);

DIR *opendir(const char *path);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dp);

#endif
