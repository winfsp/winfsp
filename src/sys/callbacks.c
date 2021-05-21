/**
 * @file sys/callbacks.c
 * Fast I/O and resource acquisition callbacks.
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

#include <sys/driver.h>

FAST_IO_CHECK_IF_POSSIBLE FspFastIoCheckIfPossible;
FAST_IO_ACQUIRE_FILE FspAcquireFileForNtCreateSection;
FAST_IO_RELEASE_FILE FspReleaseFileForNtCreateSection;
FAST_IO_ACQUIRE_FOR_MOD_WRITE FspAcquireForModWrite;
FAST_IO_RELEASE_FOR_MOD_WRITE FspReleaseForModWrite;
FAST_IO_ACQUIRE_FOR_CCFLUSH FspAcquireForCcFlush;
FAST_IO_RELEASE_FOR_CCFLUSH FspReleaseForCcFlush;
BOOLEAN FspAcquireForLazyWrite(
    PVOID Context,
    BOOLEAN Wait);
VOID FspReleaseFromLazyWrite(
    PVOID Context);
BOOLEAN FspAcquireForReadAhead(
    PVOID Context,
    BOOLEAN Wait);
VOID FspReleaseFromReadAhead(
    PVOID Context);
VOID FspPropagateTopFlags(PIRP Irp, PIRP TopLevelFlags);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFastIoCheckIfPossible)
#pragma alloc_text(PAGE, FspAcquireFileForNtCreateSection)
#pragma alloc_text(PAGE, FspReleaseFileForNtCreateSection)
#pragma alloc_text(PAGE, FspAcquireForModWrite)
#pragma alloc_text(PAGE, FspReleaseForModWrite)
#pragma alloc_text(PAGE, FspAcquireForCcFlush)
#pragma alloc_text(PAGE, FspReleaseForCcFlush)
#pragma alloc_text(PAGE, FspAcquireForLazyWrite)
#pragma alloc_text(PAGE, FspReleaseFromLazyWrite)
#pragma alloc_text(PAGE, FspAcquireForReadAhead)
#pragma alloc_text(PAGE, FspReleaseFromReadAhead)
#pragma alloc_text(PAGE, FspPropagateTopFlags)
#endif

BOOLEAN FspFastIoCheckIfPossible(
    PFILE_OBJECT FileObject,
    PLARGE_INTEGER FileOffset,
    ULONG Length,
    BOOLEAN Wait,
    ULONG LockKey,
    BOOLEAN CheckForReadOperation,
    PIO_STATUS_BLOCK IoStatus,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER_BOOL(PAGED_CODE());

    Result = FALSE;

    FSP_LEAVE_BOOL("FileObject=%p", FileObject);
}

VOID FspAcquireFileForNtCreateSection(
    PFILE_OBJECT FileObject)
{
    /* Callers:
     *     CcWriteBehind
     *     MmCreateSection and friends
     */

    FSP_ENTER_VOID(PAGED_CODE());

    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    FspFileNodeAcquireExclusive(FileNode, Full);
    ASSERT(FALSE == FileNode->Tls.CreateSection);
    FileNode->Tls.CreateSection = TRUE;

    FSP_LEAVE_VOID("FileObject=%p", FileObject);
}

VOID FspReleaseFileForNtCreateSection(
    PFILE_OBJECT FileObject)
{
    /* Callers:
     *     CcWriteBehind
     *     MmCreateSection and friends
     */

    FSP_ENTER_VOID(PAGED_CODE());

    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    FileNode->Tls.CreateSection = FALSE;
    FspFileNodeRelease(FileNode, Full);

    FSP_LEAVE_VOID("FileObject=%p", FileObject);
}

NTSTATUS FspAcquireForModWrite(
    PFILE_OBJECT FileObject,
    PLARGE_INTEGER EndingOffset,
    PERESOURCE *ResourceToRelease,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER(PAGED_CODE());

    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    BOOLEAN Success;

    ASSERT((PIRP)FSRTL_MOD_WRITE_TOP_LEVEL_IRP == IoGetTopLevelIrp());
    Success = FspFileNodeTryAcquireExclusiveF(FileNode, FspFileNodeAcquireFull, FALSE);
    if (Success)
    {
        *ResourceToRelease = 0 == FileNode->MainFileNode ?
            FileNode->Header.PagingIoResource :
            FileNode->MainFileNode->Header.PagingIoResource;
        Result = STATUS_SUCCESS;
    }
    else
    {
        *ResourceToRelease = 0;
        Result = STATUS_CANT_WAIT;
    }

    FSP_LEAVE("FileObject=%p", FileObject);
}

NTSTATUS FspReleaseForModWrite(
    PFILE_OBJECT FileObject,
    PERESOURCE ResourceToRelease,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER(PAGED_CODE());

    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    FspFileNodeRelease(FileNode, Full);
    ASSERT((PIRP)FSRTL_MOD_WRITE_TOP_LEVEL_IRP == IoGetTopLevelIrp());

    FSP_LEAVE("FileObject=%p", FileObject);
}

