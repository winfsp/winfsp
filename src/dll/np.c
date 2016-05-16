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

#define FSP_NP_NAME                     LIBRARY_NAME ".Np"
#define FSP_NP_TYPE                     ' spF'  /* pick a value hopefully not in use */

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
        return 0;
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

static inline DWORD FspNpCallLauncherPipe(PWSTR PipeBuf, ULONG SendSize, ULONG RecvSize)
{
    DWORD NpResult;
    DWORD LastError, BytesTransferred;

    LastError = CallNamedPipeW(L"" LAUNCHER_PIPE_NAME, PipeBuf, SendSize, PipeBuf, RecvSize,
        &BytesTransferred, NMPWAIT_USE_DEFAULT_WAIT) ? 0 : GetLastError();

    if (0 != LastError)
        NpResult = WN_NO_NETWORK;
    else if (sizeof(WCHAR) > BytesTransferred)
        NpResult = WN_NO_NETWORK;
    else if (LauncherSuccess == PipeBuf[0])
        NpResult = WN_SUCCESS;
    else if (LauncherFailure == PipeBuf[0])
    {
        if (BytesTransferred < RecvSize)
            PipeBuf[BytesTransferred / sizeof(WCHAR)] = L'\0';
        else
            PipeBuf[RecvSize / sizeof(WCHAR) - 1] = L'\0';

        if (0 == lstrcmpW(L"183"/*ERROR_ALREADY_EXISTS*/, PipeBuf + 1))
            NpResult = WN_ALREADY_CONNECTED;
        else
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
    return NPAddConnection3(0, lpNetResource, lpPassword, lpUserName, 0);
}

DWORD APIENTRY NPAddConnection3(HWND hwndOwner,
    LPNETRESOURCEW lpNetResource, LPWSTR lpPassword, LPWSTR lpUserName, DWORD dwFlags)
{
    DWORD NpResult;
    DWORD dwType = lpNetResource->dwType;
    LPWSTR lpRemoteName = lpNetResource->lpRemoteName;
    LPWSTR lpLocalName = lpNetResource->lpLocalName;
    WCHAR LocalNameBuf[3];
    PWSTR ClassName, InstanceName, RemoteName, P;
    ULONG ClassNameLen, InstanceNameLen;
    PWSTR PipeBuf = 0;

    if (dwType & RESOURCETYPE_PRINT)
        return WN_BAD_VALUE;

    if (!FspNpCheckRemoteName(lpRemoteName))
        return WN_BAD_NETNAME;

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

    PipeBuf = MemAlloc(LAUNCHER_PIPE_BUFFER_SIZE);
    if (0 == PipeBuf)
        return WN_OUT_OF_MEMORY;

    P = PipeBuf;
    *P++ = LauncherSvcInstanceStart;
    memcpy(P, ClassName, ClassNameLen * sizeof(WCHAR)); P += ClassNameLen; *P++ = L'\0';
    memcpy(P, InstanceName, InstanceNameLen * sizeof(WCHAR)); P += InstanceNameLen; *P++ = L'\0';
    lstrcpyW(P, RemoteName); P += lstrlenW(RemoteName) + 1;
    lstrcpyW(P, LocalNameBuf); P += lstrlenW(LocalNameBuf) + 1;

    NpResult = FspNpCallLauncherPipe(
        PipeBuf, (ULONG)(P - PipeBuf) * sizeof(WCHAR), LAUNCHER_PIPE_BUFFER_SIZE);

    MemFree(PipeBuf);

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

    MemFree(PipeBuf);

    return NpResult;
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
            if (CSTR_EQUAL == CompareStringW(LOCALE_SYSTEM_DEFAULT, NORM_IGNORECASE,
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
            if (CSTR_EQUAL == CompareStringW(LOCALE_SYSTEM_DEFAULT, NORM_IGNORECASE,
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
