/**
 * @file sys/iop.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspIopCreateRequest(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_FSCTL_TRANSACT_REQ **PRequest);
NTSTATUS FspIopDispatchPrepare(PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request);
VOID FspIopDispatchComplete(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspIopCreateRequest)
#pragma alloc_text(PAGE, FspIopDispatchPrepare)
#pragma alloc_text(PAGE, FspIopDispatchComplete)
#endif

NTSTATUS FspIopCreateRequest(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_FSCTL_TRANSACT_REQ **PRequest)
{
    PAGED_CODE();

    FSP_FSCTL_TRANSACT_REQ *Request;

    *PRequest = 0;

    if (0 != FileName)
        ExtraSize += FSP_FSCTL_DEFAULT_ALIGN_UP(FileName->Length + sizeof(WCHAR));

    if (FSP_FSCTL_TRANSACT_REQ_SIZEMAX < sizeof *Request + ExtraSize)
        return STATUS_INVALID_PARAMETER;

    Request = ExAllocatePoolWithTag(PagedPool,
        sizeof *Request + ExtraSize, FSP_TAG);
    if (0 == Request)
        return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(Request, sizeof *Request + ExtraSize);
    Request->Size = (UINT16)(sizeof *Request + ExtraSize);
    Request->Hint = (UINT_PTR)Irp;
    if (0 != FileName)
    {
        RtlCopyMemory(Request->Buffer, FileName->Buffer, FileName->Length);
        Request->Buffer[FileName->Length] = '\0';
        Request->Buffer[FileName->Length + 1] = '\0';
    }

    Irp->Tail.Overlay.DriverContext[0] = Request;
    *PRequest = Request;

    return STATUS_SUCCESS;
}

VOID FspIopCompleteRequestEx(PIRP Irp, NTSTATUS Result, BOOLEAN DeviceRelease)
{
    // !PAGED_CODE();

    ASSERT(STATUS_PENDING != Result);

    if (0 != Irp->Tail.Overlay.DriverContext[0])
    {
        ExFreePoolWithTag(Irp->Tail.Overlay.DriverContext[0], FSP_TAG);
        Irp->Tail.Overlay.DriverContext[0] = 0;
    }

    if (0 != Irp->Tail.Overlay.DriverContext[1])
    {
#if DBG
        NTSTATUS Result0;
        Result0 = ObCloseHandle(Irp->Tail.Overlay.DriverContext[1], KernelMode);
        if (!NT_SUCCESS(Result0))
            DEBUGLOG("ObCloseHandle() = %s", NtStatusSym(Result0));
#else
        ObCloseHandle(Irp->Tail.Overlay.DriverContext[1], KernelMode);
#endif

        Irp->Tail.Overlay.DriverContext[1] = 0;
    }

    PDEVICE_OBJECT DeviceObject = IoGetCurrentIrpStackLocation(Irp)->DeviceObject;

    if (!NT_SUCCESS(Result))
        Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Result;
    IoCompleteRequest(Irp, FSP_IO_INCREMENT);

    if (DeviceRelease)
        FspDeviceRelease(DeviceObject);
}

NTSTATUS FspIopDispatchPrepare(PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request)
{
    PAGED_CODE();

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    ASSERT(IRP_MJ_MAXIMUM_FUNCTION >= IrpSp->MajorFunction);
    if (0 != FspIopPrepareFunction[IrpSp->MajorFunction])
        return FspIopPrepareFunction[IrpSp->MajorFunction](Irp, Request);
    else
        return STATUS_SUCCESS;
}

VOID FspIopDispatchComplete(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    ASSERT(IRP_MJ_MAXIMUM_FUNCTION >= IrpSp->MajorFunction);
    ASSERT(0 != FspIopCompleteFunction[IrpSp->MajorFunction]);

    FspIopCompleteFunction[IrpSp->MajorFunction](Irp, Response);
}

FSP_IOPREP_DISPATCH *FspIopPrepareFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
FSP_IOCMPL_DISPATCH *FspIopCompleteFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
