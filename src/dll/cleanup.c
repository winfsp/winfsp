/**
 * @file dll/cleanup.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>

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
