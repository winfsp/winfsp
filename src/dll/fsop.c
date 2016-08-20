/**
 * @file dll/fsop.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the
 * GNU Affero General Public License version 3 as published by the
 * Free Software Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <dll/library.h>

FSP_API NTSTATUS FspFileSystemOpEnter(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    switch (FileSystem->OpGuardStrategy)
    {
    case FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE:
        if ((FspFsctlTransactCreateKind == Request->Kind &&
                FILE_OPEN != ((Request->Req.Create.CreateOptions >> 24) & 0xff)) ||
            (FspFsctlTransactCleanupKind == Request->Kind &&
                Request->Req.Cleanup.Delete) ||
            (FspFsctlTransactSetInformationKind == Request->Kind &&
                10/*FileRenameInformation*/ == Request->Req.SetInformation.FileInformationClass) ||
            FspFsctlTransactSetVolumeInformationKind == Request->Kind)
        {
            AcquireSRWLockExclusive(&FileSystem->OpGuardLock);
        }
        else
        if (FspFsctlTransactCreateKind == Request->Kind ||
            (FspFsctlTransactSetInformationKind == Request->Kind &&
                13/*FileDispositionInformation*/ == Request->Req.SetInformation.FileInformationClass) ||
            FspFsctlTransactQueryDirectoryKind == Request->Kind ||
            FspFsctlTransactQueryVolumeInformationKind == Request->Kind)
        {
            AcquireSRWLockShared(&FileSystem->OpGuardLock);
        }
        break;

    case FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_COARSE:
        AcquireSRWLockExclusive(&FileSystem->OpGuardLock);
        break;
    }

    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpLeave(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    switch (FileSystem->OpGuardStrategy)
    {
    case FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE:
        if ((FspFsctlTransactCreateKind == Request->Kind &&
                FILE_OPEN != ((Request->Req.Create.CreateOptions >> 24) & 0xff)) ||
            (FspFsctlTransactCleanupKind == Request->Kind &&
                Request->Req.Cleanup.Delete) ||
            (FspFsctlTransactSetInformationKind == Request->Kind &&
                10/*FileRenameInformation*/ == Request->Req.SetInformation.FileInformationClass) ||
            FspFsctlTransactSetVolumeInformationKind == Request->Kind)
        {
            ReleaseSRWLockExclusive(&FileSystem->OpGuardLock);
        }
        else
        if (FspFsctlTransactCreateKind == Request->Kind ||
            (FspFsctlTransactSetInformationKind == Request->Kind &&
                13/*FileDispositionInformation*/ == Request->Req.SetInformation.FileInformationClass) ||
            FspFsctlTransactQueryDirectoryKind == Request->Kind ||
            FspFsctlTransactQueryVolumeInformationKind == Request->Kind)
        {
            ReleaseSRWLockShared(&FileSystem->OpGuardLock);
        }
        break;

    case FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_COARSE:
        ReleaseSRWLockExclusive(&FileSystem->OpGuardLock);
        break;
    }

    return STATUS_SUCCESS;
}

static inline
NTSTATUS FspFileSystemCallResolveReparsePoints(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response,
    UINT32 ReparsePointIndex)
{
    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    IO_STATUS_BLOCK IoStatus;
    SIZE_T Size;

    if (0 != FileSystem->Interface->ResolveReparsePoints)
    {
        memset(&IoStatus, 0, sizeof IoStatus);
        Size = FSP_FSCTL_TRANSACT_RSP_SIZEMAX - FIELD_OFFSET(FSP_FSCTL_TRANSACT_RSP, Buffer);
        Result = FileSystem->Interface->ResolveReparsePoints(FileSystem,
            (PWSTR)Request->Buffer,
            ReparsePointIndex,
            !!(Request->Req.Create.CreateOptions & FILE_OPEN_REPARSE_POINT),
            &IoStatus,
            Response->Buffer,
            &Size);
        if (NT_SUCCESS(Result))
        {
            Result = STATUS_REPARSE;
            Response->IoStatus.Information = (UINT32)IoStatus.Information;

            if (0/*IO_REPARSE*/ == IoStatus.Information)
            {
                Response->Rsp.Create.Reparse.FileName.Offset = 0;
                Response->Rsp.Create.Reparse.FileName.Size = (UINT16)Size;
            }
            else
            {
                Response->Rsp.Create.Reparse.Data.Offset = 0;
                Response->Rsp.Create.Reparse.Data.Size = (UINT16)Size;
            }
        }
    }

    return Result;
}

