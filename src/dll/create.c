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
    PWSTR FileName, PUINT32 PFileAttributes,
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

FSP_API NTSTATUS FspAccessCheckEx(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    BOOLEAN CheckParentDirectory, BOOLEAN AllowTraverseCheck,
    UINT32 DesiredAccess, PUINT32 PGrantedAccess,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
{
    *PGrantedAccess = 0;
    if (0 != PSecurityDescriptor)
        *PSecurityDescriptor = 0;

    if (0 == FileSystem->Interface->GetSecurity ||
        (!Request->Req.Create.UserMode && 0 == PSecurityDescriptor))
    {
        *PGrantedAccess = (MAXIMUM_ALLOWED & DesiredAccess) ?
            FspFileGenericMapping.GenericAll : DesiredAccess;
        return STATUS_SUCCESS;
    }

    NTSTATUS Result;
    PWSTR Parent, Suffix, Prefix, Remain;
    UINT32 FileAttributes;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    SIZE_T SecurityDescriptorSize;
    UINT8 PrivilegeSetBuf[sizeof(PRIVILEGE_SET) + 15 * sizeof(LUID_AND_ATTRIBUTES)];
    PPRIVILEGE_SET PrivilegeSet = (PVOID)PrivilegeSetBuf;
    DWORD PrivilegeSetLength = sizeof PrivilegeSetBuf;
    UINT32 TraverseAccess;
    BOOL AccessStatus;

    if (CheckParentDirectory)
        FspPathSuffix((PWSTR)Request->Buffer, &Parent, &Suffix);

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

    Result = FspGetSecurity(FileSystem, (PWSTR)Request->Buffer, &FileAttributes,
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

FSP_API NTSTATUS FspAssignSecurity(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PSECURITY_DESCRIPTOR ParentDescriptor,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
{
    *PSecurityDescriptor = 0;

    if (!CreatePrivateObjectSecurity(
        ParentDescriptor,
        0 != Request->Req.Create.SecurityDescriptor.Offset ?
            (PSECURITY_DESCRIPTOR)(Request->Buffer + Request->Req.Create.SecurityDescriptor.Offset) : 0,
        PSecurityDescriptor,
        0 != (Request->Req.Create.CreateOptions & FILE_DIRECTORY_FILE),
        (HANDLE)Request->Req.Create.AccessToken,
        &FspFileGenericMapping))
        return FspNtStatusFromWin32(GetLastError());

    DEBUGLOGSD("SDDL=%s", *PSecurityDescriptor);

    return STATUS_SUCCESS;
}

FSP_API VOID FspDeleteSecurityDescriptor(PSECURITY_DESCRIPTOR SecurityDescriptor,
    NTSTATUS(*CreateFunc)())
{
    if ((NTSTATUS (*)())FspAccessCheckEx == CreateFunc)
        MemFree(SecurityDescriptor);
    else if ((NTSTATUS (*)())FspAssignSecurity == CreateFunc)
        DestroyPrivateObjectSecurity(&SecurityDescriptor);
}

static inline
NTSTATUS FspFileSystemCreateCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    BOOLEAN AllowTraverseCheck, PUINT32 PGrantedAccess,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
{
    NTSTATUS Result;

    Result = FspAccessCheckEx(FileSystem, Request, TRUE, AllowTraverseCheck,
        (Request->Req.Create.CreateOptions & FILE_DIRECTORY_FILE) ?
            FILE_ADD_SUBDIRECTORY : FILE_ADD_FILE,
        PGrantedAccess, PSecurityDescriptor);
    if (NT_SUCCESS(Result))
    {
        *PGrantedAccess = (MAXIMUM_ALLOWED & Request->Req.Create.DesiredAccess) ?
            FspFileGenericMapping.GenericAll : Request->Req.Create.DesiredAccess;
    }

    return Result;
}

static inline
NTSTATUS FspFileSystemOpenCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    BOOLEAN AllowTraverseCheck, PUINT32 PGrantedAccess)
{
    NTSTATUS Result;

    Result = FspAccessCheck(FileSystem, Request, FALSE, AllowTraverseCheck,
        Request->Req.Create.DesiredAccess |
            ((Request->Req.Create.CreateOptions & FILE_DELETE_ON_CLOSE) ? DELETE : 0),
        PGrantedAccess);
    if (NT_SUCCESS(Result))
    {
        if (0 == (Request->Req.Create.DesiredAccess & MAXIMUM_ALLOWED))
            *PGrantedAccess &= ~DELETE | (Request->Req.Create.DesiredAccess & DELETE);
    }

    return Result;
}

static inline
NTSTATUS FspFileSystemOverwriteCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    BOOLEAN AllowTraverseCheck, PUINT32 PGrantedAccess)
{
    NTSTATUS Result;
    BOOLEAN Supersede = FILE_SUPERSEDE == ((Request->Req.Create.CreateOptions >> 24) & 0xff);

    Result = FspAccessCheck(FileSystem, Request, FALSE, AllowTraverseCheck,
        Request->Req.Create.DesiredAccess |
            (Supersede ? DELETE : FILE_WRITE_DATA) |
            ((Request->Req.Create.CreateOptions & FILE_DELETE_ON_CLOSE) ? DELETE : 0),
        PGrantedAccess);
    if (NT_SUCCESS(Result))
    {
        if (0 == (Request->Req.Create.DesiredAccess & MAXIMUM_ALLOWED))
            *PGrantedAccess &= ~(DELETE | FILE_WRITE_DATA) |
                (Request->Req.Create.DesiredAccess & (DELETE | FILE_WRITE_DATA));
    }

    return Result;
}

static NTSTATUS FspFileSystemOpCreate_FileCreate(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;
    PSECURITY_DESCRIPTOR ParentDescriptor, ObjectDescriptor;
    PVOID FileNode;
    FSP_FSCTL_FILE_INFO FileInfo;

    Result = FspFileSystemCreateCheck(FileSystem, Request, TRUE, &GrantedAccess, &ParentDescriptor);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    Result = FspAssignSecurity(FileSystem, Request, ParentDescriptor, &ObjectDescriptor);
    FspDeleteSecurityDescriptor(ParentDescriptor, FspAccessCheckEx);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    FileNode = 0;
    memset(&FileInfo, 0, sizeof FileInfo);
    Result = FileSystem->Interface->Create(FileSystem, Request,
        (PWSTR)Request->Buffer, Request->Req.Create.CaseSensitive, Request->Req.Create.CreateOptions,
        Request->Req.Create.FileAttributes, ObjectDescriptor, Request->Req.Create.AllocationSize,
        &FileNode, &FileInfo);
    FspDeleteSecurityDescriptor(ObjectDescriptor, FspAssignSecurity);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        FILE_CREATED, FileNode, GrantedAccess, &FileInfo);
}

static NTSTATUS FspFileSystemOpCreate_FileOpen(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;
    PVOID FileNode;
    FSP_FSCTL_FILE_INFO FileInfo;

    Result = FspFileSystemOpenCheck(FileSystem, Request, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    FileNode = 0;
    memset(&FileInfo, 0, sizeof FileInfo);
    Result = FileSystem->Interface->Open(FileSystem, Request,
        (PWSTR)Request->Buffer, Request->Req.Create.CaseSensitive, Request->Req.Create.CreateOptions,
        &FileNode, &FileInfo);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        FILE_OPENED, FileNode, GrantedAccess, &FileInfo);
}

static NTSTATUS FspFileSystemOpCreate_FileOpenIf(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;
    PSECURITY_DESCRIPTOR ParentDescriptor, ObjectDescriptor;
    PVOID FileNode;
    FSP_FSCTL_FILE_INFO FileInfo;
    BOOLEAN Create = FALSE;

    Result = FspFileSystemOpenCheck(FileSystem, Request, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result))
    {
        if (STATUS_OBJECT_NAME_NOT_FOUND != Result)
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
        Create = TRUE;
    }

    if (!Create)
    {
        FileNode = 0;
        memset(&FileInfo, 0, sizeof FileInfo);
        Result = FileSystem->Interface->Open(FileSystem, Request,
            (PWSTR)Request->Buffer, Request->Req.Create.CaseSensitive, Request->Req.Create.CreateOptions,
            &FileNode, &FileInfo);
        if (!NT_SUCCESS(Result))
        {
            if (STATUS_OBJECT_NAME_NOT_FOUND != Result)
                return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
            Create = TRUE;
        }
    }

    if (Create)
    {
        Result = FspFileSystemCreateCheck(FileSystem, Request, FALSE, &GrantedAccess, &ParentDescriptor);
        if (!NT_SUCCESS(Result))
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

        Result = FspAssignSecurity(FileSystem, Request, ParentDescriptor, &ObjectDescriptor);
        FspDeleteSecurityDescriptor(ParentDescriptor, FspAccessCheckEx);
        if (!NT_SUCCESS(Result))
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

        FileNode = 0;
        memset(&FileInfo, 0, sizeof FileInfo);
        Result = FileSystem->Interface->Create(FileSystem, Request,
            (PWSTR)Request->Buffer, Request->Req.Create.CaseSensitive, Request->Req.Create.CreateOptions,
            Request->Req.Create.FileAttributes, ObjectDescriptor, Request->Req.Create.AllocationSize,
            &FileNode, &FileInfo);
        FspDeleteSecurityDescriptor(ObjectDescriptor, FspAssignSecurity);
        if (!NT_SUCCESS(Result))
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
    }

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        Create ? FILE_CREATED : FILE_OPENED, FileNode, GrantedAccess, &FileInfo);
}

