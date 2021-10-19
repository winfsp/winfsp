/**
 * @file memfs.h
 *
 * @copyright 2015-2021 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
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
    MemfsDisk                           = 0x00000000,
    MemfsNet                            = 0x00000001,
    MemfsDeviceMask                     = 0x0000000f,
    MemfsCaseInsensitive                = 0x80000000,
    MemfsFlushAndPurgeOnCleanup         = 0x40000000,
    MemfsLegacyUnlinkRename             = 0x20000000,
};

#define MemfsCreate(Flags, FileInfoTimeout, MaxFileNodes, MaxFileSize, VolumePrefix, RootSddl, PMemfs)\
    MemfsCreateFunnel(\
        Flags,\
        FileInfoTimeout,\
        MaxFileNodes,\
        MaxFileSize,\
        0/*SlowioMaxDelay*/,\
        0/*SlowioPercentDelay*/,\
        0/*SlowioRarefyDelay*/,\
        0/*FileSystemName*/,\
        VolumePrefix,\
        RootSddl,\
        PMemfs)
NTSTATUS MemfsCreateFunnel(
    ULONG Flags,
    ULONG FileInfoTimeout,
    ULONG MaxFileNodes,
    ULONG MaxFileSize,
    ULONG SlowioMaxDelay,
    ULONG SlowioPercentDelay,
    ULONG SlowioRarefyDelay,
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
