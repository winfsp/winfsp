/**
 * @file dll/security.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>

FSP_API NTSTATUS FspFileSystemOpQuerySecurity(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;

    if (0 == FileSystem->Interface->GetSecurity)
        return STATUS_INVALID_DEVICE_REQUEST;

    (VOID)Result;
    return STATUS_INVALID_DEVICE_REQUEST;
}

FSP_API NTSTATUS FspFileSystemOpSetSecurity(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;

    if (0 == FileSystem->Interface->SetSecurity)
        return STATUS_INVALID_DEVICE_REQUEST;

    (VOID)Result;
    return STATUS_INVALID_DEVICE_REQUEST;
}