NTSTATUS FspAcquireForCcFlush(
    PFILE_OBJECT FileObject,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER(PAGED_CODE());

    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    PIRP TopLevelIrp = IoGetTopLevelIrp();
    ULONG TopFlags;

    if ((PIRP)FSRTL_MAX_TOP_LEVEL_IRP_FLAG >= TopLevelIrp)
    {
        FspFileNodeAcquireExclusive(FileNode, Full);
        ASSERT(0 == FileNode->Tls.CcFlush.TopLevelIrp);
        FileNode->Tls.CcFlush.TopLevelIrp = TopLevelIrp;
        IoSetTopLevelIrp((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
    }
    else
    {
        TopFlags = FspIrpTopFlags(TopLevelIrp);
        FspIrpSetTopFlags(TopLevelIrp, FspIrpFlags(TopLevelIrp));
        FspFileNodeAcquireExclusive(FileNode, Full);
        ASSERT(0 == FileNode->Tls.CcFlush.TopFlags);
        FileNode->Tls.CcFlush.TopFlags = TopFlags;
    }

    FSP_LEAVE("FileObject=%p", FileObject);
}

NTSTATUS FspReleaseForCcFlush(
    PFILE_OBJECT FileObject,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER(PAGED_CODE());

    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    PIRP TopLevelIrp = IoGetTopLevelIrp();
    ULONG TopFlags;

    if ((PIRP)FSRTL_MAX_TOP_LEVEL_IRP_FLAG >= TopLevelIrp)
    {
        ASSERT(0 == TopLevelIrp || (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP == TopLevelIrp);
        if ((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP == TopLevelIrp)
            IoSetTopLevelIrp(FileNode->Tls.CcFlush.TopLevelIrp);
        FileNode->Tls.CcFlush.TopLevelIrp = 0;
        FspFileNodeRelease(FileNode, Full);
    }
    else
    {
        TopFlags = FileNode->Tls.CcFlush.TopFlags;
        FileNode->Tls.CcFlush.TopFlags = 0;
        FspFileNodeRelease(FileNode, Full);
        FspIrpSetTopFlags(TopLevelIrp, TopFlags);
    }

    FSP_LEAVE("FileObject=%p", FileObject);
}

BOOLEAN FspAcquireForLazyWrite(
    PVOID Context,
    BOOLEAN Wait)
{
    /* Callers:
     *     CcWriteBehind
     */

    FSP_ENTER_BOOL(PAGED_CODE());

    FSP_FILE_NODE *FileNode = Context;

    ASSERT(0 == IoGetTopLevelIrp());
    Result = FspFileNodeTryAcquireExclusiveF(FileNode, FspFileNodeAcquireFull, Wait);
    if (Result)
    {
        ASSERT(0 == FileNode->Tls.LazyWriteThread);
        FileNode->Tls.LazyWriteThread = PsGetCurrentThread();
        IoSetTopLevelIrp((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
    }

    FSP_LEAVE_BOOL("Context=%p, Wait=%d", Context, Wait);
}

VOID FspReleaseFromLazyWrite(
    PVOID Context)
{
    /* Callers:
     *     CcWriteBehind
     */

    FSP_ENTER_VOID(PAGED_CODE());

    FSP_FILE_NODE *FileNode = Context;

    ASSERT((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP == IoGetTopLevelIrp());
    ASSERT(PsGetCurrentThread() == FileNode->Tls.LazyWriteThread);
    IoSetTopLevelIrp(0);
    FileNode->Tls.LazyWriteThread = 0;
    FspFileNodeRelease(FileNode, Full);

    FSP_LEAVE_VOID("Context=%p", Context);
}

BOOLEAN FspAcquireForReadAhead(
    PVOID Context,
    BOOLEAN Wait)
{
    FSP_ENTER_BOOL(PAGED_CODE());

    FSP_FILE_NODE *FileNode = Context;

    ASSERT(0 == IoGetTopLevelIrp());
    Result = FspFileNodeTryAcquireSharedF(FileNode, FspFileNodeAcquireFull, Wait);
    if (Result)
        IoSetTopLevelIrp((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);

    FSP_LEAVE_BOOL("Context=%p, Wait=%d", Context, Wait);
}

VOID FspReleaseFromReadAhead(
    PVOID Context)
{
    FSP_ENTER_VOID(PAGED_CODE());

    FSP_FILE_NODE *FileNode = Context;

    ASSERT((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP == IoGetTopLevelIrp());
    IoSetTopLevelIrp(0);
    FspFileNodeRelease(FileNode, Full);

    FSP_LEAVE_VOID("Context=%p", Context);
}

VOID FspPropagateTopFlags(PIRP Irp, PIRP TopLevelIrp)
{
    /*
     * We place FspPropagateTopFlags in this file, because the top flags
     * are related to the resources acquired in FspAcquire*.
     */

    PAGED_CODE();

    ASSERT(0 != Irp && 0 != TopLevelIrp);

    if ((PIRP)FSRTL_MAX_TOP_LEVEL_IRP_FLAG >= TopLevelIrp)
    {
        DEBUGBREAK_EX(iorecu);

        FspIrpSetTopFlags(Irp, FspFileNodeAcquireFull);
    }
    else if ((PIRP)MM_SYSTEM_RANGE_START <= TopLevelIrp && IO_TYPE_IRP == TopLevelIrp->Type)
    {
        PFILE_OBJECT FileObject = IoGetCurrentIrpStackLocation(Irp)->FileObject;
        PFILE_OBJECT TopLevelFileObject = IoGetCurrentIrpStackLocation(TopLevelIrp)->FileObject;
        if (0 != FileObject && 0 != TopLevelFileObject &&
            FileObject->FsContext == TopLevelFileObject->FsContext &&
            FspFileNodeIsValid(FileObject->FsContext))
        {
            DEBUGBREAK_EX(iorecu);

            FspIrpSetTopFlags(Irp, FspIrpTopFlags(TopLevelIrp) | FspIrpFlags(TopLevelIrp));
        }
    }
}
