/**
 * @file dll/np.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#include <dll/library.h>
#include <npapi.h>

#define FSP_NP_NAME                     "WinFsp.Np"
#define FSP_NP_DESC                     "File System Proxy"
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
         * WNNC_CON_ADDCONECTION
         * WNNC_CON_CANCELCONNECTION
         * WNNC_CON_GETCONNECTIONS
         * WNNC_CON_ADDCONECTION3
         * WNNC_CON_GETPERFORMANCE
         * WNNC_CON_DEFER
         */
        return WNNC_CON_GETCONNECTIONS;
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

DWORD APIENTRY NPGetConnection(LPWSTR lpLocalName, LPWSTR lpRemoteName, LPDWORD lpnBufferLen)
{
    DWORD NpResult;
    NTSTATUS Result;
    WCHAR LocalNameBuf[3];
    WCHAR VolumeNameBuf[FSP_FSCTL_VOLUME_NAME_SIZEMAX / sizeof(WCHAR)];
    PWCHAR VolumeListBuf = 0, VolumeListBufEnd, VolumeName, P;
    SIZE_T VolumeListSize, VolumeNameSize;
    ULONG Backslashes;

    if (0 == lpLocalName ||
        !(
            (L'A' <= lpLocalName[0] && lpLocalName[0] <= L'Z') ||
            (L'a' <= lpLocalName[0] && lpLocalName[0] <= L'z')
        ) ||
        L':' != lpLocalName[1])
        return WN_BAD_LOCALNAME;

    LocalNameBuf[0] = lpLocalName[0];
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

    RegResult = RegSetValueExW(RegKey,
        L"Name", 0, REG_SZ, (PVOID) L"" FSP_NP_DESC, sizeof L"" FSP_NP_DESC);
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
