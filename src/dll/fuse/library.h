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

#if defined(_WIN64)
struct cyg_timespec
{
    int64_t tv_sec;
    int64_t tv_nsec;
};
#else
struct cyg_timespec
{
    int32_t tv_sec;
    int32_t tv_nsec;
};
#endif

struct cyg_stat
{
    uint32_t st_dev;
    uint64_t st_ino;
    uint32_t st_mode;
    uint16_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t st_rdev;
    int64_t st_size;
    struct cyg_timespec st_atim;
    struct cyg_timespec st_mtim;
    struct cyg_timespec st_ctim;
    int32_t st_blksize;
    int64_t st_blocks;
    struct cyg_timespec st_birthtim;
};

#if defined(_WIN64)
struct cyg_statvfs
{
    uint64_t f_bsize;
    uint64_t f_frsize;
    uint64_t f_blocks;
    uint64_t f_bfree;
    uint64_t f_bavail;
    uint64_t f_files;
    uint64_t f_ffree;
    uint64_t f_favail;
    uint64_t f_fsid;
    uint64_t f_flag;
    uint64_t f_namemax;
};
#else
struct cyg_statvfs
{
    uint32_t f_bsize;
    uint32_t f_frsize;
    uint32_t f_blocks;
    uint32_t f_bfree;
    uint32_t f_bavail;
    uint32_t f_files;
    uint32_t f_ffree;
    uint32_t f_favail;
    uint32_t f_fsid;
    uint32_t f_flag;
    uint32_t f_namemax;
};
#endif

static inline void fsp_fuse_op_get_stat_buf(
    int environment,
    const void *buf, struct fuse_stat *fspbuf)
{
    switch (environment)
    {
    case 'C':
        {
            const struct cyg_stat *cygbuf = buf;
            fspbuf->st_dev = cygbuf->st_dev;
            fspbuf->st_ino = cygbuf->st_ino;
            fspbuf->st_mode = cygbuf->st_mode;
            fspbuf->st_nlink = cygbuf->st_nlink;
            fspbuf->st_uid = cygbuf->st_uid;
            fspbuf->st_gid = cygbuf->st_gid;
            fspbuf->st_rdev = cygbuf->st_rdev;
            fspbuf->st_size = cygbuf->st_size;
            fspbuf->st_atim.tv_sec = cygbuf->st_atim.tv_sec;
            fspbuf->st_atim.tv_nsec = (int)cygbuf->st_atim.tv_nsec;
            fspbuf->st_mtim.tv_sec = cygbuf->st_mtim.tv_sec;
            fspbuf->st_mtim.tv_nsec = (int)cygbuf->st_mtim.tv_nsec;
            fspbuf->st_ctim.tv_sec = cygbuf->st_ctim.tv_sec;
            fspbuf->st_ctim.tv_nsec = (int)cygbuf->st_ctim.tv_nsec;
            fspbuf->st_blksize = cygbuf->st_blksize;
            fspbuf->st_blocks = cygbuf->st_blocks;
            fspbuf->st_birthtim.tv_sec = cygbuf->st_birthtim.tv_sec;
            fspbuf->st_birthtim.tv_nsec = (int)cygbuf->st_birthtim.tv_nsec;
        }
        break;
    default:
        memcpy(fspbuf, buf, sizeof *fspbuf);
        break;
    }
}

static inline void fsp_fuse_op_get_statvfs_buf(
    int environment,
    const void *buf, struct fuse_statvfs *fspbuf)
{
    switch (environment)
    {
    case 'C':
        {
            const struct cyg_statvfs *cygbuf = buf;
            fspbuf->f_bsize = cygbuf->f_bsize;
            fspbuf->f_frsize = cygbuf->f_frsize;
            fspbuf->f_blocks = cygbuf->f_blocks;
            fspbuf->f_bfree = cygbuf->f_bfree;
            fspbuf->f_bavail = cygbuf->f_bavail;
            fspbuf->f_files = cygbuf->f_files;
            fspbuf->f_ffree = cygbuf->f_ffree;
            fspbuf->f_favail = cygbuf->f_favail;
            fspbuf->f_fsid = cygbuf->f_fsid;
            fspbuf->f_flag = cygbuf->f_flag;
            fspbuf->f_namemax = cygbuf->f_namemax;
        }
        break;
    default:
        memcpy(fspbuf, buf, sizeof *fspbuf);
        break;
    }
}

NTSTATUS fsp_fuse_op_get_security_by_name(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize);
NTSTATUS fsp_fuse_op_create(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_overwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_cleanup(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_close(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_read(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_write(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_flush_buffers(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_query_information(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_set_information(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_query_volume_information(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_set_volume_information(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_query_directory(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_query_security(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
NTSTATUS fsp_fuse_op_set_security(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);

#endif
