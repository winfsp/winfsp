/**
 * @file dll/fuse/fuseop.c
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

#include <dll/fuse/library.h>

NTSTATUS fsp_fuse_op_get_security_by_name(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    /* not a true file system op, required for access checks! */
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS fsp_fuse_op_create(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS fsp_fuse_op_overwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS fsp_fuse_op_cleanup(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS fsp_fuse_op_close(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS fsp_fuse_op_read(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS fsp_fuse_op_write(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS fsp_fuse_op_flush_buffers(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS fsp_fuse_op_query_information(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS fsp_fuse_op_set_information(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS fsp_fuse_op_query_volume_information(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS fsp_fuse_op_set_volume_information(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS fsp_fuse_op_query_directory(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS fsp_fuse_op_query_security(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS fsp_fuse_op_set_security(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}
