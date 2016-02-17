/**
 * @file dll/fileinfo.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>

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
                    FALSE,
                    &FileInfo);
            }
        }
        break;
    case 20/*FileEndOfFileInformation*/:
        if (0 != FileSystem->Interface->SetFileSize)
            Result = FileSystem->Interface->SetFileSize(FileSystem, Request,
                (PVOID)Request->Req.SetInformation.UserContext,
                Request->Req.SetInformation.Info.EndOfFile.FileSize,
                Request->Req.SetInformation.Info.EndOfFile.AdvanceOnly,
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
