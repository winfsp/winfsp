/**
 * @file memfs.h
 *
 * @copyright 2015-2016 Bill Zissimopoulos
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
};

NTSTATUS MemfsCreate(ULONG Flags, ULONG FileInfoTimeout,
    ULONG MaxFileNodes, ULONG MaxFileSize,
    MEMFS **PMemfs);
VOID MemfsDelete(MEMFS *Memfs);
NTSTATUS MemfsStart(MEMFS *Memfs);
VOID MemfsStop(MEMFS *Memfs);
FSP_FILE_SYSTEM *MemfsFileSystem(MEMFS *Memfs);

#ifdef __cplusplus
}
#endif

#endif
