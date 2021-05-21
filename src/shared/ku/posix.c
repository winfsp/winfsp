/**
 * @file shared/ku/posix.c
 * POSIX Interop.
 *
 * This file provides routines for Windows/POSIX interoperability. It is based
 * on "Services for UNIX" and Cygwin. See the following documents:
 *
 * [PERMS]
 *     https://technet.microsoft.com/en-us/library/bb463216.aspx
 * [WKSID]
 *     https://support.microsoft.com/en-us/kb/243330
 * [IDMAP]
 *     https://cygwin.com/cygwin-ug-net/ntsec.html
 * [SNAME]
 *     https://www.cygwin.com/cygwin-ug-net/using-specialnames.html
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

#include <shared/ku/library.h>

FSP_API NTSTATUS FspPosixMapUidToSid(UINT32 Uid, PSID *PSid);
FSP_API NTSTATUS FspPosixMapSidToUid(PSID Sid, PUINT32 PUid);
static PISID FspPosixCreateSid(BYTE Authority, ULONG Count, ...);
FSP_API VOID FspDeleteSid(PSID Sid, NTSTATUS (*CreateFunc)());
FSP_API NTSTATUS FspPosixMapPermissionsToSecurityDescriptor(
    UINT32 Uid, UINT32 Gid, UINT32 Mode,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor);
FSP_API NTSTATUS FspPosixMergePermissionsToSecurityDescriptor(
    UINT32 Uid, UINT32 Gid, UINT32 Mode,
    PSECURITY_DESCRIPTOR ExistingSecurityDescriptor,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor);
FSP_API NTSTATUS FspPosixMapSecurityDescriptorToPermissions(
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    PUINT32 PUid, PUINT32 PGid, PUINT32 PMode);
FSP_API NTSTATUS FspPosixMapWindowsToPosixPathEx(PWSTR WindowsPath, char **PPosixPath,
    BOOLEAN Translate);
FSP_API NTSTATUS FspPosixMapPosixToWindowsPathEx(const char *PosixPath, PWSTR *PWindowsPath,
    BOOLEAN Translate);
FSP_API VOID FspPosixDeletePath(void *Path);
FSP_API VOID FspPosixEncodeWindowsPath(PWSTR WindowsPath, ULONG Size);
FSP_API VOID FspPosixDecodeWindowsPath(PWSTR WindowsPath, ULONG Size);

#if defined(_KERNEL_MODE)
#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspPosixMapUidToSid)
#pragma alloc_text(PAGE, FspPosixMapSidToUid)
#pragma alloc_text(PAGE, FspPosixCreateSid)
#pragma alloc_text(PAGE, FspDeleteSid)
#pragma alloc_text(PAGE, FspPosixMapPermissionsToSecurityDescriptor)
#pragma alloc_text(PAGE, FspPosixMergePermissionsToSecurityDescriptor)
#pragma alloc_text(PAGE, FspPosixMapSecurityDescriptorToPermissions)
#pragma alloc_text(PAGE, FspPosixMapWindowsToPosixPathEx)
#pragma alloc_text(PAGE, FspPosixMapPosixToWindowsPathEx)
#pragma alloc_text(PAGE, FspPosixDeletePath)
#pragma alloc_text(PAGE, FspPosixEncodeWindowsPath)
#pragma alloc_text(PAGE, FspPosixDecodeWindowsPath)
#endif
#endif

static union
{
    SID V;
    UINT8 B[sizeof(SID) - sizeof(DWORD) + (1 * sizeof(DWORD))];
} FspWorldSidBuf =
{
    /* S-1-1-0 */
    .V.Revision = SID_REVISION,
    .V.SubAuthorityCount = 1,
    .V.IdentifierAuthority.Value[5] = 1,
    .V.SubAuthority[0] = 0,
};
static union
{
    SID V;
    UINT8 B[sizeof(SID) - sizeof(DWORD) + (1 * sizeof(DWORD))];
} FspAuthUsersSidBuf =
{
    /* S-1-5-11 */
    .V.Revision = SID_REVISION,
    .V.SubAuthorityCount = 1,
    .V.IdentifierAuthority.Value[5] = 5,
    .V.SubAuthority[0] = 11,
};
static union
{
    SID V;
    UINT8 B[sizeof(SID) - sizeof(DWORD) + (1 * sizeof(DWORD))];
} FspUnmappedSidBuf =
{
    /* S-1-0-65534 */
    .V.Revision = SID_REVISION,
    .V.SubAuthorityCount = 1,
    .V.IdentifierAuthority.Value[5] = 0,
    .V.SubAuthority[0] = 65534,
};

#define FspWorldSid                     (&FspWorldSidBuf.V)
#define FspAuthUsersSid                 (&FspAuthUsersSidBuf.V)
#define FspUnmappedSid                  (&FspUnmappedSidBuf.V)
#define FspUnmappedUid                  (65534)

static PISID FspAccountDomainSid, FspPrimaryDomainSid;
static struct
{
    PSID DomainSid;
    PWSTR NetbiosDomainName;
    PWSTR DnsDomainName;
    ULONG TrustPosixOffset;
} *FspTrustedDomains;
static ULONG FspTrustedDomainCount;
static BOOLEAN FspDistinctPermsForSameOwnerGroup;
static INIT_ONCE FspPosixInitOnce = INIT_ONCE_STATIC_INIT;

#if !defined(_KERNEL_MODE)

