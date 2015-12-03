/**
 * @file sys/iop.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspIopCreateRequest(PIRP Irp, ULONG ExtraSize, FSP_FSCTL_TRANSACT_REQ **PRequest);
VOID FspIopDispatchComplete(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspIopCreateRequest)
#pragma alloc_text(PAGE, FspIopDispatchComplete)
#endif

NTSTATUS FspIopCreateRequest(PIRP Irp, ULONG ExtraSize, FSP_FSCTL_TRANSACT_REQ **PRequest)
{
    PAGED_CODE();

    *PRequest = 0;

    FSP_FSCTL_TRANSACT_REQ *Request = ExAllocatePoolWithTag(PagedPool,
        sizeof *Request + ExtraSize, FSP_TAG);
    if (0 == Request)
        return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(Request, sizeof *Request + ExtraSize);
    Request->Size = (UINT16)(sizeof *Request + ExtraSize);
    Request->Hint = (UINT_PTR)Irp;

    Irp->Tail.Overlay.DriverContext[0] = Request;
    *PRequest = Request;

    return STATUS_SUCCESS;
}

VOID FspIopCompleteRequest(PIRP Irp, NTSTATUS Result)
{
    // !PAGED_CODE();

    ASSERT(STATUS_PENDING != Result);

    if (0 != Irp->Tail.Overlay.DriverContext[0])
    {
        ExFreePoolWithTag(Irp->Tail.Overlay.DriverContext[0], FSP_TAG);
        Irp->Tail.Overlay.DriverContext[0] = 0;
    }

    PDEVICE_OBJECT DeviceObject = IoGetCurrentIrpStackLocation(Irp)->DeviceObject;

    if (!NT_SUCCESS(Result))
        Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Result;
    IoCompleteRequest(Irp, FSP_IO_INCREMENT);

    FspDeviceRelease(DeviceObject);
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
