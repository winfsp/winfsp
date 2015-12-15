/**
 * @file dll/fsctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>
#if !defined(NDEBUG)
#include <sddl.h>
#endif

#define GLOBALROOT                      L"\\\\?\\GLOBALROOT"

static inline PVOID Malloc(SIZE_T Size)
{
    PVOID P = malloc(Size);
    if (0 != P)
        SetLastError(ERROR_NO_SYSTEM_RESOURCES);
    return P;
}
static inline VOID GlobalDevicePath(PWCHAR DevicePathBuf, SIZE_T DevicePathSize, PWSTR DevicePath)
{
    StringCbPrintfW(DevicePathBuf, DevicePathSize,
        L'\\' == DevicePath[0] ? GLOBALROOT "%s" : GLOBALROOT "\\Device\\%s", DevicePath);
}

static NTSTATUS CreateSelfRelativeSecurityDescriptor(PSECURITY_DESCRIPTOR SecurityDescriptor,
    PSECURITY_DESCRIPTOR *PSelfRelativeSecurityDescriptor, PDWORD PSelfRelativeSecurityDescriptorSize)
{
    NTSTATUS Result;
    BOOLEAN Success;
    PSECURITY_DESCRIPTOR SelfRelativeSecurityDescriptor = 0;
    DWORD SelfRelativeSecurityDescriptorSize;
    SECURITY_DESCRIPTOR_CONTROL SecurityDescriptorControl;
    DWORD SecurityDescriptorRevision;
    SECURITY_DESCRIPTOR SecurityDescriptorStruct;
    PTOKEN_USER User = 0;
    PACL Acl = 0;
    DWORD UserSize = 0;
    DWORD AclSize = 0;
    HANDLE Token = 0;

    *PSelfRelativeSecurityDescriptor = 0;
    *PSelfRelativeSecurityDescriptorSize = 0;

    if (0 == SecurityDescriptor)
    {
        Success =
            OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token) &&
            (GetTokenInformation(Token, TokenUser, 0, 0, &UserSize) ||
                ERROR_INSUFFICIENT_BUFFER == GetLastError()) &&
            (User = Malloc(UserSize)) &&
            GetTokenInformation(Token, TokenUser, User, UserSize, &UserSize) &&
            (AclSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(User->User.Sid) - sizeof(DWORD)) &&
            (Acl = Malloc(AclSize)) &&
            InitializeAcl(Acl, AclSize, ACL_REVISION) &&
            AddAccessAllowedAce(Acl, ACL_REVISION, FILE_ALL_ACCESS, User->User.Sid) &&
            InitializeSecurityDescriptor(&SecurityDescriptorStruct, SECURITY_DESCRIPTOR_REVISION) &&
            SetSecurityDescriptorDacl(&SecurityDescriptorStruct, TRUE, Acl, FALSE) &&
            SetSecurityDescriptorControl(&SecurityDescriptorStruct, SE_DACL_PROTECTED, SE_DACL_PROTECTED);
        if (!Success)
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
        SecurityDescriptor = &SecurityDescriptorStruct;
    }

    if (!GetSecurityDescriptorControl(SecurityDescriptor,
        &SecurityDescriptorControl, &SecurityDescriptorRevision))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (SecurityDescriptorControl & SE_SELF_RELATIVE)
    {
        SelfRelativeSecurityDescriptorSize = GetSecurityDescriptorLength(SecurityDescriptor);
        Success =
            (SelfRelativeSecurityDescriptor = Malloc(SelfRelativeSecurityDescriptorSize)) &&
            memcpy(SelfRelativeSecurityDescriptor, SecurityDescriptor, SelfRelativeSecurityDescriptorSize);
    }
    else
    {
        SelfRelativeSecurityDescriptorSize = 0;
        Success =
            (MakeSelfRelativeSD(SecurityDescriptor, 0, &SelfRelativeSecurityDescriptorSize) ||
                ERROR_INSUFFICIENT_BUFFER == GetLastError()) &&
            (SelfRelativeSecurityDescriptor = Malloc(SelfRelativeSecurityDescriptorSize)) &&
            (MakeSelfRelativeSD(SecurityDescriptor, SelfRelativeSecurityDescriptor, &SelfRelativeSecurityDescriptorSize));
    }
    if (!Success)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    *PSelfRelativeSecurityDescriptor = SelfRelativeSecurityDescriptor;
    *PSelfRelativeSecurityDescriptorSize = SelfRelativeSecurityDescriptorSize;
    Result = STATUS_SUCCESS;

exit:
    if (0 != Token)
        CloseHandle(Token);

    free(Acl);
    free(User);

    if (STATUS_SUCCESS != Result)
        free(SelfRelativeSecurityDescriptor);

    return Result;
}

FSP_API NTSTATUS FspFsctlCreateVolume(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *Params, PSECURITY_DESCRIPTOR SecurityDescriptor,
    PWCHAR VolumePathBuf, SIZE_T VolumePathSize)
{
    NTSTATUS Result = STATUS_SUCCESS;
    PSECURITY_DESCRIPTOR SelfRelativeSecurityDescriptor = 0;
    DWORD SelfRelativeSecurityDescriptorSize;
    FSP_FSCTL_VOLUME_PARAMS *ParamsBuf = 0;
    WCHAR DevicePathBuf[MAX_PATH];
    HANDLE DeviceHandle = INVALID_HANDLE_VALUE;
    DWORD Bytes;

    if (sizeof(WCHAR) <= VolumePathSize)
        VolumePathBuf[0] = L'\0';

    Result = CreateSelfRelativeSecurityDescriptor(SecurityDescriptor,
        &SelfRelativeSecurityDescriptor, &SelfRelativeSecurityDescriptorSize);
    if (!NT_SUCCESS(Result))
        goto exit;

    ParamsBuf = Malloc(FSP_FSCTL_VOLUME_PARAMS_SIZE + SelfRelativeSecurityDescriptorSize);
    if (0 == ParamsBuf)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }
    memset(ParamsBuf, 0, FSP_FSCTL_VOLUME_PARAMS_SIZE);
    *ParamsBuf = *Params;
    memcpy((PUINT8)ParamsBuf + FSP_FSCTL_VOLUME_PARAMS_SIZE,
        SelfRelativeSecurityDescriptor, SelfRelativeSecurityDescriptorSize);

    GlobalDevicePath(DevicePathBuf, sizeof DevicePathBuf, DevicePath);

#if !defined(NDEBUG)
    {
        PSTR Sddl;
        if (ConvertSecurityDescriptorToStringSecurityDescriptorA(SelfRelativeSecurityDescriptor,
            SDDL_REVISION_1,
            OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
            &Sddl, 0))
        {
            DEBUGLOG("Device=\"%S\", Sddl=\"%s\", SdSize=%lu",
                DevicePathBuf, Sddl, SelfRelativeSecurityDescriptorSize);
            LocalFree(Sddl);
        }
    }
#endif

    DeviceHandle = CreateFileW(DevicePathBuf,
        0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == DeviceHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        if (STATUS_OBJECT_NAME_NOT_FOUND == Result)
            Result = STATUS_NO_SUCH_DEVICE;
        goto exit;
    }

    if (!DeviceIoControl(DeviceHandle, FSP_FSCTL_CREATE,
        ParamsBuf, FSP_FSCTL_VOLUME_PARAMS_SIZE + SelfRelativeSecurityDescriptorSize,
        VolumePathBuf, (DWORD)VolumePathSize,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

exit:

    if (INVALID_HANDLE_VALUE != DeviceHandle)
        CloseHandle(DeviceHandle);

    free(ParamsBuf);
    free(SelfRelativeSecurityDescriptor);

    return Result;
}

FSP_API NTSTATUS FspFsctlOpenVolume(PWSTR VolumePath,
    PHANDLE PVolumeHandle)
{
    NTSTATUS Result = STATUS_SUCCESS;
    WCHAR DevicePathBuf[MAX_PATH];
    HANDLE VolumeHandle;

    *PVolumeHandle = 0;

    GlobalDevicePath(DevicePathBuf, sizeof DevicePathBuf, VolumePath);

    DEBUGLOG("Device=\"%S\"", DevicePathBuf);

    VolumeHandle = CreateFileW(DevicePathBuf,
        0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == VolumeHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        if (STATUS_OBJECT_NAME_NOT_FOUND == Result)
            Result = STATUS_NO_SUCH_DEVICE;
        goto exit;
    }

    *PVolumeHandle = VolumeHandle;

exit:
    return Result;
}

FSP_API NTSTATUS FspFsctlDeleteVolume(HANDLE VolumeHandle)
{
    NTSTATUS Result = STATUS_SUCCESS;
    DWORD Bytes;

    if (!DeviceIoControl(VolumeHandle, FSP_FSCTL_DELETE,
        0, 0, 0, 0,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

exit:
    return Result;
}

FSP_API NTSTATUS FspFsctlTransact(HANDLE VolumeHandle,
    PVOID ResponseBuf, SIZE_T ResponseBufSize,
    PVOID RequestBuf, SIZE_T *PRequestBufSize)
{
    NTSTATUS Result = STATUS_SUCCESS;
    DWORD Bytes = 0;

    if (0 != *PRequestBufSize)
    {
        Bytes = (DWORD)*PRequestBufSize;
        *PRequestBufSize = 0;
    }

    if (!DeviceIoControl(VolumeHandle, FSP_FSCTL_TRANSACT,
        ResponseBuf, (DWORD)ResponseBufSize, RequestBuf, Bytes,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (0 != *PRequestBufSize)
        *PRequestBufSize = Bytes;

exit:
    return Result;
}
