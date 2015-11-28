/**
 * @file sys/iop.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

VOID FspIopDispatchComplete(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspIopDispatchComplete)
#endif

VOID FspIopCompleteRequest(PIRP Irp, NTSTATUS Result)
{
    // !PAGED_CODE();

    ASSERT(STATUS_PENDING != Result);

    if (0 != Irp->Tail.Overlay.DriverContext[0])
    {
        ExFreePoolWithTag(Irp->Tail.Overlay.DriverContext[0], FSP_TAG);
        Irp->Tail.Overlay.DriverContext[0] = 0;
    }

    if (!NT_SUCCESS(Result))
        Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Result;
    IoCompleteRequest(Irp, FSP_IO_INCREMENT);
}

VOID FspIopDispatchComplete(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    ASSERT(IRP_MJ_MAXIMUM_FUNCTION >= IrpSp->MajorFunction);
    ASSERT(0 != FspIopCompleteFunction[IrpSp->MajorFunction]);

    FspIopCompleteFunction[IrpSp->MajorFunction](Irp, Response);
}

FSP_IOCMPL_DISPATCH *FspIopCompleteFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
