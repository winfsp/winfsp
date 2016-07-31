/**
 * @file dll/np.c
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
#include <launcher/launcher.h>
#include <npapi.h>
#include <wincred.h>

#define FSP_NP_NAME                     LIBRARY_NAME ".Np"
#define FSP_NP_TYPE                     ' spF'  /* pick a value hopefully not in use */

/*
 * Define the following macro to use CredUIPromptForWindowsCredentials.
 * Otherwise CredUIPromptForCredentials will be used.
 */
#define FSP_NP_CREDUI_PROMPT_NEW

/*
 * Define the following macro to include support for the credential manager.
 */
#define FSP_NP_CREDENTIAL_MANAGER

enum
{
    FSP_NP_CREDENTIALS_NONE             = 0,
    FSP_NP_CREDENTIALS_PASSWORD         = 1,
    FSP_NP_CREDENTIALS_USERPASS         = 3,
};

DWORD APIENTRY NPGetCaps(DWORD Index)
{
    switch (Index)
    {
    case WNNC_ADMIN:
        /*
         * WNNC_ADM_DIRECTORYNOTIFY
         * WNNC_ADM_GETDIRECTORYTYPE
         */
        return 0;
    case WNNC_CONNECTION:
        /*
         * WNNC_CON_ADDCONNECTION
         * WNNC_CON_CANCELCONNECTION
         * WNNC_CON_GETCONNECTIONS
         * WNNC_CON_ADDCONNECTION3
         * WNNC_CON_GETPERFORMANCE
         * WNNC_CON_DEFER
         */
        return
            WNNC_CON_GETCONNECTIONS |
            WNNC_CON_ADDCONNECTION | WNNC_CON_ADDCONNECTION3 |
            WNNC_CON_CANCELCONNECTION;
    case WNNC_DIALOG:
        /*
         * WNNC_DLG_DEVICEMODE
         * WNNC_DLG_FORMATNETNAME
         * WNNC_DLG_GETRESOURCEINFORMATION
         * WNNC_DLG_GETRESOURCEPARENT
         * WNNC_DLG_PERMISSIONEDITOR
         * WNNC_DLG_PROPERTYDIALOG
         * WNNC_DLG_SEARCHDIALOG
         */
        return 0;
    case WNNC_ENUMERATION:
        /*
         * WNNC_ENUM_GLOBAL
         * WNNC_ENUM_LOCAL
         * WNNC_ENUM_CONTEXT
         */
        return WNNC_ENUM_LOCAL | WNNC_ENUM_CONTEXT;
    case WNNC_NET_TYPE:
        return FSP_NP_TYPE;
    case WNNC_SPEC_VERSION:
        return WNNC_SPEC_VERSION51;
    case WNNC_START:
        return 1;
    case WNNC_USER:
        /*
         * WNNC_USR_GETUSER
         */
        return 0;
    default:
        return 0;
    }
}

static inline BOOLEAN FspNpCheckLocalName(PWSTR LocalName)
{
    return 0 != LocalName &&
        (
            (L'A' <= LocalName[0] && LocalName[0] <= L'Z') ||
            (L'a' <= LocalName[0] && LocalName[0] <= L'z')
        ) &&
        L':' == LocalName[1] || L'\0' == LocalName[2];
}

static inline BOOLEAN FspNpCheckRemoteName(PWSTR RemoteName)
{
    return 0 != RemoteName && L'\\' == RemoteName[0] && L'\\' == RemoteName[1] &&
        sizeof(((FSP_FSCTL_VOLUME_PARAMS *)0)->Prefix) / sizeof(WCHAR) >= lstrlenW(RemoteName);
}

static inline BOOLEAN FspNpParseRemoteName(PWSTR RemoteName,
    PWSTR *PClassName, PULONG PClassNameLen,
    PWSTR *PInstanceName, PULONG PInstanceNameLen)
{
    PWSTR ClassName, InstanceName, P;
    ULONG ClassNameLen, InstanceNameLen;

    if (!FspNpCheckRemoteName(RemoteName))
        return FALSE;

    ClassName = RemoteName + 2; /* skip \\ */
    for (P = ClassName; *P; P++)
        if (L'\\' == *P)
            break;
    if (ClassName == P || L'\\' != *P)
        return FALSE;
    ClassNameLen = (ULONG)(P - ClassName);

    InstanceName = P + 1;
    for (P = InstanceName; *P; P++)
        ;
    for (;;)
    {
        if (InstanceName == P)
            return FALSE;
        if (L'\\' != P[-1])
            break;
        P--;
    }
    InstanceNameLen = (ULONG)(P - InstanceName);

    *PClassName = ClassName; *PClassNameLen = ClassNameLen;
    *PInstanceName = InstanceName; *PInstanceNameLen = InstanceNameLen;
    
    return TRUE;
}

static inline BOOLEAN FspNpParseUserName(PWSTR RemoteName,
    PWSTR UserName, ULONG UserNameSize/* in chars */)
{
    PWSTR ClassName, InstanceName, P;
    ULONG ClassNameLen, InstanceNameLen;

    if (FspNpParseRemoteName(RemoteName,
        &ClassName, &ClassNameLen, &InstanceName, &InstanceNameLen))
    {
        for (P = InstanceName; *P; P++)
            if ('@' == *P && (ULONG)(P - InstanceName) < UserNameSize)
            {
                memcpy(UserName, InstanceName, (P - InstanceName) * sizeof(WCHAR));
                UserName[P - InstanceName] = L'\0';
                return TRUE;
            }
    }

    return FALSE;
}

