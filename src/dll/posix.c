/**
 * @file dll/posix.c
 * POSIX Interop.
 *
 * This file provides routines for Windows/POSIX interoperability. It is based
 * on "Services for UNIX" and Cygwin. See the following documents:
 *
 * [PERM]
 *     https://technet.microsoft.com/en-us/library/bb463216.aspx
 * [WKSID]
 *     https://support.microsoft.com/en-us/kb/243330
 * [IDMAP]
 *     https://cygwin.com/cygwin-ug-net/ntsec.html
 * [NAME]
 *     https://www.cygwin.com/cygwin-ug-net/using-specialnames.html
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the
 * GNU Affero General Public License version 3 as published by the
 * Free Software Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <dll/library.h>
#define _NTDEF_
#include <ntsecapi.h>

static PISID FspPosixCreateSid(BYTE Authority, ULONG Count, ...);

static INIT_ONCE FspPosixInitOnceV = INIT_ONCE_STATIC_INIT;
static PISID FspAccountDomainSid, FspPrimaryDomainSid;

VOID FspPosixInitialize(BOOLEAN Dynamic)
{
}

VOID FspPosixFinalize(BOOLEAN Dynamic)
{
    /*
     * This function is called during DLL_PROCESS_DETACH. We must therefore keep
     * finalization tasks to a minimum.
     */

    if (Dynamic)
    {
        MemFree(FspAccountDomainSid);
        MemFree(FspPrimaryDomainSid);
    }
}

static BOOL WINAPI FspPosixInitOnceF(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    static LSA_OBJECT_ATTRIBUTES Obja;
    LSA_HANDLE PolicyHandle = 0;
    PPOLICY_ACCOUNT_DOMAIN_INFO AccountDomainInfo = 0;
    PPOLICY_DNS_DOMAIN_INFO PrimaryDomainInfo = 0;
    BYTE Count;
    ULONG Size;
    NTSTATUS Result;

    Result = LsaOpenPolicy(0, &Obja, POLICY_VIEW_LOCAL_INFORMATION, &PolicyHandle);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = LsaQueryInformationPolicy(PolicyHandle, PolicyAccountDomainInformation,
        &AccountDomainInfo);
    if (NT_SUCCESS(Result) && 0 != AccountDomainInfo && 0 != AccountDomainInfo->DomainSid)
    {
        Count = *GetSidSubAuthorityCount(AccountDomainInfo->DomainSid);
        Size = sizeof(SID) - sizeof(DWORD) + (Count * sizeof(DWORD));
        FspAccountDomainSid = MemAlloc(Size);
        if (0 != FspAccountDomainSid)
            memcpy(FspAccountDomainSid, AccountDomainInfo->DomainSid, Size);
    }

    Result = LsaQueryInformationPolicy(PolicyHandle, PolicyDnsDomainInformation,
        &PrimaryDomainInfo);
    if (NT_SUCCESS(Result) && 0 != PrimaryDomainInfo && 0 != PrimaryDomainInfo->Sid)
    {
        Count = *GetSidSubAuthorityCount(PrimaryDomainInfo->Sid);
        Size = sizeof(SID) - sizeof(DWORD) + (Count * sizeof(DWORD));
        FspPrimaryDomainSid = MemAlloc(Size);
        if (0 != FspPrimaryDomainSid)
            memcpy(FspPrimaryDomainSid, PrimaryDomainInfo->Sid, Size);
    }

exit:
    if (0 != PrimaryDomainInfo)
        LsaFreeMemory(PrimaryDomainInfo);

    if (0 != AccountDomainInfo)
        LsaFreeMemory(AccountDomainInfo);

    if (0 != PolicyHandle)
        LsaClose(PolicyHandle);

    return TRUE;
}