static NTSTATUS FspFileSystemOpCreate_FileOverwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;
    PVOID FileNode;
    FSP_FSCTL_FILE_INFO FileInfo;
    BOOLEAN Supersede = FILE_SUPERSEDE == ((Request->Req.Create.CreateOptions >> 24) & 0xff);

    Result = FspFileSystemOverwriteCheck(FileSystem, Request, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    FileNode = 0;
    memset(&FileInfo, 0, sizeof FileInfo);
    Result = FileSystem->Interface->Open(FileSystem, Request,
        (PWSTR)Request->Buffer, Request->Req.Create.CaseSensitive, Request->Req.Create.CreateOptions,
        &FileNode, &FileInfo);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        Supersede ? FILE_SUPERSEDED : FILE_OVERWRITTEN, FileNode, GrantedAccess, &FileInfo);
}

static NTSTATUS FspFileSystemOpCreate_FileOverwriteIf(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;
    PSECURITY_DESCRIPTOR ParentDescriptor, ObjectDescriptor;
    PVOID FileNode;
    FSP_FSCTL_FILE_INFO FileInfo;
    BOOLEAN Create = FALSE;

    Result = FspFileSystemOverwriteCheck(FileSystem, Request, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result))
    {
        if (STATUS_OBJECT_NAME_NOT_FOUND != Result)
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
        Create = TRUE;
    }

    if (!Create)
    {
        FileNode = 0;
        memset(&FileInfo, 0, sizeof FileInfo);
        Result = FileSystem->Interface->Open(FileSystem, Request,
            (PWSTR)Request->Buffer, Request->Req.Create.CaseSensitive, Request->Req.Create.CreateOptions,
            &FileNode, &FileInfo);
        if (!NT_SUCCESS(Result))
        {
            if (STATUS_OBJECT_NAME_NOT_FOUND != Result)
                return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
            Create = TRUE;
        }
    }

    if (Create)
    {
        Result = FspFileSystemCreateCheck(FileSystem, Request, FALSE, &GrantedAccess, &ParentDescriptor);
        if (!NT_SUCCESS(Result))
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

        Result = FspAssignSecurity(FileSystem, Request, ParentDescriptor, &ObjectDescriptor);
        FspDeleteSecurityDescriptor(ParentDescriptor, FspAccessCheckEx);
        if (!NT_SUCCESS(Result))
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

        FileNode = 0;
        memset(&FileInfo, 0, sizeof FileInfo);
        Result = FileSystem->Interface->Create(FileSystem, Request,
            (PWSTR)Request->Buffer, Request->Req.Create.CaseSensitive, Request->Req.Create.CreateOptions,
            Request->Req.Create.FileAttributes, ObjectDescriptor, Request->Req.Create.AllocationSize,
            &FileNode, &FileInfo);
        FspDeleteSecurityDescriptor(ObjectDescriptor, FspAssignSecurity);
        if (!NT_SUCCESS(Result))
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
    }

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        Create ? FILE_CREATED : FILE_OVERWRITTEN, FileNode, GrantedAccess, &FileInfo);
}

