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

static NTSTATUS FspGetFileSecurityDescriptor(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, PSECURITY_DESCRIPTOR *PSecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    for (;;)
    {
        NTSTATUS Result = FileSystem->QuerySecurity(FileSystem,
            FileName,
            OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
            *PSecurityDescriptor, PSecurityDescriptorSize);
        if (STATUS_BUFFER_OVERFLOW != Result)
            return Result;

        MemFree(*PSecurityDescriptor);
        *PSecurityDescriptor = MemAlloc(*PSecurityDescriptorSize);
        if (0 == *PSecurityDescriptor)
            return STATUS_INSUFFICIENT_RESOURCES;
    }
}

FSP_API NTSTATUS FspAccessCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, DWORD DesiredAccess, PDWORD PGrantedAccess)
{
    if (0 != FileSystem->AccessCheck)
        return FileSystem->AccessCheck(FileSystem, Request, DesiredAccess, PGrantedAccess);

    if (0 == FileSystem->QuerySecurity)
    {
        *PGrantedAccess = DesiredAccess;
        return STATUS_SUCCESS;
    }

    NTSTATUS Result;
    HANDLE AccessToken = 0;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    SIZE_T SecurityDescriptorSize;
    DWORD PrivilegeSetLength;
    BOOL AccessStatus;

    *PGrantedAccess = 0;

    Result = FspOpenAccessToken(FileSystem, Request, &AccessToken);
    if (!NT_SUCCESS(Result))
        goto exit;

    SecurityDescriptorSize = 1024;
    SecurityDescriptor = MemAlloc(SecurityDescriptorSize);
    if (0 == SecurityDescriptor)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (!Request->Req.Create.HasTraversePrivilege)
    {
        PWSTR Path = (PWSTR)Request->Buffer, Prefix;
        DWORD TraverseAccess;

        for (;;)
        {
            FspPathPrefix(Path, &Prefix, &Path);
            if (L'\0' == Path[0])
            {
                FspPathCombine((PWSTR)Request->Buffer, Path);
                break;
            }

            Prefix = L'\0' == Prefix[0] ? L"\\" : (PWSTR)Request->Buffer;
            Result = FspGetFileSecurityDescriptor(FileSystem, Prefix,
                &SecurityDescriptor, &SecurityDescriptorSize);

            FspPathCombine((PWSTR)Request->Buffer, Path);

            if (!NT_SUCCESS(Result))
                goto exit;

            if (AccessCheck(SecurityDescriptor, AccessToken, FILE_TRAVERSE,
                &FspFileGenericMapping, 0, &PrivilegeSetLength, &TraverseAccess, &AccessStatus))
                Result = AccessStatus ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
            else
                Result = FspNtStatusFromWin32(GetLastError());

            if (!NT_SUCCESS(Result))
                goto exit;
        }
    }

    Result = FspGetFileSecurityDescriptor(FileSystem, (PWSTR)Request->Buffer,
        &SecurityDescriptor, &SecurityDescriptorSize);
    if (!NT_SUCCESS(Result))
        goto exit;

    if (AccessCheck(SecurityDescriptor, AccessToken, DesiredAccess,
        &FspFileGenericMapping, 0, &PrivilegeSetLength, PGrantedAccess, &AccessStatus))
        Result = AccessStatus ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
    else
        Result = FspNtStatusFromWin32(GetLastError());

exit:

    MemFree(SecurityDescriptor);

    if (0 != AccessToken)
        CloseHandle(AccessToken);

    return Result;
}