static inline DWORD FspNpCallLauncherPipe(PWSTR PipeBuf, ULONG SendSize, ULONG RecvSize)
{
    DWORD NpResult;
    NTSTATUS Result;
    DWORD BytesTransferred;

    Result = FspCallNamedPipeSecurely(L"" LAUNCHER_PIPE_NAME, PipeBuf, SendSize, PipeBuf, RecvSize,
        &BytesTransferred, NMPWAIT_USE_DEFAULT_WAIT, LAUNCHER_PIPE_OWNER);

    if (!NT_SUCCESS(Result))
        NpResult = WN_NO_NETWORK;
    else if (sizeof(WCHAR) > BytesTransferred)
        NpResult = WN_NO_NETWORK;
    else if (LauncherSuccess == PipeBuf[0])
        NpResult = WN_SUCCESS;
    else if (LauncherFailure == PipeBuf[0])
    {
        NpResult = 0;
        for (PWSTR P = PipeBuf + 1, EndP = PipeBuf + BytesTransferred / sizeof(WCHAR); EndP > P; P++)
        {
            if (L'0' > *P || *P > L'9')
                break;

            NpResult = 10 * NpResult + (*P - L'0');
        }

        if (0 == NpResult)
            NpResult = WN_NO_NETWORK;
    }
    else 
        NpResult = WN_NO_NETWORK;

    return NpResult;
}

static NTSTATUS FspNpGetVolumeList(
    PWCHAR *PVolumeListBuf, PSIZE_T PVolumeListSize)
{
    NTSTATUS Result;
    PWCHAR VolumeListBuf;
    SIZE_T VolumeListSize;

    *PVolumeListBuf = 0;
    *PVolumeListSize = 0;

    for (VolumeListSize = 1024;; VolumeListSize *= 2)
    {
        VolumeListBuf = MemAlloc(VolumeListSize);
        if (0 == VolumeListBuf)
            return STATUS_INSUFFICIENT_RESOURCES;

        Result = FspFsctlGetVolumeList(L"" FSP_FSCTL_NET_DEVICE_NAME,
            VolumeListBuf, &VolumeListSize);
        if (NT_SUCCESS(Result))
        {
            *PVolumeListBuf = VolumeListBuf;
            *PVolumeListSize = VolumeListSize;
            return Result;
        }

        MemFree(VolumeListBuf);

        if (STATUS_BUFFER_TOO_SMALL != Result)
            return Result;
    }
}

static WCHAR FspNpGetDriveLetter(PDWORD PLogicalDrives, PWSTR VolumeName)
{
    WCHAR VolumeNameBuf[MAX_PATH];
    WCHAR LocalNameBuf[3];
    WCHAR Drive;

    if (0 == *PLogicalDrives)
        return 0;

    LocalNameBuf[1] = L':';
    LocalNameBuf[2] = L'\0';

    for (Drive = 'Z'; 'A' <= Drive; Drive--)
        if (0 != (*PLogicalDrives & (1 << (Drive - 'A'))))
        {
            LocalNameBuf[0] = Drive;
            if (QueryDosDeviceW(LocalNameBuf, VolumeNameBuf, sizeof VolumeNameBuf / sizeof(WCHAR)))
            {
                if (0 == lstrcmpW(VolumeNameBuf, VolumeName))
                {
                    *PLogicalDrives &= ~(1 << (Drive - 'A'));
                    return Drive;
                }
            }
        }

    return 0;
}

static DWORD FspNpGetCredentialsKind(PWSTR RemoteName, PDWORD PCredentialsKind)
{
    HKEY RegKey = 0;
    DWORD NpResult, RegSize;
    DWORD Credentials;
    PWSTR ClassName, InstanceName;
    ULONG ClassNameLen, InstanceNameLen;
    WCHAR ClassNameBuf[sizeof(((FSP_FSCTL_VOLUME_PARAMS *)0)->Prefix) / sizeof(WCHAR)];

    *PCredentialsKind = FSP_NP_CREDENTIALS_NONE;

    if (!FspNpParseRemoteName(RemoteName,
        &ClassName, &ClassNameLen, &InstanceName, &InstanceNameLen))
        return WN_BAD_NETNAME;

    if (ClassNameLen > sizeof ClassNameBuf / sizeof ClassNameBuf[0] - 1)
        ClassNameLen = sizeof ClassNameBuf / sizeof ClassNameBuf[0] - 1;
    memcpy(ClassNameBuf, ClassName, ClassNameLen * sizeof(WCHAR));
    ClassNameBuf[ClassNameLen] = '\0';

    NpResult = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"" LAUNCHER_REGKEY, 0, KEY_READ, &RegKey);
    if (ERROR_SUCCESS != NpResult)
        goto exit;

    RegSize = sizeof Credentials;
    Credentials = 0; /* default is NO credentials */
    NpResult = RegGetValueW(RegKey, ClassNameBuf, L"Credentials", RRF_RT_REG_DWORD, 0,
        &Credentials, &RegSize);
    if (ERROR_SUCCESS != NpResult && ERROR_FILE_NOT_FOUND != NpResult)
        goto exit;

    switch (Credentials)
    {
    case FSP_NP_CREDENTIALS_NONE:
    case FSP_NP_CREDENTIALS_PASSWORD:
    case FSP_NP_CREDENTIALS_USERPASS:
        *PCredentialsKind = Credentials;
        break;
    }

    NpResult = ERROR_SUCCESS;