static NTSTATUS FspFileSystemOpCreate_FileOpenTargetDirectory(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    PWSTR Parent, Suffix;
    UINT32 GrantedAccess;
    PVOID FileNode;
    FSP_FSCTL_FILE_INFO FileInfo;
    UINT_PTR Information;

    Result = FspAccessCheck(FileSystem, Request, TRUE, TRUE,
        Request->Req.Create.DesiredAccess, &GrantedAccess);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    FspPathSuffix((PWSTR)Request->Buffer, &Parent, &Suffix);
    FileNode = 0;
    memset(&FileInfo, 0, sizeof FileInfo);
    Result = FileSystem->Interface->Open(FileSystem, Request,
        (PWSTR)Request->Buffer, Request->Req.Create.CaseSensitive, Request->Req.Create.CreateOptions,
        &FileNode, &FileInfo);
    FspPathCombine((PWSTR)Request->Buffer, Suffix);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    Information = FILE_OPENED;
    if (0 != FileSystem->Interface->GetSecurity)
    {
        Result = FileSystem->Interface->GetSecurity(FileSystem, (PWSTR)Request->Buffer, 0, 0, 0);
        Information = NT_SUCCESS(Result) ? FILE_EXISTS : FILE_DOES_NOT_EXIST;
    }

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        Information, FileNode, GrantedAccess, &FileInfo);
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