static inline
NTSTATUS FspFileSystemCreateCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response,
    BOOLEAN AllowTraverseCheck, PUINT32 PGrantedAccess,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;

    /*
     * CreateCheck consists of checking the parent directory for the
     * FILE_ADD_SUBDIRECTORY or FILE_ADD_FILE rights (depending on whether
     * we are creating a file or directory).
     *
     * If the access check succeeds and MAXIMUM_ALLOWED has been requested
     * then we go ahead and grant all access to the creator.
     */

    Result = FspAccessCheckEx(FileSystem, Request, TRUE, AllowTraverseCheck,
        (Request->Req.Create.CreateOptions & FILE_DIRECTORY_FILE) ?
            FILE_ADD_SUBDIRECTORY : FILE_ADD_FILE,
        &GrantedAccess, PSecurityDescriptor);
    if (STATUS_REPARSE == Result)
        Result = FspFileSystemCallResolveReparsePoints(FileSystem, Request, Response, GrantedAccess);
    else if (NT_SUCCESS(Result))
    {
        *PGrantedAccess = (MAXIMUM_ALLOWED & Request->Req.Create.DesiredAccess) ?
            FspGetFileGenericMapping()->GenericAll : Request->Req.Create.DesiredAccess;
    }

    return Result;
}

static inline
NTSTATUS FspFileSystemOpenCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response,
    BOOLEAN AllowTraverseCheck, PUINT32 PGrantedAccess)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;

    /*
     * OpenCheck consists of checking the file for the desired access,
     * unless FILE_DELETE_ON_CLOSE is requested in which case we also
     * check for DELETE access.
     *
     * If the access check succeeds and MAXIMUM_ALLOWED was not requested
     * then we reset the DELETE access based on whether it was actually
     * requested in DesiredAccess.
     */

    Result = FspAccessCheck(FileSystem, Request, FALSE, AllowTraverseCheck,
        Request->Req.Create.DesiredAccess |
            ((Request->Req.Create.CreateOptions & FILE_DELETE_ON_CLOSE) ? DELETE : 0),
        &GrantedAccess);
    if (STATUS_REPARSE == Result)
        Result = FspFileSystemCallResolveReparsePoints(FileSystem, Request, Response, GrantedAccess);
    else if (NT_SUCCESS(Result))
    {
        *PGrantedAccess = GrantedAccess;
        if (0 == (Request->Req.Create.DesiredAccess & MAXIMUM_ALLOWED))
            *PGrantedAccess &= ~DELETE | (Request->Req.Create.DesiredAccess & DELETE);
    }

    return Result;
}

static inline
NTSTATUS FspFileSystemOverwriteCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response,
    BOOLEAN AllowTraverseCheck, PUINT32 PGrantedAccess)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;
    BOOLEAN Supersede = FILE_SUPERSEDE == ((Request->Req.Create.CreateOptions >> 24) & 0xff);

    /*
     * OverwriteCheck consists of checking the file for the desired access,
     * unless FILE_DELETE_ON_CLOSE is requested in which case we also
     * check for DELETE access. Furthermore we grant DELETE or FILE_WRITE_DATA
     * access based on whether this is a Supersede or Overwrite operation.
     *
     * If the access check succeeds and MAXIMUM_ALLOWED was not requested
     * then we reset the DELETE and FILE_WRITE_DATA accesses based on whether
     * they were actually requested in DesiredAccess.
     */

    Result = FspAccessCheck(FileSystem, Request, FALSE, AllowTraverseCheck,
        Request->Req.Create.DesiredAccess |
            (Supersede ? DELETE : FILE_WRITE_DATA) |
            ((Request->Req.Create.CreateOptions & FILE_DELETE_ON_CLOSE) ? DELETE : 0),
        &GrantedAccess);
    if (STATUS_REPARSE == Result)
        Result = FspFileSystemCallResolveReparsePoints(FileSystem, Request, Response, GrantedAccess);
    else if (NT_SUCCESS(Result))
    {
        *PGrantedAccess = GrantedAccess;
        if (0 == (Request->Req.Create.DesiredAccess & MAXIMUM_ALLOWED))
            *PGrantedAccess &= ~(DELETE | FILE_WRITE_DATA) |
                (Request->Req.Create.DesiredAccess & (DELETE | FILE_WRITE_DATA));
    }

    return Result;
}

