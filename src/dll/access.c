/**
 * @file dll/access.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
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

static NTSTATUS FspGetSecurityByName(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    for (;;)
    {
        NTSTATUS Result = FileSystem->Interface->GetSecurityByName(FileSystem,
            FileName, PFileAttributes, *PSecurityDescriptor, PSecurityDescriptorSize);
        if (STATUS_BUFFER_OVERFLOW != Result)
            return Result;

        MemFree(*PSecurityDescriptor);
        *PSecurityDescriptor = MemAlloc(*PSecurityDescriptorSize);
        if (0 == *PSecurityDescriptor)
            return STATUS_INSUFFICIENT_RESOURCES;
    }
}

FSP_API NTSTATUS FspAccessCheckEx(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    BOOLEAN CheckParentDirectory, BOOLEAN AllowTraverseCheck,
    UINT32 DesiredAccess, PUINT32 PGrantedAccess,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
{
    *PGrantedAccess = 0;
    if (0 != PSecurityDescriptor)
        *PSecurityDescriptor = 0;

    if (FspFsctlTransactCreateKind != Request->Kind)
        return STATUS_INVALID_PARAMETER;

    if (0 == FileSystem->Interface->GetSecurityByName ||
        (!Request->Req.Create.UserMode && 0 == PSecurityDescriptor))
    {
        *PGrantedAccess = (MAXIMUM_ALLOWED & DesiredAccess) ?
            FspFileGenericMapping.GenericAll : DesiredAccess;
        return STATUS_SUCCESS;
    }

    NTSTATUS Result;
    WCHAR Root[2] = L"\\", TraverseCheckRoot[2] = L"\\";
    PWSTR FileName, Suffix, Prefix, Remain;
    UINT32 FileAttributes;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    SIZE_T SecurityDescriptorSize;
    UINT8 PrivilegeSetBuf[sizeof(PRIVILEGE_SET) + 15 * sizeof(LUID_AND_ATTRIBUTES)];
    PPRIVILEGE_SET PrivilegeSet = (PVOID)PrivilegeSetBuf;
    DWORD PrivilegeSetLength = sizeof PrivilegeSetBuf;
    UINT32 TraverseAccess;
    BOOL AccessStatus;

    if (CheckParentDirectory)
        FspPathSuffix((PWSTR)Request->Buffer, &FileName, &Suffix, Root);
    else
        FileName = (PWSTR)Request->Buffer;

    SecurityDescriptorSize = 1024;
    SecurityDescriptor = MemAlloc(SecurityDescriptorSize);
    if (0 == SecurityDescriptor)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (Request->Req.Create.UserMode &&
        AllowTraverseCheck && !Request->Req.Create.HasTraversePrivilege)
    {
        Remain = (PWSTR)FileName;
        for (;;)
        {
            FspPathPrefix(Remain, &Prefix, &Remain, TraverseCheckRoot);
            if (L'\0' == Remain[0])
            {
                FspPathCombine(FileName, Remain);
                break;
            }

            Result = FspGetSecurityByName(FileSystem, Prefix, 0,
                &SecurityDescriptor, &SecurityDescriptorSize);

            FspPathCombine(FileName, Remain);

            if (!NT_SUCCESS(Result))
            {
                if (STATUS_OBJECT_NAME_NOT_FOUND == Result)
                    Result = STATUS_OBJECT_PATH_NOT_FOUND;
                goto exit;
            }

            if (0 < SecurityDescriptorSize)
            {
                if (AccessCheck(SecurityDescriptor, (HANDLE)Request->Req.Create.AccessToken, FILE_TRAVERSE,
                    &FspFileGenericMapping, PrivilegeSet, &PrivilegeSetLength, &TraverseAccess, &AccessStatus))
                    Result = AccessStatus ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
                else
                    Result = FspNtStatusFromWin32(GetLastError());
                if (!NT_SUCCESS(Result))
                    goto exit;
            }
        }
    }

    Result = FspGetSecurityByName(FileSystem, FileName, &FileAttributes,
        &SecurityDescriptor, &SecurityDescriptorSize);
    if (!NT_SUCCESS(Result))
        goto exit;

    if (Request->Req.Create.UserMode)
    {
        if (0 < SecurityDescriptorSize)
        {
            if (AccessCheck(SecurityDescriptor, (HANDLE)Request->Req.Create.AccessToken, DesiredAccess,
                &FspFileGenericMapping, PrivilegeSet, &PrivilegeSetLength, PGrantedAccess, &AccessStatus))
                Result = AccessStatus ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
            else
                Result = FspNtStatusFromWin32(GetLastError());
            if (!NT_SUCCESS(Result))
                goto exit;
        }

        if (CheckParentDirectory)
        {
            if (0 == (FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                Result = STATUS_NOT_A_DIRECTORY;
                goto exit;
            }
        }
        else
        {
            if ((Request->Req.Create.CreateOptions & FILE_DIRECTORY_FILE) &&
                0 == (FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                Result = STATUS_NOT_A_DIRECTORY;
                goto exit;
            }
            if ((Request->Req.Create.CreateOptions & FILE_NON_DIRECTORY_FILE) &&
                0 != (FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                Result = STATUS_FILE_IS_A_DIRECTORY;
                goto exit;
            }
        }

        if (0 != (FileAttributes & FILE_ATTRIBUTE_READONLY))
        {
            if (DesiredAccess &
                (FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_ADD_SUBDIRECTORY | FILE_DELETE_CHILD))
            {
                Result = STATUS_ACCESS_DENIED;
                goto exit;
            }
            if (Request->Req.Create.CreateOptions & FILE_DELETE_ON_CLOSE)
            {
                Result = STATUS_CANNOT_DELETE;
                goto exit;
            }
        }

        if (0 == SecurityDescriptorSize)
            *PGrantedAccess = (MAXIMUM_ALLOWED & DesiredAccess) ?
                FspFileGenericMapping.GenericAll : DesiredAccess;
    }
    else
        *PGrantedAccess = (MAXIMUM_ALLOWED & DesiredAccess) ?
            FspFileGenericMapping.GenericAll : DesiredAccess;

    Result = STATUS_SUCCESS;

exit:
    if (0 != PSecurityDescriptor && 0 < SecurityDescriptorSize && NT_SUCCESS(Result))
        *PSecurityDescriptor = SecurityDescriptor;
    else
        MemFree(SecurityDescriptor);

    if (CheckParentDirectory)
    {
        FspPathCombine((PWSTR)Request->Buffer, Suffix);

        if (STATUS_OBJECT_NAME_NOT_FOUND == Result)
            Result = STATUS_OBJECT_PATH_NOT_FOUND;
    }

    return Result;
}

FSP_API NTSTATUS FspCreateSecurityDescriptor(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PSECURITY_DESCRIPTOR ParentDescriptor,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
{
    *PSecurityDescriptor = 0;

    if (FspFsctlTransactCreateKind != Request->Kind)
        return STATUS_INVALID_PARAMETER;

    if (!CreatePrivateObjectSecurity(
        ParentDescriptor,
        0 != Request->Req.Create.SecurityDescriptor.Offset ?
            (PSECURITY_DESCRIPTOR)(Request->Buffer + Request->Req.Create.SecurityDescriptor.Offset) : 0,
        PSecurityDescriptor,
        0 != (Request->Req.Create.CreateOptions & FILE_DIRECTORY_FILE),
        (HANDLE)Request->Req.Create.AccessToken,
        &FspFileGenericMapping))
        return FspNtStatusFromWin32(GetLastError());

    //DEBUGLOGSD("SDDL=%s", *PSecurityDescriptor);

    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspSetSecurityDescriptor(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PSECURITY_DESCRIPTOR InputDescriptor,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
{
    *PSecurityDescriptor = 0;

    if (FspFsctlTransactSetSecurityKind != Request->Kind)
        return STATUS_INVALID_PARAMETER;

    if (0 == InputDescriptor)
        return STATUS_NO_SECURITY_ON_OBJECT;

    /*
     * SetPrivateObjectSecurity is a broken API. It assumes that the passed
     * descriptor resides on memory allocated by CreatePrivateObjectSecurity
     * or SetPrivateObjectSecurity and frees the descriptor on success.
     *
     * In our case the security descriptor comes from the user mode file system,
     * which may conjure it any way it sees fit. So we have to somehow make a copy
     * of the InputDescriptor and place it in memory that SetPrivateObjectSecurity
     * can then free. To complicate matters there is no API that can be used for
     * this purpose. What a PITA!
     */
    /* !!!: HACK! HACK! HACK!
     *
     * Turns out that SetPrivateObjectSecurity and friends really use RtlProcessHeap
     * internally, which is just another name for GetProcessHeap().
     *
     * I wish there was a cleaner way to do this!
     */

    HANDLE ProcessHeap = GetProcessHeap();
    DWORD InputDescriptorSize = GetSecurityDescriptorLength(InputDescriptor);
    PSECURITY_DESCRIPTOR CopiedDescriptor;

    CopiedDescriptor = HeapAlloc(ProcessHeap, 0, InputDescriptorSize);
    if (0 == CopiedDescriptor)
        return STATUS_INSUFFICIENT_RESOURCES;
    memcpy(CopiedDescriptor, InputDescriptor, InputDescriptorSize);
    InputDescriptor = CopiedDescriptor;

    if (!SetPrivateObjectSecurity(
        Request->Req.SetSecurity.SecurityInformation,
        (PVOID)Request->Buffer,
        &InputDescriptor,
        &FspFileGenericMapping,
        (HANDLE)Request->Req.SetSecurity.AccessToken))
    {
        HeapFree(ProcessHeap, 0, CopiedDescriptor);
        return FspNtStatusFromWin32(GetLastError());
    }

    /* CopiedDescriptor has been freed by SetPrivateObjectSecurity! */

    *PSecurityDescriptor = InputDescriptor;

    //DEBUGLOGSD("SDDL=%s", *PSecurityDescriptor);

    return STATUS_SUCCESS;
}

FSP_API VOID FspDeleteSecurityDescriptor(PSECURITY_DESCRIPTOR SecurityDescriptor,
    NTSTATUS (*CreateFunc)())
{
    if ((NTSTATUS (*)())FspAccessCheckEx == CreateFunc)
        MemFree(SecurityDescriptor);
    else
    if ((NTSTATUS (*)())FspCreateSecurityDescriptor == CreateFunc ||
        (NTSTATUS (*)())FspSetSecurityDescriptor == CreateFunc)
        DestroyPrivateObjectSecurity(&SecurityDescriptor);
}