FSP_API NTSTATUS FspFileSystemOpOverwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    FSP_FSCTL_FILE_INFO FileInfo;

    memset(&FileInfo, 0, sizeof FileInfo);
    Result = FileSystem->Interface->Overwrite(FileSystem, Request,
        (PVOID)Request->Req.Overwrite.UserContext,
        Request->Req.Overwrite.FileAttributes,
        Request->Req.Overwrite.Supersede,
        &FileInfo);
    if (!NT_SUCCESS(Result))
    {
        if (0 != FileSystem->Interface->Close)
            FileSystem->Interface->Close(FileSystem, Request,
                (PVOID)Request->Req.Overwrite.UserContext);
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
    }

    return FspFileSystemSendOverwriteResponse(FileSystem, Request, &FileInfo);
}

FSP_API NTSTATUS FspFileSystemSendCreateResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, UINT_PTR Information,
    PVOID FileNode, UINT32 GrantedAccess, const FSP_FSCTL_FILE_INFO *FileInfo)
{
    FSP_FSCTL_TRANSACT_RSP Response;

    memset(&Response, 0, sizeof Response);
    Response.Size = sizeof Response;
    Response.Kind = FspFsctlTransactCreateKind;
    Response.Hint = Request->Hint;
    Response.IoStatus.Status = STATUS_SUCCESS;
    Response.IoStatus.Information = Information;
    Response.Rsp.Create.Opened.UserContext = (UINT_PTR)FileNode;
    Response.Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    memcpy(&Response.Rsp.Create.Opened.FileInfo, FileInfo, sizeof *FileInfo);
    return FspFileSystemSendResponse(FileSystem, &Response);
}

FSP_API NTSTATUS FspFileSystemSendOverwriteResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    const FSP_FSCTL_FILE_INFO *FileInfo)
{
    FSP_FSCTL_TRANSACT_RSP Response;

    memset(&Response, 0, sizeof Response);
    Response.Size = sizeof Response;
    Response.Kind = FspFsctlTransactOverwriteKind;
    Response.Hint = Request->Hint;
    Response.IoStatus.Status = STATUS_SUCCESS;
    Response.IoStatus.Information = 0;
    memcpy(&Response.Rsp.Overwrite.FileInfo, FileInfo, sizeof *FileInfo);
    return FspFileSystemSendResponse(FileSystem, &Response);
}
