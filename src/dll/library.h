/**
 * @file dll/library.h
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

#ifndef WINFSP_DLL_LIBRARY_H_INCLUDED
#define WINFSP_DLL_LIBRARY_H_INCLUDED

#define WINFSP_DLL_INTERNAL
#include <winfsp/winfsp.h>
#include <winfsp/launch.h>
#include <shared/um/minimal.h>
#include <strsafe.h>

#include <shared/ku/config.h>

#define LIBRARY_NAME                    FSP_FSCTL_PRODUCT_NAME

/* DEBUGLOG */
#if !defined(NDEBUG)
#define DEBUGLOG(fmt, ...)              \
    FspDebugLog("[U] " LIBRARY_NAME "!" __FUNCTION__ ": " fmt "\n", __VA_ARGS__)
#define DEBUGLOGSD(fmt, SD)             \
    FspDebugLogSD("[U] " LIBRARY_NAME "!" __FUNCTION__ ": " fmt "\n", SD)
#define DEBUGLOGSID(fmt, Sid)           \
    FspDebugLogSid("[U] " LIBRARY_NAME "!" __FUNCTION__ ": " fmt "\n", Sid)
#else
#define DEBUGLOG(fmt, ...)              ((void)0)
#define DEBUGLOGSD(fmt, SD)             ((void)0)
#define DEBUGLOGSID(fmt, Sid)           ((void)0)
#endif

VOID FspWksidFinalize(BOOLEAN Dynamic);
VOID FspPosixFinalize(BOOLEAN Dynamic);
VOID FspEventLogFinalize(BOOLEAN Dynamic);
VOID FspFileSystemFinalize(BOOLEAN Dynamic);
VOID FspServiceFinalize(BOOLEAN Dynamic);
VOID fsp_fuse_finalize(BOOLEAN Dynamic);
VOID fsp_fuse_finalize_thread(VOID);

NTSTATUS FspFsctlRegister(VOID);
NTSTATUS FspFsctlUnregister(VOID);
NTSTATUS FspNpRegister(VOID);
NTSTATUS FspNpUnregister(VOID);
NTSTATUS FspEventLogRegister(VOID);
NTSTATUS FspEventLogUnregister(VOID);

PSID FspWksidNew(WELL_KNOWN_SID_TYPE WellKnownSidType, PNTSTATUS PResult);
PSID FspWksidGet(WELL_KNOWN_SID_TYPE WellKnownSidType);

ULONG FspLdapConnect(PWSTR HostName, PVOID *PLdap);
VOID FspLdapClose(PVOID Ldap);
ULONG FspLdapGetValue(PVOID Ldap, PWSTR Base, ULONG Scope, PWSTR Filter, PWSTR Attribute,
    PWSTR *PValue);
ULONG FspLdapGetDefaultNamingContext(PVOID Ldap, PWSTR *PValue);
ULONG FspLdapGetTrustPosixOffset(PVOID Ldap, PWSTR Context, PWSTR Domain, PWSTR *PValue);

PWSTR FspDiagIdent(VOID);

#define FspFileSystemDirectoryBufferEntryInvalid ((ULONG)-1)
VOID FspFileSystemPeekInDirectoryBuffer(PVOID *PDirBuffer,
    PUINT8 *PBuffer, PULONG *PIndex, PULONG PCount);

BOOL WINAPI FspServiceConsoleCtrlHandler(DWORD CtrlType);

static inline ULONG FspPathSuffixIndex(PWSTR FileName)
{
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;
    ULONG Result;

    FspPathSuffix(FileName, &Remain, &Suffix, Root);
    Result = Remain == Root ? 0 : (ULONG)(Suffix - Remain);
    FspPathCombine(FileName, Suffix);

    return Result;
}

static inline BOOLEAN FspPathIsDrive(PWSTR FileName)
{
    return
        (
            (L'A' <= FileName[0] && FileName[0] <= L'Z') ||
            (L'a' <= FileName[0] && FileName[0] <= L'z')
        ) &&
        L':' == FileName[1] && L'\0' == FileName[2];
}
static inline BOOLEAN FspPathIsMountmgrMountPoint(PWSTR FileName)
{
    return
        (
            L'\\' == FileName[0] &&
            L'\\' == FileName[1] &&
            (L'?' == FileName[2] || L'.' == FileName[2]) &&
            L'\\' == FileName[3]
        ) &&
        (
            (L'A' <= FileName[4] && FileName[4] <= L'Z') ||
            (L'a' <= FileName[4] && FileName[4] <= L'z')
        ) &&
        L':' == FileName[5];
}

#define FSP_NEXT_EA(Ea, EaEnd)          \
    (0 != (Ea)->NextEntryOffset ? (PVOID)((PUINT8)(Ea) + (Ea)->NextEntryOffset) : (EaEnd))

#endif
