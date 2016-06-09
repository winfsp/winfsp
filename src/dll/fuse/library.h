/**
 * @file dll/fuse/library.h
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

#ifndef WINFSP_DLL_FUSE_LIBRARY_H_INCLUDED
#define WINFSP_DLL_FUSE_LIBRARY_H_INCLUDED

#include <dll/library.h>
#include <fuse/fuse.h>
#include <fuse/fuse_opt.h>

#define FSP_FUSE_LIBRARY_NAME           LIBRARY_NAME "-FUSE"

struct fuse
{
    struct fsp_fuse_env *env;
    struct fuse_operations ops;
    void *data;
    UINT32 DebugLog;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    PWSTR MountPoint;
    FSP_SERVICE *Service;
    FSP_FILE_SYSTEM *FileSystem;
    BOOLEAN fsinit;
};

struct fsp_fuse_context_header
{
    FSP_FSCTL_TRANSACT_REQ *Request;
    FSP_FSCTL_TRANSACT_RSP *Response;
    char *PosixPath;
    __declspec(align(MEMORY_ALLOCATION_ALIGNMENT)) UINT8 ContextBuf[];
};

NTSTATUS fsp_fuse_op_enter(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_leave(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);

extern FSP_FILE_SYSTEM_INTERFACE fsp_fuse_intf;

#endif
