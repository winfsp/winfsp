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
    HANDLE h, fh;
    struct dirent de;
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

    if (0 == resolved)
    {
        result = malloc(PATH_MAX); /* sets errno */
        if (0 == result)
            return 0;
    }
    else
        result = resolved;

    int err = 0;
    DWORD len = GetFullPathNameA(path, PATH_MAX, result, 0);
    if (0 == len)
        err = GetLastError();
    else if (PATH_MAX < len)
        err = ERROR_INVALID_PARAMETER;

    if (0 == err)
    {
        HANDLE h = CreateFileA(result,
            FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            0,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
        if (INVALID_HANDLE_VALUE != h)
            CloseHandle(h);
        else
            err = GetLastError();
    }

    if (0 != err)
    {
        if (result != resolved)
            free(result);

        errno = maperror(err);
        result = 0;
    }

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

    return 0;
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
        CreationDisposition, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 0);

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
    stbuf->st_mode = 0777 |
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
    {
        if (ERROR_HANDLE_EOF == GetLastError())
            return 0;
        return error();
    }

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
    HANDLE h = CreateFileA(path,
        FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == h)
        return error();

    int res = fstat((int)(intptr_t)h, stbuf);

    CloseHandle(h);

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
    HANDLE h = CreateFileA(path,
        FILE_WRITE_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == h)
        return error();

    int res = ftruncate((int)(intptr_t)h, size);

    CloseHandle(h);

    return res;
}

int utime(const char *path, const struct fuse_utimbuf *timbuf)
{
    HANDLE h = CreateFileA(path,
        FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == h)
        return error();

    UINT64 LastAccessTime = timbuf->actime * 10000000 + 116444736000000000;
    UINT64 LastWriteTime = timbuf->modtime * 10000000 + 116444736000000000;

    int res = SetFileTime(h,
        0, (PFILETIME)&LastAccessTime, (PFILETIME)&LastWriteTime) ? 0 : error();

    CloseHandle(h);

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
    HANDLE h = CreateFileA(path,
        FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == h)
        return error0();

    size_t pathlen = strlen(path);
    if (0 < pathlen && '/' == path[pathlen - 1])
        pathlen--;

    DIR *dirp = malloc(sizeof *dirp + pathlen + 3); /* sets errno */
    if (0 == dirp)
    {
        CloseHandle(h);
        return 0;
    }

    memset(dirp, 0, sizeof *dirp);
    dirp->h = h;
    dirp->fh = INVALID_HANDLE_VALUE;
    memcpy(dirp->path, path, pathlen);
    dirp->path[pathlen + 0] = '/';
    dirp->path[pathlen + 1] = '*';
    dirp->path[pathlen + 2] = '\0';

    return dirp;
}

int dirfd(DIR *dirp)
{
    return (int)(intptr_t)dirp->h;
}

void rewinddir(DIR *dirp)
{
    if (INVALID_HANDLE_VALUE != dirp->fh)
    {
        FindClose(dirp->fh);
        dirp->fh = INVALID_HANDLE_VALUE;
    }
}

struct dirent *readdir(DIR *dirp)
{
    WIN32_FIND_DATAA FindData;

    if (INVALID_HANDLE_VALUE == dirp->fh)
    {
        dirp->fh = FindFirstFileA(dirp->path, &FindData);
        if (INVALID_HANDLE_VALUE == dirp->fh)
            return error0();
    }
    else
    {
        if (!FindNextFileA(dirp->fh, &FindData))
        {
            if (ERROR_NO_MORE_FILES == GetLastError())
                return 0;
            return error0();
        }
    }

    strcpy(dirp->de.d_name, FindData.cFileName);

    return &dirp->de;
}

int closedir(DIR *dirp)
{
    if (INVALID_HANDLE_VALUE != dirp->fh)
        FindClose(dirp->fh);

    CloseHandle(dirp->h);
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

static NTSTATUS WinFspLoad(VOID)
{
#if defined(_WIN64)
#define FSP_DLLNAME                     "winfsp-x64.dll"
#else
#define FSP_DLLNAME                     "winfsp-x86.dll"
#endif
#define FSP_DLLPATH                     "bin\\" FSP_DLLNAME

    WCHAR PathBuf[MAX_PATH];
    DWORD Size;
    HKEY RegKey;
    LONG Result;
    HMODULE Module;

    Module = LoadLibraryW(L"" FSP_DLLNAME);
    if (0 == Module)
    {
        Result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\WinFsp",
            0, KEY_READ | KEY_WOW64_32KEY, &RegKey);
        if (ERROR_SUCCESS == Result)
        {
            Size = sizeof PathBuf - sizeof L"" FSP_DLLPATH + sizeof(WCHAR);
            Result = RegGetValueW(RegKey, 0, L"InstallDir",
                RRF_RT_REG_SZ, 0, PathBuf, &Size);
            RegCloseKey(RegKey);
        }
        if (ERROR_SUCCESS != Result)
            return STATUS_OBJECT_NAME_NOT_FOUND;

        RtlCopyMemory(PathBuf + (Size / sizeof(WCHAR) - 1), L"" FSP_DLLPATH, sizeof L"" FSP_DLLPATH);
        Module = LoadLibraryW(PathBuf);
        if (0 == Module)
            return STATUS_DLL_NOT_FOUND;
    }

    return STATUS_SUCCESS;

#undef FSP_DLLNAME
#undef FSP_DLLPATH
}
