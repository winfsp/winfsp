/**
 * @file winfsp-tests.h
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

HANDLE WINAPI HookCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);
BOOL WINAPI HookCloseHandle(
    HANDLE hObject);
BOOL WINAPI HookSetFileAttributesW(
    LPCWSTR lpFileName,
    DWORD dwFileAttributes);
BOOL WINAPI HookCreateDirectoryW(
    LPCWSTR lpPathName,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes);
BOOL WINAPI HookDeleteFileW(
    LPCWSTR lpFileName);
BOOL WINAPI HookRemoveDirectoryW(
    LPCWSTR lpPathName);
BOOL WINAPI HookMoveFileExW(
    LPCWSTR lpExistingFileName,
    LPCWSTR lpNewFileName,
    DWORD dwFlags);
HANDLE WINAPI HookFindFirstFileW(
    LPCWSTR lpFileName,
    LPWIN32_FIND_DATAW lpFindFileData);
HANDLE WINAPI HookFindFirstStreamW(
    LPCWSTR lpFileName,
    STREAM_INFO_LEVELS InfoLevel,
    LPVOID lpFindStreamData,
    DWORD dwFlags);
BOOL WINAPI HookGetDiskFreeSpaceW(
    LPCWSTR lpRootPathName,
    LPDWORD lpSectorsPerCluster,
    LPDWORD lpBytesPerSector,
    LPDWORD lpNumberOfFreeClusters,
    LPDWORD lpTotalNumberOfClusters);
BOOL WINAPI HookGetVolumeInformationW(
    LPCWSTR lpRootPathName,
    LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPWSTR lpFileSystemNameBuffer,
    DWORD nFileSystemNameSize);
BOOL WINAPI HookSetVolumeLabelW(
    LPCWSTR lpRootPathName,
    LPCWSTR lpVolumeName);
BOOL WINAPI HookSetCurrentDirectoryW(
    LPCWSTR lpPathName);
static inline BOOL RealSetCurrentDirectoryW(
    LPCWSTR lpPathName)
{
    return SetCurrentDirectoryW(lpPathName);
}
BOOL WINAPI HookCreateProcessW(
    LPCWSTR lpApplicationName,
    LPWSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    LPVOID lpEnvironment,
    LPCWSTR lpCurrentDirectory,
    LPSTARTUPINFOW lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation);
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
#define SetCurrentDirectoryW HookSetCurrentDirectoryW
#define CreateProcessW HookCreateProcessW
#endif

HANDLE WINAPI ResilientCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);
BOOL WINAPI ResilientCloseHandle(
    HANDLE hObject);
BOOL WINAPI ResilientDeleteFileW(
    LPCWSTR lpFileName);
BOOL WINAPI ResilientRemoveDirectoryW(
    LPCWSTR lpPathName);

static inline
BOOLEAN BestEffortCreateSymbolicLinkW(
    PWSTR SymlinkFileName,
    PWSTR TargetFileName,
    DWORD Flags)
{
    BOOLEAN Success = CreateSymbolicLinkW(
        SymlinkFileName,
        TargetFileName,
        Flags | 2/*SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE*/);
    if (!Success && ERROR_INVALID_PARAMETER == GetLastError())
        Success = CreateSymbolicLinkW(
            SymlinkFileName,
            TargetFileName,
            Flags & ~2/*SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE*/);
    return Success;
}

typedef struct
{
    BOOLEAN Disposition;
} MY_FILE_DISPOSITION_INFO;
typedef struct
{
    ULONG Flags;
} MY_FILE_DISPOSITION_INFO_EX;

void *memfs_start_ex(ULONG Flags, ULONG FileInfoTimeout);
void *memfs_start(ULONG Flags);
void memfs_stop(void *data);
PWSTR memfs_volumename(void *data);

int mywcscmp(PWSTR a, int alen, PWSTR b, int blen);
int myrand(void);

#define GetTestDirectory(D)             GetTestDirectoryEx(DirBuf, sizeof DirBuf, 0)
#define GetTestDirectoryAndDrive(D,V)   GetTestDirectoryEx(DirBuf, sizeof DirBuf, V)
VOID GetTestDirectoryEx(PWSTR DirBuf, ULONG DirBufSize, PWSTR DriveBuf);

extern int NtfsTests;
extern int WinFspDiskTests;
extern int WinFspNetTests;

extern BOOLEAN OptExternal;
extern BOOLEAN OptFuseExternal;
extern BOOLEAN OptResilient;
extern BOOLEAN OptCaseInsensitiveCmp;
extern BOOLEAN OptCaseInsensitive;
extern BOOLEAN OptCaseRandomize;
extern BOOLEAN OptFlushAndPurgeOnCleanup;
extern BOOLEAN OptLegacyUnlinkRename;
extern BOOLEAN OptNotify;
extern WCHAR OptOplock;
extern WCHAR OptMountPointBuf[], *OptMountPoint;
extern WCHAR OptShareNameBuf[], *OptShareName, *OptShareTarget;
    extern WCHAR OptShareComputer[];
    extern ULONG OptSharePrefixLength;
extern HANDLE OptNoTraverseToken;
    extern LUID OptNoTraverseLuid;

extern int memfs_running;
extern HANDLE memfs_handle;
