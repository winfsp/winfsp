/**
 * @file dll/fsop.c
 *
 * @copyright 2015-2021 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#include <dll/library.h>

#define AddrOfFileContext(s)            \
    (                                   \
        (PVOID)&(((PUINT64)&(s).UserContext)[FileSystem->UmFileContextIsUserContext2])\
    )
#define ValOfFileContext(s)             \
    (                                   \
        FileSystem->UmFileContextIsFullContext ?\
            (PVOID)(&(s)) :             \
            (PVOID)(((PUINT64)&(s).UserContext)[FileSystem->UmFileContextIsUserContext2])\
    )
#define SetFileContext(t, s)            \
    (                                   \
        FileSystem->UmFileContextIsFullContext ?\
            (VOID)(                     \
                (t).UserContext = (s).UserContext,\
                (t).UserContext2 = (s).UserContext2\
            ) :                         \
            (VOID)(                     \
                ((PUINT64)&(t).UserContext)[FileSystem->UmFileContextIsUserContext2] =\
                ((PUINT64)&(s).UserContext)[FileSystem->UmFileContextIsUserContext2]\
            )                           \
    )

FSP_API NTSTATUS FspFileSystemOpEnter(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    switch (FileSystem->OpGuardStrategy)
    {
    case FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE:
        if ((FspFsctlTransactCreateKind == Request->Kind &&
                FILE_OPEN != ((Request->Req.Create.CreateOptions >> 24) & 0xff)) ||
            FspFsctlTransactOverwriteKind == Request->Kind ||
            (FspFsctlTransactCleanupKind == Request->Kind &&
                Request->Req.Cleanup.Delete) ||
            (FspFsctlTransactSetInformationKind == Request->Kind &&
                (10/*FileRenameInformation*/ == Request->Req.SetInformation.FileInformationClass ||
                65/*FileRenameInformationEx*/ == Request->Req.SetInformation.FileInformationClass)) ||
            FspFsctlTransactSetVolumeInformationKind == Request->Kind ||
            (FspFsctlTransactFlushBuffersKind == Request->Kind &&
                0 == Request->Req.FlushBuffers.UserContext &&
                0 == Request->Req.FlushBuffers.UserContext2))
        {
            AcquireSRWLockExclusive(&FileSystem->OpGuardLock);
        }
        else
        if (FspFsctlTransactCreateKind == Request->Kind ||
            (FspFsctlTransactSetInformationKind == Request->Kind &&
                (13/*FileDispositionInformation*/ == Request->Req.SetInformation.FileInformationClass ||
                64/*FileDispositionInformationEx*/ == Request->Req.SetInformation.FileInformationClass)) ||
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
            FspFsctlTransactOverwriteKind == Request->Kind ||
            (FspFsctlTransactCleanupKind == Request->Kind &&
                Request->Req.Cleanup.Delete) ||
            (FspFsctlTransactSetInformationKind == Request->Kind &&
                (10/*FileRenameInformation*/ == Request->Req.SetInformation.FileInformationClass ||
                65/*FileRenameInformationEx*/ == Request->Req.SetInformation.FileInformationClass)) ||
            FspFsctlTransactSetVolumeInformationKind == Request->Kind ||
            (FspFsctlTransactFlushBuffersKind == Request->Kind &&
                0 == Request->Req.FlushBuffers.UserContext &&
                0 == Request->Req.FlushBuffers.UserContext2))
        {
            ReleaseSRWLockExclusive(&FileSystem->OpGuardLock);
        }
        else
        if (FspFsctlTransactCreateKind == Request->Kind ||
            (FspFsctlTransactSetInformationKind == Request->Kind &&
                (13/*FileDispositionInformation*/ == Request->Req.SetInformation.FileInformationClass ||
                64/*FileDispositionInformationEx*/ == Request->Req.SetInformation.FileInformationClass)) ||
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
        Size = FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX;
        Result = FileSystem->Interface->ResolveReparsePoints(FileSystem,
            (PWSTR)Request->Buffer,
            ReparsePointIndex,
            !(Request->Req.Create.CreateOptions & FILE_OPEN_REPARSE_POINT),
            &IoStatus,
            Response->Buffer,
            &Size);
        if (NT_SUCCESS(Result))
        {
            Result = STATUS_REPARSE;
            Response->IoStatus.Information = (UINT32)IoStatus.Information;

            Response->Size = (UINT16)(sizeof *Response + Size);
            Response->Rsp.Create.Reparse.Buffer.Offset = 0;
            Response->Rsp.Create.Reparse.Buffer.Size = (UINT16)Size;
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
    UINT32 ParentDesiredAccess, GrantedAccess;

    /*
     * CreateCheck does different checks depending on whether we are
     * creating a new file/directory or a new stream.
     *
     * -   CreateCheck for a new file consists of checking the parent directory
     *     for the FILE_ADD_SUBDIRECTORY or FILE_ADD_FILE rights (depending on
     *     whether we are creating a file or directory).
     *
     *     If the access check succeeds and MAXIMUM_ALLOWED has been requested
     *     then we go ahead and grant all access to the creator.
     *
     * -   CreateCheck for a new stream consists of checking the main file for
     *     FILE_WRITE_DATA access, unless FILE_DELETE_ON_CLOSE is requested in
     *     which case we also check for DELETE access.
     *
     *     If the access check succeeds and MAXIMUM_ALLOWED was not requested
     *     then we reset the DELETE and FILE_WRITE_DATA accesses based on whether
     *     they were actually requested in DesiredAccess.
     */

    if (!Request->Req.Create.NamedStream)
    {
        if (Request->Req.Create.HasRestorePrivilege)
            ParentDesiredAccess = 0;
        else if (Request->Req.Create.CreateOptions & FILE_DIRECTORY_FILE)
            ParentDesiredAccess = FILE_ADD_SUBDIRECTORY;
        else
            ParentDesiredAccess = FILE_ADD_FILE;
        if (Request->Req.Create.HasTrailingBackslash &&
            !(Request->Req.Create.CreateOptions & FILE_DIRECTORY_FILE))
            Result = STATUS_OBJECT_NAME_INVALID;
        else if ((Request->Req.Create.FileAttributes & FILE_ATTRIBUTE_READONLY) &&
            (Request->Req.Create.CreateOptions & FILE_DELETE_ON_CLOSE))
            Result = STATUS_CANNOT_DELETE;
        else
            Result = FspAccessCheckEx(FileSystem, Request, TRUE, AllowTraverseCheck,
                ParentDesiredAccess,
                &GrantedAccess, PSecurityDescriptor);
        if (STATUS_REPARSE == Result)
            Result = FspFileSystemCallResolveReparsePoints(FileSystem, Request, Response, GrantedAccess);
        else if (NT_SUCCESS(Result))
        {
            *PGrantedAccess = (MAXIMUM_ALLOWED & Request->Req.Create.DesiredAccess) ?
                FspGetFileGenericMapping()->GenericAll : Request->Req.Create.DesiredAccess;
            *PGrantedAccess |= Request->Req.Create.GrantedAccess;
        }
    }
    else
    {
        *PSecurityDescriptor = 0;

        if (Request->Req.Create.HasTrailingBackslash)
            Result = STATUS_OBJECT_NAME_INVALID;
        else
            Result = FspAccessCheckEx(FileSystem, Request, TRUE, AllowTraverseCheck,
                Request->Req.Create.DesiredAccess |
                    FILE_WRITE_DATA |
                    ((Request->Req.Create.CreateOptions & FILE_DELETE_ON_CLOSE) ? DELETE : 0),
                &GrantedAccess, 0);
        if (STATUS_REPARSE == Result)
            Result = FspFileSystemCallResolveReparsePoints(FileSystem, Request, Response, GrantedAccess);
        else if (NT_SUCCESS(Result))
        {
            *PGrantedAccess = GrantedAccess;
            if (0 == (Request->Req.Create.DesiredAccess & MAXIMUM_ALLOWED))
                *PGrantedAccess &= ~(DELETE | FILE_WRITE_DATA) |
                    (Request->Req.Create.DesiredAccess & (DELETE | FILE_WRITE_DATA));
            *PGrantedAccess |= Request->Req.Create.GrantedAccess;
        }
    }

    return Result;
}

static inline
NTSTATUS FspFileSystemOpenCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response,
    BOOLEAN AllowTraverseCheck, PUINT32 PGrantedAccess,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
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

    Result = FspAccessCheckEx(FileSystem, Request, FALSE, AllowTraverseCheck,
        Request->Req.Create.DesiredAccess |
            ((Request->Req.Create.CreateOptions & FILE_DELETE_ON_CLOSE) ? DELETE : 0),
        &GrantedAccess,
        Request->Req.Create.AcceptsSecurityDescriptor ? PSecurityDescriptor : 0);
    if (STATUS_REPARSE == Result)
        Result = FspFileSystemCallResolveReparsePoints(FileSystem, Request, Response, GrantedAccess);
    else if (NT_SUCCESS(Result))
    {
        *PGrantedAccess = GrantedAccess;
        if (0 == (Request->Req.Create.DesiredAccess & MAXIMUM_ALLOWED))
            *PGrantedAccess &= ~DELETE | (Request->Req.Create.DesiredAccess & DELETE);
        *PGrantedAccess |= Request->Req.Create.GrantedAccess;
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
        *PGrantedAccess |= Request->Req.Create.GrantedAccess;
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
        *PGrantedAccess = GrantedAccess | Request->Req.Create.GrantedAccess;

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
        (65/*FileRenameInformationEx*/ == Request->Req.SetInformation.FileInformationClass &&
        0 != (0x40/*IGNORE_READONLY_ATTRIBUTE*/ & Request->Req.SetInformation.Info.RenameEx.Flags) ?
            0 :
            FILE_DELETE_ON_CLOSE) |     /* force read-only check! */
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

static inline
VOID FspFileSystemOpCreate_SetOpenDescriptor(FSP_FSCTL_TRANSACT_RSP *Response,
    PSECURITY_DESCRIPTOR OpenDescriptor)
{
    FSP_FSCTL_TRANSACT_BUF Buf;
    Buf.Offset = FSP_FSCTL_DEFAULT_ALIGN_UP(Response->Rsp.Create.Opened.FileName.Size);
    Buf.Size = (UINT16)GetSecurityDescriptorLength(OpenDescriptor);

    if (FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX >= Buf.Offset + Buf.Size)
    {
        Response->Size += Buf.Offset + Buf.Size;
        Response->Rsp.Create.Opened.SecurityDescriptor.Offset = Buf.Offset;
        Response->Rsp.Create.Opened.SecurityDescriptor.Size = Buf.Size;
        Response->Rsp.Create.Opened.HasSecurityDescriptor = 1;
        memcpy(Response->Buffer + Buf.Offset, OpenDescriptor, Buf.Size);
    }
}

static NTSTATUS FspFileSystemOpCreate_FileCreate(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;
    PSECURITY_DESCRIPTOR ParentDescriptor;
    FSP_FSCTL_TRANSACT_FULL_CONTEXT FullContext;
    FSP_FSCTL_OPEN_FILE_INFO OpenFileInfo;
    PSECURITY_DESCRIPTOR OpenDescriptor = 0;

    Result = FspFileSystemCreateCheck(FileSystem, Request, Response, TRUE,
        &GrantedAccess, &ParentDescriptor);
    if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
        return Result;

    Result = FspCreateSecurityDescriptor(FileSystem, Request, ParentDescriptor, &OpenDescriptor);
    FspDeleteSecurityDescriptor(ParentDescriptor, FspAccessCheckEx);
    if (!NT_SUCCESS(Result))
        return Result;

    FullContext.UserContext = 0;
    FullContext.UserContext2 = 0;
    memset(&OpenFileInfo, 0, sizeof OpenFileInfo);
    OpenFileInfo.NormalizedName = (PVOID)Response->Buffer;
    OpenFileInfo.NormalizedNameSize = FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX;
    if (0 != FileSystem->Interface->CreateEx)
        Result = FileSystem->Interface->CreateEx(FileSystem,
            (PWSTR)Request->Buffer, Request->Req.Create.CreateOptions, GrantedAccess,
            Request->Req.Create.FileAttributes, OpenDescriptor, Request->Req.Create.AllocationSize,
            0 != Request->Req.Create.Ea.Size ?
                (PVOID)(Request->Buffer + Request->Req.Create.Ea.Offset) : 0,
            Request->Req.Create.Ea.Size,
            Request->Req.Create.EaIsReparsePoint,
            AddrOfFileContext(FullContext), &OpenFileInfo.FileInfo);
    else
        Result = FileSystem->Interface->Create(FileSystem,
            (PWSTR)Request->Buffer, Request->Req.Create.CreateOptions, GrantedAccess,
            Request->Req.Create.FileAttributes, OpenDescriptor, Request->Req.Create.AllocationSize,
            AddrOfFileContext(FullContext), &OpenFileInfo.FileInfo);
    if (!NT_SUCCESS(Result))
    {
        FspDeleteSecurityDescriptor(OpenDescriptor, FspCreateSecurityDescriptor);
        return Result;
    }

    if (FSP_FSCTL_TRANSACT_PATH_SIZEMAX >= OpenFileInfo.NormalizedNameSize)
    {
        Response->Size = (UINT16)(sizeof *Response + OpenFileInfo.NormalizedNameSize);
        Response->Rsp.Create.Opened.FileName.Offset = 0;
        Response->Rsp.Create.Opened.FileName.Size = (UINT16)OpenFileInfo.NormalizedNameSize;
    }

    if (0 != OpenDescriptor)
    {
        FspFileSystemOpCreate_SetOpenDescriptor(Response, OpenDescriptor);
        FspDeleteSecurityDescriptor(OpenDescriptor, FspCreateSecurityDescriptor);
    }

    Response->IoStatus.Information = FILE_CREATED;
    SetFileContext(Response->Rsp.Create.Opened, FullContext);
    Response->Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    memcpy(&Response->Rsp.Create.Opened.FileInfo,
        &OpenFileInfo.FileInfo, sizeof OpenFileInfo.FileInfo);
    return STATUS_SUCCESS;
}

static NTSTATUS FspFileSystemOpCreate_FileOpen(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;
    FSP_FSCTL_TRANSACT_FULL_CONTEXT FullContext;
    FSP_FSCTL_OPEN_FILE_INFO OpenFileInfo;
    PSECURITY_DESCRIPTOR OpenDescriptor = 0;

    Result = FspFileSystemOpenCheck(FileSystem, Request, Response, TRUE, &GrantedAccess,
        &OpenDescriptor);
    if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
        return Result;

    FullContext.UserContext = 0;
    FullContext.UserContext2 = 0;
    memset(&OpenFileInfo, 0, sizeof OpenFileInfo);
    OpenFileInfo.NormalizedName = (PVOID)Response->Buffer;
    OpenFileInfo.NormalizedNameSize = FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX;
    Result = FileSystem->Interface->Open(FileSystem,
        (PWSTR)Request->Buffer, Request->Req.Create.CreateOptions, GrantedAccess,
        AddrOfFileContext(FullContext), &OpenFileInfo.FileInfo);
    if (!NT_SUCCESS(Result))
    {
        FspDeleteSecurityDescriptor(OpenDescriptor, FspAccessCheckEx);
        return Result;
    }

    if (FSP_FSCTL_TRANSACT_PATH_SIZEMAX >= OpenFileInfo.NormalizedNameSize)
    {
        Response->Size = (UINT16)(sizeof *Response + OpenFileInfo.NormalizedNameSize);
        Response->Rsp.Create.Opened.FileName.Offset = 0;
        Response->Rsp.Create.Opened.FileName.Size = (UINT16)OpenFileInfo.NormalizedNameSize;
    }

    if (0 != OpenDescriptor)
    {
        FspFileSystemOpCreate_SetOpenDescriptor(Response, OpenDescriptor);
        FspDeleteSecurityDescriptor(OpenDescriptor, FspAccessCheckEx);
    }

    Response->IoStatus.Information = FILE_OPENED;
    SetFileContext(Response->Rsp.Create.Opened, FullContext);
    Response->Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    memcpy(&Response->Rsp.Create.Opened.FileInfo,
        &OpenFileInfo.FileInfo, sizeof OpenFileInfo.FileInfo);
    return STATUS_SUCCESS;
}

static NTSTATUS FspFileSystemOpCreate_FileOpenIf(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;
    PSECURITY_DESCRIPTOR ParentDescriptor;
    FSP_FSCTL_TRANSACT_FULL_CONTEXT FullContext;
    FSP_FSCTL_OPEN_FILE_INFO OpenFileInfo;
    PSECURITY_DESCRIPTOR OpenDescriptor = 0;
    BOOLEAN Create = FALSE;

    Result = FspFileSystemOpenCheck(FileSystem, Request, Response, TRUE, &GrantedAccess,
        &OpenDescriptor);
    if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
    {
        if (STATUS_OBJECT_NAME_NOT_FOUND != Result)
            return Result;
        Create = TRUE;
    }

    if (!Create)
    {
        FullContext.UserContext = 0;
        FullContext.UserContext2 = 0;
        memset(&OpenFileInfo, 0, sizeof OpenFileInfo);
        OpenFileInfo.NormalizedName = (PVOID)Response->Buffer;
        OpenFileInfo.NormalizedNameSize = FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX;
        Result = FileSystem->Interface->Open(FileSystem,
            (PWSTR)Request->Buffer, Request->Req.Create.CreateOptions, GrantedAccess,
            AddrOfFileContext(FullContext), &OpenFileInfo.FileInfo);
        if (!NT_SUCCESS(Result))
        {
            FspDeleteSecurityDescriptor(OpenDescriptor, FspAccessCheckEx);
            OpenDescriptor = 0;

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

        Result = FspCreateSecurityDescriptor(FileSystem, Request, ParentDescriptor, &OpenDescriptor);
        FspDeleteSecurityDescriptor(ParentDescriptor, FspAccessCheckEx);
        if (!NT_SUCCESS(Result))
            return Result;

        FullContext.UserContext = 0;
        FullContext.UserContext2 = 0;
        memset(&OpenFileInfo, 0, sizeof OpenFileInfo);
        OpenFileInfo.NormalizedName = (PVOID)Response->Buffer;
        OpenFileInfo.NormalizedNameSize = FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX;
        if (0 != FileSystem->Interface->CreateEx)
            Result = FileSystem->Interface->CreateEx(FileSystem,
                (PWSTR)Request->Buffer, Request->Req.Create.CreateOptions, GrantedAccess,
                Request->Req.Create.FileAttributes, OpenDescriptor, Request->Req.Create.AllocationSize,
                0 != Request->Req.Create.Ea.Size ?
                    (PVOID)(Request->Buffer + Request->Req.Create.Ea.Offset) : 0,
                Request->Req.Create.Ea.Size,
                Request->Req.Create.EaIsReparsePoint,
                AddrOfFileContext(FullContext), &OpenFileInfo.FileInfo);
        else
            Result = FileSystem->Interface->Create(FileSystem,
                (PWSTR)Request->Buffer, Request->Req.Create.CreateOptions, GrantedAccess,
                Request->Req.Create.FileAttributes, OpenDescriptor, Request->Req.Create.AllocationSize,
                AddrOfFileContext(FullContext), &OpenFileInfo.FileInfo);
        if (!NT_SUCCESS(Result))
        {
            FspDeleteSecurityDescriptor(OpenDescriptor, FspCreateSecurityDescriptor);
            return Result;
        }
    }

    if (FSP_FSCTL_TRANSACT_PATH_SIZEMAX >= OpenFileInfo.NormalizedNameSize)
    {
        Response->Size = (UINT16)(sizeof *Response + OpenFileInfo.NormalizedNameSize);
        Response->Rsp.Create.Opened.FileName.Offset = 0;
        Response->Rsp.Create.Opened.FileName.Size = (UINT16)OpenFileInfo.NormalizedNameSize;
    }

    if (0 != OpenDescriptor)
    {
        FspFileSystemOpCreate_SetOpenDescriptor(Response, OpenDescriptor);
        FspDeleteSecurityDescriptor(OpenDescriptor, Create ?
            (NTSTATUS (*)())FspCreateSecurityDescriptor : (NTSTATUS (*)())FspAccessCheckEx);
    }

    Response->IoStatus.Information = Create ? FILE_CREATED : FILE_OPENED;
    SetFileContext(Response->Rsp.Create.Opened, FullContext);
    Response->Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    memcpy(&Response->Rsp.Create.Opened.FileInfo,
        &OpenFileInfo.FileInfo, sizeof OpenFileInfo.FileInfo);
    return STATUS_SUCCESS;
}

static NTSTATUS FspFileSystemOpCreate_FileOverwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;
    FSP_FSCTL_TRANSACT_FULL_CONTEXT FullContext;
    FSP_FSCTL_OPEN_FILE_INFO OpenFileInfo;

    Result = FspFileSystemOverwriteCheck(FileSystem, Request, Response, TRUE, &GrantedAccess);
    if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
        return Result;

    FullContext.UserContext = 0;
    FullContext.UserContext2 = 0;
    memset(&OpenFileInfo, 0, sizeof OpenFileInfo);
    OpenFileInfo.NormalizedName = (PVOID)Response->Buffer;
    OpenFileInfo.NormalizedNameSize = FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX;
    Result = FileSystem->Interface->Open(FileSystem,
        (PWSTR)Request->Buffer, Request->Req.Create.CreateOptions, GrantedAccess,
        AddrOfFileContext(FullContext), &OpenFileInfo.FileInfo);
    if (!NT_SUCCESS(Result))
        return Result;

    if (FSP_FSCTL_TRANSACT_PATH_SIZEMAX >= OpenFileInfo.NormalizedNameSize)
    {
        Response->Size = (UINT16)(sizeof *Response + OpenFileInfo.NormalizedNameSize);
        Response->Rsp.Create.Opened.FileName.Offset = 0;
        Response->Rsp.Create.Opened.FileName.Size = (UINT16)OpenFileInfo.NormalizedNameSize;
    }

    Response->IoStatus.Information = FILE_OVERWRITTEN;
    SetFileContext(Response->Rsp.Create.Opened, FullContext);
    Response->Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    memcpy(&Response->Rsp.Create.Opened.FileInfo,
        &OpenFileInfo.FileInfo, sizeof OpenFileInfo.FileInfo);
    return STATUS_SUCCESS;
}

static NTSTATUS FspFileSystemOpCreate_FileOverwriteIf(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    UINT32 GrantedAccess;
    PSECURITY_DESCRIPTOR ParentDescriptor, ObjectDescriptor;
    FSP_FSCTL_TRANSACT_FULL_CONTEXT FullContext;
    FSP_FSCTL_OPEN_FILE_INFO OpenFileInfo;
    BOOLEAN Supersede = FILE_SUPERSEDE == ((Request->Req.Create.CreateOptions >> 24) & 0xff);
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
        FullContext.UserContext = 0;
        FullContext.UserContext2 = 0;
        memset(&OpenFileInfo, 0, sizeof OpenFileInfo);
        OpenFileInfo.NormalizedName = (PVOID)Response->Buffer;
        OpenFileInfo.NormalizedNameSize = FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX;
        Result = FileSystem->Interface->Open(FileSystem,
            (PWSTR)Request->Buffer, Request->Req.Create.CreateOptions, GrantedAccess,
            AddrOfFileContext(FullContext), &OpenFileInfo.FileInfo);
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

        FullContext.UserContext = 0;
        FullContext.UserContext2 = 0;
        memset(&OpenFileInfo, 0, sizeof OpenFileInfo);
        OpenFileInfo.NormalizedName = (PVOID)Response->Buffer;
        OpenFileInfo.NormalizedNameSize = FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX;
        if (0 != FileSystem->Interface->CreateEx)
            Result = FileSystem->Interface->CreateEx(FileSystem,
                (PWSTR)Request->Buffer, Request->Req.Create.CreateOptions, GrantedAccess,
                Request->Req.Create.FileAttributes, ObjectDescriptor, Request->Req.Create.AllocationSize,
                0 != Request->Req.Create.Ea.Size ?
                    (PVOID)(Request->Buffer + Request->Req.Create.Ea.Offset) : 0,
                Request->Req.Create.Ea.Size,
                Request->Req.Create.EaIsReparsePoint,
                AddrOfFileContext(FullContext), &OpenFileInfo.FileInfo);
        else
            Result = FileSystem->Interface->Create(FileSystem,
                (PWSTR)Request->Buffer, Request->Req.Create.CreateOptions, GrantedAccess,
                Request->Req.Create.FileAttributes, ObjectDescriptor, Request->Req.Create.AllocationSize,
                AddrOfFileContext(FullContext), &OpenFileInfo.FileInfo);
        FspDeleteSecurityDescriptor(ObjectDescriptor, FspCreateSecurityDescriptor);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    if (FSP_FSCTL_TRANSACT_PATH_SIZEMAX >= OpenFileInfo.NormalizedNameSize)
    {
        Response->Size = (UINT16)(sizeof *Response + OpenFileInfo.NormalizedNameSize);
        Response->Rsp.Create.Opened.FileName.Offset = 0;
        Response->Rsp.Create.Opened.FileName.Size = (UINT16)OpenFileInfo.NormalizedNameSize;
    }

    Response->IoStatus.Information = Create ? FILE_CREATED :
        (Supersede ? FILE_SUPERSEDED : FILE_OVERWRITTEN);
    SetFileContext(Response->Rsp.Create.Opened, FullContext);
    Response->Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    memcpy(&Response->Rsp.Create.Opened.FileInfo,
        &OpenFileInfo.FileInfo, sizeof OpenFileInfo.FileInfo);
    return STATUS_SUCCESS;
}

static NTSTATUS FspFileSystemOpCreate_FileOpenTargetDirectory(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    WCHAR Root[2] = L"\\";
    PWSTR Parent, Suffix;
    UINT32 CreateOptions, GrantedAccess;
    FSP_FSCTL_TRANSACT_FULL_CONTEXT FullContext;
    FSP_FSCTL_OPEN_FILE_INFO OpenFileInfo;
    UINT32 Information;

    Result = FspFileSystemOpenTargetDirectoryCheck(FileSystem, Request, Response, &GrantedAccess);
    if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
        return Result;

    FullContext.UserContext = 0;
    FullContext.UserContext2 = 0;
    memset(&OpenFileInfo, 0, sizeof OpenFileInfo);
    OpenFileInfo.NormalizedName = (PVOID)Response->Buffer;
    OpenFileInfo.NormalizedNameSize = FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX;
    FspPathSuffix((PWSTR)Request->Buffer, &Parent, &Suffix, Root);
    CreateOptions =
        (Request->Req.Create.CreateOptions | FILE_DIRECTORY_FILE) & ~FILE_NON_DIRECTORY_FILE;
    Result = FileSystem->Interface->Open(FileSystem,
        Parent, CreateOptions, GrantedAccess,
        AddrOfFileContext(FullContext), &OpenFileInfo.FileInfo);
    FspPathCombine((PWSTR)Request->Buffer, Suffix);
    if (!NT_SUCCESS(Result))
        return Result;

    Information = FILE_OPENED;
    if (0 != FileSystem->Interface->GetSecurityByName)
    {
        Result = FileSystem->Interface->GetSecurityByName(FileSystem, (PWSTR)Request->Buffer, 0, 0, 0);
        Information = NT_SUCCESS(Result) ? FILE_EXISTS : FILE_DOES_NOT_EXIST;
    }

    if (FSP_FSCTL_TRANSACT_PATH_SIZEMAX >= OpenFileInfo.NormalizedNameSize)
    {
        Response->Size = (UINT16)(sizeof *Response + OpenFileInfo.NormalizedNameSize);
        Response->Rsp.Create.Opened.FileName.Offset = 0;
        Response->Rsp.Create.Opened.FileName.Size = (UINT16)OpenFileInfo.NormalizedNameSize;
    }

    Response->IoStatus.Information = Information;
    SetFileContext(Response->Rsp.Create.Opened, FullContext);
    Response->Rsp.Create.Opened.GrantedAccess = GrantedAccess;
    memcpy(&Response->Rsp.Create.Opened.FileInfo,
        &OpenFileInfo.FileInfo, sizeof OpenFileInfo.FileInfo);
    return STATUS_SUCCESS;
}

static NTSTATUS FspFileSystemOpCreate_NotFoundCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    /*
     * This handles an Open of a named stream done via a symlink. The file system
     * has returned STATUS_OBJECT_NAME_NOT_FOUND, but we check to see if the main
     * file is a reparse point that can be resolved.
     */

    NTSTATUS Result;
    UINT32 FileAttributes;

    if (0 == FileSystem->Interface->GetSecurityByName ||
        0 == FileSystem->Interface->ResolveReparsePoints)
        return STATUS_OBJECT_NAME_NOT_FOUND;

    if (!Request->Req.Create.NamedStream ||
        (Request->Req.Create.CreateOptions & FILE_OPEN_REPARSE_POINT))
        return STATUS_OBJECT_NAME_NOT_FOUND;

    ((PWSTR)Request->Buffer)[Request->Req.Create.NamedStream / sizeof(WCHAR)] = L'\0';
    Result = FileSystem->Interface->GetSecurityByName(
        FileSystem, (PWSTR)Request->Buffer, &FileAttributes, 0, 0);
    ((PWSTR)Request->Buffer)[Request->Req.Create.NamedStream / sizeof(WCHAR)] = L':';
    if (STATUS_SUCCESS != Result)
        return STATUS_OBJECT_NAME_NOT_FOUND;

    if (0 == (FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return STATUS_OBJECT_NAME_NOT_FOUND;

    FileAttributes = FspPathSuffixIndex((PWSTR)Request->Buffer);
    Result = FspFileSystemCallResolveReparsePoints(
        FileSystem, Request, Response, FileAttributes);
    if (STATUS_REPARSE != Result)
        return STATUS_OBJECT_NAME_NOT_FOUND;

    return STATUS_REPARSE;
}

static NTSTATUS FspFileSystemOpCreate_CollisionCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    /*
     * This handles a Create that resulted in STATUS_OBJECT_NAME_COLLISION. We
     * handle two separate cases:
     *
     * 1)  A Create colliding with a directory and the FILE_NON_DIRECTORY_FILE
     *     flag set. We then change the result code to STATUS_FILE_IS_A_DIRECTORY.
     *
     * 2)  A Create colliding with a symlink (reparse point) that can be resolved.
     *     In this case we resolve the reparse point and return STATUS_REPARSE.
     */

    NTSTATUS Result;
    UINT32 FileAttributes;

    if (0 == FileSystem->Interface->GetSecurityByName)
        return STATUS_OBJECT_NAME_COLLISION;

    Result = FileSystem->Interface->GetSecurityByName(
        FileSystem, (PWSTR)Request->Buffer, &FileAttributes, 0, 0);
    if (STATUS_SUCCESS != Result)
        return STATUS_OBJECT_NAME_COLLISION;

    if ((Request->Req.Create.CreateOptions & FILE_NON_DIRECTORY_FILE) &&
        (FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        return STATUS_FILE_IS_A_DIRECTORY;

    if (0 == FileSystem->Interface->ResolveReparsePoints)
        return STATUS_OBJECT_NAME_COLLISION;

    if (Request->Req.Create.CreateOptions & FILE_OPEN_REPARSE_POINT)
        return STATUS_OBJECT_NAME_COLLISION;

    if (0 == (FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return STATUS_OBJECT_NAME_COLLISION;

    FileAttributes = FspPathSuffixIndex((PWSTR)Request->Buffer);
    Result = FspFileSystemCallResolveReparsePoints(
        FileSystem, Request, Response, FileAttributes);
    if (STATUS_REPARSE != Result)
        return STATUS_OBJECT_NAME_COLLISION;

    return STATUS_REPARSE;
}

FSP_API NTSTATUS FspFileSystemOpCreate(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;

    if ((0 == FileSystem->Interface->Create && 0 == FileSystem->Interface->CreateEx) ||
        0 == FileSystem->Interface->Open ||
        (0 == FileSystem->Interface->Overwrite && 0 == FileSystem->Interface->OverwriteEx))
        return STATUS_INVALID_DEVICE_REQUEST;

    if (Request->Req.Create.OpenTargetDirectory)
        return FspFileSystemOpCreate_FileOpenTargetDirectory(FileSystem, Request, Response);

    switch ((Request->Req.Create.CreateOptions >> 24) & 0xff)
    {
    case FILE_CREATE:
        Result = FspFileSystemOpCreate_FileCreate(FileSystem, Request, Response);
        break;
    case FILE_OPEN:
        Result = FspFileSystemOpCreate_FileOpen(FileSystem, Request, Response);
        break;
    case FILE_OPEN_IF:
        Result = FspFileSystemOpCreate_FileOpenIf(FileSystem, Request, Response);
        break;
    case FILE_OVERWRITE:
        Result = FspFileSystemOpCreate_FileOverwrite(FileSystem, Request, Response);
        break;
    case FILE_OVERWRITE_IF:
    case FILE_SUPERSEDE:
        Result = FspFileSystemOpCreate_FileOverwriteIf(FileSystem, Request, Response);
        break;
    default:
        Result = STATUS_INVALID_PARAMETER;
        break;
    }

    if (STATUS_OBJECT_NAME_NOT_FOUND == Result)
        Result = FspFileSystemOpCreate_NotFoundCheck(FileSystem, Request, Response);
    else if (STATUS_OBJECT_NAME_COLLISION == Result)
        Result = FspFileSystemOpCreate_CollisionCheck(FileSystem, Request, Response);

    return Result;
}

FSP_API NTSTATUS FspFileSystemOpOverwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    FSP_FSCTL_FILE_INFO FileInfo;

    if (0 == FileSystem->Interface->Overwrite && 0 == FileSystem->Interface->OverwriteEx)
        return STATUS_INVALID_DEVICE_REQUEST;

    memset(&FileInfo, 0, sizeof FileInfo);
    if (0 != FileSystem->Interface->OverwriteEx)
        Result = FileSystem->Interface->OverwriteEx(FileSystem,
            (PVOID)ValOfFileContext(Request->Req.Overwrite),
            Request->Req.Overwrite.FileAttributes,
            Request->Req.Overwrite.Supersede,
            Request->Req.Overwrite.AllocationSize,
            0 != Request->Req.Overwrite.Ea.Size ?
                (PVOID)(Request->Buffer + Request->Req.Overwrite.Ea.Offset) : 0,
            Request->Req.Overwrite.Ea.Size,
            &FileInfo);
    else
        Result = FileSystem->Interface->Overwrite(FileSystem,
            (PVOID)ValOfFileContext(Request->Req.Overwrite),
            Request->Req.Overwrite.FileAttributes,
            Request->Req.Overwrite.Supersede,
            Request->Req.Overwrite.AllocationSize,
            &FileInfo);
    if (!NT_SUCCESS(Result))
    {
        if (0 != FileSystem->Interface->Close)
            FileSystem->Interface->Close(FileSystem,
                (PVOID)ValOfFileContext(Request->Req.Overwrite));
        return Result;
    }

    memcpy(&Response->Rsp.Overwrite.FileInfo, &FileInfo, sizeof FileInfo);
    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpCleanup(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    if (0 != FileSystem->Interface->Cleanup)
        FileSystem->Interface->Cleanup(FileSystem,
            (PVOID)ValOfFileContext(Request->Req.Cleanup),
            0 != Request->FileName.Size ? (PWSTR)Request->Buffer : 0,
            (0 != Request->Req.Cleanup.Delete ? FspCleanupDelete : 0) |
            (0 != Request->Req.Cleanup.SetAllocationSize ? FspCleanupSetAllocationSize : 0) |
            (0 != Request->Req.Cleanup.SetArchiveBit ? FspCleanupSetArchiveBit : 0) |
            (0 != Request->Req.Cleanup.SetLastAccessTime ? FspCleanupSetLastAccessTime : 0) |
            (0 != Request->Req.Cleanup.SetLastWriteTime ? FspCleanupSetLastWriteTime : 0) |
            (0 != Request->Req.Cleanup.SetChangeTime ? FspCleanupSetChangeTime : 0));

    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpClose(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    if (0 != FileSystem->Interface->Close)
        FileSystem->Interface->Close(FileSystem,
            (PVOID)ValOfFileContext(Request->Req.Close));

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
    Result = FileSystem->Interface->Read(FileSystem,
        (PVOID)ValOfFileContext(Request->Req.Read),
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
    Result = FileSystem->Interface->Write(FileSystem,
        (PVOID)ValOfFileContext(Request->Req.Write),
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
    NTSTATUS Result;
    FSP_FSCTL_FILE_INFO FileInfo;

    memset(&FileInfo, 0, sizeof FileInfo);
    if (0 == FileSystem->Interface->Flush)
        Result = FileSystem->Interface->GetFileInfo(FileSystem,
            (PVOID)ValOfFileContext(Request->Req.FlushBuffers), &FileInfo);
    else
        Result = FileSystem->Interface->Flush(FileSystem,
            (PVOID)ValOfFileContext(Request->Req.FlushBuffers), &FileInfo);
    if (!NT_SUCCESS(Result))
        return Result;

    memcpy(&Response->Rsp.FlushBuffers.FileInfo, &FileInfo, sizeof FileInfo);
    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpQueryInformation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    FSP_FSCTL_FILE_INFO FileInfo;

    if (0 == FileSystem->Interface->GetFileInfo)
        return STATUS_INVALID_DEVICE_REQUEST;

    memset(&FileInfo, 0, sizeof FileInfo);
    Result = FileSystem->Interface->GetFileInfo(FileSystem,
        (PVOID)ValOfFileContext(Request->Req.QueryInformation), &FileInfo);
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
            Result = FileSystem->Interface->SetBasicInfo(FileSystem,
                (PVOID)ValOfFileContext(Request->Req.SetInformation),
                Request->Req.SetInformation.Info.Basic.FileAttributes,
                Request->Req.SetInformation.Info.Basic.CreationTime,
                Request->Req.SetInformation.Info.Basic.LastAccessTime,
                Request->Req.SetInformation.Info.Basic.LastWriteTime,
                Request->Req.SetInformation.Info.Basic.ChangeTime,
                &FileInfo);
        break;
    case 19/*FileAllocationInformation*/:
        if (0 != FileSystem->Interface->SetFileSize)
            Result = FileSystem->Interface->SetFileSize(FileSystem,
                (PVOID)ValOfFileContext(Request->Req.SetInformation),
                Request->Req.SetInformation.Info.Allocation.AllocationSize, TRUE,
                &FileInfo);
        break;
    case 20/*FileEndOfFileInformation*/:
        if (0 != FileSystem->Interface->SetFileSize)
            Result = FileSystem->Interface->SetFileSize(FileSystem,
                (PVOID)ValOfFileContext(Request->Req.SetInformation),
                Request->Req.SetInformation.Info.EndOfFile.FileSize, FALSE,
                &FileInfo);
        break;
    case 13/*FileDispositionInformation*/:
    case 64/*FileDispositionInformationEx*/:
        if (0 == (0x10/*IGNORE_READONLY_ATTRIBUTE*/ & Request->Req.SetInformation.Info.DispositionEx.Flags) &&
            0 != FileSystem->Interface->GetFileInfo)
        {
            Result = FileSystem->Interface->GetFileInfo(FileSystem,
                (PVOID)ValOfFileContext(Request->Req.SetInformation), &FileInfo);
            if (NT_SUCCESS(Result) && 0 != (FileInfo.FileAttributes & FILE_ATTRIBUTE_READONLY))
            {
                Result = STATUS_CANNOT_DELETE;
                break;
            }
        }
        if (0 != FileSystem->Interface->SetDelete)
        {
            Result = FileSystem->Interface->SetDelete(FileSystem,
                (PVOID)ValOfFileContext(Request->Req.SetInformation),
                (PWSTR)Request->Buffer,
                0 != (1/*DELETE*/ & Request->Req.SetInformation.Info.DispositionEx.Flags));
        }
        else if (0 != FileSystem->Interface->CanDelete)
        {
            if (0 != (1/*DELETE*/ & Request->Req.SetInformation.Info.DispositionEx.Flags))
                Result = FileSystem->Interface->CanDelete(FileSystem,
                    (PVOID)ValOfFileContext(Request->Req.SetInformation),
                    (PWSTR)Request->Buffer);
            else
                Result = STATUS_SUCCESS;
        }
        break;
    case 10/*FileRenameInformation*/:
    case 65/*FileRenameInformationEx*/:
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
            Result = FileSystem->Interface->Rename(FileSystem,
                (PVOID)ValOfFileContext(Request->Req.SetInformation),
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

FSP_API NTSTATUS FspFileSystemOpQueryEa(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    ULONG BytesTransferred;

    if (0 == FileSystem->Interface->GetEa)
        return STATUS_INVALID_DEVICE_REQUEST;

    BytesTransferred = 0;
    Result = FileSystem->Interface->GetEa(FileSystem,
        (PVOID)ValOfFileContext(Request->Req.QueryEa),
        (PVOID)Response->Buffer, FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX, &BytesTransferred);
    if (!NT_SUCCESS(Result))
        return STATUS_BUFFER_OVERFLOW != Result ? Result : STATUS_EA_LIST_INCONSISTENT;

    Response->Size = (UINT16)(sizeof *Response + BytesTransferred);
    Response->Rsp.QueryEa.Ea.Offset = 0;
    Response->Rsp.QueryEa.Ea.Size = (UINT16)BytesTransferred;
    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpSetEa(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    FSP_FSCTL_FILE_INFO FileInfo;

    if (0 == FileSystem->Interface->SetEa)
        return STATUS_INVALID_DEVICE_REQUEST;

    memset(&FileInfo, 0, sizeof FileInfo);
    Result = FileSystem->Interface->SetEa(FileSystem,
        (PVOID)ValOfFileContext(Request->Req.SetEa),
        (PVOID)Request->Buffer, Request->Req.SetEa.Ea.Size,
        &FileInfo);
    if (!NT_SUCCESS(Result))
        return Result;

    memcpy(&Response->Rsp.SetEa.FileInfo, &FileInfo, sizeof FileInfo);
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
    Result = FileSystem->Interface->GetVolumeInfo(FileSystem, &VolumeInfo);
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
            Result = FileSystem->Interface->SetVolumeLabel(FileSystem,
                (PWSTR)Request->Buffer,
                &VolumeInfo);
        break;
    }

    if (!NT_SUCCESS(Result))
        return Result;

    memcpy(&Response->Rsp.SetVolumeInformation.VolumeInfo, &VolumeInfo, sizeof VolumeInfo);
    return STATUS_SUCCESS;
}

static NTSTATUS FspFileSystemOpQueryDirectory_GetDirInfoByName(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PWSTR FileName,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    NTSTATUS Result;
    union
    {
        FSP_FSCTL_DIR_INFO V;
        UINT8 B[sizeof(FSP_FSCTL_DIR_INFO) + 255 * sizeof(WCHAR)];
    } DirInfoBuf;
    FSP_FSCTL_DIR_INFO *DirInfo = &DirInfoBuf.V;

    /* The FSD will never send us a Marker that we need to worry about! */

    memset(DirInfo, 0, sizeof *DirInfo);
    Result = FileSystem->Interface->GetDirInfoByName(FileSystem, FileContext, FileName, DirInfo);
    if (NT_SUCCESS(Result))
    {
        if (FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred))
            FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);
    }
    else if (STATUS_OBJECT_NAME_NOT_FOUND == Result)
    {
        Result = STATUS_SUCCESS;
        FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);
    }

    return Result;
}

FSP_API NTSTATUS FspFileSystemOpQueryDirectory(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    ULONG BytesTransferred;

    if (0 == FileSystem->Interface->ReadDirectory)
        return STATUS_INVALID_DEVICE_REQUEST;

    BytesTransferred = 0;
    if (0 != FileSystem->Interface->GetDirInfoByName &&
        0 != Request->Req.QueryDirectory.Pattern.Size && Request->Req.QueryDirectory.PatternIsFileName)
        Result = FspFileSystemOpQueryDirectory_GetDirInfoByName(FileSystem,
            (PVOID)ValOfFileContext(Request->Req.QueryDirectory),
            (PWSTR)(Request->Buffer + Request->Req.QueryDirectory.Pattern.Offset),
            (PVOID)Request->Req.QueryDirectory.Address,
            Request->Req.QueryDirectory.Length,
            &BytesTransferred);
    else
        Result = FileSystem->Interface->ReadDirectory(FileSystem,
            (PVOID)ValOfFileContext(Request->Req.QueryDirectory),
            0 != Request->Req.QueryDirectory.Pattern.Size ?
                (PWSTR)(Request->Buffer + Request->Req.QueryDirectory.Pattern.Offset) : 0,
            0 != Request->Req.QueryDirectory.Marker.Size ?
                (PWSTR)(Request->Buffer + Request->Req.QueryDirectory.Marker.Offset) : 0,
            (PVOID)Request->Req.QueryDirectory.Address,
            Request->Req.QueryDirectory.Length,
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
    SIZE_T Size;

    Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (Request->Req.FileSystemControl.FsControlCode)
    {
    case FSCTL_GET_REPARSE_POINT:
        if (0 != FileSystem->Interface->GetReparsePoint)
        {
            ReparseData = (PREPARSE_DATA_BUFFER)Response->Buffer;
            memset(ReparseData, 0, sizeof *ReparseData);

            Size = FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX;
            Result = FileSystem->Interface->GetReparsePoint(FileSystem,
                (PVOID)ValOfFileContext(Request->Req.FileSystemControl),
                (PWSTR)Request->Buffer, ReparseData, &Size);
            if (NT_SUCCESS(Result))
            {
                Response->Size = (UINT16)(sizeof *Response + Size);
                Response->Rsp.FileSystemControl.Buffer.Offset = 0;
                Response->Rsp.FileSystemControl.Buffer.Size = (UINT16)Size;
            }
        }
        break;
    case FSCTL_SET_REPARSE_POINT:
        if (0 != FileSystem->Interface->SetReparsePoint)
        {
            ReparseData = (PREPARSE_DATA_BUFFER)
                (Request->Buffer + Request->Req.FileSystemControl.Buffer.Offset);

            Result = FileSystem->Interface->SetReparsePoint(FileSystem,
                (PVOID)ValOfFileContext(Request->Req.FileSystemControl),
                (PWSTR)Request->Buffer,
                ReparseData,
                Request->Req.FileSystemControl.Buffer.Size);
        }
        break;
    case FSCTL_DELETE_REPARSE_POINT:
        if (0 != FileSystem->Interface->DeleteReparsePoint)
        {
            ReparseData = (PREPARSE_DATA_BUFFER)
                (Request->Buffer + Request->Req.FileSystemControl.Buffer.Offset);

            Result = FileSystem->Interface->DeleteReparsePoint(FileSystem,
                (PVOID)ValOfFileContext(Request->Req.FileSystemControl),
                (PWSTR)Request->Buffer,
                ReparseData,
                Request->Req.FileSystemControl.Buffer.Size);
        }
        break;
    }

    return Result;
}

FSP_API NTSTATUS FspFileSystemOpDeviceControl(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    ULONG BytesTransferred;

    if (0 == FileSystem->Interface->Control)
        return STATUS_INVALID_DEVICE_REQUEST;

    Result = FileSystem->Interface->Control(FileSystem,
        (PVOID)ValOfFileContext(Request->Req.DeviceControl),
        Request->Req.DeviceControl.IoControlCode,
        Request->Buffer, Request->Req.DeviceControl.Buffer.Size,
        Response->Buffer, Request->Req.DeviceControl.OutputLength/* FSD guarantees correct size! */,
        &BytesTransferred);
    if (!NT_SUCCESS(Result))
        return STATUS_BUFFER_OVERFLOW != Result ? Result : STATUS_BUFFER_TOO_SMALL;

    Response->Size = (UINT16)(sizeof *Response + BytesTransferred);
    Response->Rsp.DeviceControl.Buffer.Offset = 0;
    Response->Rsp.DeviceControl.Buffer.Size = (UINT16)BytesTransferred;
    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFileSystemOpQuerySecurity(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    SIZE_T SecurityDescriptorSize;

    if (0 == FileSystem->Interface->GetSecurity)
        return STATUS_INVALID_DEVICE_REQUEST;

    SecurityDescriptorSize = FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX;
    Result = FileSystem->Interface->GetSecurity(FileSystem,
        (PVOID)ValOfFileContext(Request->Req.QuerySecurity),
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

    return FileSystem->Interface->SetSecurity(FileSystem,
        (PVOID)ValOfFileContext(Request->Req.SetSecurity),
        Request->Req.SetSecurity.SecurityInformation,
        (PSECURITY_DESCRIPTOR)Request->Buffer);
}

FSP_API NTSTATUS FspFileSystemOpQueryStreamInformation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;
    ULONG BytesTransferred;

    if (0 == FileSystem->Interface->GetStreamInfo)
        return STATUS_INVALID_DEVICE_REQUEST;

    BytesTransferred = 0;
    Result = FileSystem->Interface->GetStreamInfo(FileSystem,
        (PVOID)ValOfFileContext(Request->Req.QueryStreamInformation),
        Response->Buffer,
        FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX,
        &BytesTransferred);
    if (!NT_SUCCESS(Result))
        return Result;

    Response->Size = (UINT16)(sizeof *Response + BytesTransferred);
    Response->Rsp.QueryStreamInformation.Buffer.Offset = 0;
    Response->Rsp.QueryStreamInformation.Buffer.Size = (UINT16)BytesTransferred;
    return STATUS_SUCCESS;
}

FSP_FSCTL_STATIC_ASSERT(
    sizeof(UINT16) == sizeof ((FSP_FSCTL_DIR_INFO *)0)->Size &&
    sizeof(UINT16) == sizeof ((FSP_FSCTL_STREAM_INFO *)0)->Size,
    "FSP_FSCTL_DIR_INFO::Size and FSP_FSCTL_STREAM_INFO::Size: sizeof must be 2.");
static BOOLEAN FspFileSystemAddXxxInfo(PVOID Info,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    static UINT8 Zero[sizeof(UINT16)] = { 0 };
    PVOID BufferEnd = (PUINT8)Buffer + Length;
    PVOID SrcBuffer;
    ULONG SrcLength, DstLength;

    if (0 != Info)
    {
        SrcBuffer = Info;
        SrcLength = *(PUINT16)Info;
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

FSP_API BOOLEAN FspFileSystemAddDirInfo(FSP_FSCTL_DIR_INFO *DirInfo,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    return FspFileSystemAddXxxInfo(DirInfo, Buffer, Length, PBytesTransferred);
}

FSP_API BOOLEAN FspFileSystemFindReparsePoint(FSP_FILE_SYSTEM *FileSystem,
    NTSTATUS (*GetReparsePointByName)(
        FSP_FILE_SYSTEM *FileSystem, PVOID Context,
        PWSTR FileName, BOOLEAN IsDirectory, PVOID Buffer, PSIZE_T PSize),
    PVOID Context,
    PWSTR FileName, PUINT32 PReparsePointIndex)
{
    PWSTR RemainderPath, LastPathComponent;
    NTSTATUS Result;

    RemainderPath = FileName;

    for (;;)
    {
        while (L'\\' == *RemainderPath)
            RemainderPath++;
        LastPathComponent = RemainderPath;
        while (L'\\' != *RemainderPath)
        {
            if (L'\0' == *RemainderPath || L':' == *RemainderPath)
                return FALSE;
            RemainderPath++;
        }

        *RemainderPath = L'\0';
        Result = GetReparsePointByName(FileSystem, Context, FileName, TRUE, 0, 0);
        *RemainderPath = L'\\';

        if (STATUS_NOT_A_REPARSE_POINT == Result)
            /* it was not a reparse point; continue */
            continue;
        else if (!NT_SUCCESS(Result))
            return FALSE;

        /*
         * Found a reparse point!
         */

        if (0 != PReparsePointIndex)
            *PReparsePointIndex = (ULONG)(LastPathComponent - FileName);

        return TRUE;
    }

    return FALSE;
}

static NTSTATUS FspFileSystemResolveReparsePointsInternal(FSP_FILE_SYSTEM *FileSystem,
    NTSTATUS (*GetReparsePointByName)(
        FSP_FILE_SYSTEM *FileSystem, PVOID Context,
        PWSTR FileName, BOOLEAN IsDirectory, PVOID Buffer, PSIZE_T PSize),
    PVOID Context,
    PREPARSE_DATA_BUFFER ReparseData, SIZE_T ReparseDataSize0,
    PWSTR FileName, UINT32 ReparsePointIndex, BOOLEAN ResolveLastPathComponent0,
    PIO_STATUS_BLOCK PIoStatus, PVOID Buffer, PSIZE_T PSize)
{
    PREPARSE_DATA_BUFFER OutputReparseData;
    PWSTR TargetPath, RemainderPath, LastPathComponent, NewRemainderPath, ReparseTargetPath;
    WCHAR RemainderChar;
    SIZE_T ReparseDataSize, RemainderPathSize, ReparseTargetPathLength;
    BOOLEAN ResolveLastPathComponent;
    ULONG MaxTries = 32;
    NTSTATUS Result;

    RemainderPathSize = (lstrlenW(FileName) + 1) * sizeof(WCHAR);
    if (FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) +
        RemainderPathSize > *PSize)
        return STATUS_REPARSE_POINT_NOT_RESOLVED;

    OutputReparseData = Buffer;
    memset(OutputReparseData, 0,
        FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer));
    OutputReparseData->ReparseTag = IO_REPARSE_TAG_SYMLINK;
    OutputReparseData->SymbolicLinkReparseBuffer.Flags = SYMLINK_FLAG_RELATIVE;
    TargetPath = OutputReparseData->SymbolicLinkReparseBuffer.PathBuffer;
    memcpy(TargetPath, FileName, RemainderPathSize);

    ResolveLastPathComponent = ResolveLastPathComponent0;
    RemainderPath = TargetPath + ReparsePointIndex;

    for (;;)
    {
        while (L'\\' == *RemainderPath)
            RemainderPath++;
        LastPathComponent = RemainderPath;
        while (L'\\' != *RemainderPath)
        {
            if (L'\0' == *RemainderPath || L':' == *RemainderPath)
            {
                if (!ResolveLastPathComponent)
                    goto symlink_exit;
                ResolveLastPathComponent = FALSE;
                break;
            }
            RemainderPath++;
        }

        /* handle dot and dotdot! */
        if (L'.' == LastPathComponent[0])
        {
            if (RemainderPath == LastPathComponent + 1)
            {
                /* dot */
                ReparseTargetPath = 0;
                ReparseTargetPathLength = 0;

                NewRemainderPath = LastPathComponent;
                while (TargetPath < NewRemainderPath)
                {
                    NewRemainderPath--;
                    if (L'\\' == *NewRemainderPath)
                        break;
                }

                goto reparse;
            }

            if (L'.' == LastPathComponent[1] && RemainderPath == LastPathComponent + 2)
            {
                /* dotdot */
                ReparseTargetPath = 0;
                ReparseTargetPathLength = 0;

                NewRemainderPath = LastPathComponent;
                while (TargetPath < NewRemainderPath)
                {
                    NewRemainderPath--;
                    if (L'\\' != *NewRemainderPath)
                        break;
                }
                while (TargetPath < NewRemainderPath)
                {
                    NewRemainderPath--;
                    if (L'\\' == *NewRemainderPath)
                        break;
                }

                goto reparse;
            }
        }

        RemainderChar = *RemainderPath; *RemainderPath = L'\0';
        ReparseDataSize = ReparseDataSize0;
        Result = GetReparsePointByName(FileSystem, Context, TargetPath, L'\\' == RemainderChar,
            ReparseData, &ReparseDataSize);
        *RemainderPath = RemainderChar;

        if (STATUS_NOT_A_REPARSE_POINT == Result)
            /* it was not a reparse point; continue */
            continue;
        else if (STATUS_OBJECT_NAME_NOT_FOUND == Result && L'\\' != RemainderChar)
            goto symlink_exit;
        else if (!NT_SUCCESS(Result))
        {
            if (STATUS_OBJECT_NAME_NOT_FOUND != Result)
                Result = STATUS_OBJECT_PATH_NOT_FOUND;
            return Result;
        }

        /*
         * Found a reparse point!
         */

        /* if not a symlink return the full reparse point */
        if (IO_REPARSE_TAG_SYMLINK != ReparseData->ReparseTag)
            goto reparse_data_exit;

        if (0 == --MaxTries)
            return STATUS_REPARSE_POINT_NOT_RESOLVED;

        ReparseTargetPath = ReparseData->SymbolicLinkReparseBuffer.PathBuffer +
            ReparseData->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR);
        ReparseTargetPathLength = ReparseData->SymbolicLinkReparseBuffer.SubstituteNameLength;

        /* if device relative symlink replace whole path; else replace last path component */
        NewRemainderPath = ReparseTargetPathLength >= sizeof(WCHAR) && L'\\' == ReparseTargetPath[0] ?
            TargetPath : LastPathComponent;

    reparse:
        RemainderPathSize = (lstrlenW(RemainderPath) + 1) * sizeof(WCHAR);
        if ((PUINT8)NewRemainderPath + ReparseTargetPathLength + RemainderPathSize >
            (PUINT8)Buffer + *PSize)
            return STATUS_REPARSE_POINT_NOT_RESOLVED;

        /* move remainder path to its new position */
        memmove((PUINT8)NewRemainderPath + ReparseTargetPathLength,
            RemainderPath, RemainderPathSize);

        /* copy symlink target */
        memcpy(NewRemainderPath, ReparseTargetPath, ReparseTargetPathLength);

        /* if an absolute (in the NT namespace) symlink exit now */
        if (0 != ReparseTargetPath /* ensure we are not doing dot handling */ &&
            0 == (ReparseData->SymbolicLinkReparseBuffer.Flags & SYMLINK_FLAG_RELATIVE) &&
            ReparseTargetPathLength >= sizeof(WCHAR) && L'\\' == ReparseTargetPath[0])
        {
            OutputReparseData->SymbolicLinkReparseBuffer.Flags = 0;
            goto symlink_exit;
        }

        ResolveLastPathComponent = ResolveLastPathComponent0;
        RemainderPath = NewRemainderPath;
    }

symlink_exit:
    OutputReparseData->SymbolicLinkReparseBuffer.SubstituteNameLength =
        OutputReparseData->SymbolicLinkReparseBuffer.PrintNameLength =
        (USHORT)lstrlenW(OutputReparseData->SymbolicLinkReparseBuffer.PathBuffer) * sizeof(WCHAR);
    OutputReparseData->ReparseDataLength =
        FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) -
        FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer) +
        OutputReparseData->SymbolicLinkReparseBuffer.SubstituteNameLength;

    *PSize = FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer) +
        OutputReparseData->ReparseDataLength;

    PIoStatus->Status = STATUS_REPARSE;
    PIoStatus->Information = ReparseData->ReparseTag;
    return STATUS_REPARSE;

reparse_data_exit:
    if (ReparseDataSize > *PSize)
        return IO_REPARSE_TAG_SYMLINK != ReparseData->ReparseTag ?
            STATUS_IO_REPARSE_DATA_INVALID : STATUS_REPARSE_POINT_NOT_RESOLVED;

    if (IO_REPARSE_TAG_MOUNT_POINT == ReparseData->ReparseTag)
        RemainderPathSize = lstrlenW(RemainderPath) * sizeof(WCHAR);

    *PSize = ReparseDataSize;
    memcpy(Buffer, ReparseData, ReparseDataSize);

    if (IO_REPARSE_TAG_MOUNT_POINT == ReparseData->ReparseTag)
        OutputReparseData->Reserved = (USHORT)RemainderPathSize;

    PIoStatus->Status = STATUS_REPARSE;
    PIoStatus->Information = ReparseData->ReparseTag;
    return STATUS_REPARSE;
}

FSP_API NTSTATUS FspFileSystemResolveReparsePoints(FSP_FILE_SYSTEM *FileSystem,
    NTSTATUS (*GetReparsePointByName)(
        FSP_FILE_SYSTEM *FileSystem, PVOID Context,
        PWSTR FileName, BOOLEAN IsDirectory, PVOID Buffer, PSIZE_T PSize),
    PVOID Context,
    PWSTR FileName, UINT32 ReparsePointIndex, BOOLEAN ResolveLastPathComponent,
    PIO_STATUS_BLOCK PIoStatus, PVOID Buffer, PSIZE_T PSize)
{
    PREPARSE_DATA_BUFFER ReparseData;
    NTSTATUS Result;

    ReparseData = MemAlloc(FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX);
    if (0 == ReparseData)
        return STATUS_INSUFFICIENT_RESOURCES;

    Result = FspFileSystemResolveReparsePointsInternal(FileSystem,
        GetReparsePointByName, Context,
        ReparseData, FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX,
        FileName, ReparsePointIndex, ResolveLastPathComponent,
        PIoStatus, Buffer, PSize);

    MemFree(ReparseData);

    return Result;
}

FSP_API NTSTATUS FspFileSystemCanReplaceReparsePoint(
    PVOID CurrentReparseData, SIZE_T CurrentReparseDataSize,
    PVOID ReplaceReparseData, SIZE_T ReplaceReparseDataSize)
{
    if (sizeof(ULONG) > CurrentReparseDataSize ||
        sizeof(ULONG) > ReplaceReparseDataSize)
        return STATUS_IO_REPARSE_DATA_INVALID; /* should not happen! */
    else if (*(PULONG)CurrentReparseData != *(PULONG)ReplaceReparseData)
        return STATUS_IO_REPARSE_TAG_MISMATCH;
    else if (!IsReparseTagMicrosoft(*(PULONG)CurrentReparseData) && (
        (SIZE_T)REPARSE_GUID_DATA_BUFFER_HEADER_SIZE > CurrentReparseDataSize ||
        (SIZE_T)REPARSE_GUID_DATA_BUFFER_HEADER_SIZE > ReplaceReparseDataSize ||
        *(PUINT32)&((PREPARSE_GUID_DATA_BUFFER)CurrentReparseData)->ReparseGuid.Data1 !=
        *(PUINT32)&((PREPARSE_GUID_DATA_BUFFER)ReplaceReparseData)->ReparseGuid.Data1 ||
        *(PUINT32)&((PREPARSE_GUID_DATA_BUFFER)CurrentReparseData)->ReparseGuid.Data2 !=
        *(PUINT32)&((PREPARSE_GUID_DATA_BUFFER)ReplaceReparseData)->ReparseGuid.Data2 ||
        *(PUINT32)&((PREPARSE_GUID_DATA_BUFFER)CurrentReparseData)->ReparseGuid.Data4[0] !=
        *(PUINT32)&((PREPARSE_GUID_DATA_BUFFER)ReplaceReparseData)->ReparseGuid.Data4[0] ||
        *(PUINT32)&((PREPARSE_GUID_DATA_BUFFER)CurrentReparseData)->ReparseGuid.Data4[4] !=
        *(PUINT32)&((PREPARSE_GUID_DATA_BUFFER)ReplaceReparseData)->ReparseGuid.Data4[4]))
        return STATUS_REPARSE_ATTRIBUTE_CONFLICT;
    else
        return STATUS_SUCCESS;
}

FSP_API BOOLEAN FspFileSystemAddStreamInfo(FSP_FSCTL_STREAM_INFO *StreamInfo,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    return FspFileSystemAddXxxInfo(StreamInfo, Buffer, Length, PBytesTransferred);
}

FSP_API NTSTATUS FspFileSystemEnumerateEa(FSP_FILE_SYSTEM *FileSystem,
    NTSTATUS (*EnumerateEa)(
        FSP_FILE_SYSTEM *FileSystem, PVOID Context,
        PFILE_FULL_EA_INFORMATION SingleEa),
    PVOID Context,
    PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength)
{
    NTSTATUS Result = STATUS_SUCCESS;
    for (PFILE_FULL_EA_INFORMATION EaEnd = (PVOID)((PUINT8)Ea + EaLength);
        EaEnd > Ea; Ea = FSP_NEXT_EA(Ea, EaEnd))
    {
        Result = EnumerateEa(FileSystem, Context, Ea);
        if (!NT_SUCCESS(Result))
            break;
    }
    return Result;
}

FSP_API BOOLEAN FspFileSystemAddEa(PFILE_FULL_EA_INFORMATION SingleEa,
    PFILE_FULL_EA_INFORMATION Ea, ULONG Length, PULONG PBytesTransferred)
{
    if (0 != SingleEa)
    {
        PUINT8 EaPtr = (PUINT8)Ea + FSP_FSCTL_ALIGN_UP(*PBytesTransferred, sizeof(ULONG));
        PUINT8 EaEnd = (PUINT8)Ea + Length;
        ULONG EaLen = FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) +
            SingleEa->EaNameLength + 1 + SingleEa->EaValueLength;

        if (EaEnd < EaPtr + EaLen)
            return FALSE;

        memcpy(EaPtr, SingleEa, EaLen);
        ((PFILE_FULL_EA_INFORMATION)EaPtr)->NextEntryOffset = FSP_FSCTL_ALIGN_UP(EaLen, sizeof(ULONG));
        *PBytesTransferred = (ULONG)(EaPtr + EaLen - (PUINT8)Ea);
    }
    else if ((ULONG)FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) <= *PBytesTransferred)
    {
        PUINT8 EaEnd = (PUINT8)Ea + *PBytesTransferred;

        while (EaEnd > (PUINT8)Ea + Ea->NextEntryOffset)
            Ea = (PVOID)((PUINT8)Ea + Ea->NextEntryOffset);

        Ea->NextEntryOffset = 0;
    }

    return TRUE;
}

FSP_API BOOLEAN FspFileSystemAddNotifyInfo(FSP_FSCTL_NOTIFY_INFO *NotifyInfo,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    return FspFileSystemAddXxxInfo(NotifyInfo, Buffer, Length, PBytesTransferred);
}
