/**
 * @file dll/create.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>

static inline
BOOLEAN FspIsRootDirectory(PWSTR FileName)
{
    for (PWSTR Pointer = FileName; *Pointer; Pointer++)
        if (L'\\' != *Pointer)
            return FALSE;
    return TRUE;
}

static inline
NTSTATUS FspCreateCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN AllowTraverseCheck,
    PDWORD PGrantedAccess)
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

static NTSTATUS FspFileSystemOpCreate_FileCreate(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    if (FspIsRootDirectory((PWSTR)Request->Buffer))
        return STATUS_ACCESS_DENIED;

    NTSTATUS Result;
    DWORD GrantedAccess;
    FSP_FILE_NODE *FileNode;

    Result = FspCreateCheck(FileSystem, Request, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    Result = FileSystem->Interface->Create(FileSystem, Request, &FileNode);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    memset(&FileNode->Flags, 0, sizeof FileNode->Flags);
    memset(&FileNode->ShareAccess, 0, sizeof FileNode->ShareAccess);

    FspShareCheck(FileSystem, GrantedAccess, Request->Req.Create.ShareAccess, FileNode);

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        FILE_CREATED, FileNode, GrantedAccess);
}

static NTSTATUS FspFileSystemOpCreate_FileOpen(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    DWORD GrantedAccess;
    FSP_FILE_NODE *FileNode;

    Result = FspAccessCheck(FileSystem, Request, FALSE, TRUE,
        Request->Req.Create.DesiredAccess, &GrantedAccess);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    Result = FileSystem->Interface->Open(FileSystem, Request, &FileNode);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    Result = FspShareCheck(FileSystem, GrantedAccess, Request->Req.Create.ShareAccess, FileNode);
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

    Result = FspAccessCheck(FileSystem, Request, FALSE, TRUE,
        Request->Req.Create.DesiredAccess, &GrantedAccess);
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
    }

    if (Create)
    {
        if (FspIsRootDirectory((PWSTR)Request->Buffer))
            return STATUS_ACCESS_DENIED;

        Result = FspCreateCheck(FileSystem, Request, FALSE, &GrantedAccess);
        if (!NT_SUCCESS(Result))
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

        Result = FileSystem->Interface->Create(FileSystem, Request, &FileNode);
        if (!NT_SUCCESS(Result))
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

        memset(&FileNode->Flags, 0, sizeof FileNode->Flags);
        memset(&FileNode->ShareAccess, 0, sizeof FileNode->ShareAccess);
    }

    Result = FspShareCheck(FileSystem, GrantedAccess, Request->Req.Create.ShareAccess, FileNode);
    if (!NT_SUCCESS(Result))
    {
        if (0 != FileSystem->Interface->Close)
            FileSystem->Interface->Close(FileSystem, Request, FileNode);
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
    }

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        Create ? FILE_CREATED : FILE_OPENED, FileNode, GrantedAccess);
}

static NTSTATUS FspFileSystemOpCreate_FileOverwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN Supersede)
{
    if (FspIsRootDirectory((PWSTR)Request->Buffer))
        return STATUS_ACCESS_DENIED;

    NTSTATUS Result;
    DWORD GrantedAccess;
    FSP_FILE_NODE *FileNode;

    Result = FspAccessCheck(FileSystem, Request, FALSE, TRUE,
        Request->Req.Create.DesiredAccess | (Supersede ? DELETE : FILE_WRITE_DATA),
        &GrantedAccess);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    if (0 == (Request->Req.Create.DesiredAccess & MAXIMUM_ALLOWED))
        GrantedAccess &= Supersede ?
            (~DELETE | (Request->Req.Create.DesiredAccess & DELETE)) :
            (~FILE_WRITE_DATA | (Request->Req.Create.DesiredAccess & FILE_WRITE_DATA));

    Result = FileSystem->Interface->Open(FileSystem, Request, &FileNode);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    Result = FspShareCheck(FileSystem, GrantedAccess, Request->Req.Create.ShareAccess, FileNode);
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
    if (FspIsRootDirectory((PWSTR)Request->Buffer))
        return STATUS_ACCESS_DENIED;

    NTSTATUS Result;
    DWORD GrantedAccess;
    FSP_FILE_NODE *FileNode;
    BOOLEAN Create = FALSE;

    Result = FspAccessCheck(FileSystem, Request, FALSE, TRUE,
        Request->Req.Create.DesiredAccess | FILE_WRITE_DATA,
        &GrantedAccess);
    if (!NT_SUCCESS(Result))
    {
        if (STATUS_OBJECT_NAME_NOT_FOUND != Result)
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
        Create = TRUE;
    }
    else
    {
        if (0 == (Request->Req.Create.DesiredAccess & MAXIMUM_ALLOWED))
            GrantedAccess &= ~FILE_WRITE_DATA | (Request->Req.Create.DesiredAccess & FILE_WRITE_DATA);
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
    }

    if (Create)
    {
        Result = FspCreateCheck(FileSystem, Request, FALSE, &GrantedAccess);
        if (!NT_SUCCESS(Result))
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

        Result = FileSystem->Interface->Create(FileSystem, Request, &FileNode);
        if (!NT_SUCCESS(Result))
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

        memset(&FileNode->Flags, 0, sizeof FileNode->Flags);
        memset(&FileNode->ShareAccess, 0, sizeof FileNode->ShareAccess);
    }

    Result = FspShareCheck(FileSystem, GrantedAccess, Request->Req.Create.ShareAccess, FileNode);
    if (!NT_SUCCESS(Result))
    {
        if (0 != FileSystem->Interface->Close)
            FileSystem->Interface->Close(FileSystem, Request, FileNode);
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
    }

    if (!Create)
    {
        Result = FileSystem->Interface->Overwrite(FileSystem, Request, FALSE, FileNode);
        if (!NT_SUCCESS(Result))
        {
            if (0 != FileSystem->Interface->Close)
                FileSystem->Interface->Close(FileSystem, Request, FileNode);
            return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
        }
    }

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        Create ? FILE_CREATED : FILE_OVERWRITTEN, FileNode, GrantedAccess);
}

static NTSTATUS FspFileSystemOpCreate_FileOpenTargetDirectory(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    if (FspIsRootDirectory((PWSTR)Request->Buffer))
        return STATUS_ACCESS_DENIED;

    NTSTATUS Result;
    DWORD GrantedAccess;
    FSP_FILE_NODE *FileNode;
    BOOLEAN FileExists;

    Result = FspAccessCheck(FileSystem, Request, TRUE, TRUE,
        Request->Req.Create.DesiredAccess, &GrantedAccess);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    Result = FileSystem->Interface->OpenParentDirectory(FileSystem, Request,
        &FileNode, &FileExists);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    Result = FspShareCheck(FileSystem, GrantedAccess, Request->Req.Create.ShareAccess, FileNode);
    if (!NT_SUCCESS(Result))
    {
        if (0 != FileSystem->Interface->Close)
            FileSystem->Interface->Close(FileSystem, Request, FileNode);
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);
    }

    return FspFileSystemSendCreateResponse(FileSystem, Request,
        FileExists ? FILE_EXISTS : FILE_DOES_NOT_EXIST, FileNode, GrantedAccess);
}

FSP_API NTSTATUS FspFileSystemOpCreate(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    if (0 == FileSystem->Interface->Create ||
        0 == FileSystem->Interface->Open ||
        0 == FileSystem->Interface->Overwrite ||
        0 == FileSystem->Interface->OpenParentDirectory)
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
        return FspFileSystemOpCreate_FileOverwrite(FileSystem, Request, FALSE);
    case FILE_SUPERSEDE:
        return FspFileSystemOpCreate_FileOverwrite(FileSystem, Request, TRUE);
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
