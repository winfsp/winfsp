/**
 * @file sys/iop.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspIopCreateRequest(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_FSCTL_TRANSACT_REQ **PRequest);
NTSTATUS FspIopPostWorkRequest(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_TRANSACT_REQ *Request);
static IO_COMPLETION_ROUTINE FspIopPostWorkRequestCompletion;
NTSTATUS FspIopDispatchPrepare(PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request);
VOID FspIopDispatchComplete(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspIopCreateRequest)
#pragma alloc_text(PAGE, FspIopPostWorkRequest)
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
        Request->FileName.Offset = 0;
        Request->FileName.Size = FileName->Length + sizeof(WCHAR);
    }
    else
    {
        Request->FileName.Offset = 0;
        Request->FileName.Size = 0;
    }

    if (0 != Irp)
        FspIrpContextRequest(Irp) = Request;
    *PRequest = Request;

    return STATUS_SUCCESS;
}

static inline
VOID FspIopDeleteRequest(FSP_FSCTL_TRANSACT_REQ *Request)
{
    ExFreePoolWithTag(Request, FSP_TAG);
}

NTSTATUS FspIopPostWorkRequest(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_TRANSACT_REQ *Request)
{
    PAGED_CODE();

    ASSERT(0 == Request->Hint);

    NTSTATUS Result;
    PIRP Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);
    if (0 == Irp)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    PIO_STACK_LOCATION IrpSp = IoGetNextIrpStackLocation(Irp);
    IrpSp->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    IrpSp->Parameters.DeviceIoControl.IoControlCode = FSP_FSCTL_WORK;
    IrpSp->Parameters.DeviceIoControl.InputBufferLength = Request->Size;
    IrpSp->Parameters.DeviceIoControl.Type3InputBuffer = Request;

    ASSERT(METHOD_NEITHER == (IrpSp->Parameters.DeviceIoControl.IoControlCode & 3));

    IoSetCompletionRoutine(Irp, FspIopPostWorkRequestCompletion, 0, TRUE, TRUE, TRUE);

    Result = IoCallDriver(DeviceObject, Irp);
    if (STATUS_PENDING == Result)
        return STATUS_SUCCESS;

    /*
     * If we did not receive STATUS_PENDING, we still own the Request and must delete it!
     */

exit:
    FspIopDeleteRequest(Request);

    return Result;
}

static NTSTATUS FspIopPostWorkRequestCompletion(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    // !PAGED_CODE();

    IoFreeIrp(Irp);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

VOID FspIopCompleteIrpEx(PIRP Irp, NTSTATUS Result, BOOLEAN DeviceRelease)
{
    // !PAGED_CODE();

    ASSERT(STATUS_PENDING != Result);
    ASSERT(0 == Irp->Tail.Overlay.DriverContext[3]);

    if (0 != FspIrpContextRequest(Irp))
    {
        FspIopDeleteRequest(FspIrpContextRequest(Irp));
        FspIrpContextRequest(Irp) = 0;
    }

    if (0 != FspIrpContextHandle(Irp))
    {
#if DBG
        NTSTATUS Result0;
        Result0 = ObCloseHandle(FspIrpContextHandle(Irp), KernelMode);
        if (!NT_SUCCESS(Result0))
            DEBUGLOG("ObCloseHandle() = %s", NtStatusSym(Result0));
#else
        ObCloseHandle(FspIrpContextHandle(Irp), KernelMode);
#endif

        FspIrpContextHandle(Irp) = 0;
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
