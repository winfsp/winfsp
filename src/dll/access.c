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

static NTSTATUS FspGetSecurity(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, PDWORD PFileAttributes,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    for (;;)
    {
        NTSTATUS Result = FileSystem->Interface->GetSecurity(FileSystem,
            FileName, PFileAttributes, *PSecurityDescriptor, PSecurityDescriptorSize);
        if (STATUS_BUFFER_OVERFLOW != Result)
            return Result;

        MemFree(*PSecurityDescriptor);
        *PSecurityDescriptor = MemAlloc(*PSecurityDescriptorSize);
        if (0 == *PSecurityDescriptor)
            return STATUS_INSUFFICIENT_RESOURCES;
    }
}

FSP_API NTSTATUS FspAccessCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN AllowTraverseCheck, DWORD DesiredAccess,
    PDWORD PGrantedAccess)
{
    if (0 != FileSystem->Interface->AccessCheck)
        return FileSystem->Interface->AccessCheck(FileSystem,
            Request, AllowTraverseCheck, DesiredAccess, PGrantedAccess);

    if (0 == FileSystem->Interface->GetSecurity)
    {
        *PGrantedAccess = DesiredAccess;
        return STATUS_SUCCESS;
    }

    NTSTATUS Result;
    DWORD FileAttributes;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    SIZE_T SecurityDescriptorSize;
    DWORD PrivilegeSetLength;
    BOOL AccessStatus;

    *PGrantedAccess = 0;

    SecurityDescriptorSize = 1024;
    SecurityDescriptor = MemAlloc(SecurityDescriptorSize);
    if (0 == SecurityDescriptor)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (AllowTraverseCheck && !Request->Req.Create.HasTraversePrivilege)
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
            Result = FspGetSecurity(FileSystem, Prefix, &FileAttributes,
                &SecurityDescriptor, &SecurityDescriptorSize);

            FspPathCombine((PWSTR)Request->Buffer, Path);

            if (!NT_SUCCESS(Result))
            {
                if (STATUS_OBJECT_NAME_NOT_FOUND == Result)
                    Result = STATUS_OBJECT_PATH_NOT_FOUND;
                goto exit;
            }

            if (AccessCheck(SecurityDescriptor, (HANDLE)Request->Req.Create.AccessToken, FILE_TRAVERSE,
                &FspFileGenericMapping, 0, &PrivilegeSetLength, &TraverseAccess, &AccessStatus))
                Result = AccessStatus ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
            else
                Result = FspNtStatusFromWin32(GetLastError());

            if (!NT_SUCCESS(Result))
                goto exit;
        }
    }

    Result = FspGetSecurity(FileSystem, (PWSTR)Request->Buffer, &FileAttributes,
        &SecurityDescriptor, &SecurityDescriptorSize);
    if (!NT_SUCCESS(Result))
        goto exit;

    if (0 != (FileAttributes && FILE_ATTRIBUTE_READONLY))
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

    if (AccessCheck(SecurityDescriptor, (HANDLE)Request->Req.Create.AccessToken, DesiredAccess,
        &FspFileGenericMapping, 0, &PrivilegeSetLength, PGrantedAccess, &AccessStatus))
        Result = AccessStatus ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
    else
        Result = FspNtStatusFromWin32(GetLastError());

exit:
    MemFree(SecurityDescriptor);

    return Result;
}

FSP_API NTSTATUS FspShareCheck(FSP_FILE_SYSTEM *FileSystem,
    DWORD GrantedAccess, DWORD ShareAccess, FSP_FILE_NODE *FileNode)
{
    BOOLEAN ReadAccess, WriteAccess, DeleteAccess;
    BOOLEAN SharedRead, SharedWrite, SharedDelete;
    ULONG OpenCount;

    ReadAccess = 0 != (GrantedAccess & (FILE_READ_DATA | FILE_EXECUTE));
    WriteAccess = 0 != (GrantedAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA));
    DeleteAccess = 0 != (GrantedAccess & DELETE);

    if (ReadAccess || WriteAccess || DeleteAccess)
    {
        SharedRead = 0 != (ShareAccess & FILE_SHARE_READ);
        SharedWrite = 0 != (ShareAccess & FILE_SHARE_WRITE);
        SharedDelete = 0 != (ShareAccess & FILE_SHARE_DELETE);

        OpenCount = FileNode->ShareAccess.OpenCount;

        /*
         * IF ReadAccess AND there are already some exclusive readers
         * OR WriteAccess AND there are already some exclusive writers
         * OR DeleteAccess AND there are already some exclusive deleters
         * OR exclusive read requested AND there are already some readers
         * OR exclusive write requested AND there are already some writers
         * OR exclusive delete requested AND there are already some deleters
         */
        if (ReadAccess && (FileNode->ShareAccess.SharedRead < OpenCount))
            return STATUS_SHARING_VIOLATION;
        if (WriteAccess && (FileNode->ShareAccess.SharedWrite < OpenCount))
            return STATUS_SHARING_VIOLATION;
        if (DeleteAccess && (FileNode->ShareAccess.SharedDelete < OpenCount))
            return STATUS_SHARING_VIOLATION;
        if (!SharedRead && 0 != FileNode->ShareAccess.Readers)
            return STATUS_SHARING_VIOLATION;
        if (!SharedWrite && 0 != FileNode->ShareAccess.Writers)
            return STATUS_SHARING_VIOLATION;
        if (!SharedDelete && 0 != FileNode->ShareAccess.Deleters)
            return STATUS_SHARING_VIOLATION;

        FileNode->ShareAccess.OpenCount++;
        FileNode->ShareAccess.Readers += ReadAccess;
        FileNode->ShareAccess.Writers += WriteAccess;
        FileNode->ShareAccess.Deleters += DeleteAccess;
        FileNode->ShareAccess.SharedRead += SharedRead;
        FileNode->ShareAccess.SharedWrite += SharedWrite;
        FileNode->ShareAccess.SharedDelete += SharedDelete;
    }

    return STATUS_SUCCESS;
}
