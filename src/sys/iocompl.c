/**
 * @file sys/iocompl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

#ifdef ALLOC_PRAGMA
#endif

VOID FspCompleteRequest(PIRP Irp, NTSTATUS Result)
{
    // !PAGED_CODE();

    ASSERT(STATUS_PENDING != Result);

    if (0 != Irp->Tail.Overlay.DriverContext[0])
        ExFreePoolWithTag(Irp->Tail.Overlay.DriverContext[0], FSP_TAG);

    if (!NT_SUCCESS(Result))
        Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Result;
    IoCompleteRequest(Irp, FSP_IO_INCREMENT);
}

VOID FspDispatchProcessedIrp(PIRP Irp, FSP_FSCTL_TRANSACT_RSP *Response)
{
}
