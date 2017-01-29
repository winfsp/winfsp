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

#include <windows.h>
#include <fcntl.h>
#include <fuse.h>
#include "winposix.h"

static int maperror(int winerrno);

char *realpath(const char *path, char *resolved)
{
    char *result;

    HANDLE h = CreateFileA(path,
        FILE_READ_ATTRIBUTES, 0, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);

    if (INVALID_HANDLE_VALUE == h)
    {
        errno = maperror(GetLastError());
        return 0;
    }

    if (0 == resolved)
    {
        result = malloc(PATH_MAX); /* sets errno */
        if (0 == result)
            return 0;
    }
    else
        result = resolved;

    if (PATH_MAX < GetFinalPathNameByHandleA(h, resolved, PATH_MAX, 0))
    {
        if (result != resolved)
            free(result);
        errno = EINVAL;
        result = 0;
    }

    CloseHandle(h);

    return result;
}

int statvfs(const char *path, struct fuse_statvfs *stbuf)
{
    char root[PATH_MAX];
    DWORD
        VolumeSerialNumber,
        MaxComponentLength,
        SectorsPerCluster,
        BytesPerSector,
        NumberOfFreeClusters,
        TotalNumberOfClusters;

    if (!GetVolumePathNameA(path, root, PATH_MAX) ||
        !GetVolumeInformationA(root, 0, 0, &VolumeSerialNumber, &MaxComponentLength, 0, 0, 0) ||
        !GetDiskFreeSpaceA(root, &SectorsPerCluster, &BytesPerSector,
            &NumberOfFreeClusters, &TotalNumberOfClusters))
    {
        errno = maperror(GetLastError());
        return -1;
    }

    memset(stbuf, 0, sizeof *stbuf);
    stbuf->f_bsize = SectorsPerCluster * BytesPerSector;
    stbuf->f_frsize = BytesPerSector;
    stbuf->f_blocks = TotalNumberOfClusters;
    stbuf->f_bfree = NumberOfFreeClusters;
    stbuf->f_bavail = TotalNumberOfClusters;
    stbuf->f_fsid = VolumeSerialNumber;
    stbuf->f_namemax = MaxComponentLength;

    return -1;
}

int open(const char *path, int oflag, ...)
{
    static DWORD da[] = { GENERIC_READ, GENERIC_WRITE, GENERIC_READ | GENERIC_WRITE, 0 };
    static DWORD cd[] = { OPEN_EXISTING, OPEN_ALWAYS, TRUNCATE_EXISTING, CREATE_ALWAYS };
    DWORD DesiredAccess = 0 == (oflag & _O_APPEND) ?
        da[oflag & (_O_RDONLY | _O_WRONLY | _O_RDWR)] :
        (da[oflag & (_O_RDONLY | _O_WRONLY | _O_RDWR)] & ~FILE_WRITE_DATA) | FILE_APPEND_DATA;
    DWORD CreationDisposition = (_O_CREAT | _O_EXCL) == (oflag & (_O_CREAT | _O_EXCL)) ?
        CREATE_NEW :
        cd[(oflag & (_O_CREAT | _O_TRUNC)) >> 8];

    HANDLE h = CreateFileA(path,
        DesiredAccess, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
        CreationDisposition, FILE_ATTRIBUTE_NORMAL, 0);

    if (INVALID_HANDLE_VALUE == h)
    {
        errno = maperror(GetLastError());
        return -1;
    }

    return (int)(intptr_t)h;
}

int fstat(int fd, struct fuse_stat *stbuf)
{
    return -1;
}

int ftruncate(int fd, fuse_off_t size)
{
    return -1;
}

int pread(int fd, void *buf, size_t nbyte, fuse_off_t offset)
{
    return -1;
}

int pwrite(int fd, const void *buf, size_t nbyte, fuse_off_t offset)
{
    return -1;
}

int fsync(int fd)
{
    return -1;
}

int close(int fd)
{
    return -1;
}

int lstat(const char *path, struct fuse_stat *stbuf)
{
    return -1;
}

int chmod(const char *path, fuse_mode_t mode)
{
    return -1;
}

int lchown(const char *path, fuse_uid_t uid, fuse_gid_t gid)
{
    return -1;
}

int truncate(const char *path, fuse_off_t size)
{
    return -1;
}

