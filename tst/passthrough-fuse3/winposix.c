/**
 * @file winposix.c
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

/*
 * This is a very simple Windows POSIX layer. It handles all the POSIX
 * file API's required to implement passthrough-fuse in POSIX, however
 * the API handling is rather unsophisticated.
 *
 * Ways to improve it: use the FspPosix* API's to properly handle
 * file names and security.
 */

#include <winfsp/winfsp.h>
#include <fcntl.h>
#include <fuse.h>
#include "winposix.h"

#pragma comment(lib, "ntdll.lib")

typedef struct _FILE_GET_EA_INFORMATION
{
    ULONG NextEntryOffset;
    UCHAR EaNameLength;
    CHAR EaName[1];
} FILE_GET_EA_INFORMATION, *PFILE_GET_EA_INFORMATION;

NTSYSAPI NTSTATUS NTAPI NtQueryEaFile(
    IN HANDLE               FileHandle,
    OUT PIO_STATUS_BLOCK    IoStatusBlock,
    OUT PVOID               Buffer,
    IN ULONG                Length,
    IN BOOLEAN              ReturnSingleEntry,
    IN PVOID                EaList OPTIONAL,
    IN ULONG                EaListLength,
    IN PULONG               EaIndex OPTIONAL,
    IN BOOLEAN              RestartScan);
NTSYSAPI NTSTATUS NTAPI NtSetEaFile(
    IN HANDLE               FileHandle,
    OUT PIO_STATUS_BLOCK    IoStatusBlock,
    IN PVOID                EaBuffer,
    IN ULONG                EaBufferSize);
#define NEXT_EA(Ea, EaEnd)              \
    (0 != (Ea)->NextEntryOffset ? (PVOID)((PUINT8)(Ea) + (Ea)->NextEntryOffset) : (EaEnd))

struct _DIR
{
    HANDLE h, fh;
    struct dirent de;
    char path[];
};

#if defined(FSP_FUSE_USE_STAT_EX)
static inline uint32_t MapFileAttributesToFlags(UINT32 FileAttributes)
{
    uint32_t flags = 0;

    if (FileAttributes & FILE_ATTRIBUTE_READONLY)
        flags |= FSP_FUSE_UF_READONLY;
    if (FileAttributes & FILE_ATTRIBUTE_HIDDEN)
        flags |= FSP_FUSE_UF_HIDDEN;
    if (FileAttributes & FILE_ATTRIBUTE_SYSTEM)
        flags |= FSP_FUSE_UF_SYSTEM;
    if (FileAttributes & FILE_ATTRIBUTE_ARCHIVE)
        flags |= FSP_FUSE_UF_ARCHIVE;

    return flags;
}

