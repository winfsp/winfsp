/**
 * @file shared/ku/library.h
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

#ifndef WINFSP_SHARED_KU_LIBRARY_H_INCLUDED
#define WINFSP_SHARED_KU_LIBRARY_H_INCLUDED

#if !defined(_KERNEL_MODE)

#include <dll/library.h>
#include <aclapi.h>
#include <dsgetdc.h>
#include <lm.h>
#define _NTDEF_
#include <ntsecapi.h>

#define FSP_KU_CODE                     ((void)0)

#else

#include <sys/driver.h>

#define FSP_KU_CODE                     PAGED_CODE(); NTSTATUS fsp_ku_status = STATUS_SUCCESS; (VOID)fsp_ku_status

#define FSP_API                         FSP_DDI

#define BYTE                            UINT8
#define BOOL                            BOOLEAN
#define LPBOOL                          PBOOLEAN
#define UINT                            ULONG

#define GetLastError()                  ((DWORD)fsp_ku_status)
#define FspNtStatusFromWin32(Err)       ((NTSTATUS)(Err))
#define ERROR_INSUFFICIENT_BUFFER       STATUS_BUFFER_TOO_SMALL

#define InitOnceExecuteOnce(I, F, P, C) RtlRunOnceExecuteOnce(I, F, P, C)
#define INIT_ONCE                       RTL_RUN_ONCE
#define INIT_ONCE_STATIC_INIT           RTL_RUN_ONCE_INIT

#define AddAccessAllowedAce(Acl, Rev, Acc, Sid)\
                                        (fsp_ku_status = RtlAddAccessAllowedAce(Acl, Rev, Acc, Sid),\
                                            NT_SUCCESS(fsp_ku_status))
#define AddAccessDeniedAce(Acl, Rev, Acc, Sid)\
                                        (fsp_ku_status = FspKuAddAccessDeniedAce(Acl, Rev, Acc, Sid),\
                                            NT_SUCCESS(fsp_ku_status))
#define EqualSid(Sid1, Sid2)            (fsp_ku_status = 0, RtlEqualSid(Sid1, Sid2))
#define GetAce(Acl, Idx, Ace)           (fsp_ku_status = RtlGetAce(Acl, Idx, Ace), NT_SUCCESS(fsp_ku_status))
#define GetAclInformation(Acl, Inf, Len, Cls)\
                                        (fsp_ku_status = FspKuQueryInformationAcl(Acl, Inf, Len, Cls),\
                                            NT_SUCCESS(fsp_ku_status))
#define GetLengthSid(Sid)               (fsp_ku_status = 0, RtlLengthSid(Sid))
#define GetSecurityDescriptorDacl(Sec, Prs, Dac, Def)\
                                        (fsp_ku_status = RtlGetDaclSecurityDescriptor(Sec, Prs, Dac, Def),\
                                            NT_SUCCESS(fsp_ku_status))
#define GetSecurityDescriptorGroup(Sec, Grp, Def)\
                                        (fsp_ku_status = RtlGetGroupSecurityDescriptor(Sec, Grp, Def),\
                                            NT_SUCCESS(fsp_ku_status))
#define GetSecurityDescriptorOwner(Sec, Own, Def)\
                                        (fsp_ku_status = RtlGetOwnerSecurityDescriptor(Sec, Own, Def),\
                                            NT_SUCCESS(fsp_ku_status))
#define GetSidIdentifierAuthority(Sid)  (fsp_ku_status = 0, &((PISID)(Sid))->IdentifierAuthority)
#define GetSidSubAuthority(Sid, Sub)    (fsp_ku_status = 0, RtlSubAuthoritySid(Sid, Sub))
#define GetSidSubAuthorityCount(Sid)    (fsp_ku_status = 0, RtlSubAuthorityCountSid(Sid))
#define InitializeAcl(Acl, Len, Rev)    (fsp_ku_status = RtlCreateAcl(Acl, Len, Rev), NT_SUCCESS(fsp_ku_status))
#define InitializeSecurityDescriptor(Sec, Rev)\
                                        (fsp_ku_status = RtlCreateSecurityDescriptor(Sec, Rev),\
                                            NT_SUCCESS(fsp_ku_status))
#define InitializeSid(Sid, Aut, Cnt)    (fsp_ku_status = RtlInitializeSid(Sid, Aut, Cnt), NT_SUCCESS(fsp_ku_status))
#define IsValidSid(Sid)                 (RtlValidSid(Sid) || (fsp_ku_status = STATUS_INVALID_SID, FALSE))
#define MakeSelfRelativeSD(Abs, Rel, Len)\
                                        (fsp_ku_status = RtlAbsoluteToSelfRelativeSD(Abs, Rel, Len),\
                                            NT_SUCCESS(fsp_ku_status))
#define SetSecurityDescriptorControl(Sec, Msk, Bit)\
                                        (fsp_ku_status = FspKuSetControlSecurityDescriptor(Sec, Msk, Bit),\
                                            NT_SUCCESS(fsp_ku_status))
#define SetSecurityDescriptorDacl(Sec, Prs, Dac, Def)\
                                        (fsp_ku_status = RtlSetDaclSecurityDescriptor(Sec, Prs, Dac, Def),\
                                            NT_SUCCESS(fsp_ku_status))
#define SetSecurityDescriptorGroup(Sec, Grp, Def)\
                                        (fsp_ku_status = RtlSetGroupSecurityDescriptor(Sec, Grp, Def),\
                                            NT_SUCCESS(fsp_ku_status))
#define SetSecurityDescriptorOwner(Sec, Own, Def)\
                                        (fsp_ku_status = RtlSetOwnerSecurityDescriptor(Sec, Own, Def),\
                                            NT_SUCCESS(fsp_ku_status))
static inline NTSTATUS FspKuAddAccessDeniedAce(
    PACL Acl,
    ULONG AceRevision,
    ACCESS_MASK AccessMask,
    PSID Sid)
{
    /* We are missing RtlAddAccessDeniedAce. So we need this malarkey! */
    NTSTATUS Result;
    PACE_HEADER Ace;
    Result = RtlAddAccessAllowedAce(Acl, AceRevision, AccessMask, Sid);
    if (!NT_SUCCESS(Result))
        return Result;
    Result = RtlGetAce(Acl, Acl->AceCount - 1, &Ace);
    if (!NT_SUCCESS(Result))
        return Result;
    Ace->AceType = ACCESS_DENIED_ACE_TYPE;
    return STATUS_SUCCESS;
}
typedef enum
{
    AclRevisionInformation__DO_NOT_USE = 1,
    AclSizeInformation,
} ACL_INFORMATION_CLASS;
typedef struct
{
    DWORD AceCount;
    DWORD AclBytesInUse__DO_NOT_USE;
    DWORD AclBytesFree__DO_NOT_USE;
} ACL_SIZE_INFORMATION, *PACL_SIZE_INFORMATION;
static inline NTSTATUS FspKuQueryInformationAcl(
    PACL Acl,
    PVOID AclInformation,
    ULONG AclInformationLength,
    ACL_INFORMATION_CLASS AclInformationClass)
{
    ASSERT(AclSizeInformation == AclInformationClass);
    ASSERT(sizeof(ACL_SIZE_INFORMATION) <= AclInformationLength);
    ((PACL_SIZE_INFORMATION)AclInformation)->AceCount = Acl->AceCount;
    return STATUS_SUCCESS;
}
static inline NTSTATUS FspKuSetControlSecurityDescriptor(
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    SECURITY_DESCRIPTOR_CONTROL ControlMask,
    SECURITY_DESCRIPTOR_CONTROL ControlBits)
{
    ((PUSHORT)(SecurityDescriptor))[1] &= ~ControlMask;
    ((PUSHORT)(SecurityDescriptor))[1] |= ControlBits;
    return STATUS_SUCCESS;
}