exit:
    if (0 != RegKey)
        RegCloseKey(RegKey);

    return NpResult;
}

static DWORD FspNpGetCredentials(
    HWND hwndOwner, PWSTR Caption, DWORD PrevNpResult,
    DWORD CredentialsKind,
    PBOOL PSave,
    PWSTR UserName, ULONG UserNameSize/* in chars */,
    PWSTR Password, ULONG PasswordSize/* in chars */)
{
    DWORD NpResult;
    CREDUI_INFOW UiInfo;

    memset(&UiInfo, 0, sizeof UiInfo);
    UiInfo.cbSize = sizeof UiInfo;
    UiInfo.hwndParent = hwndOwner;
    UiInfo.pszCaptionText = Caption;
    UiInfo.pszMessageText = L"Enter credentials to unlock this file system.";

#if !defined(FSP_NP_CREDUI_PROMPT_NEW)
    NpResult = CredUIPromptForCredentialsW(&UiInfo, L"NONE", 0, 0,
        UserName, UserNameSize,
        Password, PasswordSize,
        PSave,
        CREDUI_FLAGS_GENERIC_CREDENTIALS |
        CREDUI_FLAGS_DO_NOT_PERSIST |
        CREDUI_FLAGS_ALWAYS_SHOW_UI |
        (0 != PrevNpResult ? CREDUI_FLAGS_INCORRECT_PASSWORD : 0) |
        (0 != PSave ? CREDUI_FLAGS_SHOW_SAVE_CHECK_BOX : 0) |
        (FSP_NP_CREDENTIALS_PASSWORD == CredentialsKind ? 0/*CREDUI_FLAGS_KEEP_USERNAME*/ : 0));
#else
    WCHAR Domain[CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1];
    ULONG AuthPackage = 0;
    PVOID InAuthBuf = 0, OutAuthBuf = 0;
    ULONG InAuthSize, OutAuthSize, DomainSize;

    InAuthSize = 0;
    if (!CredPackAuthenticationBufferW(
            CRED_PACK_GENERIC_CREDENTIALS, UserName, Password, 0, &InAuthSize) &&
        ERROR_INSUFFICIENT_BUFFER != GetLastError())
    {
        NpResult = GetLastError();
        goto exit;
    }

    InAuthBuf = MemAlloc(InAuthSize);
    if (0 == InAuthBuf)
    {
        NpResult = ERROR_NO_SYSTEM_RESOURCES;
        goto exit;
    }

    if (!CredPackAuthenticationBufferW(
        CRED_PACK_GENERIC_CREDENTIALS, UserName, Password, InAuthBuf, &InAuthSize))
    {
        NpResult = GetLastError();
        goto exit;
    }

    NpResult = CredUIPromptForWindowsCredentialsW(&UiInfo, PrevNpResult,
        &AuthPackage, InAuthBuf, InAuthSize, &OutAuthBuf, &OutAuthSize, PSave,
        CREDUIWIN_GENERIC | (0 != PSave ? CREDUIWIN_CHECKBOX : 0));
    if (ERROR_SUCCESS != NpResult)
        goto exit;

    DomainSize = sizeof Domain / sizeof Domain[0];
    if (!CredUnPackAuthenticationBufferW(0, OutAuthBuf, OutAuthSize,
        UserName, &UserNameSize, Domain, &DomainSize, Password, &PasswordSize))
    {
        NpResult = GetLastError();
        goto exit;
    }

    NpResult = ERROR_SUCCESS;

exit:
    if (0 != OutAuthBuf)
    {
        SecureZeroMemory(OutAuthBuf, OutAuthSize);
        CoTaskMemFree(OutAuthBuf);
    }

    if (0 != InAuthBuf)
    {
        SecureZeroMemory(InAuthBuf, InAuthSize);
        MemFree(InAuthBuf);
    }
#endif

    return NpResult;
}

DWORD APIENTRY NPGetConnection(
    LPWSTR lpLocalName, LPWSTR lpRemoteName, LPDWORD lpnBufferLen)
{
    DWORD NpResult;
    NTSTATUS Result;
    WCHAR LocalNameBuf[3];
    WCHAR VolumeNameBuf[FSP_FSCTL_VOLUME_NAME_SIZEMAX / sizeof(WCHAR)];
    PWCHAR VolumeListBuf = 0, VolumeListBufEnd, VolumeName, P;
    SIZE_T VolumeListSize, VolumeNameSize;
    ULONG Backslashes;

    if (!FspNpCheckLocalName(lpLocalName))
        return WN_BAD_LOCALNAME;

    LocalNameBuf[0] = lpLocalName[0] & ~0x20; /* convert to uppercase */
    LocalNameBuf[1] = L':';
    LocalNameBuf[2] = L'\0';

    if (0 == QueryDosDeviceW(LocalNameBuf, VolumeNameBuf, sizeof VolumeNameBuf))
        return WN_NOT_CONNECTED;

    Result = FspNpGetVolumeList(&VolumeListBuf, &VolumeListSize);
    if (!NT_SUCCESS(Result))
        return WN_OUT_OF_MEMORY;

    NpResult = WN_NOT_CONNECTED;
    for (P = VolumeListBuf, VolumeListBufEnd = (PVOID)((PUINT8)P + VolumeListSize), VolumeName = P;
        VolumeListBufEnd > P; P++)
    {
        if (L'\0' == *P)
        {
            if (0 == lstrcmpW(VolumeNameBuf, VolumeName))
            {
                /*
                 * Looks like this is a WinFsp device. Extract the VolumePrefix from the VolumeName.
                 *
                 * The VolumeName will have the following syntax:
                 *     \Device\Volume{GUID}\Server\Share
                 *
                 * We want to extract the \Server\Share part. We will simply count backslashes and
                 * stop at the third one. Since we are about to break this loop, it is ok to mess
                 * with the loop variables.
                 */

                for (Backslashes = 0; VolumeName < P; VolumeName++)
                    if (L'\\' == *VolumeName)
                        if (3 == ++Backslashes)
                            break;

                if (3 == Backslashes)
                {
                    VolumeNameSize = lstrlenW(VolumeName) + 1/* term-0 */;
                    if (*lpnBufferLen >= 1/* lead-\ */ + VolumeNameSize)
                    {
                        *lpRemoteName = L'\\';
                        memcpy(lpRemoteName + 1, VolumeName, VolumeNameSize * sizeof(WCHAR));
                        NpResult = WN_SUCCESS;
                    }
                    else
                    {
                        *lpnBufferLen = (DWORD)(1/* lead-\ */ + VolumeNameSize);
                        NpResult = WN_MORE_DATA;
                    }
                }

                break;
            }
            else
                VolumeName = P + 1;
        }
    }

    MemFree(VolumeListBuf);

    return NpResult;
}

