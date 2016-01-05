/**
 * @file dll/cleanup.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>

FSP_API NTSTATUS FspFileSystemOpCleanup(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    if (0 == FileSystem->Interface->Cleanup)
        return FspFileSystemSendResponseWithStatus(FileSystem, Request, STATUS_INVALID_DEVICE_REQUEST);

    FSP_FILE_NODE *FileNode = (PVOID)Request->Req.Close.UserContext;
    BOOLEAN DeletePending;
    LONG OpenCount;

    FspFileNodeLock(FileNode);

    /* propagate the DeleteOnClose flag to DeletePending */
    if (FileNode->Flags.DeleteOnClose)
        FileNode->Flags.DeletePending = TRUE;
    DeletePending = FileNode->Flags.DeletePending;

    /* all handles on the kernel FILE_OBJECT gone; decrement the FileNode's OpenCount */
    OpenCount = FspFileNodeClose(FileNode);

    FspFileNodeUnlock(FileNode);

    FileSystem->Interface->Cleanup(FileSystem, Request, FileNode, 0 == OpenCount && DeletePending);

    return FspFileSystemSendCleanupResponse(FileSystem, Request);
}

FSP_API NTSTATUS FspFileSystemSendCleanupResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    FSP_FSCTL_TRANSACT_RSP Response;

    memset(&Response, 0, sizeof Response);
    Response.Size = sizeof Response;
    Response.Kind = Request->Kind;
    Response.Hint = Request->Hint;
    Response.IoStatus.Status = STATUS_SUCCESS;
    Response.IoStatus.Information = 0;
    return FspFileSystemSendResponse(FileSystem, &Response);
}