#define WideCharToMultiByte(C, F, W, w, B, b, D, d)\
                                        (FspKuWideCharToMultiByte(C, F, W, w, B, b, D, d, &fsp_ku_status))
#define MultiByteToWideChar(C, F, B, b, W, w)\
                                        (FspKuMultiByteToWideChar(C, F, B, b, W, w, &fsp_ku_status))
#define CP_UTF8                         65001
static inline int FspKuWideCharToMultiByte(
    UINT CodePage,
    DWORD dwFlags,
    LPCWCH lpWideCharStr,
    int cchWideChar,
    LPSTR lpMultiByteStr,
    int cbMultiByte,
    LPCCH lpDefaultChar,
    LPBOOL lpUsedDefaultChar,
    PNTSTATUS PResult)
{
    ASSERT(CP_UTF8 == CodePage);
    ASSERT(0 == dwFlags);
    ASSERT(0 == lpDefaultChar);
    ASSERT(0 == lpUsedDefaultChar);
    NTSTATUS Result;
    ULONG ByteCount;
    if (-1 == cchWideChar)
        cchWideChar = (int)wcslen(lpWideCharStr) + 1;
    Result = RtlUnicodeToUTF8N(
        lpMultiByteStr, cbMultiByte, &ByteCount,
        lpWideCharStr, cchWideChar * sizeof(WCHAR));
    if (STATUS_SOME_NOT_MAPPED == Result)
        Result = STATUS_SUCCESS;
    else if (!NT_SUCCESS(Result))
        return 0;
    *PResult = Result;
    return ByteCount;
}
static inline int FspKuMultiByteToWideChar(
    UINT CodePage,
    DWORD dwFlags,
    LPCCH lpMultiByteStr,
    int cbMultiByte,
    LPWSTR lpWideCharStr,
    int cchWideChar,
    PNTSTATUS PResult)
{
    ASSERT(CP_UTF8 == CodePage);
    ASSERT(0 == dwFlags);
    NTSTATUS Result;
    ULONG ByteCount;
    if (-1 == cbMultiByte)
        cbMultiByte = (int)strlen(lpMultiByteStr) + 1;
    Result = RtlUTF8ToUnicodeN(
        lpWideCharStr, cchWideChar * sizeof(WCHAR), &ByteCount,
        lpMultiByteStr, cbMultiByte);
    if (STATUS_SOME_NOT_MAPPED == Result)
        Result = STATUS_SUCCESS;
    else if (!NT_SUCCESS(Result))
        return 0;
    *PResult = Result;
    return ByteCount / sizeof(WCHAR);
}

static inline PGENERIC_MAPPING FspGetFileGenericMapping(VOID)
{
    return IoGetFileObjectGenericMapping();
}

static inline void *MemAlloc(size_t Size)
{
    return FspAlloc(Size);
}
static inline void MemFree(void *Pointer)
{
    if (0 != Pointer)
        FspFree(Pointer);
}

#endif

#endif