DWORD APIENTRY NPAddConnection(LPNETRESOURCEW lpNetResource, LPWSTR lpPassword, LPWSTR lpUserName)
{
    DWORD NpResult;
    DWORD dwType = lpNetResource->dwType;
    LPWSTR lpRemoteName = lpNetResource->lpRemoteName;
    LPWSTR lpLocalName = lpNetResource->lpLocalName;
    WCHAR LocalNameBuf[3];
    PWSTR ClassName, InstanceName, RemoteName, P;
    ULONG ClassNameLen, InstanceNameLen;
    DWORD CredentialsKind;
    PWSTR PipeBuf = 0;
#if defined(FSP_NP_CREDENTIAL_MANAGER)
    PCREDENTIALW Credential = 0;
#endif

    if (dwType & RESOURCETYPE_PRINT)
        return WN_BAD_VALUE;

    if (!FspNpParseRemoteName(lpRemoteName,
        &ClassName, &ClassNameLen, &InstanceName, &InstanceNameLen))
        return WN_BAD_NETNAME;
    RemoteName = lpRemoteName + 1;

    LocalNameBuf[0] = L'\0';
    if (0 != lpLocalName && L'\0' != lpLocalName[0])
    {
        if (!FspNpCheckLocalName(lpLocalName))
            return WN_BAD_LOCALNAME;

        LocalNameBuf[0] = lpLocalName[0] & ~0x20; /* convert to uppercase */
        LocalNameBuf[1] = L':';
        LocalNameBuf[2] = L'\0';

        if (GetLogicalDrives() & (1 << (LocalNameBuf[0] - 'A')))
            return WN_ALREADY_CONNECTED;
    }

    FspNpGetCredentialsKind(lpRemoteName, &CredentialsKind);

#if defined(FSP_NP_CREDENTIAL_MANAGER)
    /* if we need credentials and none were passed check with the credential manager */
    if (FSP_NP_CREDENTIALS_NONE != CredentialsKind && 0 == lpPassword &&
        CredReadW(lpRemoteName, CRED_TYPE_GENERIC, 0, &Credential))
    {
        if (sizeof(WCHAR) <= Credential->CredentialBlobSize &&
            L'\0' == ((PWSTR)(Credential->CredentialBlob))
                [(Credential->CredentialBlobSize / sizeof(WCHAR)) - 1])
        {
            lpUserName = Credential->UserName;
            lpPassword = (PVOID)Credential->CredentialBlob;
        }
    }
#endif

    /* if we need credentials and we don't have any return ACCESS DENIED */
    if (FSP_NP_CREDENTIALS_NONE != CredentialsKind)
    {
        int Length;
        if (0 == lpPassword ||
            (0 == (Length = lstrlenW(lpPassword))) || CREDUI_MAX_PASSWORD_LENGTH < Length)
        {
            NpResult = WN_ACCESS_DENIED;
            goto exit;
        }
    }
    if (FSP_NP_CREDENTIALS_USERPASS == CredentialsKind)
    {
        int Length;
        if (0 == lpUserName ||
            (0 == (Length = lstrlenW(lpUserName))) || CREDUI_MAX_USERNAME_LENGTH < Length)
        {
            NpResult = WN_ACCESS_DENIED;
            goto exit;
        }
    }

    PipeBuf = MemAlloc(LAUNCHER_PIPE_BUFFER_SIZE);
    if (0 == PipeBuf)
    {
        NpResult = WN_OUT_OF_MEMORY;
        goto exit;
    }

    /* we do not explicitly check, but assumption is it all fits in LAUNCHER_PIPE_BUFFER_SIZE */
    P = PipeBuf;
    *P++ = FSP_NP_CREDENTIALS_NONE != CredentialsKind ?
        LauncherSvcInstanceStartWithSecret : LauncherSvcInstanceStart;
    memcpy(P, ClassName, ClassNameLen * sizeof(WCHAR)); P += ClassNameLen; *P++ = L'\0';
    memcpy(P, InstanceName, InstanceNameLen * sizeof(WCHAR)); P += InstanceNameLen; *P++ = L'\0';
    lstrcpyW(P, RemoteName); P += lstrlenW(RemoteName) + 1;
    lstrcpyW(P, LocalNameBuf); P += lstrlenW(LocalNameBuf) + 1;
    if (FSP_NP_CREDENTIALS_USERPASS == CredentialsKind)
    {
        lstrcpyW(P, lpUserName); P += lstrlenW(lpUserName) + 1;
    }
    if (FSP_NP_CREDENTIALS_NONE != CredentialsKind)
    {
        lstrcpyW(P, lpPassword); P += lstrlenW(lpPassword) + 1;
    }

    NpResult = FspNpCallLauncherPipe(
        PipeBuf, (ULONG)(P - PipeBuf) * sizeof(WCHAR), LAUNCHER_PIPE_BUFFER_SIZE);
    switch (NpResult)
    {
    case WN_SUCCESS:
    case WN_ACCESS_DENIED:
        break;
    case ERROR_ALREADY_EXISTS:
        /*
         * The file system is already running! If we are being asked for a drive mapping,
         * see if it is the one we already have to decide on the error code to return.
         */
        if (L'\0' != LocalNameBuf[0])
        {
            WCHAR ExpectRemoteNameBuf[sizeof(((FSP_FSCTL_VOLUME_PARAMS *)0)->Prefix) / sizeof(WCHAR)];
            WCHAR RemoteNameBuf[sizeof(((FSP_FSCTL_VOLUME_PARAMS *)0)->Prefix) / sizeof(WCHAR)];
            DWORD RemoteNameSize;

            P = ExpectRemoteNameBuf;
            *P++ = L'\\'; *P++ = L'\\';
            memcpy(P, ClassName, ClassNameLen * sizeof(WCHAR)); P += ClassNameLen; *P++ = L'\\';
            memcpy(P, InstanceName, InstanceNameLen * sizeof(WCHAR)); P += InstanceNameLen; *P++ = L'\0';

            RemoteNameSize = sizeof RemoteNameBuf / sizeof(WCHAR);
            NpResult = NPGetConnection(LocalNameBuf, RemoteNameBuf, &RemoteNameSize);
            if (WN_SUCCESS == NpResult)
                NpResult = 0 == lstrcmpW(ExpectRemoteNameBuf, RemoteNameBuf) ? WN_SUCCESS : WN_NO_NETWORK;
            else
                NpResult = WN_NO_NETWORK;
        }
        else
            /* we are not being asked for a drive mapping, so whatever we have is good! */
            NpResult = WN_SUCCESS;
        break;
    default:
        NpResult = WN_NO_NETWORK;
        break;
    }

exit:
    MemFree(PipeBuf);

#if defined(FSP_NP_CREDENTIAL_MANAGER)
    if (0 != Credential)
        CredFree(Credential);
#endif

    return NpResult;
}

