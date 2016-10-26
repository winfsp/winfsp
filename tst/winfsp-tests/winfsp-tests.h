/**
 * @file winfsp-tests.h
 *
 * @copyright 2015-2016 Bill Zissimopoulos
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

#define ABORT(s)\
    do\
    {\
        void tlib_printf(const char *fmt, ...);\
        tlib_printf("ABORT: %s: %s\n", __func__, s);\
        abort();\
    } while (0,0)

#define testalpha(c)                    ('a' <= ((c) | 0x20) && ((c) | 0x20) <= 'z')
#define togglealpha(c)                  ((c) ^ 0x20)

#if !defined(WINFSP_TESTS_NO_HOOKS)
#define CreateFileW HookCreateFileW
#define CloseHandle HookCloseHandle
#define SetFileAttributesW HookSetFileAttributesW
#define CreateDirectoryW HookCreateDirectoryW
#define DeleteFileW HookDeleteFileW
#define RemoveDirectoryW HookRemoveDirectoryW
#define MoveFileExW HookMoveFileExW
#define FindFirstFileW HookFindFirstFileW
#define FindFirstStreamW HookFindFirstStreamW
#define GetDiskFreeSpaceW HookGetDiskFreeSpaceW
#define GetVolumeInformationW HookGetVolumeInformationW
#define SetVolumeLabelW HookSetVolumeLabelW
#endif
HANDLE HookCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);
BOOL HookCloseHandle(
    HANDLE hObject);
BOOL HookSetFileAttributesW(
    LPCWSTR lpFileName,
    DWORD dwFileAttributes);
BOOL HookCreateDirectoryW(
    LPCWSTR lpPathName,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes);
BOOL HookDeleteFileW(
    LPCWSTR lpFileName);
BOOL HookRemoveDirectoryW(
    LPCWSTR lpPathName);
BOOL HookMoveFileExW(
    LPCWSTR lpExistingFileName,
    LPCWSTR lpNewFileName,
    DWORD dwFlags);
HANDLE HookFindFirstFileW(
    LPCWSTR lpFileName,
    LPWIN32_FIND_DATAW lpFindFileData);
HANDLE HookFindFirstStreamW(
    LPCWSTR lpFileName,
    STREAM_INFO_LEVELS InfoLevel,
    LPVOID lpFindStreamData,
    DWORD dwFlags);
BOOL HookGetDiskFreeSpaceW(
    LPCWSTR lpRootPathName,
    LPDWORD lpSectorsPerCluster,
    LPDWORD lpBytesPerSector,
    LPDWORD lpNumberOfFreeClusters,
    LPDWORD lpTotalNumberOfClusters);
BOOL HookGetVolumeInformationW(
    LPCWSTR lpRootPathName,
    LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPWSTR lpFileSystemNameBuffer,
    DWORD nFileSystemNameSize);
BOOL HookSetVolumeLabelW(
    LPCWSTR lpRootPathName,
    LPCWSTR lpVolumeName);

HANDLE ResilientCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);
BOOL ResilientCloseHandle(
    HANDLE hObject);
BOOL ResilientDeleteFileW(
    LPCWSTR lpFileName);

typedef struct
{
    BOOLEAN Disposition;
} MY_FILE_DISPOSITION_INFO;

void *memfs_start_ex(ULONG Flags, ULONG FileInfoTimeout);
void *memfs_start(ULONG Flags);
void memfs_stop(void *data);
PWSTR memfs_volumename(void *data);

int mywcscmp(PWSTR a, int alen, PWSTR b, int blen);
int myrand(void);

extern int NtfsTests;
extern int WinFspDiskTests;
extern int WinFspNetTests;

extern BOOLEAN OptResilient;
extern BOOLEAN OptCaseInsensitive;
extern BOOLEAN OptCaseRandomize;
extern WCHAR OptMountPointBuf[], *OptMountPoint;
extern WCHAR OptShareNameBuf[], *OptShareName, *OptShareTarget;
    extern WCHAR OptShareComputer[];
    extern ULONG OptSharePrefixLength;
extern HANDLE OptNoTraverseToken;
    extern LUID OptNoTraverseLuid;

extern int memfs_running;