static inline UINT32 MapFlagsToFileAttributes(uint32_t flags)
{
    UINT32 FileAttributes = 0;

    if (flags & FSP_FUSE_UF_READONLY)
        FileAttributes |= FILE_ATTRIBUTE_READONLY;
    if (flags & FSP_FUSE_UF_HIDDEN)
        FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
    if (flags & FSP_FUSE_UF_SYSTEM)
        FileAttributes |= FILE_ATTRIBUTE_SYSTEM;
    if (flags & FSP_FUSE_UF_ARCHIVE)
        FileAttributes |= FILE_ATTRIBUTE_ARCHIVE;

    return FileAttributes;
}
#endif

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
    stbuf->f_frsize = SectorsPerCluster * BytesPerSector;
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

    if (!GetFileInformationByHandle(h, &FileInfo))
        return error();

    memset(stbuf, 0, sizeof *stbuf);
    stbuf->st_mode = 0777 |
        ((FileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 0040000/* S_IFDIR */ : 0);
    stbuf->st_nlink = 1;
    stbuf->st_size = ((UINT64)FileInfo.nFileSizeHigh << 32) | ((UINT64)FileInfo.nFileSizeLow);
    FspPosixFileTimeToUnixTime(*(PUINT64)&FileInfo.ftCreationTime, (void *)&stbuf->st_birthtim);
    FspPosixFileTimeToUnixTime(*(PUINT64)&FileInfo.ftLastAccessTime, (void *)&stbuf->st_atim);
    FspPosixFileTimeToUnixTime(*(PUINT64)&FileInfo.ftLastWriteTime, (void *)&stbuf->st_mtim);
    FspPosixFileTimeToUnixTime(*(PUINT64)&FileInfo.ftLastWriteTime, (void *)&stbuf->st_ctim);
#if defined(FSP_FUSE_USE_STAT_EX)
    stbuf->st_flags = MapFileAttributesToFlags(FileInfo.dwFileAttributes);
#endif

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

int lchflags(const char *path, uint32_t flags)
{
#if defined(FSP_FUSE_USE_STAT_EX)
    UINT32 FileAttributes = MapFlagsToFileAttributes(flags);

    if (0 == FileAttributes)
        FileAttributes = FILE_ATTRIBUTE_NORMAL;

    if (!SetFileAttributesA(path, FileAttributes))
        return error();
#endif

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
    if (0 == timbuf)
        return utimensat(AT_FDCWD, path, 0, AT_SYMLINK_NOFOLLOW);
    else
    {
        struct fuse_timespec times[2];
        times[0].tv_sec = timbuf->actime;
        times[0].tv_nsec = 0;
        times[1].tv_sec = timbuf->modtime;
        times[1].tv_nsec = 0;
        return utimensat(AT_FDCWD, path, times, AT_SYMLINK_NOFOLLOW);
    }
}

int utimensat(int dirfd, const char *path, const struct fuse_timespec times[2], int flag)
{
    /* ignore dirfd and assume that it is always AT_FDCWD */
    /* ignore flag and assume that it is always AT_SYMLINK_NOFOLLOW */

    HANDLE h = CreateFileA(path,
        FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == h)
        return error();

    UINT64 LastAccessTime, LastWriteTime;
    if (0 == times)
    {
        FILETIME FileTime;
        GetSystemTimeAsFileTime(&FileTime);
        LastAccessTime = LastWriteTime = *(PUINT64)&FileTime;
    }
    else
    {
        FspPosixUnixTimeToFileTime((void *)&times[0], &LastAccessTime);
        FspPosixUnixTimeToFileTime((void *)&times[1], &LastWriteTime);
    }

    int res = SetFileTime(h,
        0, (PFILETIME)&LastAccessTime, (PFILETIME)&LastWriteTime) ? 0 : error();

    CloseHandle(h);

    return res;
}

int setcrtime(const char *path, const struct fuse_timespec *tv)
{
    HANDLE h = CreateFileA(path,
        FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == h)
        return error();

    UINT64 CreationTime;
    FspPosixUnixTimeToFileTime((void *)tv, &CreationTime);

    int res = SetFileTime(h,
        (PFILETIME)&CreationTime, 0, 0) ? 0 : error();

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

static int lsetea(const char *path, PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength)
{
    HANDLE h = CreateFileA(path,
        FILE_WRITE_EA | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == h)
        return error();

    IO_STATUS_BLOCK Iosb;
    NTSTATUS Status = NtSetEaFile(h, &Iosb, Ea, EaLength);

    CloseHandle(h);

    if (!NT_SUCCESS(Status))
        switch (Status)
        {
        case STATUS_INVALID_EA_NAME:
        case STATUS_EA_LIST_INCONSISTENT:
        case STATUS_EA_CORRUPT_ERROR:
        case STATUS_NONEXISTENT_EA_ENTRY:
        case STATUS_NO_MORE_EAS:
        case STATUS_NO_EAS_ON_FILE:
            errno = EINVAL;
            return -1;
        default:
            SetLastError(RtlNtStatusToDosError(Status));
            return error();
        }

    return 0;
}

static int lgetea(const char *path,
    PFILE_GET_EA_INFORMATION GetEa, ULONG GetEaLength,
    PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength)
{
    HANDLE h = CreateFileA(path,
        FILE_READ_EA | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == h)
        return error();

    IO_STATUS_BLOCK Iosb;
    NTSTATUS Status = NtQueryEaFile(h, &Iosb, Ea, EaLength, FALSE, GetEa, GetEaLength, 0, TRUE);

    CloseHandle(h);

    if (!NT_SUCCESS(Status))
        switch (Status)
        {
        case STATUS_INVALID_EA_NAME:
        case STATUS_EA_LIST_INCONSISTENT:
        case STATUS_EA_CORRUPT_ERROR:
        case STATUS_NONEXISTENT_EA_ENTRY:
        case STATUS_NO_MORE_EAS:
            errno = EINVAL;
            return -1;
        case STATUS_NO_EAS_ON_FILE:
            if (0 == GetEa)
                return 0;
            else
            {
                errno = ENODATA;
                return -1;
            }
        default:
            SetLastError(RtlNtStatusToDosError(Status));
            return error();
        }
    else if (0 == GetEa &&
        (FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) > Iosb.Information || 0 == Ea->EaValueLength))
    {
        errno = ENODATA;
        return -1;
    }

    return (ULONG)Iosb.Information;
}

int lsetxattr(const char *path, const char *name, const void *value, size_t size, int flags)
{
    union
    {
        FILE_FULL_EA_INFORMATION V;
        UINT8 B[1024];
    } EaBuf;
    PFILE_FULL_EA_INFORMATION Ea = &EaBuf.V;
    ULONG EaLength;

    size_t namelen = strlen(name);
    if (254 < namelen || 0xffff < size)
    {
        errno = EINVAL;
        return -1;
    }

    EaLength = (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + namelen + 1 + size);
    if (sizeof EaBuf < EaLength)
    {
        Ea = malloc(EaLength); /* sets errno */
        if (0 == Ea)
            return -1;
    }

    memset(Ea, 0, sizeof(FILE_FULL_EA_INFORMATION));
    Ea->EaNameLength = (UCHAR)namelen;
    Ea->EaValueLength = (USHORT)size;
    memcpy(Ea->EaName, name, namelen + 1);
    memcpy(Ea->EaName + namelen + 1, value, size);

    int res = lsetea(path, Ea, EaLength); /* sets errno */

    if (&EaBuf.V != Ea)
        free(Ea);

    return res;
}

int lgetxattr(const char *path, const char *name, void *value, size_t size0)
{
    size_t size = 0 == size0 || 0xffff < size0 ? 0xffff : size0;
    union
    {
        FILE_GET_EA_INFORMATION V;
        UINT8 B[FIELD_OFFSET(FILE_GET_EA_INFORMATION, EaName) + 255];
    } GetEaBuf;
    PFILE_GET_EA_INFORMATION GetEa = &GetEaBuf.V;
    union
    {
        FILE_FULL_EA_INFORMATION V;
        UINT8 B[1024];
    } EaBuf;
    PFILE_FULL_EA_INFORMATION Ea = &EaBuf.V;
    ULONG GetEaLength, EaLength;

    size_t namelen = strlen(name);
    if (254 < namelen)
    {
        errno = EINVAL;
        return -1;
    }

    EaLength = (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + namelen + 1 + size);
    if (sizeof EaBuf < EaLength)
    {
        Ea = malloc(EaLength); /* sets errno */
        if (0 == Ea)
            return -1;
    }

    GetEaLength = (ULONG)(FIELD_OFFSET(FILE_GET_EA_INFORMATION, EaName) + namelen + 1);
    memset(GetEa, 0, sizeof(FILE_GET_EA_INFORMATION));
    GetEa->EaNameLength = (UCHAR)namelen;
    memcpy(GetEa->EaName, name, namelen + 1);

    int res = lgetea(path, GetEa, GetEaLength, Ea, EaLength);
    if (0 < res)
    {
        res = Ea->EaValueLength;
        if (0 == size0)
            ;
        else if (res <= size0)
            memcpy(value, Ea->EaName + Ea->EaNameLength + 1, res);
        else
        {
            errno = ERANGE;
            res = -1;
        }
    }
    else if (0 == res) /* should not happen! */
    {
    }

    if (&EaBuf.V != Ea)
        free(Ea);

    return res;
}

int llistxattr(const char *path, char *namebuf, size_t size)
{
    PFILE_FULL_EA_INFORMATION Ea = 0;
    ULONG EaLength;

    EaLength = (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + 254 + 1 + 0xffff);
    Ea = malloc(EaLength); /* sets errno */
    if (0 == Ea)
        return -1;

    int res = lgetea(path, 0, 0, Ea, EaLength);
    if (0 < res)
    {
        PFILE_FULL_EA_INFORMATION EaEnd = (PVOID)((PUINT8)Ea + res);
        res = 0;
        for (PFILE_FULL_EA_INFORMATION EaPtr = Ea; EaEnd > EaPtr; EaPtr = NEXT_EA(EaPtr, EaEnd))
            res += EaPtr->EaNameLength + 1;

        if (0 == size)
            ;
        else if (res <= size)
        {
            char *p = namebuf;
            for (PFILE_FULL_EA_INFORMATION EaPtr = Ea; EaEnd > EaPtr; EaPtr = NEXT_EA(EaPtr, EaEnd))
            {
                memcpy(p, EaPtr->EaName, EaPtr->EaNameLength + 1);
                p += EaPtr->EaNameLength + 1;
            }
        }
        else
        {
            errno = ERANGE;
            res = -1;
        }
    }

    free(Ea);

    return res;
}

int lremovexattr(const char *path, const char *name)
{
    return lsetxattr(path, name, 0, 0, 0);
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
    struct fuse_stat *stbuf = &dirp->de.d_stat;

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

    memset(stbuf, 0, sizeof *stbuf);
    stbuf->st_mode = 0777 |
        ((FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 0040000/* S_IFDIR */ : 0);
    stbuf->st_nlink = 1;
    stbuf->st_size = ((UINT64)FindData.nFileSizeHigh << 32) | ((UINT64)FindData.nFileSizeLow);
    FspPosixFileTimeToUnixTime(*(PUINT64)&FindData.ftCreationTime, (void *)&stbuf->st_birthtim);
    FspPosixFileTimeToUnixTime(*(PUINT64)&FindData.ftLastAccessTime, (void *)&stbuf->st_atim);
    FspPosixFileTimeToUnixTime(*(PUINT64)&FindData.ftLastWriteTime, (void *)&stbuf->st_mtim);
    FspPosixFileTimeToUnixTime(*(PUINT64)&FindData.ftLastWriteTime, (void *)&stbuf->st_ctim);
#if defined(FSP_FUSE_USE_STAT_EX)
    stbuf->st_flags = MapFileAttributesToFlags(FindData.dwFileAttributes);
#endif

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

long WinFspLoad(void)
{
    return FspLoad(0);
}