static ULONG FspPosixInitializeTrustPosixOffsets(VOID)
{
    PVOID Ldap = 0;
    PWSTR DefaultNamingContext = 0;
    PWSTR TrustPosixOffsetString = 0;
    ULONG LdapResult;

    LdapResult = FspLdapConnect(0/* default LDAP server */, &Ldap);
    if (0 != LdapResult)
        goto exit;

    LdapResult = FspLdapGetDefaultNamingContext(Ldap, &DefaultNamingContext);
    if (0 != LdapResult)
        goto exit;

    /* get the "trustPosixOffset" for each trusted domain */
    for (ULONG I = 0; FspTrustedDomainCount > I; I++)
    {
        MemFree(TrustPosixOffsetString);
        LdapResult = FspLdapGetTrustPosixOffset(Ldap,
            DefaultNamingContext, FspTrustedDomains[I].DnsDomainName, &TrustPosixOffsetString);
        if (0 == LdapResult)
            FspTrustedDomains[I].TrustPosixOffset = wcstouint(TrustPosixOffsetString, 0, 10, 1);
    }

    LdapResult = 0;

exit:
    MemFree(TrustPosixOffsetString);
    MemFree(DefaultNamingContext);
    if (0 != Ldap)
        FspLdapClose(Ldap);

    /* if the "trustPosixOffset" looks wrong, fix it up using Cygwin magic value 0xfe500000 */
    for (ULONG I = 0; FspTrustedDomainCount > I; I++)
    {
        if (0x100000 > FspTrustedDomains[I].TrustPosixOffset)
            FspTrustedDomains[I].TrustPosixOffset = 0xfe500000;
    }

    return LdapResult;
}

static VOID FspPosixInitializeFromRegistry(VOID)
{
    HKEY RegKey;
    LONG Result;
    DWORD Size;
    DWORD DistinctPermsForSameOwnerGroup;

    DistinctPermsForSameOwnerGroup = 0;

    Result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\" FSP_FSCTL_PRODUCT_NAME,
        0, KEY_READ | KEY_WOW64_32KEY, &RegKey);
    if (ERROR_SUCCESS == Result)
    {
        Size = sizeof DistinctPermsForSameOwnerGroup;
        Result = RegGetValueW(RegKey, 0, L"DistinctPermsForSameOwnerGroup",
            RRF_RT_REG_DWORD, 0, &DistinctPermsForSameOwnerGroup, &Size);
        RegCloseKey(RegKey);
    }

    FspDistinctPermsForSameOwnerGroup = !!DistinctPermsForSameOwnerGroup;
}

static BOOL WINAPI FspPosixInitialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    static LSA_OBJECT_ATTRIBUTES Obja;
    LSA_HANDLE PolicyHandle = 0;
    PPOLICY_ACCOUNT_DOMAIN_INFO AccountDomainInfo = 0;
    PPOLICY_DNS_DOMAIN_INFO PrimaryDomainInfo = 0;
    PDS_DOMAIN_TRUSTSW TrustedDomains = 0;
    ULONG TrustedDomainCount, RealTrustedDomainCount;
    BYTE Count;
    ULONG Size, Temp;
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

        if (ERROR_SUCCESS == DsEnumerateDomainTrustsW(
            0, DS_DOMAIN_DIRECT_INBOUND | DS_DOMAIN_DIRECT_OUTBOUND | DS_DOMAIN_IN_FOREST,
            &TrustedDomains, &TrustedDomainCount))
        {
            Size = 0;
            RealTrustedDomainCount = 0;
            for (ULONG I = 0; TrustedDomainCount > I; I++)
            {
                if (0 == TrustedDomains[I].DomainSid ||
                    (0 == TrustedDomains[I].NetbiosDomainName &&
                        0 == TrustedDomains[I].DnsDomainName) ||
                    EqualSid(TrustedDomains[I].DomainSid, FspPrimaryDomainSid))
                    continue;
                if (0 != TrustedDomains[I].DomainSid)
                {
                    Size = FSP_FSCTL_DEFAULT_ALIGN_UP(Size);
                    Size += GetLengthSid(TrustedDomains[I].DomainSid);
                }
                if (0 != TrustedDomains[I].NetbiosDomainName)
                {
                    Size = FSP_FSCTL_ALIGN_UP(Size, sizeof(WCHAR));
                    Size += (lstrlenW(TrustedDomains[I].NetbiosDomainName) + 1) * sizeof(WCHAR);
                }
                if (0 != TrustedDomains[I].DnsDomainName)
                {
                    Size = FSP_FSCTL_ALIGN_UP(Size, sizeof(WCHAR));
                    Size += (lstrlenW(TrustedDomains[I].DnsDomainName) + 1) * sizeof(WCHAR);
                }
                RealTrustedDomainCount++;
            }
            Size = FSP_FSCTL_DEFAULT_ALIGN_UP(sizeof FspTrustedDomains[0] * RealTrustedDomainCount) + Size;
            if (0 < RealTrustedDomainCount)
            {
                FspTrustedDomains = MemAlloc(Size);
                if (0 != FspTrustedDomains)
                {
                    Size = FSP_FSCTL_DEFAULT_ALIGN_UP(sizeof FspTrustedDomains[0] * RealTrustedDomainCount);
                    for (ULONG I = 0, J = 0; TrustedDomainCount > I; I++)
                    {
                        if (0 == TrustedDomains[I].DomainSid ||
                            (0 == TrustedDomains[I].NetbiosDomainName &&
                                0 == TrustedDomains[I].DnsDomainName) ||
                            EqualSid(TrustedDomains[I].DomainSid, FspPrimaryDomainSid))
                            continue;
                        FspTrustedDomains[J].DomainSid = 0;
                        FspTrustedDomains[J].NetbiosDomainName = 0;
                        FspTrustedDomains[J].DnsDomainName = 0;
                        FspTrustedDomains[J].TrustPosixOffset = 0;
                        if (0 != TrustedDomains[I].DomainSid)
                        {
                            Size = FSP_FSCTL_DEFAULT_ALIGN_UP(Size);
                            FspTrustedDomains[J].DomainSid =
                                (PVOID)((PUINT8)FspTrustedDomains + Size);
                            Size += (Temp = GetLengthSid(TrustedDomains[I].DomainSid));
                            memcpy(FspTrustedDomains[J].DomainSid,
                                TrustedDomains[I].DomainSid, Temp);
                        }
                        if (0 != TrustedDomains[I].NetbiosDomainName)
                        {
                            Size = FSP_FSCTL_ALIGN_UP(Size, sizeof(WCHAR));
                            FspTrustedDomains[J].NetbiosDomainName =
                                (PVOID)((PUINT8)FspTrustedDomains + Size);
                            Size += (Temp = (lstrlenW(TrustedDomains[I].NetbiosDomainName) + 1) * sizeof(WCHAR));
                            memcpy(FspTrustedDomains[J].NetbiosDomainName,
                                TrustedDomains[I].NetbiosDomainName, Temp);
                        }
                        if (0 != TrustedDomains[I].DnsDomainName)
                        {
                            Size = FSP_FSCTL_ALIGN_UP(Size, sizeof(WCHAR));
                            FspTrustedDomains[J].DnsDomainName =
                                (PVOID)((PUINT8)FspTrustedDomains + Size);
                            Size += (Temp = (lstrlenW(TrustedDomains[I].DnsDomainName) + 1) * sizeof(WCHAR));
                            memcpy(FspTrustedDomains[J].DnsDomainName,
                                TrustedDomains[I].DnsDomainName, Temp);
                        }
                        if (0 == FspTrustedDomains[J].NetbiosDomainName)
                            FspTrustedDomains[J].NetbiosDomainName =
                                FspTrustedDomains[J].DnsDomainName;
                        else
                        if (0 == FspTrustedDomains[J].DnsDomainName)
                            FspTrustedDomains[J].DnsDomainName =
                                FspTrustedDomains[J].NetbiosDomainName;
                        J++;
                    }
                    FspTrustedDomainCount = RealTrustedDomainCount;
                }
            }
        }
    }

    if (0 < FspTrustedDomainCount)
        FspPosixInitializeTrustPosixOffsets();

    FspPosixInitializeFromRegistry();