DWORD APIENTRY NPAddConnection3(HWND hwndOwner,
    LPNETRESOURCEW lpNetResource, LPWSTR lpPassword, LPWSTR lpUserName, DWORD dwFlags)
{
    DWORD NpResult;
    PWSTR RemoteName = lpNetResource->lpRemoteName;
    DWORD CredentialsKind;
    WCHAR UserName[CREDUI_MAX_USERNAME_LENGTH + 1], Password[CREDUI_MAX_PASSWORD_LENGTH + 1];
#if defined(FSP_NP_CREDENTIAL_MANAGER)
    BOOL Save = TRUE;
#endif

    //dwFlags |= CONNECT_INTERACTIVE | CONNECT_PROMPT; /* TESTING ONLY! */

    /* CONNECT_PROMPT is only valid if CONNECT_INTERACTIVE is also set */
    if (CONNECT_PROMPT == (dwFlags & (CONNECT_INTERACTIVE | CONNECT_PROMPT)))
        return WN_BAD_VALUE;

    /* if not CONNECT_PROMPT go ahead and attempt to NPAddConnection once */
    if (0 == (dwFlags & CONNECT_PROMPT))
    {
        NpResult = NPAddConnection(lpNetResource, lpPassword, lpUserName);
        if (WN_ACCESS_DENIED != NpResult || 0 == (dwFlags & CONNECT_INTERACTIVE))
            return NpResult;
    }

    FspNpGetCredentialsKind(RemoteName, &CredentialsKind);
    if (FSP_NP_CREDENTIALS_NONE == CredentialsKind)
        return WN_CANCEL;

    /* if CONNECT_INTERACTIVE keep asking the user for valid credentials or cancel */
    NpResult = WN_SUCCESS;
    lstrcpyW(UserName, L"UNSPECIFIED");
    Password[0] = L'\0';
    if (FSP_NP_CREDENTIALS_PASSWORD == CredentialsKind)
        FspNpParseUserName(RemoteName, UserName, sizeof UserName / sizeof UserName[0]);
    do
    {
        NpResult = FspNpGetCredentials(
            hwndOwner, RemoteName, NpResult,
            CredentialsKind,
#if defined(FSP_NP_CREDENTIAL_MANAGER)
            &Save,
#else
            0,
#endif
            UserName, sizeof UserName / sizeof UserName[0],
            Password, sizeof Password / sizeof Password[0]);
        if (WN_SUCCESS != NpResult)
            break;

        NpResult = NPAddConnection(lpNetResource, Password, UserName);
    } while (WN_ACCESS_DENIED == NpResult);

#if defined(FSP_NP_CREDENTIAL_MANAGER)
    if (WN_SUCCESS == NpResult && Save)
    {
        CREDENTIALW Credential;

        memset(&Credential, 0, sizeof Credential);
        Credential.Type = CRED_TYPE_GENERIC;
        Credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
        Credential.TargetName = RemoteName;
        Credential.UserName = UserName;
        Credential.CredentialBlobSize = (lstrlenW(Password) + 1) * sizeof(WCHAR);
        Credential.CredentialBlob = (PVOID)Password;

        CredWriteW(&Credential, 0);
    }
#endif

    SecureZeroMemory(Password, sizeof Password);

    return NpResult;
}

