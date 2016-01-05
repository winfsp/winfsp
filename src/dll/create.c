/**
 * @file dll/create.c
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
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN CheckParentDirectory, BOOLEAN AllowTraverseCheck,
    DWORD DesiredAccess, PDWORD PGrantedAccess)
{
    if (0 == FileSystem->Interface->GetSecurity)
    {
        *PGrantedAccess = DesiredAccess;
        return STATUS_SUCCESS;
    }

    NTSTATUS Result;
    PWSTR Parent, Suffix, Prefix, Remain;
    DWORD FileAttributes;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    SIZE_T SecurityDescriptorSize;
    DWORD PrivilegeSetLength;
    DWORD TraverseAccess;
    BOOL AccessStatus;

    *PGrantedAccess = 0;

    if (CheckParentDirectory)
        FspPathSuffix((PWSTR)Request->Buffer, &Parent, &Suffix);

    SecurityDescriptorSize = 1024;
    SecurityDescriptor = MemAlloc(SecurityDescriptorSize);
    if (0 == SecurityDescriptor)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (AllowTraverseCheck && !Request->Req.Create.HasTraversePrivilege)
    {
        Remain = (PWSTR)Request->Buffer;
        for (;;)
        {
            FspPathPrefix(Remain, &Prefix, &Remain);
            if (L'\0' == Remain[0])
            {
                FspPathCombine((PWSTR)Request->Buffer, Remain);
                break;
            }

            Prefix = L'\0' == Prefix[0] ? L"\\" : (PWSTR)Request->Buffer;
            Result = FspGetSecurity(FileSystem, Prefix, 0,
                &SecurityDescriptor, &SecurityDescriptorSize);

            FspPathCombine((PWSTR)Request->Buffer, Remain);

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

    if (AccessCheck(SecurityDescriptor, (HANDLE)Request->Req.Create.AccessToken, DesiredAccess,
        &FspFileGenericMapping, 0, &PrivilegeSetLength, PGrantedAccess, &AccessStatus))
        Result = AccessStatus ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
    else
        Result = FspNtStatusFromWin32(GetLastError());
    if (!NT_SUCCESS(Result))
        goto exit;

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

exit:
    MemFree(SecurityDescriptor);

    if (CheckParentDirectory)
        FspPathCombine((PWSTR)Request->Buffer, Suffix);

    return Result;
}

FSP_API NTSTATUS FspShareCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, DWORD GrantedAccess, FSP_FILE_NODE *FileNode)
{
    DWORD ShareAccess = Request->Req.Create.ShareAccess;
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

FSP_API NTSTATUS FspFileSystemPreCreateCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN AllowTraverseCheck, PDWORD PGrantedAccess)
{
    NTSTATUS Result;

    Result = FspAccessCheck(FileSystem, Request, TRUE, AllowTraverseCheck,
        (Request->Req.Create.CreateOptions & FILE_DIRECTORY_FILE) ?
            FILE_ADD_SUBDIRECTORY : FILE_ADD_FILE,
        PGrantedAccess);
    if (NT_SUCCESS(Result))
        *PGrantedAccess = (MAXIMUM_ALLOWED & Request->Req.Create.DesiredAccess) ?
            FILE_ALL_ACCESS : Request->Req.Create.DesiredAccess;

    return Result;
}

FSP_API VOID FspFileSystemPostCreateCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, DWORD GrantedAccess, FSP_FILE_NODE *FileNode)
{
    FspFileNodeLock(FileNode);

    FspShareCheck(FileSystem, Request, GrantedAccess, FileNode);

    if (Request->Req.Create.CreateOptions & FILE_DELETE_ON_CLOSE)
        FileNode->Flags.DeleteOnClose = TRUE;

    FspFileNodeOpen(FileNode);

    FspFileNodeUnlock(FileNode);
}

FSP_API NTSTATUS FspFileSystemPreOpenCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN AllowTraverseCheck, PDWORD PGrantedAccess)
{
    return FspAccessCheck(FileSystem, Request, FALSE, AllowTraverseCheck,
        Request->Req.Create.DesiredAccess, PGrantedAccess);
}

FSP_API NTSTATUS FspFileSystemPostOpenCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, DWORD GrantedAccess, FSP_FILE_NODE *FileNode)
{
    NTSTATUS Result;

    FspFileNodeLock(FileNode);

    if (FileNode->Flags.DeletePending)
    {
        Result = STATUS_DELETE_PENDING;
        goto exit;
    }

    Result = FspShareCheck(FileSystem, Request, GrantedAccess, FileNode);
    if (!NT_SUCCESS(Result))
        goto exit;

    if (Request->Req.Create.CreateOptions & FILE_DELETE_ON_CLOSE)
        FileNode->Flags.DeleteOnClose = TRUE;

    FspFileNodeOpen(FileNode);

exit:
    FspFileNodeUnlock(FileNode);

    return Result;
}

FSP_API NTSTATUS FspFileSystemPreOverwriteCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN AllowTraverseCheck, PDWORD PGrantedAccess)
{
    NTSTATUS Result;
    BOOLEAN Supersede = FILE_SUPERSEDE == ((Request->Req.Create.CreateOptions >> 24) & 0xff);

    Result = FspAccessCheck(FileSystem, Request, FALSE, AllowTraverseCheck,
        Request->Req.Create.DesiredAccess | (Supersede ? DELETE : FILE_WRITE_DATA),
        PGrantedAccess);
    if (NT_SUCCESS(Result))
    {
        if (0 == (Request->Req.Create.DesiredAccess & MAXIMUM_ALLOWED))
            *PGrantedAccess &= Supersede ?
                (~DELETE | (Request->Req.Create.DesiredAccess & DELETE)) :
                (~FILE_WRITE_DATA | (Request->Req.Create.DesiredAccess & FILE_WRITE_DATA));
    }

    return Result;
}

FSP_API NTSTATUS FspFileSystemPostOverwriteCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, DWORD GrantedAccess, FSP_FILE_NODE *FileNode)
{
    return FspFileSystemPostOpenCheck(FileSystem, Request, GrantedAccess, FileNode);
}

static BOOLEAN FspIsRootDirectory(PWSTR FileName)
{
    for (PWSTR Pointer = FileName; *Pointer; Pointer++)
        if (L'\\' != *Pointer)
            return FALSE;
    return TRUE;
}

static NTSTATUS FspFileSystemOpCreate_FileCreate(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    DWORD GrantedAccess;
    FSP_FILE_NODE *FileNode;

    if (FspIsRootDirectory((PWSTR)Request->Buffer))
        return STATUS_ACCESS_DENIED;

    Result = FspFileSystemPreCreateCheck(FileSystem, Request, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    Result = FileSystem->Interface->Create(FileSystem, Request, &FileNode);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    FspFileSystemPostCreateCheck(FileSystem, Request, GrantedAccess, FileNode);

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        FILE_CREATED, FileNode, GrantedAccess);
}

static NTSTATUS FspFileSystemOpCreate_FileOpen(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    DWORD GrantedAccess;
    FSP_FILE_NODE *FileNode;

    Result = FspFileSystemPreOpenCheck(FileSystem, Request, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    Result = FileSystem->Interface->Open(FileSystem, Request, &FileNode);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    Result = FspFileSystemPostOpenCheck(FileSystem, Request, GrantedAccess, FileNode);
    if (!NT_SUCCESS(Result))
    {
        if (0 != FileSystem->Interface->Close)
            FileSystem->Interface->Close(FileSystem, Request, FileNode);
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
    }

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        FILE_OPENED, FileNode, GrantedAccess);
}

static NTSTATUS FspFileSystemOpCreate_FileOpenIf(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    DWORD GrantedAccess;
    FSP_FILE_NODE *FileNode;
    BOOLEAN Create = FALSE;

    Result = FspFileSystemPreOpenCheck(FileSystem, Request, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result))
    {
        if (STATUS_OBJECT_NAME_NOT_FOUND != Result)
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
        Create = TRUE;
    }

    if (!Create)
    {
        Result = FileSystem->Interface->Open(FileSystem, Request, &FileNode);
        if (!NT_SUCCESS(Result))
        {
            if (STATUS_OBJECT_NAME_NOT_FOUND != Result)
                return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
            Create = TRUE;
        }
        else
        {
            Result = FspFileSystemPostOpenCheck(FileSystem, Request, GrantedAccess, FileNode);
            if (!NT_SUCCESS(Result))
            {
                if (0 != FileSystem->Interface->Close)
                    FileSystem->Interface->Close(FileSystem, Request, FileNode);
                return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
            }
        }
    }

    if (Create)
    {
        if (FspIsRootDirectory((PWSTR)Request->Buffer))
            return STATUS_ACCESS_DENIED;

        Result = FspFileSystemPreCreateCheck(FileSystem, Request, FALSE, &GrantedAccess);
        if (!NT_SUCCESS(Result))
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

        Result = FileSystem->Interface->Create(FileSystem, Request, &FileNode);
        if (!NT_SUCCESS(Result))
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

        FspFileSystemPostCreateCheck(FileSystem, Request, GrantedAccess, FileNode);
    }

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        Create ? FILE_CREATED : FILE_OPENED, FileNode, GrantedAccess);
}

static NTSTATUS FspFileSystemOpCreate_FileOverwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    DWORD GrantedAccess;
    FSP_FILE_NODE *FileNode;
    BOOLEAN Supersede = FILE_SUPERSEDE == ((Request->Req.Create.CreateOptions >> 24) & 0xff);

    if (FspIsRootDirectory((PWSTR)Request->Buffer))
        return STATUS_ACCESS_DENIED;

    Result = FspFileSystemPreOverwriteCheck(FileSystem, Request, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    Result = FileSystem->Interface->Open(FileSystem, Request, &FileNode);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    Result = FspFileSystemPostOverwriteCheck(FileSystem, Request, GrantedAccess, FileNode);
    if (!NT_SUCCESS(Result))
    {
        if (0 != FileSystem->Interface->Close)
            FileSystem->Interface->Close(FileSystem, Request, FileNode);
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
    }

    Result = FileSystem->Interface->Overwrite(FileSystem, Request, Supersede, FileNode);
    if (!NT_SUCCESS(Result))
    {
        if (0 != FileSystem->Interface->Close)
            FileSystem->Interface->Close(FileSystem, Request, FileNode);
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
    }

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        Supersede ? FILE_SUPERSEDED : FILE_OVERWRITTEN, FileNode, GrantedAccess);
}

static NTSTATUS FspFileSystemOpCreate_FileOverwriteIf(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    DWORD GrantedAccess;
    FSP_FILE_NODE *FileNode;
    BOOLEAN Create = FALSE;

    if (FspIsRootDirectory((PWSTR)Request->Buffer))
        return STATUS_ACCESS_DENIED;

    Result = FspFileSystemPreOverwriteCheck(FileSystem, Request, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result))
    {
        if (STATUS_OBJECT_NAME_NOT_FOUND != Result)
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
        Create = TRUE;
    }

    if (!Create)
    {
        Result = FileSystem->Interface->Open(FileSystem, Request, &FileNode);
        if (!NT_SUCCESS(Result))
        {
            if (STATUS_OBJECT_NAME_NOT_FOUND != Result)
                return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
            Create = TRUE;
        }
        else
        {
            Result = FspFileSystemPostOverwriteCheck(FileSystem, Request, GrantedAccess, FileNode);
            if (!NT_SUCCESS(Result))
            {
                if (0 != FileSystem->Interface->Close)
                    FileSystem->Interface->Close(FileSystem, Request, FileNode);
                return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
            }

            Result = FileSystem->Interface->Overwrite(FileSystem, Request, FALSE, FileNode);
            if (!NT_SUCCESS(Result))
            {
                if (0 != FileSystem->Interface->Close)
                    FileSystem->Interface->Close(FileSystem, Request, FileNode);
                return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
            }
        }
    }

    if (Create)
    {
        Result = FspFileSystemPreCreateCheck(FileSystem, Request, FALSE, &GrantedAccess);
        if (!NT_SUCCESS(Result))
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

        Result = FileSystem->Interface->Create(FileSystem, Request, &FileNode);
        if (!NT_SUCCESS(Result))
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

        FspFileSystemPostCreateCheck(FileSystem, Request, GrantedAccess, FileNode);
    }

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        Create ? FILE_CREATED : FILE_OVERWRITTEN, FileNode, GrantedAccess);
}

static NTSTATUS FspFileSystemOpCreate_FileOpenTargetDirectory(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    PWSTR Parent, Suffix;
    DWORD GrantedAccess;
    FSP_FILE_NODE *FileNode;
    UINT_PTR Information;

    if (FspIsRootDirectory((PWSTR)Request->Buffer))
        return STATUS_ACCESS_DENIED;

    Result = FspAccessCheck(FileSystem, Request, TRUE, TRUE,
        Request->Req.Create.DesiredAccess, &GrantedAccess);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    FspPathSuffix((PWSTR)Request->Buffer, &Parent, &Suffix);
    Result = FileSystem->Interface->Open(FileSystem, Request, &FileNode);
    FspPathCombine((PWSTR)Request->Buffer, Suffix);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    Result = FspFileSystemPostOpenCheck(FileSystem, Request, GrantedAccess, FileNode);
    if (!NT_SUCCESS(Result))
    {
        if (0 != FileSystem->Interface->Close)
            FileSystem->Interface->Close(FileSystem, Request, FileNode);
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
    }

    Information = FILE_OPENED;
    if (0 != FileSystem->Interface->GetSecurity)
    {
        Result = FileSystem->Interface->GetSecurity(FileSystem, (PWSTR)Request->Buffer, 0, 0, 0);
        Information = NT_SUCCESS(Result) ? FILE_EXISTS : FILE_DOES_NOT_EXIST;
    }

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        Information, FileNode, GrantedAccess);
}

FSP_API NTSTATUS FspFileSystemOpCreate(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    if (0 == FileSystem->Interface->Create ||
        0 == FileSystem->Interface->Open ||
        0 == FileSystem->Interface->Overwrite)
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, STATUS_INVALID_DEVICE_REQUEST);

    if (Request->Req.Create.OpenTargetDirectory)
        return FspFileSystemOpCreate_FileOpenTargetDirectory(FileSystem, Request);

    switch ((Request->Req.Create.CreateOptions >> 24) & 0xff)
    {
    case FILE_CREATE:
        return FspFileSystemOpCreate_FileCreate(FileSystem, Request);
    case FILE_OPEN:
        return FspFileSystemOpCreate_FileOpen(FileSystem, Request);
    case FILE_OPEN_IF:
        return FspFileSystemOpCreate_FileOpenIf(FileSystem, Request);
    case FILE_OVERWRITE:
        return FspFileSystemOpCreate_FileOverwrite(FileSystem, Request);
    case FILE_SUPERSEDE:
        return FspFileSystemOpCreate_FileOverwrite(FileSystem, Request);
    case FILE_OVERWRITE_IF:
        return FspFileSystemOpCreate_FileOverwriteIf(FileSystem, Request);
    default:
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, STATUS_INVALID_PARAMETER);
    }
}

FSP_API NTSTATUS FspFileSystemSendCreateResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, UINT_PTR Information,
    FSP_FILE_NODE *FileNode, DWORD GrantedAccess)
{
    FSP_FSCTL_TRANSACT_RSP Response;

    memset(&Response, 0, sizeof Response);
    Response.Size = sizeof Response;
    Response.Kind = Request->Kind;
    Response.Hint = Request->Hint;
    Response.IoStatus.Status = STATUS_SUCCESS;
    Response.IoStatus.Information = Information;
    Response.Rsp.Create.Opened.UserContext = (UINT_PTR)FileNode;
    Response.Rsp.Create.Opened.AllocationSize = FileNode->AllocationSize;
    Response.Rsp.Create.Opened.FileSize = FileNode->FileSize;
    Response.Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    return FspFileSystemSendResponse(FileSystem, &Response);
}