exit:
    if (0 != TrustedDomains)
        NetApiBufferFree(TrustedDomains);

    if (0 != PrimaryDomainInfo)
        LsaFreeMemory(PrimaryDomainInfo);

    if (0 != AccountDomainInfo)
        LsaFreeMemory(AccountDomainInfo);

    if (0 != PolicyHandle)
        LsaClose(PolicyHandle);

    return TRUE;
}

VOID FspPosixFinalize(BOOLEAN Dynamic)
{
    /*
     * This function is called during DLL_PROCESS_DETACH. We must therefore keep
     * finalization tasks to a minimum.
     */

    if (Dynamic)
    {
        MemFree(FspTrustedDomains);
        MemFree(FspAccountDomainSid);
        MemFree(FspPrimaryDomainSid);
    }
}

#else

ULONG NTAPI FspPosixInitialize(
    PRTL_RUN_ONCE RunOnce, PVOID Parameter, PVOID *Context)
{
    static union
    {
        SID V;
        UINT8 B[SECURITY_MAX_SID_SIZE];
    } FspAccountDomainSidBuf;
    static union
    {
        SID V;
        UINT8 B[SECURITY_MAX_SID_SIZE];
    } FspPrimaryDomainSidBuf;
    UNICODE_STRING Path;
    UNICODE_STRING Name;
    union
    {
        KEY_VALUE_PARTIAL_INFORMATION V;
        UINT8 B[FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data) + SECURITY_MAX_SID_SIZE];
    } Value;
    ULONG Length;
    NTSTATUS Result;

    RtlInitUnicodeString(&Path, L"\\Registry\\Machine\\SECURITY\\Policy\\PolAcDmS");
    RtlZeroMemory(&Name, sizeof Name);
    Length = sizeof Value;
    Result = FspRegistryGetValue(&Path, &Name, &Value.V, &Length);
    if (NT_SUCCESS(Result) && REG_NONE == Value.V.Type &&
        sizeof(SID) <= Value.V.DataLength && RtlValidSid((PSID)&Value.V.Data))
    {
        RtlCopyMemory(&FspAccountDomainSidBuf.V, &Value.V.Data, Value.V.DataLength);
        FspAccountDomainSid = &FspAccountDomainSidBuf.V;
    }

    RtlInitUnicodeString(&Path, L"\\Registry\\Machine\\SECURITY\\Policy\\PolPrDmS");
    RtlZeroMemory(&Name, sizeof Name);
    Length = sizeof Value;
    Result = FspRegistryGetValue(&Path, &Name, &Value.V, &Length);
    if (NT_SUCCESS(Result) && REG_NONE == Value.V.Type &&
        sizeof(SID) <= Value.V.DataLength && RtlValidSid((PSID)&Value.V.Data))
    {
        RtlCopyMemory(&FspPrimaryDomainSidBuf.V, &Value.V.Data, Value.V.DataLength);
        FspPrimaryDomainSid = &FspPrimaryDomainSidBuf.V;
    }

    /* always enable permissive permissions for same owner group in kernel mode */
    FspDistinctPermsForSameOwnerGroup = TRUE;

    return TRUE;
}

#endif

static inline BOOLEAN FspPosixIsRelativeSid(PISID Sid1, PISID Sid2)
{
    if (Sid1->Revision != Sid2->Revision)
        return FALSE;
    if (Sid1->IdentifierAuthority.Value[0] != Sid2->IdentifierAuthority.Value[0] ||
        Sid1->IdentifierAuthority.Value[1] != Sid2->IdentifierAuthority.Value[1] ||
        Sid1->IdentifierAuthority.Value[2] != Sid2->IdentifierAuthority.Value[2] ||
        Sid1->IdentifierAuthority.Value[3] != Sid2->IdentifierAuthority.Value[3] ||
        Sid1->IdentifierAuthority.Value[4] != Sid2->IdentifierAuthority.Value[4] ||
        Sid1->IdentifierAuthority.Value[5] != Sid2->IdentifierAuthority.Value[5])
        return FALSE;
    if (Sid1->SubAuthorityCount + 1 != Sid2->SubAuthorityCount)
        return FALSE;
    for (ULONG I = 0; Sid1->SubAuthorityCount > I; I++)
        if (Sid1->SubAuthority[I] != Sid2->SubAuthority[I])
            return FALSE;
    return TRUE;
}

