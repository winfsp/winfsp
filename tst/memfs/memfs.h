/**
 * @file memfs.h
 *
 * @copyright 2015-2017 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#ifndef MEMFS_H_INCLUDED
#define MEMFS_H_INCLUDED

#include <winfsp/winfsp.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _MEMFS MEMFS;

enum
{
    MemfsDisk                           = 0x00,
    MemfsNet                            = 0x01,
    MemfsCaseInsensitive                = 0x80,
};

#define MemfsCreate(Flags, FileInfoTimeout, MaxFileNodes, MaxFileSize, VolumePrefix, RootSddl, PMemfs)\
    MemfsCreateFunnel(Flags, FileInfoTimeout, MaxFileNodes, MaxFileSize, 0, VolumePrefix, RootSddl, PMemfs)
NTSTATUS MemfsCreateFunnel(
    ULONG Flags,
    ULONG FileInfoTimeout,
    ULONG MaxFileNodes,
    ULONG MaxFileSize,
    PWSTR FileSystemName,
    PWSTR VolumePrefix,
    PWSTR RootSddl,
    MEMFS **PMemfs);
VOID MemfsDelete(MEMFS *Memfs);
NTSTATUS MemfsStart(MEMFS *Memfs);
VOID MemfsStop(MEMFS *Memfs);
FSP_FILE_SYSTEM *MemfsFileSystem(MEMFS *Memfs);

NTSTATUS MemfsHeapConfigure(SIZE_T InitialSize, SIZE_T MaximumSize, SIZE_T Alignment);

#ifdef __cplusplus
}
#endif

#endif
