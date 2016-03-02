/**
 * @file sys/wq.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static VOID FspWqWorkRoutine(PVOID Context);

NTSTATUS FspWqCreateAndPostIrpWorkItem(PIRP Irp,
    FSP_WQ_REQUEST_WORK *WorkRoutine, FSP_IOP_REQUEST_FINI *RequestFini,
    BOOLEAN CreateAndPost)
{
    FSP_FSCTL_TRANSACT_REQ *RequestWorkItem = FspIrpRequest(Irp);

    if (0 == RequestWorkItem)
    {
        NTSTATUS Result;

        /* probe and lock the user buffer (if not an MDL request) */
        PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
        if (0 == Irp->MdlAddress &&
            (IRP_MJ_READ == IrpSp->MajorFunction || IRP_MJ_WRITE == IrpSp->MajorFunction) &&
            !FlagOn(IrpSp->MinorFunction, IRP_MN_MDL))
        {
            Result = FspLockUserBuffer(Irp->UserBuffer, IrpSp->Parameters.Write.Length,
                Irp->RequestorMode,
                IRP_MJ_READ == IrpSp->MajorFunction ? IoWriteAccess : IoReadAccess,
                &Irp->MdlAddress);
            if (!NT_SUCCESS(Result))
                return Result;
        }

        Result = FspIopCreateRequestWorkItem(Irp, sizeof(WORK_QUEUE_ITEM),
            RequestFini, &RequestWorkItem);
        if (!NT_SUCCESS(Result))
            return Result;

        ASSERT(sizeof(FSP_WQ_REQUEST_WORK *) == sizeof(PVOID));

        FspIopRequestContext(RequestWorkItem, FspWqRequestWorkRoutine) =
            (PVOID)(UINT_PTR)WorkRoutine;
        ExInitializeWorkItem((PWORK_QUEUE_ITEM)&RequestWorkItem->Buffer, FspWqWorkRoutine, Irp);
    }

    if (!CreateAndPost)
        return STATUS_SUCCESS;

    FspWqPostIrpWorkItem(Irp);
    return STATUS_PENDING;
}

VOID FspWqPostIrpWorkItem(PIRP Irp)
{
    FSP_FSCTL_TRANSACT_REQ *RequestWorkItem = FspIrpRequest(Irp);

    ASSERT(RequestWorkItem->Kind == FspFsctlTransactReservedKind);
    ASSERT(RequestWorkItem->Size == sizeof *RequestWorkItem + sizeof(WORK_QUEUE_ITEM));
    ASSERT(RequestWorkItem->Hint == (UINT_PTR)Irp);

    IoMarkIrpPending(Irp);
    ExQueueWorkItem((PWORK_QUEUE_ITEM)&RequestWorkItem->Buffer, CriticalWorkQueue);
}

static VOID FspWqWorkRoutine(PVOID Context)
{
    PIRP Irp = Context;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    FSP_FSCTL_TRANSACT_REQ *RequestWorkItem = FspIrpRequest(Irp);
    FSP_WQ_REQUEST_WORK *WorkRoutine = (FSP_WQ_REQUEST_WORK *)(UINT_PTR)
        FspIopRequestContext(RequestWorkItem, FspWqRequestWorkRoutine);
    NTSTATUS Result;

    IoSetTopLevelIrp(Irp);

    Result = WorkRoutine(IrpSp->DeviceObject, Irp, IrpSp, FALSE);
    if (STATUS_PENDING != Result)
    {
        DEBUGLOGIRP(Irp, Result);
        FspIopCompleteIrp(Irp, Result);
    }

    IoSetTopLevelIrp(0);
}
