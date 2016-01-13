/**
 * @file memfs.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef MEMFS_H_INCLUDED
#define MEMFS_H_INCLUDED

#include <winfsp/winfsp.h>

typedef struct _MEMFS
{
    FSP_FILE_SYSTEM *FileSystem;
    ULONG MaxFileNodes;
    ULONG MaxFileSize;
} MEMFS;

NTSTATUS MemfsCreate(PWSTR DevicePath, ULONG MaxFileNodes, ULONG MaxFileSize,
    MEMFS **PMemfs);
VOID MemfsDelete(MEMFS *Memfs);

#endif