static inline
NTSTATUS FspFileSystemOpenTargetDirectoryCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response,
    PUINT32 PGrantedAccess)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;

    /*
     * OpenTargetDirectoryCheck consists of checking the parent directory
     * for the desired access.
     */

    Result = FspAccessCheck(FileSystem, Request, TRUE, TRUE, Request->Req.Create.DesiredAccess,
        &GrantedAccess);
    if (STATUS_REPARSE == Result)
        Result = FspFileSystemCallResolveReparsePoints(FileSystem, Request, Response, GrantedAccess);
    else if (NT_SUCCESS(Result))
        *PGrantedAccess = GrantedAccess;

    return Result;
}

static inline
NTSTATUS FspFileSystemRenameCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    FSP_FSCTL_TRANSACT_REQ *CreateRequest = 0;
    UINT32 GrantedAccess;

    /*
     * RenameCheck consists of checking the new file name for DELETE access.
     *
     * The following assumptions are being made here for a file that is going
     * to be replaced:
     * -   The new file is in the same directory as the old one. In that case
     *     there is no need for traverse access checks as they have been already
     *     performed (if necessary) when opening the file under the existing file
     *     name.
     * -   The new file is in a different directory than the old one. In that case
     *     NTOS called us with SL_OPEN_TARGET_DIRECTORY and we performed any
     *     necessary traverse access checks at that time.
     *
     * FspAccessCheckEx only works on Create requests, so we have to build
     * a fake one just for that purpose. Sigh!
     */

    CreateRequest = MemAlloc(sizeof *CreateRequest +
        Request->Req.SetInformation.Info.Rename.NewFileName.Size);
    if (0 == CreateRequest)
        return STATUS_INSUFFICIENT_RESOURCES;

    memset(CreateRequest, 0, sizeof *CreateRequest);
    CreateRequest->Size = sizeof CreateRequest +
        Request->Req.SetInformation.Info.Rename.NewFileName.Size;
    CreateRequest->Kind = FspFsctlTransactCreateKind;
    CreateRequest->Req.Create.CreateOptions =
        FILE_DELETE_ON_CLOSE |          /* force read-only check! */
        FILE_OPEN_REPARSE_POINT;        /* allow rename over reparse point */
    CreateRequest->Req.Create.AccessToken = Request->Req.SetInformation.Info.Rename.AccessToken;
    CreateRequest->Req.Create.UserMode = TRUE;
    CreateRequest->FileName.Offset = 0;
    CreateRequest->FileName.Size = Request->Req.SetInformation.Info.Rename.NewFileName.Size;
    memcpy(CreateRequest->Buffer,
        Request->Buffer + Request->Req.SetInformation.Info.Rename.NewFileName.Offset,
        Request->Req.SetInformation.Info.Rename.NewFileName.Size);

    Result = FspAccessCheck(FileSystem, CreateRequest, FALSE, FALSE, DELETE, &GrantedAccess);

    MemFree(CreateRequest);

    if (STATUS_REPARSE == Result)
        Result = STATUS_SUCCESS; /* file system should not return STATUS_REPARSE during rename */

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

    Result = FspFileSystemCreateCheck(FileSystem, Request, Response, TRUE,
        &GrantedAccess, &ParentDescriptor);
    if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
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

    Result = FspFileSystemOpenCheck(FileSystem, Request, Response, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
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

    Result = FspFileSystemOpenCheck(FileSystem, Request, Response, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
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
        Result = FspFileSystemCreateCheck(FileSystem, Request, Response, FALSE,
            &GrantedAccess, &ParentDescriptor);
        if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
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

    Result = FspFileSystemOverwriteCheck(FileSystem, Request, Response, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
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

    Result = FspFileSystemOverwriteCheck(FileSystem, Request, Response, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
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
        Result = FspFileSystemCreateCheck(FileSystem, Request, Response,
            FALSE, &GrantedAccess, &ParentDescriptor);
        if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
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
    UINT32 Information;

    Result = FspFileSystemOpenTargetDirectoryCheck(FileSystem, Request, Response, &GrantedAccess);
    if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
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

    if (0 == FileSystem->Interface->Read)
        return STATUS_INVALID_DEVICE_REQUEST;

    BytesTransferred = 0;
    Result = FileSystem->Interface->Read(FileSystem, Request,
        (PVOID)Request->Req.Read.UserContext,
        (PVOID)Request->Req.Read.Address,
        Request->Req.Read.Offset,
        Request->Req.Read.Length,
        &BytesTransferred);
    if (!NT_SUCCESS(Result))
        return Result;

    if (STATUS_PENDING != Result)
        Response->IoStatus.Information = BytesTransferred;

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

    BytesTransferred = 0;
    Result = FileSystem->Interface->Write(FileSystem, Request,
        (PVOID)Request->Req.Write.UserContext,
        (PVOID)Request->Req.Write.Address,
        Request->Req.Write.Offset,
        Request->Req.Write.Length,
        (UINT64)-1LL == Request->Req.Write.Offset,
        0 != Request->Req.Write.ConstrainedIo,
        &BytesTransferred,
        &FileInfo);
    if (!NT_SUCCESS(Result))
        return Result;

    if (STATUS_PENDING != Result)
    {
        Response->IoStatus.Information = BytesTransferred;
        memcpy(&Response->Rsp.Write.FileInfo, &FileInfo, sizeof FileInfo);
    }

    return Result;
}

FSP_API NTSTATUS FspFileSystemOpFlushBuffers(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    if (0 == FileSystem->Interface->Flush)
        return STATUS_SUCCESS; /* liar! */

    return FileSystem->Interface->Flush(FileSystem, Request,
        (PVOID)Request->Req.FlushBuffers.UserContext);
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
        if (0 != FileSystem->Interface->SetFileSize)
            Result = FileSystem->Interface->SetFileSize(FileSystem, Request,
                (PVOID)Request->Req.SetInformation.UserContext,
                Request->Req.SetInformation.Info.Allocation.AllocationSize, TRUE,
                &FileInfo);
        break;
    case 20/*FileEndOfFileInformation*/:
        if (0 != FileSystem->Interface->SetFileSize)
            Result = FileSystem->Interface->SetFileSize(FileSystem, Request,
                (PVOID)Request->Req.SetInformation.UserContext,
                Request->Req.SetInformation.Info.EndOfFile.FileSize, FALSE,
                &FileInfo);
        break;
    case 13/*FileDispositionInformation*/:
        if (0 != FileSystem->Interface->GetFileInfo)
        {
            Result = FileSystem->Interface->GetFileInfo(FileSystem, Request,
                (PVOID)Request->Req.SetInformation.UserContext, &FileInfo);
            if (NT_SUCCESS(Result) && 0 != (FileInfo.FileAttributes & FILE_ATTRIBUTE_READONLY))
            {
                Result = STATUS_CANNOT_DELETE;
                break;
            }
        }
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
        {
            if (0 != Request->Req.SetInformation.Info.Rename.AccessToken)
            {
                Result = FspFileSystemRenameCheck(FileSystem, Request);
                if (!NT_SUCCESS(Result) &&
                    STATUS_OBJECT_PATH_NOT_FOUND != Result &&
                    STATUS_OBJECT_NAME_NOT_FOUND != Result)
                    break;
            }
            Result = FileSystem->Interface->Rename(FileSystem, Request,
                (PVOID)Request->Req.SetInformation.UserContext,
                (PWSTR)Request->Buffer,
                (PWSTR)(Request->Buffer + Request->Req.SetInformation.Info.Rename.NewFileName.Offset),
                0 != Request->Req.SetInformation.Info.Rename.AccessToken);
        }
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
        break;
    }

    if (!NT_SUCCESS(Result))
        return Result;

    memcpy(&Response->Rsp.SetVolumeInformation.VolumeInfo, &VolumeInfo, sizeof VolumeInfo);
    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpQueryDirectory(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    ULONG BytesTransferred;

    if (0 == FileSystem->Interface->ReadDirectory)
        return STATUS_INVALID_DEVICE_REQUEST;

    BytesTransferred = 0;
    Result = FileSystem->Interface->ReadDirectory(FileSystem, Request,
        (PVOID)Request->Req.QueryDirectory.UserContext,
        (PVOID)Request->Req.QueryDirectory.Address,
        Request->Req.QueryDirectory.Offset,
        Request->Req.QueryDirectory.Length,
        0 != Request->Req.QueryDirectory.Pattern.Size ?
            (PWSTR)(Request->Buffer + Request->Req.QueryDirectory.Pattern.Offset) : 0,
        &BytesTransferred);
    if (!NT_SUCCESS(Result))
        return Result;

    if (STATUS_PENDING != Result)
        Response->IoStatus.Information = BytesTransferred;

    return Result;
}

FSP_API NTSTATUS FspFileSystemOpFileSystemControl(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    PREPARSE_DATA_BUFFER ReparseData;
    SIZE_T Offset, Size;

    Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (Request->Req.FileSystemControl.FsControlCode)
    {
    case FSCTL_GET_REPARSE_POINT:
        if (0 != FileSystem->Interface->GetReparsePoint)
        {
            ReparseData = (PREPARSE_DATA_BUFFER)Response->Buffer;
            memset(ReparseData, 0, sizeof *ReparseData);

            if (FileSystem->ReparsePointsSymbolicLinks)
            {
                Size = FSP_FSCTL_TRANSACT_RSP_SIZEMAX - FIELD_OFFSET(FSP_FSCTL_TRANSACT_RSP, Buffer) -
                    FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer);
                Result = FileSystem->Interface->GetReparsePoint(FileSystem, Request,
                    (PVOID)Request->Req.FileSystemControl.UserContext,
                    (PWSTR)Request->Buffer,
                    ReparseData->SymbolicLinkReparseBuffer.PathBuffer,
                    &Size);
                if (NT_SUCCESS(Result))
                {
                    Offset = 0;
                    if (Size > 4 * sizeof(WCHAR) &&
                        '\\' == ReparseData->SymbolicLinkReparseBuffer.PathBuffer[0] &&
                        '?'  == ReparseData->SymbolicLinkReparseBuffer.PathBuffer[1] &&
                        '?'  == ReparseData->SymbolicLinkReparseBuffer.PathBuffer[2] &&
                        '\\' == ReparseData->SymbolicLinkReparseBuffer.PathBuffer[3])
                        Offset = 4 * sizeof(WCHAR);

                    ReparseData->ReparseTag = IO_REPARSE_TAG_SYMLINK;
                    ReparseData->ReparseDataLength = (USHORT)(
                        FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) -
                        FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer) +
                        Size);
                    ReparseData->SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;
                    ReparseData->SymbolicLinkReparseBuffer.SubstituteNameLength = (USHORT)Size;
                    ReparseData->SymbolicLinkReparseBuffer.PrintNameOffset = (USHORT)Offset;
                    ReparseData->SymbolicLinkReparseBuffer.PrintNameLength = (USHORT)(Size - Offset);
                    ReparseData->SymbolicLinkReparseBuffer.Flags = 0 == Offset ?
                        0 : SYMLINK_FLAG_RELATIVE;

                    Response->Rsp.FileSystemControl.Buffer.Size = (UINT16)(
                        FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) +
                        Size);
                }
            }
            else
            {
                Size = FSP_FSCTL_TRANSACT_RSP_SIZEMAX - FIELD_OFFSET(FSP_FSCTL_TRANSACT_RSP, Buffer);
                Result = FileSystem->Interface->GetReparsePoint(FileSystem, Request,
                    (PVOID)Request->Req.FileSystemControl.UserContext,
                    (PWSTR)Request->Buffer, ReparseData, &Size);
                if (NT_SUCCESS(Result))
                    Response->Rsp.FileSystemControl.Buffer.Size = (UINT16)Size;
            }
        }
        break;
    case FSCTL_SET_REPARSE_POINT:
        if (0 != FileSystem->Interface->SetReparsePoint)
        {
            ReparseData = (PREPARSE_DATA_BUFFER)
                (Request->Buffer + Request->Req.FileSystemControl.Buffer.Offset);

            if (FileSystem->ReparsePointsSymbolicLinks)
            {
                Result = FileSystem->Interface->SetReparsePoint(FileSystem, Request,
                    (PVOID)Request->Req.FileSystemControl.UserContext,
                    (PWSTR)Request->Buffer,
                    ReparseData->SymbolicLinkReparseBuffer.PathBuffer +
                        ReparseData->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR),
                    ReparseData->SymbolicLinkReparseBuffer.SubstituteNameLength);
            }
            else
                Result = FileSystem->Interface->SetReparsePoint(FileSystem, Request,
                    (PVOID)Request->Req.FileSystemControl.UserContext,
                    (PWSTR)Request->Buffer,
                    ReparseData,
                    Request->Req.FileSystemControl.Buffer.Size);
        }
        break;
    case FSCTL_DELETE_REPARSE_POINT:
        if (0 != FileSystem->Interface->SetReparsePoint)
            Result = FileSystem->Interface->SetReparsePoint(FileSystem, Request,
                (PVOID)Request->Req.FileSystemControl.UserContext,
                (PWSTR)Request->Buffer, 0, 0);
        break;
    }

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

