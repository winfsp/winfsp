/**
 * @file dll/fsop.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>

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
            FspGetFileGenericMapping()->GenericAll : Request->Req.Create.DesiredAccess;
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
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;
    PSECURITY_DESCRIPTOR ParentDescriptor, ObjectDescriptor;
    PVOID FileNode;
    FSP_FSCTL_FILE_INFO FileInfo;

    Result = FspFileSystemCreateCheck(FileSystem, Request, TRUE, &GrantedAccess, &ParentDescriptor);
    if (!NT_SUCCESS(Result))
        return Result;

    Result = FspCreateSecurityDescriptor(FileSystem, Request, ParentDescriptor, &ObjectDescriptor);
    FspDeleteSecurityDescriptor(ParentDescriptor, FspAccessCheckEx);
    if (!NT_SUCCESS(Result))
        return Result;

    FileNode = 0;
    memset(&FileInfo, 0, sizeof FileInfo);
    Result = FileSystem->Interface->Create(FileSystem, Request,
        (PWSTR)Request->Buffer, Request->Req.Create.CaseSensitive, Request->Req.Create.CreateOptions,
        Request->Req.Create.FileAttributes, ObjectDescriptor, Request->Req.Create.AllocationSize,
        &FileNode, &FileInfo);
    FspDeleteSecurityDescriptor(ObjectDescriptor, FspCreateSecurityDescriptor);
    if (!NT_SUCCESS(Result))
        return Result;

    Response->IoStatus.Information = FILE_CREATED;
    Response->Rsp.Create.Opened.UserContext = (UINT_PTR)FileNode;
    Response->Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    memcpy(&Response->Rsp.Create.Opened.FileInfo, &FileInfo, sizeof FileInfo);
    return STATUS_SUCCESS;
}

static NTSTATUS FspFileSystemOpCreate_FileOpen(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;
    PVOID FileNode;
    FSP_FSCTL_FILE_INFO FileInfo;

    Result = FspFileSystemOpenCheck(FileSystem, Request, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result))
        return Result;

    FileNode = 0;
    memset(&FileInfo, 0, sizeof FileInfo);
    Result = FileSystem->Interface->Open(FileSystem, Request,
        (PWSTR)Request->Buffer, Request->Req.Create.CaseSensitive, Request->Req.Create.CreateOptions,
        &FileNode, &FileInfo);
    if (!NT_SUCCESS(Result))
        return Result;

    Response->IoStatus.Information = FILE_OPENED;
    Response->Rsp.Create.Opened.UserContext = (UINT_PTR)FileNode;
    Response->Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    memcpy(&Response->Rsp.Create.Opened.FileInfo, &FileInfo, sizeof FileInfo);
    return STATUS_SUCCESS;
}

static NTSTATUS FspFileSystemOpCreate_FileOpenIf(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
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
            return Result;
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
                return Result;
            Create = TRUE;
        }
    }

    if (Create)
    {
        Result = FspFileSystemCreateCheck(FileSystem, Request, FALSE, &GrantedAccess, &ParentDescriptor);
        if (!NT_SUCCESS(Result))
            return Result;

        Result = FspCreateSecurityDescriptor(FileSystem, Request, ParentDescriptor, &ObjectDescriptor);
        FspDeleteSecurityDescriptor(ParentDescriptor, FspAccessCheckEx);
        if (!NT_SUCCESS(Result))
            return Result;

        FileNode = 0;
        memset(&FileInfo, 0, sizeof FileInfo);
        Result = FileSystem->Interface->Create(FileSystem, Request,
            (PWSTR)Request->Buffer, Request->Req.Create.CaseSensitive, Request->Req.Create.CreateOptions,
            Request->Req.Create.FileAttributes, ObjectDescriptor, Request->Req.Create.AllocationSize,
            &FileNode, &FileInfo);
        FspDeleteSecurityDescriptor(ObjectDescriptor, FspCreateSecurityDescriptor);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    Response->IoStatus.Information = Create ? FILE_CREATED : FILE_OPENED;
    Response->Rsp.Create.Opened.UserContext = (UINT_PTR)FileNode;
    Response->Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    memcpy(&Response->Rsp.Create.Opened.FileInfo, &FileInfo, sizeof FileInfo);
    return STATUS_SUCCESS;
}

static NTSTATUS FspFileSystemOpCreate_FileOverwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;
    PVOID FileNode;
    FSP_FSCTL_FILE_INFO FileInfo;
    BOOLEAN Supersede = FILE_SUPERSEDE == ((Request->Req.Create.CreateOptions >> 24) & 0xff);

    Result = FspFileSystemOverwriteCheck(FileSystem, Request, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result))
        return Result;

    FileNode = 0;
    memset(&FileInfo, 0, sizeof FileInfo);
    Result = FileSystem->Interface->Open(FileSystem, Request,
        (PWSTR)Request->Buffer, Request->Req.Create.CaseSensitive, Request->Req.Create.CreateOptions,
        &FileNode, &FileInfo);
    if (!NT_SUCCESS(Result))
        return Result;

    Response->IoStatus.Information = Supersede ? FILE_SUPERSEDED : FILE_OVERWRITTEN;
    Response->Rsp.Create.Opened.UserContext = (UINT_PTR)FileNode;
    Response->Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    memcpy(&Response->Rsp.Create.Opened.FileInfo, &FileInfo, sizeof FileInfo);
    return STATUS_SUCCESS;
}

static NTSTATUS FspFileSystemOpCreate_FileOverwriteIf(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
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
            return Result;
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
                return Result;
            Create = TRUE;
        }
    }

    if (Create)
    {
        Result = FspFileSystemCreateCheck(FileSystem, Request, FALSE, &GrantedAccess, &ParentDescriptor);
        if (!NT_SUCCESS(Result))
            return Result;

        Result = FspCreateSecurityDescriptor(FileSystem, Request, ParentDescriptor, &ObjectDescriptor);
        FspDeleteSecurityDescriptor(ParentDescriptor, FspAccessCheckEx);
        if (!NT_SUCCESS(Result))
            return Result;

        FileNode = 0;
        memset(&FileInfo, 0, sizeof FileInfo);
        Result = FileSystem->Interface->Create(FileSystem, Request,
            (PWSTR)Request->Buffer, Request->Req.Create.CaseSensitive, Request->Req.Create.CreateOptions,
            Request->Req.Create.FileAttributes, ObjectDescriptor, Request->Req.Create.AllocationSize,
            &FileNode, &FileInfo);
        FspDeleteSecurityDescriptor(ObjectDescriptor, FspCreateSecurityDescriptor);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    Response->IoStatus.Information = Create ? FILE_CREATED : FILE_OVERWRITTEN;
    Response->Rsp.Create.Opened.UserContext = (UINT_PTR)FileNode;
    Response->Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    memcpy(&Response->Rsp.Create.Opened.FileInfo, &FileInfo, sizeof FileInfo);
    return STATUS_SUCCESS;
}

static NTSTATUS FspFileSystemOpCreate_FileOpenTargetDirectory(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    WCHAR Root[2] = L"\\";
    PWSTR Parent, Suffix;
    UINT32 GrantedAccess;
    PVOID FileNode;
    FSP_FSCTL_FILE_INFO FileInfo;
    UINT_PTR Information;

    Result = FspAccessCheck(FileSystem, Request, TRUE, TRUE,
        Request->Req.Create.DesiredAccess, &GrantedAccess);
    if (!NT_SUCCESS(Result))
        return Result;

    FileNode = 0;
    memset(&FileInfo, 0, sizeof FileInfo);
    FspPathSuffix((PWSTR)Request->Buffer, &Parent, &Suffix, Root);
    Result = FileSystem->Interface->Open(FileSystem, Request,
        Parent, Request->Req.Create.CaseSensitive, Request->Req.Create.CreateOptions,
        &FileNode, &FileInfo);
    FspPathCombine((PWSTR)Request->Buffer, Suffix);
    if (!NT_SUCCESS(Result))
        return Result;

    Information = FILE_OPENED;
    if (0 != FileSystem->Interface->GetSecurityByName)
    {
        Result = FileSystem->Interface->GetSecurityByName(FileSystem, (PWSTR)Request->Buffer, 0, 0, 0);
        Information = NT_SUCCESS(Result) ? FILE_EXISTS : FILE_DOES_NOT_EXIST;
    }

    Response->IoStatus.Information = Information;
    Response->Rsp.Create.Opened.UserContext = (UINT_PTR)FileNode;
    Response->Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    memcpy(&Response->Rsp.Create.Opened.FileInfo, &FileInfo, sizeof FileInfo);
    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpCreate(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    if (0 == FileSystem->Interface->Create ||
        0 == FileSystem->Interface->Open ||
        0 == FileSystem->Interface->Overwrite)
        return STATUS_INVALID_DEVICE_REQUEST;

    if (Request->Req.Create.OpenTargetDirectory)
        return FspFileSystemOpCreate_FileOpenTargetDirectory(FileSystem, Request, Response);

    switch ((Request->Req.Create.CreateOptions >> 24) & 0xff)
    {
    case FILE_CREATE:
        return FspFileSystemOpCreate_FileCreate(FileSystem, Request, Response);
    case FILE_OPEN:
        return FspFileSystemOpCreate_FileOpen(FileSystem, Request, Response);
    case FILE_OPEN_IF:
        return FspFileSystemOpCreate_FileOpenIf(FileSystem, Request, Response);
    case FILE_OVERWRITE:
    case FILE_SUPERSEDE:
        return FspFileSystemOpCreate_FileOverwrite(FileSystem, Request, Response);
    case FILE_OVERWRITE_IF:
        return FspFileSystemOpCreate_FileOverwriteIf(FileSystem, Request, Response);
    default:
        return STATUS_INVALID_PARAMETER;
    }
}

FSP_API NTSTATUS FspFileSystemOpOverwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    FSP_FSCTL_FILE_INFO FileInfo;

    if (0 == FileSystem->Interface->Overwrite)
        return STATUS_INVALID_DEVICE_REQUEST;

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
        return Result;
    }

    memcpy(&Response->Rsp.Overwrite.FileInfo, &FileInfo, sizeof FileInfo);
    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpCleanup(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    if (0 != FileSystem->Interface->Cleanup)
        FileSystem->Interface->Cleanup(FileSystem, Request,
            (PVOID)Request->Req.Cleanup.UserContext,
            0 != Request->FileName.Size ? (PWSTR)Request->Buffer : 0,
            0 != Request->Req.Cleanup.Delete);

    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpClose(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    if (0 != FileSystem->Interface->Close)
        FileSystem->Interface->Close(FileSystem, Request,
            (PVOID)Request->Req.Close.UserContext);

    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpRead(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    ULONG BytesTransferred;
    FSP_FSCTL_FILE_INFO FileInfo;

    if (0 == FileSystem->Interface->Read)
        return STATUS_INVALID_DEVICE_REQUEST;

    Result = FileSystem->Interface->Read(FileSystem, Request,
        (PVOID)Request->Req.Read.UserContext,
        (PVOID)Request->Req.Read.Address,
        Request->Req.Read.Offset,
        Request->Req.Read.Length,
        &BytesTransferred,
        &FileInfo);
    if (!NT_SUCCESS(Result))
        return Result;

    if (STATUS_PENDING != Result)
    {
        Response->IoStatus.Information = BytesTransferred;
        memcpy(&Response->Rsp.Read.FileInfo, &FileInfo, sizeof FileInfo);
    }

    return Result;
}

FSP_API NTSTATUS FspFileSystemOpWrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    ULONG BytesTransferred;
    FSP_FSCTL_FILE_INFO FileInfo;

    if (0 == FileSystem->Interface->Write)
        return STATUS_INVALID_DEVICE_REQUEST;

    Result = FileSystem->Interface->Write(FileSystem, Request,
        (PVOID)Request->Req.Write.UserContext,
        (PVOID)Request->Req.Write.Address,
        Request->Req.Write.Offset,
        Request->Req.Write.Length,
        Request->Req.Write.Constrained,
        &BytesTransferred,
        &FileInfo);
    if (!NT_SUCCESS(Result))
        return Result;

    if (STATUS_PENDING != Result)
    {
        Response->IoStatus.Information = BytesTransferred;
        memcpy(&Response->Rsp.Read.FileInfo, &FileInfo, sizeof FileInfo);
    }

    return Result;
}

FSP_API NTSTATUS FspFileSystemOpQueryInformation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    FSP_FSCTL_FILE_INFO FileInfo;

    if (0 == FileSystem->Interface->GetFileInfo)
        return STATUS_INVALID_DEVICE_REQUEST;

    memset(&FileInfo, 0, sizeof FileInfo);
    Result = FileSystem->Interface->GetFileInfo(FileSystem, Request,
        (PVOID)Request->Req.QueryInformation.UserContext, &FileInfo);
    if (!NT_SUCCESS(Result))
        return Result;

    memcpy(&Response->Rsp.QueryInformation.FileInfo, &FileInfo, sizeof FileInfo);
    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpSetInformation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    FSP_FSCTL_FILE_INFO FileInfo;

    Result = STATUS_INVALID_DEVICE_REQUEST;
    memset(&FileInfo, 0, sizeof FileInfo);
    switch (Request->Req.SetInformation.FileInformationClass)
    {
    case 4/*FileBasicInformation*/:
        if (0 != FileSystem->Interface->SetBasicInfo)
            Result = FileSystem->Interface->SetBasicInfo(FileSystem, Request,
                (PVOID)Request->Req.SetInformation.UserContext,
                Request->Req.SetInformation.Info.Basic.FileAttributes,
                Request->Req.SetInformation.Info.Basic.CreationTime,
                Request->Req.SetInformation.Info.Basic.LastAccessTime,
                Request->Req.SetInformation.Info.Basic.LastWriteTime,
                &FileInfo);
        break;
    case 19/*FileAllocationInformation*/:
        if (0 != FileSystem->Interface->SetAllocationSize)
            Result = FileSystem->Interface->SetAllocationSize(FileSystem, Request,
                (PVOID)Request->Req.SetInformation.UserContext,
                Request->Req.SetInformation.Info.Allocation.AllocationSize,
                &FileInfo);
        else
        if (0 != FileSystem->Interface->GetFileInfo &&
            0 != FileSystem->Interface->SetFileSize)
        {
            Result = FileSystem->Interface->GetFileInfo(FileSystem, Request,
                (PVOID)Request->Req.SetInformation.UserContext, &FileInfo);
            if (NT_SUCCESS(Result) &&
                Request->Req.SetInformation.Info.Allocation.AllocationSize < FileInfo.FileSize)
            {
                Result = FileSystem->Interface->SetFileSize(FileSystem, Request,
                    (PVOID)Request->Req.SetInformation.UserContext,
                    Request->Req.SetInformation.Info.Allocation.AllocationSize,
                    &FileInfo);
            }
        }
        break;
    case 20/*FileEndOfFileInformation*/:
        if (0 != FileSystem->Interface->SetFileSize)
            Result = FileSystem->Interface->SetFileSize(FileSystem, Request,
                (PVOID)Request->Req.SetInformation.UserContext,
                Request->Req.SetInformation.Info.EndOfFile.FileSize,
                &FileInfo);
        break;
    case 13/*FileDispositionInformation*/:
        if (0 != FileSystem->Interface->CanDelete)
            if (Request->Req.SetInformation.Info.Disposition.Delete)
                Result = FileSystem->Interface->CanDelete(FileSystem, Request,
                    (PVOID)Request->Req.SetInformation.UserContext,
                    (PWSTR)Request->Buffer);
            else
                Result = STATUS_SUCCESS;
        break;
    case 10/*FileRenameInformation*/:
        if (0 != FileSystem->Interface->Rename)
            Result = FileSystem->Interface->Rename(FileSystem, Request,
                (PVOID)Request->Req.SetInformation.UserContext,
                (PWSTR)Request->Buffer,
                (PWSTR)(Request->Buffer + Request->Req.SetInformation.Info.Rename.NewFileName.Offset),
                Request->Req.SetInformation.Info.Rename.ReplaceIfExists);
        break;
    }

    if (!NT_SUCCESS(Result))
        return Result;

    memcpy(&Response->Rsp.SetInformation.FileInfo, &FileInfo, sizeof FileInfo);
    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpQueryVolumeInformation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    FSP_FSCTL_VOLUME_INFO VolumeInfo;

    if (0 == FileSystem->Interface->GetVolumeInfo)
        return STATUS_INVALID_DEVICE_REQUEST;

    memset(&VolumeInfo, 0, sizeof VolumeInfo);
    Result = FileSystem->Interface->GetVolumeInfo(FileSystem, Request, &VolumeInfo);
    if (!NT_SUCCESS(Result))
        return Result;

    memcpy(&Response->Rsp.QueryVolumeInformation.VolumeInfo, &VolumeInfo, sizeof VolumeInfo);
    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpSetVolumeInformation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    FSP_FSCTL_VOLUME_INFO VolumeInfo;

    Result = STATUS_INVALID_DEVICE_REQUEST;
    memset(&VolumeInfo, 0, sizeof VolumeInfo);
    switch (Request->Req.SetVolumeInformation.FsInformationClass)
    {
    case 2/*FileFsLabelInformation*/:
        if (0 != FileSystem->Interface->SetVolumeLabel)
            Result = FileSystem->Interface->SetVolumeLabel(FileSystem, Request,
                (PWSTR)Request->Buffer,
                &VolumeInfo);
    }

    if (!NT_SUCCESS(Result))
        return Result;

    memcpy(&Response->Rsp.SetVolumeInformation.VolumeInfo, &VolumeInfo, sizeof VolumeInfo);
    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpQuerySecurity(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    SIZE_T SecurityDescriptorSize;

    if (0 == FileSystem->Interface->GetSecurity)
        return STATUS_INVALID_DEVICE_REQUEST;

    SecurityDescriptorSize = FSP_FSCTL_TRANSACT_RSP_SIZEMAX - sizeof *Response;
    Result = FileSystem->Interface->GetSecurity(FileSystem, Request,
        (PVOID)Request->Req.QuerySecurity.UserContext,
        Response->Buffer, &SecurityDescriptorSize);
    if (!NT_SUCCESS(Result))
        return STATUS_BUFFER_OVERFLOW != Result ? Result : STATUS_INVALID_SECURITY_DESCR;

    Response->Size = (UINT16)(sizeof *Response + SecurityDescriptorSize);
    Response->Rsp.QuerySecurity.SecurityDescriptor.Offset = 0;
    Response->Rsp.QuerySecurity.SecurityDescriptor.Size = (UINT16)SecurityDescriptorSize;
    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpSetSecurity(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    if (0 == FileSystem->Interface->SetSecurity)
        return STATUS_INVALID_DEVICE_REQUEST;

    return FileSystem->Interface->SetSecurity(FileSystem, Request,
        (PVOID)Request->Req.SetSecurity.UserContext,
        Request->Req.SetSecurity.SecurityInformation,
        (PSECURITY_DESCRIPTOR)Request->Buffer);
}
