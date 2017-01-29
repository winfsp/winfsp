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

/*
 * This is a very simple Windows POSIX layer. It handles all the POSIX
 * file API's required to implement passthrough-fuse in POSIX, however
 * the API handling is rather unsophisticated.
 *
 * Ways to improve it: use the FspPosix* API's to properly handle
 * file names and security.
 */

#include <windows.h>
#include <fcntl.h>
#include <fuse.h>
#include "winposix.h"

struct _DIR
{
    HANDLE handle;
    struct dirent dirent;
    char path[];
};

static int maperror(int winerrno);

static inline void *error0(void)
{
    errno = maperror(GetLastError());
    return 0;
}

static inline int error(void)
{
    errno = maperror(GetLastError());
    return -1;
}

char *realpath(const char *path, char *resolved)
{
    char *result;

    HANDLE h = CreateFileA(path,
        FILE_READ_ATTRIBUTES, 0, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);

    if (INVALID_HANDLE_VALUE == h)
        return error0();

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
        int LastError = GetLastError();

        if (result != resolved)
            free(result);

        errno = maperror(LastError);
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
        return error();
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
        DesiredAccess, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0/* default security */,
        CreationDisposition, FILE_ATTRIBUTE_NORMAL, 0);

    if (INVALID_HANDLE_VALUE == h)
        return error();

    return (int)(intptr_t)h;
}

