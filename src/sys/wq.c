/**
 * @file sys/wq.c
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

NTSTATUS FspWqCreateAndPostIrpWorkItem(PIRP Irp,
    FSP_IOP_REQUEST_WORK *WorkRoutine, FSP_IOP_REQUEST_FINI *RequestFini,
    BOOLEAN CreateAndPost);
VOID FspWqPostIrpWorkItem(PIRP Irp);
static VOID FspWqWorkRoutine(PVOID Context);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspWqCreateAndPostIrpWorkItem)
#pragma alloc_text(PAGE, FspWqPostIrpWorkItem)
#pragma alloc_text(PAGE, FspWqWorkRoutine)
#endif

static inline
NTSTATUS FspWqPrepareIrpWorkItem(PIRP Irp)
{
    NTSTATUS Result = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    /* lock/buffer the user buffer */
    if ((IRP_MJ_READ == IrpSp->MajorFunction || IRP_MJ_WRITE == IrpSp->MajorFunction) &&
        !FlagOn(IrpSp->MinorFunction, IRP_MN_MDL))
    {
        if (IRP_MJ_READ == IrpSp->MajorFunction)
            Result = FspLockUserBuffer(Irp, IrpSp->Parameters.Read.Length, IoWriteAccess);
        else
            Result = FspLockUserBuffer(Irp, IrpSp->Parameters.Write.Length, IoReadAccess);
    }
    else
    if (IRP_MJ_DIRECTORY_CONTROL == IrpSp->MajorFunction)
        Result = FspLockUserBuffer(Irp, IrpSp->Parameters.QueryDirectory.Length, IoWriteAccess);

    return Result;
}

NTSTATUS FspWqCreateAndPostIrpWorkItem(PIRP Irp,
    FSP_IOP_REQUEST_WORK *WorkRoutine, FSP_IOP_REQUEST_FINI *RequestFini,
    BOOLEAN CreateAndPost)
{
    PAGED_CODE();

    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    FSP_FSCTL_TRANSACT_REQ_WORK_ITEM *RequestWorkItem;
    NTSTATUS Result;

    if (0 == Request)
    {
        Result = FspWqPrepareIrpWorkItem(Irp);
        if (!NT_SUCCESS(Result))
            return Result;

        Result = FspIopCreateRequestAndWorkItem(Irp, 0, RequestFini, &Request);
        if (!NT_SUCCESS(Result))
            return Result;

        RequestWorkItem = FspIopRequestWorkItem(Request);
        RequestWorkItem->WorkRoutine = WorkRoutine;
        ExInitializeWorkItem(&RequestWorkItem->WorkQueueItem, FspWqWorkRoutine, Irp);
    }
    else if (0 == FspIopRequestWorkItem(Request))
    {
        Result = FspWqPrepareIrpWorkItem(Irp);
        if (!NT_SUCCESS(Result))
            return Result;

        Result = FspIopCreateRequestWorkItem(Request);
        if (!NT_SUCCESS(Result))
            return Result;

        RequestWorkItem = FspIopRequestWorkItem(Request);
        RequestWorkItem->WorkRoutine = WorkRoutine;
        ExInitializeWorkItem(&RequestWorkItem->WorkQueueItem, FspWqWorkRoutine, Irp);
    }

    ASSERT(RequestFini == ((FSP_FSCTL_TRANSACT_REQ_HEADER *)
        ((PUINT8)Request - sizeof(FSP_FSCTL_TRANSACT_REQ_HEADER)))->RequestFini);

    if (!CreateAndPost)
        return STATUS_SUCCESS;

    FspWqPostIrpWorkItem(Irp);
    return STATUS_PENDING;
}

VOID FspWqPostIrpWorkItem(PIRP Irp)
{
    PAGED_CODE();

    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    FSP_FSCTL_TRANSACT_REQ_WORK_ITEM *RequestWorkItem = FspIopRequestWorkItem(Request);

#if DBG
    switch (IoGetCurrentIrpStackLocation(Irp)->MajorFunction)
    {
    case IRP_MJ_CREATE:
        ASSERT(0 != FspIopIrpResponse(Irp));
        break;
    case IRP_MJ_READ:
    case IRP_MJ_WRITE:
    case IRP_MJ_DIRECTORY_CONTROL:
    case IRP_MJ_LOCK_CONTROL:
        break;
    default:
        ASSERT(0);
        break;
    }
#endif
    ASSERT(Request->Hint == (UINT_PTR)Irp);
    ASSERT(0 != RequestWorkItem);
    ASSERT(0 != RequestWorkItem->WorkRoutine);

    IoMarkIrpPending(Irp);
    ExQueueWorkItem(&RequestWorkItem->WorkQueueItem, CriticalWorkQueue);
}

static VOID FspWqWorkRoutine(PVOID Context)
{
    PAGED_CODE();

    PIRP Irp = Context;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_OBJECT DeviceObject = IrpSp->DeviceObject;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    FSP_FSCTL_TRANSACT_REQ_WORK_ITEM *RequestWorkItem = FspIopRequestWorkItem(Request);
    FSP_IOP_REQUEST_WORK *WorkRoutine = RequestWorkItem->WorkRoutine;
    NTSTATUS Result;

    IoSetTopLevelIrp(Irp);

    Result = WorkRoutine(DeviceObject, Irp, IrpSp, TRUE);
    if (STATUS_PENDING != Result && !(FSP_STATUS_IGNORE_BIT & Result))
    {
        ASSERT(0 == (FSP_STATUS_PRIVATE_BIT & Result) ||
            FSP_STATUS_IOQ_POST == Result || FSP_STATUS_IOQ_POST_BEST_EFFORT == Result);

        DEBUGLOGIRP(Irp, Result);

        if (FSP_STATUS_PRIVATE_BIT & Result)
        {
            FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
                FspFsvolDeviceExtension(DeviceObject);
            if (!FspIoqPostIrpEx(FsvolDeviceExtension->Ioq, Irp,
                FSP_STATUS_IOQ_POST_BEST_EFFORT == Result, &Result))
            {
                DEBUGLOG("FspIoqPostIrpEx = %s", NtStatusSym(Result));
                FspIopCompleteIrp(Irp, Result);
            }
        }
        else
            FspIopCompleteIrp(Irp, Result);
    }

    IoSetTopLevelIrp(0);
}