int utime(const char *path, const struct fuse_utimbuf *timbuf)
{
    return -1;
}

int unlink(const char *path)
{
    return -1;
}

int rename(const char *oldpath, const char *newpath)
{
    return -1;
}

int mkdir(const char *path, fuse_mode_t mode)
{
    return -1;
}

int rmdir(const char *path)
{
    return -1;
}

DIR *opendir(const char *path)
{
    return 0;
}

struct dirent *readdir(DIR *dirp)
{
    return 0;
}

int closedir(DIR *dp)
{
    return -1;
}

static int maperror(int winerrno)
{
    switch (winerrno)
    {
    case ERROR_INVALID_FUNCTION:
        return EINVAL;
    case ERROR_FILE_NOT_FOUND:
        return ENOENT;
    case ERROR_PATH_NOT_FOUND:
        return ENOENT;
    case ERROR_TOO_MANY_OPEN_FILES:
        return EMFILE;
    case ERROR_ACCESS_DENIED:
        return EACCES;
    case ERROR_INVALID_HANDLE:
        return EBADF;
    case ERROR_ARENA_TRASHED:
        return ENOMEM;
    case ERROR_NOT_ENOUGH_MEMORY:
        return ENOMEM;
    case ERROR_INVALID_BLOCK:
        return ENOMEM;
    case ERROR_BAD_ENVIRONMENT:
        return E2BIG;
    case ERROR_BAD_FORMAT:
        return ENOEXEC;
    case ERROR_INVALID_ACCESS:
        return EINVAL;
    case ERROR_INVALID_DATA:
        return EINVAL;
    case ERROR_INVALID_DRIVE:
        return ENOENT;
    case ERROR_CURRENT_DIRECTORY:
        return EACCES;
    case ERROR_NOT_SAME_DEVICE:
        return EXDEV;
    case ERROR_NO_MORE_FILES:
        return ENOENT;
    case ERROR_LOCK_VIOLATION:
        return EACCES;
    case ERROR_BAD_NETPATH:
        return ENOENT;
    case ERROR_NETWORK_ACCESS_DENIED:
        return EACCES;
    case ERROR_BAD_NET_NAME:
        return ENOENT;
    case ERROR_FILE_EXISTS:
        return EEXIST;
    case ERROR_CANNOT_MAKE:
        return EACCES;
    case ERROR_FAIL_I24:
        return EACCES;
    case ERROR_INVALID_PARAMETER:
        return EINVAL;
    case ERROR_NO_PROC_SLOTS:
        return EAGAIN;
    case ERROR_DRIVE_LOCKED:
        return EACCES;
    case ERROR_BROKEN_PIPE:
        return EPIPE;
    case ERROR_DISK_FULL:
        return ENOSPC;
    case ERROR_INVALID_TARGET_HANDLE:
        return EBADF;
    case ERROR_WAIT_NO_CHILDREN:
        return ECHILD;
    case ERROR_CHILD_NOT_COMPLETE:
        return ECHILD;
    case ERROR_DIRECT_ACCESS_HANDLE:
        return EBADF;
    case ERROR_NEGATIVE_SEEK:
        return EINVAL;
    case ERROR_SEEK_ON_DEVICE:
        return EACCES;
    case ERROR_DIR_NOT_EMPTY:
        return ENOTEMPTY;
    case ERROR_NOT_LOCKED:
        return EACCES;
    case ERROR_BAD_PATHNAME:
        return ENOENT;
    case ERROR_MAX_THRDS_REACHED:
        return EAGAIN;
    case ERROR_LOCK_FAILED:
        return EACCES;
    case ERROR_ALREADY_EXISTS:
        return EEXIST;
    case ERROR_FILENAME_EXCED_RANGE:
        return ENOENT;
    case ERROR_NESTING_NOT_ALLOWED:
        return EAGAIN;
    case ERROR_NOT_ENOUGH_QUOTA:
        return ENOMEM;
    default:
        if (ERROR_WRITE_PROTECT <= winerrno && winerrno <= ERROR_SHARING_BUFFER_EXCEEDED)
            return EACCES;
        else if (ERROR_INVALID_STARTING_CODESEG <= winerrno && winerrno <= ERROR_INFLOOP_IN_RELOC_CHAIN)
            return ENOEXEC;
        else
            return EINVAL;
    }
}
