/**
 * @file dll/fileinfo.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>

FSP_API NTSTATUS FspFileSystemOpQueryInformation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    FSP_FSCTL_FILE_INFO FileInfo;

    if (0 == FileSystem->Interface->GetFileInfo)
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, STATUS_INVALID_DEVICE_REQUEST);

    memset(&FileInfo, 0, sizeof FileInfo);
    Result = FileSystem->Interface->GetFileInfo(FileSystem, Request,
        (PVOID)Request->Req.Close.UserContext, &FileInfo);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    return FspFileSystemSendQueryInformationResponse(FileSystem, Request, &FileInfo);
}

FSP_API NTSTATUS FspFileSystemSendQueryInformationResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_FILE_INFO *FileInfo)
{
    FSP_FSCTL_TRANSACT_RSP Response;

    memset(&Response, 0, sizeof Response);
    Response.Size = sizeof Response;
    Response.Kind = FspFsctlTransactQueryInformationKind;
    Response.Hint = Request->Hint;
    Response.IoStatus.Status = STATUS_SUCCESS;
    Response.IoStatus.Information = 0;
    Response.Rsp.QueryInformation.FileInfo = *FileInfo;
    return FspFileSystemSendResponse(FileSystem, &Response);
}