FSP_API BOOLEAN FspFileSystemAddDirInfo(FSP_FSCTL_DIR_INFO *DirInfo,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    static UINT8 Zero[sizeof DirInfo->Size] = { 0 };
    PVOID BufferEnd = (PUINT8)Buffer + Length;
    PVOID SrcBuffer;
    ULONG SrcLength, DstLength;

    if (0 != DirInfo)
    {
        SrcBuffer = DirInfo;
        SrcLength = DirInfo->Size;
        DstLength = FSP_FSCTL_DEFAULT_ALIGN_UP(SrcLength);
    }
    else
    {
        SrcBuffer = &Zero;
        SrcLength = sizeof Zero;
        DstLength = SrcLength;
    }

    Buffer = (PVOID)((PUINT8)Buffer + *PBytesTransferred);
    if ((PUINT8)Buffer + DstLength > (PUINT8)BufferEnd)
        return FALSE;

    memcpy(Buffer, SrcBuffer, SrcLength);
    *PBytesTransferred += DstLength;

    return TRUE;
}

FSP_API BOOLEAN FspFileSystemFindReparsePoint(FSP_FILE_SYSTEM *FileSystem,
    NTSTATUS (*GetReparsePointByName)(
        FSP_FILE_SYSTEM *FileSystem, PVOID Context, PWSTR FileName, PVOID Buffer, PSIZE_T PSize),
    PVOID Context,
    PWSTR FileName, PUINT32 PReparsePointIndex)
{
    PWSTR p, lastp;
    NTSTATUS Result;

    p = FileName;
    for (;;)
    {
        while (L'\\' == *p)
            p++;
        lastp = p;
        while (L'\\' != *p)
        {
            if (L'\0' == *p)
                return FALSE;
            p++;
        }

        *p = L'\0';
        Result = GetReparsePointByName(FileSystem, Context, FileName, 0, 0);
        *p = L'\\';

        if (!NT_SUCCESS(Result))
            return FALSE;
        else
        {
            if (0 != PReparsePointIndex)
                *PReparsePointIndex = (ULONG)(lastp - FileName);

            return TRUE;
        }
    }

    return FALSE;
}