DWORD APIENTRY NPCancelConnection(LPWSTR lpName, BOOL fForce)
{
    DWORD NpResult;
    WCHAR RemoteNameBuf[sizeof(((FSP_FSCTL_VOLUME_PARAMS *)0)->Prefix) / sizeof(WCHAR)];
    DWORD RemoteNameSize;
    PWSTR ClassName, InstanceName, RemoteName, P;
    ULONG ClassNameLen, InstanceNameLen;
    PWSTR PipeBuf = 0;

    if (FspNpCheckLocalName(lpName))
    {
        RemoteNameSize = sizeof RemoteNameBuf / sizeof(WCHAR);
        NpResult = NPGetConnection(lpName, RemoteNameBuf, &RemoteNameSize);
        if (WN_SUCCESS != NpResult)
            return NpResult;

        RemoteName = RemoteNameBuf;
    }
    else if (FspNpCheckRemoteName(lpName))
        RemoteName = lpName;
    else
        return WN_BAD_NETNAME;

    if (!FspNpParseRemoteName(RemoteName,
        &ClassName, &ClassNameLen, &InstanceName, &InstanceNameLen))
        return WN_BAD_NETNAME;

    PipeBuf = MemAlloc(LAUNCHER_PIPE_BUFFER_SIZE);
    if (0 == PipeBuf)
        return WN_OUT_OF_MEMORY;

    P = PipeBuf;
    *P++ = LauncherSvcInstanceStop;
    memcpy(P, ClassName, ClassNameLen * sizeof(WCHAR)); P += ClassNameLen; *P++ = L'\0';
    memcpy(P, InstanceName, InstanceNameLen * sizeof(WCHAR)); P += InstanceNameLen; *P++ = L'\0';

    NpResult = FspNpCallLauncherPipe(
        PipeBuf, (ULONG)(P - PipeBuf) * sizeof(WCHAR), LAUNCHER_PIPE_BUFFER_SIZE);
    switch (NpResult)
    {
    case WN_SUCCESS:
        break;
    case ERROR_FILE_NOT_FOUND:
        NpResult = WN_NOT_CONNECTED;
        break;
    default:
        NpResult = WN_NO_NETWORK;
        break;
    }

    MemFree(PipeBuf);

    return NpResult;
}

typedef struct
{
    DWORD Signature;                    /* cheap and cheerful! */
    DWORD dwScope;
    PWCHAR VolumeListBuf, VolumeListBufEnd, VolumeName;
    DWORD LogicalDrives;
} FSP_NP_ENUM;

static inline BOOLEAN FspNpValidateEnum(FSP_NP_ENUM *Enum)
{
#if 0
    return
        0 != Enum &&
        HeapValidate(GetProcessHeap(), 0, Enum) &&
        'munE' == Enum->Signature;
#else
    return
        0 != Enum &&
        'munE' == Enum->Signature;
#endif
}

DWORD APIENTRY NPOpenEnum(
    DWORD dwScope, DWORD dwType, DWORD dwUsage, LPNETRESOURCEW lpNetResource, LPHANDLE lphEnum)
{
    NTSTATUS Result;
    FSP_NP_ENUM *Enum = 0;
    SIZE_T VolumeListSize;

    switch (dwScope)
    {
    case RESOURCE_CONNECTED:
    case RESOURCE_CONTEXT:
        /* ignore lpNetResource; according to documentation it should be NULL */
        break;
    default:
        return WN_NOT_SUPPORTED;
    }

    if (dwType & RESOURCETYPE_PRINT)
        return WN_BAD_VALUE;

    Enum = MemAlloc(sizeof *Enum);
    if (0 == Enum)
        return WN_OUT_OF_MEMORY;

    Result = FspNpGetVolumeList(&Enum->VolumeListBuf, &VolumeListSize);
    if (!NT_SUCCESS(Result))
    {
        MemFree(Enum);
        return WN_OUT_OF_MEMORY;
    }

    Enum->Signature = 'munE';
    Enum->dwScope = dwScope;
    Enum->VolumeListBufEnd = (PVOID)((PUINT8)Enum->VolumeListBuf + VolumeListSize);
    Enum->VolumeName = Enum->VolumeListBuf;
    Enum->LogicalDrives = GetLogicalDrives();

    *lphEnum = Enum;

    return WN_SUCCESS;
}

