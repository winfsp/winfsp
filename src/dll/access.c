/**
 * @file dll/access.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>

static GENERIC_MAPPING FspFileGenericMapping =
{
    .GenericRead = FILE_GENERIC_READ,
    .GenericWrite = FILE_GENERIC_WRITE,
    .GenericExecute = FILE_GENERIC_EXECUTE,
    .GenericAll = FILE_ALL_ACCESS,
};

FSP_API PGENERIC_MAPPING FspGetFileGenericMapping(VOID)
{
    return &FspFileGenericMapping;
}

FSP_API NTSTATUS FspOpenAccessToken(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, PHANDLE PAccessToken)
{
    return FspFsctlOpenAccessToken(FileSystem->VolumeHandle, Request->Hint, PAccessToken);
}

#if 0
FSP_API NTSTATUS FspAccessCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, PUINT32 PGrantedAccess)
{
    if (0 != FileSystem->AccessCheck)
        return FileSystem->AccessCheck(FileSystem, Request, PGrantedAccess);

    NTSTATUS Result;
    PWSTR FileName = (PVOID)Request->Buffer;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    HANDLE AccessToken = 0;
    DWORD PrivilegeSetLength;
    BOOLEAN AccessStatus;
    s
    *PGrantedAccess = 0;

    SecurityDescriptor = MemAlloc(1024);
    if (0 == SecurityDescriptor)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    Result = FspGetSecurityDescriptor();

    Result = FspOpenAccessToken(FileSystem, Request, &AccessToken);
    if (!NT_SUCCESS(Result))
        goto exit;

    if (AccessCheck(&SecurityDescriptor, AccessToken, Request->Req.Create.DesiredAccess,
        &FspFileGenericMapping, 0, &PrivilegeSetLength, PGrantedAccess, &AccessStatus))
        Result = AccessStatus ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
    else
        Result = FspNtStatusFromWin32(GetLastError());

exit:
    if (0 != AccessToken)
        CloseHandle(AccessToken);
    MemFree(SecurityDescriptor);

    return Result;
}
#endif