FSP_API NTSTATUS FspFileSystemResolveReparsePoints(FSP_FILE_SYSTEM *FileSystem,
    NTSTATUS (*GetReparsePointByName)(
        FSP_FILE_SYSTEM *FileSystem, PVOID Context, PWSTR FileName, PVOID Buffer, PSIZE_T PSize),
    PVOID Context,
    PWSTR FileName, UINT32 ReparsePointIndex, BOOLEAN OpenReparsePoint,
    PIO_STATUS_BLOCK PIoStatus, PVOID Buffer, PSIZE_T PSize)
{
    WCHAR c, *p, *lastp;
    PWSTR TargetPath, TargetPathEnd, TargetLink;
    union
    {
        REPARSE_DATA_BUFFER V;
        UINT8 B[FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) +
            /* assumption: the substitute and print paths fit in the same buffer */
            FSP_FSCTL_TRANSACT_PATH_SIZEMAX];
    } ReparseDataBuf;
    PREPARSE_DATA_BUFFER ReparseData = &ReparseDataBuf.V;
    SIZE_T Size, MaxTries = 32;
    NTSTATUS Result;

    TargetPath = Buffer;
    TargetPathEnd = TargetPath + *PSize / sizeof(WCHAR);
    Size = (lstrlenW(FileName) + 1) * sizeof(WCHAR);
    if (Size > *PSize)
        return STATUS_OBJECT_NAME_INVALID;
    memcpy(TargetPath, FileName, Size);

    p = TargetPath + ReparsePointIndex;
    c = *p;

    for (;;)
    {
        while (L'\\' == *p)
            p++;
        lastp = p;
        while (L'\\' != *p)
        {
            if (L'\0' == *p)
            {
                if (!OpenReparsePoint)
                    goto exit;
                OpenReparsePoint = FALSE;
                break;
            }
            p++;
        }

        /* handle dot and dotdot! */
        if (L'.' == lastp[0])
        {
            if (p == lastp + 1)
                /* dot */
                continue;

            if (L'.' == lastp[1] && p == lastp + 2)
            {
                /* dotdot */
                p = lastp;
                while (TargetPath < p)
                {
                    p--;
                    if (L'\\' != *p)
                        break;
                }
                while (TargetPath < p)
                {
                    p--;
                    if (L'\\' == *p)
                        break;
                }
                continue;
            }
        }

        c = *p;
        *p = '\0';
        if (FileSystem->ReparsePointsSymbolicLinks)
        {
            Size = FSP_FSCTL_TRANSACT_PATH_SIZEMAX;
            Result = GetReparsePointByName(FileSystem, Context, TargetPath,
                ReparseData->SymbolicLinkReparseBuffer.PathBuffer, &Size);
        }
        else
        {
            Size = sizeof ReparseDataBuf;
            Result = GetReparsePointByName(FileSystem, Context, TargetPath,
                ReparseData, &Size);
        }
        *p = c;

        if (STATUS_NOT_A_REPARSE_POINT == Result)
            /* it was not a reparse point; continue */
            continue;
        else if (!NT_SUCCESS(Result))
        {
            if (STATUS_OBJECT_NAME_NOT_FOUND == Result && '\0' != c)
                Result = STATUS_OBJECT_PATH_NOT_FOUND;
            return Result;
        }

        /* found a reparse point */

        if (0 == --MaxTries)
            return STATUS_REPARSE_POINT_NOT_RESOLVED;

        if (FileSystem->ReparsePointsSymbolicLinks)
            TargetLink = ReparseData->SymbolicLinkReparseBuffer.PathBuffer;
        else
        {
            if (IO_REPARSE_TAG_SYMLINK == ReparseData->ReparseTag)
            {
                TargetLink = ReparseData->SymbolicLinkReparseBuffer.PathBuffer +
                    ReparseData->SymbolicLinkReparseBuffer.SubstituteNameOffset;
                Size = ReparseData->SymbolicLinkReparseBuffer.SubstituteNameLength;
            }
            else
            {
                /* not a symlink; return the full reparse point! */
                if (Size > *PSize)
                    return STATUS_OBJECT_NAME_INVALID;
                memcpy(Buffer, ReparseData, Size);

                goto no_symlink_exit;
            }
        }

        if (Size > 4 * sizeof(WCHAR) &&
            '\\' == TargetLink[0] &&
            '?'  == TargetLink[1] &&
            '?'  == TargetLink[2] &&
            '\\' == TargetLink[3])
        {
            /* absolute symlink; replace whole path */
            if (Size > *PSize)
                return STATUS_OBJECT_NAME_INVALID;
            memcpy(TargetPath, TargetLink, Size);
            p = TargetPath + Size / sizeof(WCHAR);

            /* absolute symlinks on Windows are in NT namespace; send to FSD for further processing */
            goto exit;
        }
        else
        {
            /* relative symlink; replace last path component seen */
            if (Size + sizeof(WCHAR) > (TargetPathEnd - lastp) * sizeof(WCHAR))
                return STATUS_OBJECT_NAME_INVALID;
            memcpy(lastp, TargetLink, Size);
            lastp[Size / sizeof(WCHAR)] = L'\0';
            p = lastp;
        }
    }

exit:
    *PSize = (PUINT8)p - (PUINT8)TargetPath;
    PIoStatus->Status = STATUS_REPARSE;
    PIoStatus->Information = 0/*IO_REPARSE*/;
    return STATUS_REPARSE;

no_symlink_exit:
    *PSize = Size;
    PIoStatus->Status = STATUS_REPARSE;
    PIoStatus->Information = ReparseData->ReparseTag;
    return STATUS_REPARSE;
}