FSP_API NTSTATUS FspPosixMapUidToSid(UINT32 Uid, PSID *PSid)
{
    *PSid = 0;

    /*
     * UID namespace partitioning (from [IDMAP] rules):
     *
     * 0x000000 + RID              S-1-5-RID,S-1-5-32-RID
     * 0x000ffe                    OtherSession
     * 0x000fff                    CurrentSession
     * 0x001000 * X + RID          S-1-5-X-RID ([WKSID]: X=1-15,17-21,32,64,80,83)
     * 0x010000 + 0x100 * X + Y    S-1-X-Y ([WKSID]: X=1,2,3,4,5,9,16)
     * 0x030000 + RID              S-1-5-21-X-Y-Z-RID
     * 0x060000 + RID              S-1-16-RID
     * 0x100000 + RID              S-1-5-21-X-Y-Z-RID
     */

    /* [IDMAP]
     * Well-known SIDs in the NT_AUTHORITY domain of the S-1-5-RID type,
     * or aliases of the S-1-5-32-RID type are mapped to the uid/gid value RID.
     * Examples:
     *     "SYSTEM" S-1-5-18                   <=> uid/gid: 18
     *     "Users"  S-1-5-32-545               <=> uid/gid: 545
     */
    if (0x200 > Uid || 1000 == Uid)
        *PSid = FspPosixCreateSid(5, 1, Uid);
    else if (1000 > Uid)
        *PSid = FspPosixCreateSid(5, 2, 32, Uid);

    /* [IDMAP]
     * Logon SIDs: The LogonSid of the current user's session is converted
     * to the fixed uid 0xfff == 4095 and named "CurrentSession". Any other
     * LogonSid is converted to the fixed uid 0xffe == 4094 and named
     * "OtherSession".
     */
    else if (0xfff == Uid || 0xffe == Uid)
    {
        /*
         * Actually we do not support Logon SID's for translation.
         * We need an access token to find its Logon SID and we do not have one.
         */
    }

    /* [IDMAP]
     * Accounts from the local machine's user DB (SAM):
     *     S-1-5-21-X-Y-Z-RID                  <=> uid/gid: 0x30000 + RID
     *
     * Accounts from the machine's primary domain:
     *     S-1-5-21-X-Y-Z-RID                  <=> uid/gid: 0x100000 + RID
     *
     * Accounts from a trusted domain of the machine's primary domain:
     *     S-1-5-21-X-Y-Z-RID                  <=> uid/gid: trustPosixOffset(domain) + RID
     */
    else if (0x30000 <= Uid && Uid < 0x40000)
    {
        InitOnceExecuteOnce(&FspPosixInitOnceV, FspPosixInitOnceF, 0, 0);

        if (5 == FspAccountDomainSid->IdentifierAuthority.Value[5] &&
            4 == FspAccountDomainSid->SubAuthorityCount)
        {
            *PSid = FspPosixCreateSid(5, 5,
                21,
                FspAccountDomainSid->SubAuthority[1],
                FspAccountDomainSid->SubAuthority[2],
                FspAccountDomainSid->SubAuthority[3],
                Uid - 0x30000);
        }
    }
    else if (0x100000 <= Uid && Uid < 0x200000)
    {
        InitOnceExecuteOnce(&FspPosixInitOnceV, FspPosixInitOnceF, 0, 0);

        if (5 == FspPrimaryDomainSid->IdentifierAuthority.Value[5] &&
            4 == FspPrimaryDomainSid->SubAuthorityCount)
        {
            *PSid = FspPosixCreateSid(5, 5,
                21,
                FspPrimaryDomainSid->SubAuthority[1],
                FspPrimaryDomainSid->SubAuthority[2],
                FspPrimaryDomainSid->SubAuthority[3],
                Uid - 0x100000);
        }
    }
    /*
     * I am sorry, I am not going to bother with all that trustPosixOffset stuff.
     * But if you need it, I accept patches :)
     */

    /* [IDMAP]
     * Mandatory Labels:
     *     S-1-16-RID                          <=> uid/gid: 0x60000 + RID
     */
    else if (0x60000 <= Uid && Uid < 0x70000)
        *PSid = FspPosixCreateSid(16, 1, Uid - 0x60000);

    /* [IDMAP]
     * Other well-known SIDs:
     *     S-1-X-Y                             <=> uid/gid: 0x10000 + 0x100 * X + Y
     */
    else if (0x10000 <= Uid && Uid < 0x11000)
        *PSid = FspPosixCreateSid((Uid - 0x10000) >> 8, 1, (Uid - 0x10000) & 0xff);

    /* [IDMAP]
     * Other well-known SIDs in the NT_AUTHORITY domain (S-1-5-X-RID):
     *     S-1-5-X-RID                         <=> uid/gid: 0x1000 * X + RID
     */
    else if (0x1000 <= Uid && Uid < 0x100000)
        *PSid = FspPosixCreateSid(5, 2, Uid >> 12, Uid & 0xfff);

    if (0 == *PSid)
        return STATUS_NONE_MAPPED;

    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspPosixMapSidToUid(PSID Sid, PUINT32 PUid)
{
    BYTE Authority;
    BYTE Count;
    UINT32 SubAuthority0, Rid;

    *PUid = -1;

    if (!IsValidSid(Sid) || 0 == (Count = *GetSidSubAuthorityCount(Sid)))
        return STATUS_INVALID_SID;

    Authority = GetSidIdentifierAuthority(Sid)->Value[5];
    SubAuthority0 = 2 <= Count ? *GetSidSubAuthority(Sid, 0) : 0;
    Rid = *GetSidSubAuthority(Sid, Count - 1);

    if (5 == Authority)
    {
        /* [IDMAP]
         * Well-known SIDs in the NT_AUTHORITY domain of the S-1-5-RID type,
         * or aliases of the S-1-5-32-RID type are mapped to the uid/gid value RID.
         * Examples:
         *     "SYSTEM" S-1-5-18                   <=> uid/gid: 18
         *     "Users"  S-1-5-32-545               <=> uid/gid: 545
         */
        if (1 == Count)
            *PUid = Rid;
        else if (2 == Count && 32 == SubAuthority0)
            *PUid = Rid;

        /* [IDMAP]
         * Logon SIDs: The LogonSid of the current user's session is converted
         * to the fixed uid 0xfff == 4095 and named "CurrentSession". Any other
         * LogonSid is converted to the fixed uid 0xffe == 4094 and named
         * "OtherSession".
         */
        else if (2 <= Count && 5 == SubAuthority0)
        {
            /*
             * Actually we do not support Logon SID's for translation.
             * We need an access token to find its Logon SID and we do not have one.
             */
        }

        /* [IDMAP]
         * Accounts from the local machine's user DB (SAM):
         *     S-1-5-21-X-Y-Z-RID                  <=> uid/gid: 0x30000 + RID
         *
         * Accounts from the machine's primary domain:
         *     S-1-5-21-X-Y-Z-RID                  <=> uid/gid: 0x100000 + RID
         *
         * Accounts from a trusted domain of the machine's primary domain:
         *     S-1-5-21-X-Y-Z-RID                  <=> uid/gid: trustPosixOffset(domain) + RID
         */
        else if (5 <= Count && 21 == SubAuthority0)
        {
            InitOnceExecuteOnce(&FspPosixInitOnceV, FspPosixInitOnceF, 0, 0);

            /*
             * The order is important! A server that is also a domain controller
             * has PrimaryDomainSid == AccountDomainSid.
             */

            BOOL EqualDomains = FALSE;
            if (0 != FspPrimaryDomainSid &&
                EqualDomainSid(FspPrimaryDomainSid, Sid, &EqualDomains) && EqualDomains)
                *PUid = 0x100000 + Rid;
            else if (0 != FspAccountDomainSid &&
                EqualDomainSid(FspAccountDomainSid, Sid, &EqualDomains) && EqualDomains)
                *PUid = 0x30000 + Rid;

            /*
             * I am sorry, I am not going to bother with all that trustPosixOffset stuff.
             * But if you need it, I accept patches :)
             */
        }

        /* [IDMAP]
         * Other well-known SIDs in the NT_AUTHORITY domain (S-1-5-X-RID):
         *     S-1-5-X-RID                         <=> uid/gid: 0x1000 * X + RID
         */
        else if (2 == Count)
        {
            *PUid = 0x1000 * SubAuthority0 + Rid;
        }
    }
    else if (16 == Authority)
    {
        /* [IDMAP]
         * Mandatory Labels:
         *     S-1-16-RID                          <=> uid/gid: 0x60000 + RID
         */
        *PUid = 0x60000 + Rid;
    }
    else
    {
        /* [IDMAP]
         * Other well-known SIDs:
         *     S-1-X-Y                             <=> uid/gid: 0x10000 + 0x100 * X + Y
         */
        *PUid = 0x10000 + 0x100 * Authority + Rid;
    }

    if (-1 == *PUid)
        return STATUS_NONE_MAPPED;

    return STATUS_SUCCESS;
}

static PISID FspPosixCreateSid(BYTE Authority, ULONG Count, ...)
{
    PISID Sid;
    SID_IDENTIFIER_AUTHORITY IdentifierAuthority;
    va_list ap;

    Sid = MemAlloc(sizeof(SID) - sizeof(DWORD) + (Count * sizeof(DWORD)));
    if (0 == Sid)
        return 0;

    memset(&IdentifierAuthority, 0, sizeof IdentifierAuthority);
    IdentifierAuthority.Value[5] = Authority;

    InitializeSid(Sid, &IdentifierAuthority, (BYTE)Count);
    va_start(ap, Count);
    for (ULONG Index = 0; Count > Index; Index++)
        Sid->SubAuthority[Index] = va_arg(ap, DWORD);
    va_end(ap);

    return Sid;
}

FSP_API VOID FspDeleteSid(PSID Sid, NTSTATUS (*CreateFunc)())
{
    if ((NTSTATUS (*)())FspPosixMapUidToSid == CreateFunc)
        MemFree(Sid);
}

FSP_API NTSTATUS FspPosixMapPermissionsToSecurityDescriptor(
    UINT32 Uid, UINT32 Gid, UINT32 Mode,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
{
    return STATUS_NOT_IMPLEMENTED;
}

FSP_API NTSTATUS FspPosixMapSecurityDescriptorToPermissions(
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    PUINT32 PUid, PUINT32 PGid, PUINT32 PMode)
{
    return STATUS_NOT_IMPLEMENTED;
}
