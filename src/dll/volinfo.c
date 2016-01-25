/**
 * @file dll/volinfo.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>

FSP_API NTSTATUS FspFileSystemOpQueryVolumeInformation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS Result;
    FSP_FSCTL_VOLUME_INFO VolumeInfo;

    if (0 == FileSystem->Interface->GetVolumeInfo)
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, STATUS_INVALID_DEVICE_REQUEST);

    Result = FileSystem->Interface->GetVolumeInfo(FileSystem, Request, &VolumeInfo);
    if (!NT_SUCCESS(Result))
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, Result);

    return FspFileSystemSendQueryVolumeInformationResponse(FileSystem, Request, &VolumeInfo);
}

FSP_API NTSTATUS FspFileSystemSendQueryVolumeInformationResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    FSP_FSCTL_TRANSACT_RSP Response;

    memset(&Response, 0, sizeof Response);
    Response.Size = sizeof Response;
    Response.Kind = FspFsctlTransactQueryVolumeInformationKind;
    Response.Hint = Request->Hint;
    Response.IoStatus.Status = STATUS_SUCCESS;
    Response.IoStatus.Information = 0;
    Response.Rsp.QueryVolumeInformation.VolumeInfo = *VolumeInfo;
    return FspFileSystemSendResponse(FileSystem, &Response);
}