DWORD APIENTRY NPEnumResource(
    HANDLE hEnum, LPDWORD lpcCount, LPVOID lpBuffer, LPDWORD lpBufferSize)
{
    FSP_NP_ENUM *Enum = hEnum;
    DWORD NpResult;
    LPNETRESOURCEW Resource;            /* grows upwards */
    PWCHAR Strings;                     /* grows downwards */
    PWCHAR ProviderName = 0;
    DWORD Count;
    PWCHAR P, VolumePrefix;
    ULONG Backslashes;
    WCHAR Drive;

    if (!FspNpValidateEnum(Enum))
        return WN_BAD_HANDLE;

    if (0 == lpcCount || 0 == lpBuffer || 0 == lpBufferSize)
        return WN_BAD_VALUE;

    Resource = lpBuffer;
    Strings = (PVOID)((PUINT8)lpBuffer + (*lpBufferSize & ~1/* WCHAR alignment */));
    Count = 0;
    for (P = Enum->VolumeName; *lpcCount > Count && Enum->VolumeListBufEnd > P; P++)
    {
        if (L'\0' == *P)
        {
            /*
             * Extract the VolumePrefix from the VolumeName.
             *
             * The VolumeName will have the following syntax:
             *     \Device\Volume{GUID}\Server\Share
             *
             * We want to extract the \Server\Share part. We will simply count backslashes and
             * stop at the third one.
             */

            for (Backslashes = 0, VolumePrefix = Enum->VolumeName; VolumePrefix < P; VolumePrefix++)
                if (L'\\' == *VolumePrefix)
                    if (3 == ++Backslashes)
                        break;

            if (3 == Backslashes)
            {
                Drive = FspNpGetDriveLetter(&Enum->LogicalDrives, Enum->VolumeName);

                Strings -= (Drive ? 3 : 0) + 2/* backslash + term-0 */ + lstrlenW(VolumePrefix) +
                    (0 == ProviderName ? lstrlenW(L"" FSP_NP_NAME) + 1 : 0);

                if ((PVOID)(Resource + 1) > (PVOID)Strings)
                {
                    if (0 == Count)
                    {
                        *lpBufferSize =
                            (DWORD)((PUINT8)(Resource + 1) - (PUINT8)lpBuffer) +
                            (DWORD)((PUINT8)lpBuffer + *lpBufferSize - (PUINT8)Strings);
                        NpResult = WN_MORE_DATA;
                    }
                    else
                    {
                        *lpcCount = Count;
                        NpResult = WN_SUCCESS;
                    }

                    goto exit;
                }

                if (0 == ProviderName)
                {
                    ProviderName = Strings + (Drive ? 3 : 0) + 2/* backslash + term-0 */ + lstrlenW(VolumePrefix);
                    lstrcpyW(ProviderName, L"" FSP_NP_NAME);
                }

                if (Drive)
                {
                    Strings[0] = Drive;
                    Strings[1] = L':';
                    Strings[2] = L'\0';

                    Strings[3] = L'\\';
                    lstrcpyW(Strings + 4, VolumePrefix);
                }
                else
                {
                    Strings[0] = L'\\';
                    lstrcpyW(Strings + 1, VolumePrefix);
                }

                Resource->dwScope = Enum->dwScope;
                Resource->dwType = RESOURCETYPE_DISK;
                Resource->dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
                Resource->dwUsage = RESOURCEUSAGE_CONNECTABLE;
                Resource->lpLocalName = Drive ? Strings : 0;
                Resource->lpRemoteName = Drive ? Strings + 3 : Strings;
                Resource->lpComment = 0;
                Resource->lpProvider = ProviderName;
                Resource++;

                Count++;
            }

            Enum->VolumeName = P + 1;
        }
    }

    if (0 == Count)
        NpResult = WN_NO_MORE_ENTRIES;
    else
    {
        *lpcCount = Count;
        NpResult = WN_SUCCESS;
    }

exit:
    return NpResult;
}

DWORD APIENTRY NPCloseEnum(HANDLE hEnum)
{
    FSP_NP_ENUM *Enum = hEnum;

    if (!FspNpValidateEnum(Enum))
        return WN_BAD_HANDLE;

    MemFree(Enum->VolumeListBuf);
    MemFree(Enum);

    return WN_SUCCESS;
}

