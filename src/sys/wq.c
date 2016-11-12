/**
 * @file sys/wq.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
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

#include <sys/driver.h>

NTSTATUS FspWqCreateAndPostIrpWorkItem(PIRP Irp,
    FSP_IOP_REQUEST_WORK *WorkRoutine, FSP_IOP_REQUEST_FINI *RequestFini,
    BOOLEAN CreateAndPost);
VOID FspWqPostIrpWorkItem(PIRP Irp);
static VOID FspWqWorkRoutine(PVOID Context);
VOID FspWqOplockPrepare(PVOID Context, PIRP Irp);
VOID FspWqOplockComplete(PVOID Context, PIRP Irp);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspWqCreateAndPostIrpWorkItem)
#pragma alloc_text(PAGE, FspWqPostIrpWorkItem)
#pragma alloc_text(PAGE, FspWqWorkRoutine)
#pragma alloc_text(PAGE, FspWqOplockPrepare)
#pragma alloc_text(PAGE, FspWqOplockComplete)
#endif

static inline
NTSTATUS FspWqPrepareIrpWorkItem(PIRP Irp)
{
    NTSTATUS Result;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    /* lock/buffer the user buffer */
    if ((IRP_MJ_READ == IrpSp->MajorFunction || IRP_MJ_WRITE == IrpSp->MajorFunction) &&
        !FlagOn(IrpSp->MinorFunction, IRP_MN_MDL))
    {
        if (IRP_MJ_READ == IrpSp->MajorFunction)
            Result = FspLockUserBuffer(Irp, IrpSp->Parameters.Read.Length, IoWriteAccess);
        else
            Result = FspLockUserBuffer(Irp, IrpSp->Parameters.Write.Length, IoReadAccess);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    return STATUS_SUCCESS;
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
    if (STATUS_PENDING != Result)
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

VOID FspWqOplockPrepare(PVOID Context, PIRP Irp)
{
    PAGED_CODE();

    NTSTATUS Result;

    FSP_FSCTL_STATIC_ASSERT(sizeof(PVOID) == sizeof(VOID (*)(VOID)),
        "Data and code pointers must have same size!");

    Result = FspWqCreateAndPostIrpWorkItem(Irp, (FSP_IOP_REQUEST_WORK *)(UINT_PTR)Context, 0, TRUE);
    if (!NT_SUCCESS(Result))
        /*
         * Only way to communicate failure is through ExRaiseStatus.
         * We will catch it in FspCheckOplock, etc.
         */
        ExRaiseStatus(Result);
}

VOID FspWqOplockComplete(PVOID Context, PIRP Irp)
{
    PAGED_CODE();

    if (STATUS_SUCCESS == Irp->IoStatus.Status)
        FspWqPostIrpWorkItem(Irp);
    else
        FspIopCompleteIrp(Irp, Irp->IoStatus.Status);
}