int fstat(int fd, struct fuse_stat *stbuf)
{
    HANDLE h = (HANDLE)(intptr_t)fd;
    BY_HANDLE_FILE_INFORMATION FileInfo;
    UINT64 CreationTime, LastAccessTime, LastWriteTime;

    if (!GetFileInformationByHandle(h, &FileInfo))
        return error();

    CreationTime = ((PLARGE_INTEGER)(&FileInfo.ftCreationTime))->QuadPart - 116444736000000000;
    LastAccessTime = ((PLARGE_INTEGER)(&FileInfo.ftLastAccessTime))->QuadPart - 116444736000000000;
    LastWriteTime = ((PLARGE_INTEGER)(&FileInfo.ftLastWriteTime))->QuadPart - 116444736000000000;

    memset(stbuf, 0, sizeof *stbuf);
    stbuf->st_mode = 0755 |
        ((FileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 0040000/* S_IFDIR */ : 0);
    stbuf->st_nlink = 1;
    stbuf->st_size = ((UINT64)FileInfo.nFileSizeHigh << 32) | ((UINT64)FileInfo.nFileSizeLow);
    stbuf->st_atim.tv_sec = LastAccessTime / 10000000;
    stbuf->st_atim.tv_nsec = LastAccessTime % 10000000 * 100;
    stbuf->st_mtim.tv_sec = LastWriteTime / 10000000;
    stbuf->st_mtim.tv_nsec = LastWriteTime % 10000000 * 100;
    stbuf->st_ctim.tv_sec = LastWriteTime / 10000000;
    stbuf->st_ctim.tv_nsec = LastWriteTime % 10000000 * 100;
    stbuf->st_birthtim.tv_sec = CreationTime / 10000000;
    stbuf->st_birthtim.tv_nsec = CreationTime % 10000000 * 100;

    return 0;
}

int ftruncate(int fd, fuse_off_t size)
{
    HANDLE h = (HANDLE)(intptr_t)fd;
    FILE_END_OF_FILE_INFO EndOfFileInfo;

    EndOfFileInfo.EndOfFile.QuadPart = size;

    if (!SetFileInformationByHandle(h, FileEndOfFileInfo, &EndOfFileInfo, sizeof EndOfFileInfo))
        return error();

    return 0;
}

int pread(int fd, void *buf, size_t nbyte, fuse_off_t offset)
{
    HANDLE h = (HANDLE)(intptr_t)fd;
    OVERLAPPED Overlapped = { 0 };
    DWORD BytesTransferred;

    Overlapped.Offset = (DWORD)offset;
    Overlapped.OffsetHigh = (DWORD)(offset >> 32);

    if (!ReadFile(h, buf, (DWORD)nbyte, &BytesTransferred, &Overlapped))
        return error();

    return BytesTransferred;
}

int pwrite(int fd, const void *buf, size_t nbyte, fuse_off_t offset)
{
    HANDLE h = (HANDLE)(intptr_t)fd;
    OVERLAPPED Overlapped = { 0 };
    DWORD BytesTransferred;

    Overlapped.Offset = (DWORD)offset;
    Overlapped.OffsetHigh = (DWORD)(offset >> 32);

    if (!WriteFile(h, buf, (DWORD)nbyte, &BytesTransferred, &Overlapped))
        return error();

    return BytesTransferred;
}

int fsync(int fd)
{
    HANDLE h = (HANDLE)(intptr_t)fd;

    if (!FlushFileBuffers(h))
        return error();

    return 0;
}

int close(int fd)
{
    HANDLE h = (HANDLE)(intptr_t)fd;

    if (!CloseHandle(h))
        return error();

    return 0;
}

int lstat(const char *path, struct fuse_stat *stbuf)
{
    int res = -1;
    int fd;

    fd = open(path, O_RDONLY);
    if (-1 != fd)
    {
        res = fstat(fd, stbuf);
        close(fd);
    }

    return res;
}

int chmod(const char *path, fuse_mode_t mode)
{
    /* we do not support file security */
    return 0;
}

int lchown(const char *path, fuse_uid_t uid, fuse_gid_t gid)
{
    /* we do not support file security */
    return 0;
}

int truncate(const char *path, fuse_off_t size)
{
    int res = -1;
    int fd;

    fd = open(path, O_WRONLY);
    if (-1 != fd)
    {
        res = ftruncate(fd, size);
        close(fd);
    }

    return res;
}

int utime(const char *path, const struct fuse_utimbuf *timbuf)
{
    int res = -1;
    int fd;
    UINT64 LastAccessTime, LastWriteTime;

    fd = open(path, O_WRONLY);
    if (-1 != fd)
    {
        LastAccessTime = timbuf->actime * 10000000 + 116444736000000000;
        LastWriteTime = timbuf->modtime * 10000000 + 116444736000000000;

        res = SetFileTime((HANDLE)(intptr_t)fd,
            0, (PFILETIME)&LastAccessTime, (PFILETIME)&LastWriteTime) ? 0 : error();

        close(fd);
    }

    return res;
}

int unlink(const char *path)
{
    if (!DeleteFileA(path))
        return error();

    return 0;
}

int rename(const char *oldpath, const char *newpath)
{
    if (!MoveFileExA(oldpath, newpath, MOVEFILE_REPLACE_EXISTING))
        return error();

    return 0;
}

int mkdir(const char *path, fuse_mode_t mode)
{
    if (!CreateDirectoryA(path, 0/* default security */))
        return error();

    return 0;
}

int rmdir(const char *path)
{
    if (!RemoveDirectoryA(path))
        return error();

    return 0;
}

DIR *opendir(const char *path)
{
    DWORD FileAttributes = GetFileAttributesA(path);
    if (INVALID_FILE_ATTRIBUTES == FileAttributes)
        return error0();
    if (0 == (FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        errno = ENOTDIR;
        return 0;
    }

    size_t pathlen = strlen(path);
    if (0 < pathlen && '/' == path[pathlen - 1])
        pathlen--;

    DIR *dirp = malloc(sizeof *dirp + pathlen + 3); /* sets errno */
    if (0 == dirp)
        return 0;

    memset(dirp, 0, sizeof *dirp);
    dirp->handle = INVALID_HANDLE_VALUE;
    memcpy(dirp->path, path, pathlen);
    dirp->path[pathlen + 0] = '/';
    dirp->path[pathlen + 1] = '*';
    dirp->path[pathlen + 2] = '\0';

    return dirp;
}

void rewinddir(DIR *dirp)
{
    if (INVALID_HANDLE_VALUE != dirp->handle)
        FindClose(dirp->handle);
}

struct dirent *readdir(DIR *dirp)
{
    WIN32_FIND_DATAA FindData;

    if (INVALID_HANDLE_VALUE != dirp->handle)
    {
        dirp->handle = FindFirstFileA(dirp->path, &FindData);
        if (INVALID_HANDLE_VALUE == dirp)
            return error0();
    }
    else
    {
        if (!FindNextFileA(dirp->handle, &FindData))
            return error0();
    }

    strcpy(dirp->dirent.d_name, FindData.cFileName);

    return &dirp->dirent;
}

int closedir(DIR *dirp)
{
    if (INVALID_HANDLE_VALUE != dirp->handle)
        FindClose(dirp->handle);

    free(dirp);

    return 0;
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