NTSTATUS FspNpRegister(VOID)
{
    extern HINSTANCE DllInstance;
    WCHAR ProviderPath[MAX_PATH];
    WCHAR RegBuffer[1024];
    PWSTR P, Part;
    DWORD RegResult, RegType, RegBufferSize;
    HKEY RegKey;
    BOOLEAN FoundProvider;

    if (0 == GetModuleFileNameW(DllInstance, ProviderPath, MAX_PATH))
        return FspNtStatusFromWin32(GetLastError());

    RegResult = RegCreateKeyExW(
        HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\" FSP_NP_NAME,
        0, 0, 0, KEY_ALL_ACCESS, 0, &RegKey, 0);
    if (ERROR_SUCCESS != RegResult)
        return FspNtStatusFromWin32(RegResult);

    RegResult = RegSetValueExW(RegKey,
        L"Group", 0, REG_SZ, (PVOID) L"NetworkProvider", sizeof L"NetworkProvider");
    if (ERROR_SUCCESS != RegResult)
        goto close_and_exit;

    RegCloseKey(RegKey);

    RegResult = RegCreateKeyExW(
        HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\" FSP_NP_NAME "\\NetworkProvider",
        0, 0, 0, KEY_ALL_ACCESS, 0, &RegKey, 0);
    if (ERROR_SUCCESS != RegResult)
        return FspNtStatusFromWin32(RegResult);

    RegResult = ERROR_RESOURCE_NAME_NOT_FOUND; /* not a real resource error! */
    {
        PVOID VersionInfo = 0;
        DWORD Size;
        PWSTR Description;

        Size = GetFileVersionInfoSizeW(ProviderPath, &Size/*dummy*/);
        if (0 < Size)
        {
            VersionInfo = MemAlloc(Size);
            if (0 != VersionInfo &&
                GetFileVersionInfoW(ProviderPath, 0, Size, VersionInfo) &&
                VerQueryValueW(VersionInfo, L"\\StringFileInfo\\040904b0\\FileDescription",
                    &Description, &Size))
            {
                Size = Size * 2 + sizeof(WCHAR);
                RegResult = RegSetValueExW(RegKey,
                    L"Name", 0, REG_SZ, (PVOID)Description, Size);
            }

            MemFree(VersionInfo);
        }
    }
    if (ERROR_SUCCESS != RegResult)
        goto close_and_exit;

    RegResult = RegSetValueExW(RegKey,
        L"ProviderPath", 0, REG_SZ, (PVOID)ProviderPath, (lstrlenW(ProviderPath) + 1) * sizeof(WCHAR));
    if (ERROR_SUCCESS != RegResult)
        goto close_and_exit;

    RegCloseKey(RegKey);

    RegResult = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\NetworkProvider\\Order",
        0, KEY_ALL_ACCESS, &RegKey);
    if (ERROR_SUCCESS != RegResult)
        return FspNtStatusFromWin32(RegResult);

    RegBufferSize = sizeof RegBuffer - sizeof L"," FSP_NP_NAME;
    RegResult = RegQueryValueExW(RegKey,
        L"ProviderOrder", 0, &RegType, (PVOID)RegBuffer, &RegBufferSize);
    if (ERROR_SUCCESS != RegResult)
        goto close_and_exit;
    RegBufferSize /= sizeof(WCHAR);

    FoundProvider = FALSE;
    RegBuffer[RegBufferSize] = L'\0';
    P = RegBuffer, Part = P;
    do
    {
        if (L',' == *P || '\0' == *P)
        {
            if (CSTR_EQUAL == CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE,
                Part, (int)(P - Part),
                L"" FSP_NP_NAME, (int)(sizeof L"" FSP_NP_NAME - sizeof(WCHAR)) / sizeof(WCHAR)))
            {
                FoundProvider = TRUE;
                break;
            }
            else
                Part = P + 1;
        }
    } while (L'\0' != *P++);

    if (!FoundProvider)
    {
        P--;
        memcpy(P, L"," FSP_NP_NAME, sizeof L"," FSP_NP_NAME);

        RegBufferSize = lstrlenW(RegBuffer);
        RegBufferSize++;

        RegResult = RegSetValueExW(RegKey,
            L"ProviderOrder", 0, REG_SZ, (PVOID)RegBuffer, RegBufferSize * sizeof(WCHAR));
        if (ERROR_SUCCESS != RegResult)
            goto close_and_exit;
    }

    RegCloseKey(RegKey);

    return STATUS_SUCCESS;

close_and_exit:
    RegCloseKey(RegKey);
    return FspNtStatusFromWin32(RegResult);
}

NTSTATUS FspNpUnregister(VOID)
{
    WCHAR RegBuffer[1024];
    PWSTR P, Part;
    DWORD RegResult, RegType, RegBufferSize;
    HKEY RegKey;
    BOOLEAN FoundProvider;

    RegResult = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\NetworkProvider\\Order",
        0, KEY_ALL_ACCESS, &RegKey);
    if (ERROR_SUCCESS != RegResult)
        return FspNtStatusFromWin32(RegResult);

    RegBufferSize = sizeof RegBuffer - sizeof L"," FSP_NP_NAME;
    RegResult = RegQueryValueExW(RegKey,
        L"ProviderOrder", 0, &RegType, (PVOID)RegBuffer, &RegBufferSize);
    if (ERROR_SUCCESS != RegResult)
        goto close_and_exit;
    RegBufferSize /= sizeof(WCHAR);

    FoundProvider = FALSE;
    RegBuffer[RegBufferSize] = L'\0';
    P = RegBuffer, Part = P;
    do
    {
        if (L',' == *P || '\0' == *P)
        {
            if (CSTR_EQUAL == CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE,
                Part, (int)(P - Part),
                L"" FSP_NP_NAME, (int)(sizeof L"" FSP_NP_NAME - sizeof(WCHAR)) / sizeof(WCHAR)))
            {
                FoundProvider = TRUE;
                break;
            }
            else
                Part = P + 1;
        }
    } while (L'\0' != *P++);

    if (FoundProvider)
    {
        if (L',' == *P)
            P++;

        RtlMoveMemory(Part, P, (lstrlenW(P) + 1) * sizeof(WCHAR));

        RegBufferSize = lstrlenW(RegBuffer);
        while (0 < RegBufferSize && ',' == RegBuffer[RegBufferSize - 1])
            RegBufferSize--;
        RegBuffer[RegBufferSize] = L'\0';
        RegBufferSize++;

        RegResult = RegSetValueExW(RegKey,
            L"ProviderOrder", 0, REG_SZ, (PVOID)RegBuffer, RegBufferSize * sizeof(WCHAR));
        if (ERROR_SUCCESS != RegResult)
            goto close_and_exit;
    }

    RegCloseKey(RegKey);

    RegResult = RegDeleteTreeW(
        HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\" FSP_NP_NAME);
    if (ERROR_SUCCESS != RegResult && ERROR_FILE_NOT_FOUND != RegResult)
        return FspNtStatusFromWin32(RegResult);

    return STATUS_SUCCESS;

close_and_exit:
    RegCloseKey(RegKey);
    return FspNtStatusFromWin32(RegResult);
}