FSP_API NTSTATUS FspPosixMapUidToSid(UINT32 Uid, PSID *PSid)
{
    FSP_KU_CODE;

    InitOnceExecuteOnce(&FspPosixInitOnce, FspPosixInitialize, 0, 0);

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
        if (0 != FspAccountDomainSid &&
            5 == FspAccountDomainSid->IdentifierAuthority.Value[5] &&
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
    else if (0x100000 <= Uid && Uid < 0xff000000)
    {
        if ((Uid < 0x300000 || 0 == FspTrustedDomainCount) &&
            0 != FspPrimaryDomainSid &&
            5 == FspPrimaryDomainSid->IdentifierAuthority.Value[5] &&
            4 == FspPrimaryDomainSid->SubAuthorityCount)
        {
            *PSid = FspPosixCreateSid(5, 5,
                21,
                FspPrimaryDomainSid->SubAuthority[1],
                FspPrimaryDomainSid->SubAuthority[2],
                FspPrimaryDomainSid->SubAuthority[3],
                Uid - 0x100000);
        }
        else
        {
            PISID DomainSid = 0;
            ULONG TrustPosixOffset = 0;
            for (ULONG I = 0; FspTrustedDomainCount > I; I++)
            {
                if (FspTrustedDomains[I].TrustPosixOffset <= Uid &&
                    FspTrustedDomains[I].TrustPosixOffset > TrustPosixOffset)
                {
                    DomainSid = FspTrustedDomains[I].DomainSid;
                    TrustPosixOffset = FspTrustedDomains[I].TrustPosixOffset;
                }
            }
            if (0 != DomainSid)
            {
                *PSid = FspPosixCreateSid(5, 5,
                    21,
                    DomainSid->SubAuthority[1],
                    DomainSid->SubAuthority[2],
                    DomainSid->SubAuthority[3],
                    Uid - TrustPosixOffset);
            }
        }
    }

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
        *PSid = FspPosixCreateSid((BYTE)((Uid - 0x10000) >> 8), 1, (Uid - 0x10000) & 0xff);

    /* [IDMAP]
     * Other well-known SIDs in the NT_AUTHORITY domain (S-1-5-X-RID):
     *     S-1-5-X-RID                         <=> uid/gid: 0x1000 * X + RID
     */
    else if (FspUnmappedUid != Uid && 0x1000 <= Uid && Uid < 0x100000)
        *PSid = FspPosixCreateSid(5, 2, Uid >> 12, Uid & 0xfff);

    if (0 == *PSid)
        *PSid = FspUnmappedSid;

    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspPosixMapSidToUid(PSID Sid, PUINT32 PUid)
{
    FSP_KU_CODE;

    InitOnceExecuteOnce(&FspPosixInitOnce, FspPosixInitialize, 0, 0);

    BYTE Authority;
    BYTE Count;
    UINT32 SubAuthority0, Rid;

    *PUid = (UINT32)-1;

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
            /*
             * The order is important! A server that is also a domain controller
             * has PrimaryDomainSid == AccountDomainSid.
             */

            if (0 != FspPrimaryDomainSid &&
                FspPosixIsRelativeSid(FspPrimaryDomainSid, Sid))
                *PUid = 0x100000 + Rid;
            else if (0 != FspAccountDomainSid &&
                FspPosixIsRelativeSid(FspAccountDomainSid, Sid))
                *PUid = 0x30000 + Rid;
            else
                for (ULONG I = 0; FspTrustedDomainCount > I; I++)
                {
                    if (FspPosixIsRelativeSid(FspTrustedDomains[I].DomainSid, Sid))
                    {
                        *PUid = FspTrustedDomains[I].TrustPosixOffset + Rid;
                        break;
                    }
                }
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
    else if (
        FspUnmappedSid->IdentifierAuthority.Value[5] != Authority ||
        FspUnmappedSid->SubAuthority[0] != Rid)
    {
        /* [IDMAP]
         * Other well-known SIDs:
         *     S-1-X-Y                             <=> uid/gid: 0x10000 + 0x100 * X + Y
         */
        *PUid = 0x10000 + 0x100 * Authority + Rid;
    }

    if (-1 == *PUid)
        *PUid = FspUnmappedUid;

    return STATUS_SUCCESS;
}

static PISID FspPosixCreateSid(BYTE Authority, ULONG Count, ...)
{
    FSP_KU_CODE;

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
    FSP_KU_CODE;

    if (FspUnmappedSid == Sid)
        ;
    else if ((NTSTATUS (*)())FspPosixMapUidToSid == CreateFunc)
        MemFree(Sid);
}

/* [PERMS]
 * By default, all access-allowed ACEs will contain the following Windows access rights.
 */
#define FspPosixDefaultPerm             \
    (SYNCHRONIZE | READ_CONTROL | FILE_READ_ATTRIBUTES | FILE_READ_EA)
/* [PERMS]
 * There are some additional Windows access rights that are always set in the
 * access-allowed ACE for the file's owner.
 */
#define FspPosixOwnerDefaultPerm        \
    (FspPosixDefaultPerm | DELETE | WRITE_DAC | WRITE_OWNER | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA)

static inline ACCESS_MASK FspPosixMapPermissionToAccessMask(UINT32 Mode, UINT32 Perm)
{
    /*
     * We use only the 0040000 (directory) and 0001000 (sticky) bits from Mode.
     * If this is a directory and it does not have the sticky bit set (and the
     * write permission is enabled) we add FILE_DELETE_CHILD access.
     *
     * When calling this function for computing the Owner access mask, we always
     * pass Mode & ~0001000 to remove the sticky bit and thus add FILE_DELETE_CHILD
     * access if it is a directory. For Group and World permissions we do not
     * remove the sticky bit as we do not want FILE_DELETE_CHILD access in these
     * cases.
     */

    ACCESS_MASK DeleteChild = 0040000 == (Mode & 0041000) ? FILE_DELETE_CHILD : 0;

    /* [PERMS]
     * Additionally, if the UNIX read permission bit is set, then the Windows
     * File_Read access right is added to the ACE. When enabled on directories,
     * this allows them to be searched. When enabled on files, it allows the data
     * to be viewed. If the UNIX execute permission bit is set, then the Windows
     * File_Execute access right is added to the ACE. On directories this enables
     * the directory to be traversed. On files it allows the file to be executed.
     *
     * If the UNIX write permission bit is set then the following Windows access
     * rights are added: Write_Data, Write_Attributes, Append_Data, Delete_Child.
     *
     * Notice how Windows has four separate access rights to UNIX's single "write"
     * permission. In UNIX, the write permission bit on a directory permits both
     * the creation and removal of new files or sub-directories in the directory.
     * On Windows, the Write_Data access right controls the creation of new
     * sub-files and the Delete_Child access right controls the deletion. The
     * Delete_Child access right is not always present in all ACEs. In the case
     * where the UNIX sticky-bit is enabled, the Delete_Child bit will be set only
     * in the file owner ACE and no other ACEs. This will permit only the directory
     * owner to remove any files or sub-directories from this directory regardless
     * of the ownership on these sub-files. Other users will be allowed to delete
     * files or sub-directories only if they are granted the Delete access right
     * in an ACE of the file or sub-directory itself.
     */

    return
        ((Perm & 4) ? FILE_READ_DATA : 0) |
        ((Perm & 2) ? FILE_WRITE_ATTRIBUTES | FILE_WRITE_DATA | FILE_APPEND_DATA | DeleteChild : 0) |
        ((Perm & 1) ? FILE_EXECUTE : 0);
}

FSP_API NTSTATUS FspPosixMapPermissionsToSecurityDescriptor(
    UINT32 Uid, UINT32 Gid, UINT32 Mode,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
{
    FSP_KU_CODE;

    PSID OwnerSid = 0, GroupSid = 0;
    UINT32 OwnerPerm, OwnerDeny, GroupPerm, GroupDeny, WorldPerm;
    PACL Acl = 0;
    SECURITY_DESCRIPTOR SecurityDescriptor;
    PSECURITY_DESCRIPTOR RelativeSecurityDescriptor = 0;
    ULONG Size;
    NTSTATUS Result;

    *PSecurityDescriptor = 0;

    Result = FspPosixMapUidToSid(Uid, &OwnerSid);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = FspPosixMapUidToSid(Gid, &GroupSid);
    if (!NT_SUCCESS(Result))
        goto exit;

    OwnerPerm = (Mode & 0700) >> 6;
    GroupPerm = (Mode & 0070) >> 3;
    WorldPerm = (Mode & 0007);

    /* [PERMS]
     * What about the case where both owner and group are the same SID and
     * a chmod(1) request is made where the owner and the group permission
     * bits are different?. In this case, the most restrictive permissions
     * are chosen and assigned to both ACEs.
     */
    if (!FspDistinctPermsForSameOwnerGroup && EqualSid(OwnerSid, GroupSid))
        OwnerPerm = GroupPerm = OwnerPerm & GroupPerm;

    /* [PERMS]
     * There are situations where one or two access-denied ACEs must be added
     * to the DACL. If you recall, the UNIX file access algorithm makes a
     * distinction between owner, group and other such that each is unique
     * and the ID used in an access request can only match one of them.
     * However, the Windows file access algorithm makes no such distinction
     * while scanning the DACL. If the ID in the request is granted permission
     * in any of the access-allowed ACEs then the request is permitted. This
     * is a problem when the owner permissions are specified to be more
     * restrictive than say the group or the other permissions (eg. when a
     * "chmod 577 foobar" is executed) So, to support UNIX semantics we must
     * examine the permissions granted to Everyone and if they are more
     * permissive than those in the group permissions then a special
     * access-denied ACE must be created for the group. And similarly, if
     * either the group or other permissions are more permissive than the
     * owner permissions, then an access-denied ACE must be created for the owner.
     */
    OwnerDeny = (OwnerPerm ^ GroupPerm) & GroupPerm;
    OwnerDeny |= (OwnerPerm ^ WorldPerm) & WorldPerm;
    GroupDeny = (GroupPerm ^ WorldPerm) & WorldPerm;

    Size = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) * 3 +
        sizeof(ACCESS_DENIED_ACE) * (!!OwnerDeny + !!GroupDeny);
    Size += GetLengthSid(OwnerSid) - sizeof(DWORD);
    Size += GetLengthSid(GroupSid) - sizeof(DWORD);
    Size += GetLengthSid(FspWorldSid) - sizeof(DWORD);
    if (OwnerDeny)
        Size += GetLengthSid(OwnerSid) - sizeof(DWORD);
    if (GroupDeny)
        Size += GetLengthSid(GroupSid) - sizeof(DWORD);
    Size += sizeof(DWORD) - 1;
    Size &= ~(sizeof(DWORD) - 1);

    Acl = MemAlloc(Size);
    if (0 == Acl)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (!InitializeAcl(Acl, Size, ACL_REVISION))
        goto lasterror;

    if (!AddAccessAllowedAce(Acl, ACL_REVISION,
        FspPosixOwnerDefaultPerm | FspPosixMapPermissionToAccessMask(Mode & ~001000, OwnerPerm),
        OwnerSid))
        goto lasterror;
    if (OwnerDeny)
    {
        if (!AddAccessDeniedAce(Acl, ACL_REVISION,
            ~FILE_WRITE_ATTRIBUTES & FspPosixMapPermissionToAccessMask(Mode & ~001000, OwnerDeny),
            OwnerSid))
            goto lasterror;
    }

    if (!AddAccessAllowedAce(Acl, ACL_REVISION,
        FspPosixDefaultPerm | FspPosixMapPermissionToAccessMask(Mode, GroupPerm),
        GroupSid))
        goto lasterror;
    if (GroupDeny)
    {
        if (!AddAccessDeniedAce(Acl, ACL_REVISION,
            FspPosixMapPermissionToAccessMask(Mode, GroupDeny),
            GroupSid))
            goto lasterror;
    }

    if (!AddAccessAllowedAce(Acl, ACL_REVISION,
        FspPosixDefaultPerm | FspPosixMapPermissionToAccessMask(Mode, WorldPerm),
        FspWorldSid))
        goto lasterror;

    if (!InitializeSecurityDescriptor(&SecurityDescriptor, SECURITY_DESCRIPTOR_REVISION))
        goto lasterror;

    if (!SetSecurityDescriptorControl(&SecurityDescriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED))
        goto lasterror;
    if (!SetSecurityDescriptorOwner(&SecurityDescriptor, OwnerSid, FALSE))
        goto lasterror;
    if (!SetSecurityDescriptorGroup(&SecurityDescriptor, GroupSid, FALSE))
        goto lasterror;
    if (!SetSecurityDescriptorDacl(&SecurityDescriptor, TRUE, Acl, FALSE))
        goto lasterror;

    Size = 0;
    if (!MakeSelfRelativeSD(&SecurityDescriptor, 0, &Size) &&
        ERROR_INSUFFICIENT_BUFFER != GetLastError())
        goto lasterror;

    RelativeSecurityDescriptor = MemAlloc(Size);
    if (0 == RelativeSecurityDescriptor)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (!MakeSelfRelativeSD(&SecurityDescriptor, RelativeSecurityDescriptor, &Size))
        goto lasterror;

    *PSecurityDescriptor = RelativeSecurityDescriptor;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
        MemFree(RelativeSecurityDescriptor);

    MemFree(Acl);

    if (0 != GroupSid)
        FspDeleteSid(GroupSid, FspPosixMapUidToSid);

    if (0 != OwnerSid)
        FspDeleteSid(OwnerSid, FspPosixMapUidToSid);

    return Result;

lasterror:
    Result = FspNtStatusFromWin32(GetLastError());
    goto exit;
}

FSP_API NTSTATUS FspPosixMergePermissionsToSecurityDescriptor(
    UINT32 Uid, UINT32 Gid, UINT32 Mode,
    PSECURITY_DESCRIPTOR ExistingSecurityDescriptor,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
{
    FSP_KU_CODE;

    if (0 == ExistingSecurityDescriptor)
        return FspPosixMapPermissionsToSecurityDescriptor(Uid, Gid, Mode, PSecurityDescriptor);

    PSID ExistingOwnerSid = 0, ExistingGroupSid = 0;
    BOOL Defaulted, ExistingDaclPresent;
    PACL ExistingAcl = 0;
    PSID OwnerSid = 0, GroupSid = 0;
    SECURITY_DESCRIPTOR SecurityDescriptor;
    PSECURITY_DESCRIPTOR RelativeSecurityDescriptor = 0;
    ULONG Size;
    NTSTATUS Result;

    *PSecurityDescriptor = 0;

    if (!GetSecurityDescriptorOwner(ExistingSecurityDescriptor, &ExistingOwnerSid, &Defaulted))
        goto lasterror;
    if (!GetSecurityDescriptorGroup(ExistingSecurityDescriptor, &ExistingGroupSid, &Defaulted))
        goto lasterror;
    if (!GetSecurityDescriptorDacl(ExistingSecurityDescriptor, &ExistingDaclPresent, &ExistingAcl, &Defaulted))
        goto lasterror;

    if (0 == ExistingOwnerSid)
    {
        Result = FspPosixMapUidToSid(Uid, &OwnerSid);
        if (!NT_SUCCESS(Result))
            goto exit;
        ExistingOwnerSid = OwnerSid;
    }

    if (0 == ExistingGroupSid)
    {
        Result = FspPosixMapUidToSid(Gid, &GroupSid);
        if (!NT_SUCCESS(Result))
            goto exit;
        ExistingGroupSid = GroupSid;
    }

    if (!InitializeSecurityDescriptor(&SecurityDescriptor, SECURITY_DESCRIPTOR_REVISION))
        goto lasterror;

    if (!SetSecurityDescriptorOwner(&SecurityDescriptor, ExistingOwnerSid, FALSE))
        goto lasterror;
    if (!SetSecurityDescriptorGroup(&SecurityDescriptor, ExistingGroupSid, FALSE))
        goto lasterror;
    if (0 != ExistingAcl)
    {
        if (!SetSecurityDescriptorControl(&SecurityDescriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED))
            goto lasterror;
        if (!SetSecurityDescriptorDacl(&SecurityDescriptor, TRUE, ExistingAcl, FALSE))
            goto lasterror;
    }

    Size = 0;
    if (!MakeSelfRelativeSD(&SecurityDescriptor, 0, &Size) &&
        ERROR_INSUFFICIENT_BUFFER != GetLastError())
        goto lasterror;

    RelativeSecurityDescriptor = MemAlloc(Size);
    if (0 == RelativeSecurityDescriptor)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (!MakeSelfRelativeSD(&SecurityDescriptor, RelativeSecurityDescriptor, &Size))
        goto lasterror;

    *PSecurityDescriptor = RelativeSecurityDescriptor;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
        MemFree(RelativeSecurityDescriptor);

    if (0 != GroupSid)
        FspDeleteSid(GroupSid, FspPosixMapUidToSid);

    if (0 != OwnerSid)
        FspDeleteSid(OwnerSid, FspPosixMapUidToSid);

    return Result;

lasterror:
    Result = FspNtStatusFromWin32(GetLastError());
    goto exit;
}

static inline ACCESS_MASK FspPosixCanonicalizeAccessMask(ACCESS_MASK AccessMask)
{
    PGENERIC_MAPPING Mapping = FspGetFileGenericMapping();
    if (AccessMask & GENERIC_READ)
        AccessMask |= Mapping->GenericRead;
    if (AccessMask & GENERIC_WRITE)
        AccessMask |= Mapping->GenericWrite;
    if (AccessMask & GENERIC_EXECUTE)
        AccessMask |= Mapping->GenericExecute;
    if (AccessMask & GENERIC_ALL)
        AccessMask |= Mapping->GenericAll;
    return AccessMask & ~(GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL);
}

static inline UINT32 FspPosixMapAccessMaskToPermission(ACCESS_MASK AccessMask)
{
    /* [PERMS]
     * Once all the granted Windows access right bits have been collected,
     * then the UNIX permission bits are assembled. For each class, if the
     * Read_Data bit is granted, then the corresponding "r" permission bit
     * is set. If both the Write_Data and Append_Data access rights are
     * granted then the "w" permission bit is set. And finally, if the
     * Execute access right is granted, then the "x" permission bit is set.
     */

    return
        ((AccessMask & FILE_READ_DATA) ? 4 : 0) |
        ((FILE_WRITE_DATA | FILE_APPEND_DATA) ==
            (AccessMask & (FILE_WRITE_DATA | FILE_APPEND_DATA)) ? 2 : 0) |
        ((AccessMask & FILE_EXECUTE) ? 1 : 0);
}

FSP_API NTSTATUS FspPosixMapSecurityDescriptorToPermissions(
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    PUINT32 PUid, PUINT32 PGid, PUINT32 PMode)
{
    FSP_KU_CODE;

    BOOLEAN OwnerOptional = (UINT_PTR)PUid & 1;
    PUid = (PVOID)((UINT_PTR)PUid & ~1);
    UINT32 OrigUid = *PUid;

    BOOLEAN GroupOptional = (UINT_PTR)PGid & 1;
    PGid = (PVOID)((UINT_PTR)PGid & ~1);
    UINT32 OrigGid = *PGid;

    PSID OwnerSid = 0, GroupSid = 0;
    BOOL Defaulted, DaclPresent;
    PACL Acl = 0;
    ACL_SIZE_INFORMATION AclSizeInfo;
    PACE_HEADER Ace;
    PSID AceSid;
    DWORD AceAccessMask;
    DWORD OwnerAllow, OwnerDeny, GroupAllow, GroupDeny, WorldAllow, WorldDeny;
    UINT32 AceUid = 0;
    UINT32 Uid, Gid, Mode;
    NTSTATUS Result;

    *PUid = 0;
    *PGid = 0;
    *PMode = 0;

    if (!GetSecurityDescriptorOwner(SecurityDescriptor, &OwnerSid, &Defaulted))
        goto lasterror;
    if (!GetSecurityDescriptorGroup(SecurityDescriptor, &GroupSid, &Defaulted))
        goto lasterror;
    if (!GetSecurityDescriptorDacl(SecurityDescriptor, &DaclPresent, &Acl, &Defaulted))
        goto lasterror;

    if (0 == OwnerSid && OwnerOptional)
        Uid = OrigUid;
    else
    {
        Result = FspPosixMapSidToUid(OwnerSid, &Uid);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    if (0 == GroupSid && GroupOptional)
        Gid = OrigGid;
    else
    {
        Result = FspPosixMapSidToUid(GroupSid, &Gid);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    if (0 != Acl)
    {
        OwnerAllow = OwnerDeny = GroupAllow = GroupDeny = WorldAllow = WorldDeny = 0;

        if (!GetAclInformation(Acl, &AclSizeInfo, sizeof AclSizeInfo, AclSizeInformation))
            goto lasterror;

        /* [PERMS]
         * For each of the ACEs in the file's DACL
         */
        for (ULONG Index = 0; AclSizeInfo.AceCount > Index; Index++)
        {
            if (!GetAce(Acl, Index, &Ace))
                goto lasterror;

            /* [PERMS]
             * Ignore the ACE if it is not an access-denied or access-allowed type.
             */
            if (ACCESS_ALLOWED_ACE_TYPE == Ace->AceType)
            {
                AceSid = &((PACCESS_ALLOWED_ACE)Ace)->SidStart;
                AceAccessMask = ((PACCESS_ALLOWED_ACE)Ace)->Mask;
            }
            else if (ACCESS_DENIED_ACE_TYPE == Ace->AceType)
            {
                AceSid = &((PACCESS_DENIED_ACE)Ace)->SidStart;
                AceAccessMask = ((PACCESS_DENIED_ACE)Ace)->Mask;
            }
            else
                continue;

            AceAccessMask = FspPosixCanonicalizeAccessMask(AceAccessMask);

            /* [PERMS]
             * If the ACE contains the Authenticated Users SID or the World SID then
             * add the allowed or denied access right bits into the "owner", "group"
             * and "other" collections.
             */
            if (EqualSid(FspWorldSid, AceSid) || EqualSid(FspAuthUsersSid, AceSid))
            {
                /* [PERMS]
                 * If this is an access-denied ACE, then add each access right to the set
                 * of denied rights in each collection but only if the access right is not
                 * already present in the set of granted rights in that collection. Similarly
                 * If this is an access-allowed ACE, then add each access right to the set
                 * of granted rights in each collection but only if the access right is not
                 * already present in the set of denied rights in that collection.
                 */
                if (ACCESS_ALLOWED_ACE_TYPE == Ace->AceType)
                {
                    WorldAllow |= AceAccessMask & ~WorldDeny;
                    GroupAllow |= AceAccessMask & ~GroupDeny;
                    OwnerAllow |= AceAccessMask & ~OwnerDeny;
                }
                else //if (ACCESS_DENIED_ACE_TYPE == Ace->AceType)
                {
                    WorldDeny |= AceAccessMask & ~WorldAllow;
                    GroupDeny |= AceAccessMask & ~GroupAllow;
                    OwnerDeny |= AceAccessMask & ~OwnerAllow;
                }
            }
            else
            {
                if (0 == OwnerSid || 0 == GroupSid)
                    FspPosixMapSidToUid(AceSid, &AceUid);

                /* [PERMS]
                 * Note that if the file owner and file group SIDs are the same,
                 * then the access rights are saved in both the "owner" and "group"
                 * collections.
                 */

                /* [PERMS]
                 * If the ACE contains the file's group SID, then save the access rights
                 * in the "group" collection as appropriate in the corresponding set of
                 * granted or denied rights (as described above).
                 */
                if (0 != GroupSid ? EqualSid(GroupSid, AceSid) : (Gid == AceUid))
                {
                    if (ACCESS_ALLOWED_ACE_TYPE == Ace->AceType)
                        GroupAllow |= AceAccessMask & ~GroupDeny;
                    else //if (ACCESS_DENIED_ACE_TYPE == Ace->AceType)
                        GroupDeny |= AceAccessMask & ~GroupAllow;
                }

                /* [PERMS]
                 * If the ACE contains the file's owner SID, then save the access rights
                 * in the "owner" collection as appropriate in the corresponding set of
                 * granted or denied rights (as described above).
                 */
                if (0 != OwnerSid ? EqualSid(OwnerSid, AceSid) : (Uid == AceUid))
                {
                    if (ACCESS_ALLOWED_ACE_TYPE == Ace->AceType)
                        OwnerAllow |= AceAccessMask & ~OwnerDeny;
                    else //if (ACCESS_DENIED_ACE_TYPE == Ace->AceType)
                        OwnerDeny |= AceAccessMask & ~OwnerAllow;
                }
            }
        }

        Mode =
            (FspPosixMapAccessMaskToPermission(OwnerAllow) << 6) |
            (FspPosixMapAccessMaskToPermission(GroupAllow) << 3) |
            (FspPosixMapAccessMaskToPermission(WorldAllow));
        if (0 != (OwnerAllow & FILE_DELETE_CHILD) &&
            (
                (0 == (GroupAllow & FILE_DELETE_CHILD) && 0 != (Mode & 0000020)) ||
                (0 == (WorldAllow & FILE_DELETE_CHILD) && 0 != (Mode & 0000002))
            ))
            Mode |= 0001000; /* sticky bit */
    }
    else
        Mode = 0777;

    *PUid = Uid;
    *PGid = Gid;
    *PMode = Mode;

    Result = STATUS_SUCCESS;

exit:
    return Result;

lasterror:
    Result = FspNtStatusFromWin32(GetLastError());
    goto exit;
}

/*
 * Services for Macintosh and Cygwin compatible filename transformation:
 * Transform characters invalid for Windows filenames to the Unicode
 * private use area in the U+F0XX range.
 *
 * The invalid maps are produced by the following Python script:
 *     reserved = ['<', '>', ':', '"', '\\', '|', '?', '*']
 *     l = [str(int(0 < i < 32 or chr(i) in reserved)) for i in xrange(0, 128)]
 *     print "0x%08x" % int("".join(l[0:32]), 2)
 *     print "0x%08x" % int("".join(l[32:64]), 2)
 *     print "0x%08x" % int("".join(l[64:96]), 2)
 *     print "0x%08x" % int("".join(l[96:128]), 2)
 */
static UINT32 FspPosixInvalidPathChars[4] =
{
    0x7fffffff,
    0x2020002b,
    0x00000008,
    0x00000008,
};

FSP_API NTSTATUS FspPosixMapWindowsToPosixPathEx(PWSTR WindowsPath, char **PPosixPath,
    BOOLEAN Translate)
{
    FSP_KU_CODE;

    NTSTATUS Result;
    ULONG Size;
    char *PosixPath = 0, *p, *q;

    *PPosixPath = 0;

    Size = WideCharToMultiByte(CP_UTF8, 0, WindowsPath, -1, 0, 0, 0, 0);
    if (0 == Size)
        goto lasterror;

    PosixPath = MemAlloc(Size);
    if (0 == PosixPath)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    Size = WideCharToMultiByte(CP_UTF8, 0, WindowsPath, -1, PosixPath, Size, 0, 0);
    if (0 == Size)
        goto lasterror;

    if (Translate)
    {
        for (p = PosixPath, q = p; *p; p++)
        {
            unsigned char c = *p;

            if ('\\' == c)
                *q++ = '/';
            /* encode characters in the Unicode private use area: U+F0XX -> XX */
            else if (0xef == c && 0x80 == (0xfc & p[1]) && 0x80 == (0xc0 & p[2]))
            {
                c = ((p[1] & 0x3) << 6) | (p[2] & 0x3f);
                if (128 > c && (FspPosixInvalidPathChars[c >> 5] & (0x80000000 >> (c & 0x1f))))
                    *q++ = c, p += 2;
                else
                    *q++ = *p++, *q++ = *p++, *q++ = *p;
            }
            else
                *q++ = c;
        }
        *q = '\0';
    }

    *PPosixPath = PosixPath;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
        MemFree(PosixPath);

    return Result;

lasterror:
    Result = FspNtStatusFromWin32(GetLastError());
    goto exit;
}

FSP_API NTSTATUS FspPosixMapPosixToWindowsPathEx(const char *PosixPath, PWSTR *PWindowsPath,
    BOOLEAN Translate)
{
    FSP_KU_CODE;

    NTSTATUS Result;
    ULONG Size;
    PWSTR WindowsPath = 0, p;

    *PWindowsPath = 0;

    Size = MultiByteToWideChar(CP_UTF8, 0, PosixPath, -1, 0, 0);
    if (0 == Size)
        goto lasterror;

    WindowsPath = MemAlloc(Size * sizeof(WCHAR));
    if (0 == PosixPath)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    Size = MultiByteToWideChar(CP_UTF8, 0, PosixPath, -1, WindowsPath, Size);
    if (0 == Size)
        goto lasterror;

    if (Translate)
    {
        for (p = WindowsPath; *p; p++)
        {
            WCHAR c = *p;

            if (L'/' == c)
                *p = L'\\';
            else if (128 > c && (FspPosixInvalidPathChars[c >> 5] & (0x80000000 >> (c & 0x1f))))
                *p |= 0xf000;
        }
    }

    *PWindowsPath = WindowsPath;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
        MemFree(WindowsPath);

    return Result;

lasterror:
    Result = FspNtStatusFromWin32(GetLastError());
    goto exit;
}

FSP_API VOID FspPosixDeletePath(void *Path)
{
    FSP_KU_CODE;

    MemFree(Path);
}

FSP_API VOID FspPosixEncodeWindowsPath(PWSTR WindowsPath, ULONG Size)
{
    FSP_KU_CODE;

    for (PWSTR p = WindowsPath, endp = p + Size; endp > p; p++)
    {
        WCHAR c = *p;

        if (L'\\' == c)
            *p = L'/';
        /* encode characters in the Unicode private use area: U+F0XX -> XX */
        else if (0xf000 <= c && c <= 0xf0ff)
            *p &= ~0xf000;
    }
}

FSP_API VOID FspPosixDecodeWindowsPath(PWSTR WindowsPath, ULONG Size)
{
    FSP_KU_CODE;

    for (PWSTR p = WindowsPath, endp = p + Size; endp > p; p++)
    {
        WCHAR c = *p;

        if (L'/' == c)
            *p = L'\\';
        else if (128 > c && (FspPosixInvalidPathChars[c >> 5] & (0x80000000 >> (c & 0x1f))))
            *p |= 0xf000;
    }
}
