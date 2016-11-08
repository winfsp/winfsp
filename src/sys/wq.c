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

static VOID FspWqWorkRoutine(PVOID Context);

NTSTATUS FspWqCreateAndPostIrpWorkItem(PIRP Irp,
    FSP_IOP_REQUEST_WORK *WorkRoutine, FSP_IOP_REQUEST_FINI *RequestFini,
    BOOLEAN CreateAndPost)
{
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);

    if (0 == Request)
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

        Result = FspIopCreateRequestWorkItem(Irp, sizeof(WORK_QUEUE_ITEM),
            RequestFini, &Request);
        if (!NT_SUCCESS(Result))
            return Result;

        ASSERT(sizeof(FSP_IOP_REQUEST_WORK *) == sizeof(PVOID));

        FspIopRequestContext(Request, FspWqRequestWorkRoutine) =
            (PVOID)(UINT_PTR)WorkRoutine;
        ExInitializeWorkItem((PWORK_QUEUE_ITEM)&Request->Buffer, FspWqWorkRoutine, Irp);
    }

    if (!CreateAndPost)
        return STATUS_SUCCESS;

    FspWqPostIrpWorkItem(Irp);
    return STATUS_PENDING;
}

VOID FspWqPostIrpWorkItem(PIRP Irp)
{
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);

    ASSERT(Request->Kind == FspFsctlTransactReservedKind);
    ASSERT(Request->Size == sizeof *Request + sizeof(WORK_QUEUE_ITEM));
    ASSERT(Request->Hint == (UINT_PTR)Irp);

    IoMarkIrpPending(Irp);
    ExQueueWorkItem((PWORK_QUEUE_ITEM)&Request->Buffer, CriticalWorkQueue);
}

static VOID FspWqWorkRoutine(PVOID Context)
{
    PIRP Irp = Context;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_OBJECT DeviceObject = IrpSp->DeviceObject;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    FSP_IOP_REQUEST_WORK *WorkRoutine = (FSP_IOP_REQUEST_WORK *)(UINT_PTR)
        FspIopRequestContext(Request, FspWqRequestWorkRoutine);
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
