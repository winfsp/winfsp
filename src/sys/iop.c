/**
 * @file sys/iop.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspIopCreateRequestEx(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_IOP_REQUEST_FINI *RequestFini,
    FSP_FSCTL_TRANSACT_REQ **PRequest);
static VOID FspIopDeleteRequest(FSP_FSCTL_TRANSACT_REQ *Request);
PVOID *FspIopRequestContextAddress(FSP_FSCTL_TRANSACT_REQ *Request, ULONG I);
NTSTATUS FspIopPostWorkRequest(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_TRANSACT_REQ *Request);
static IO_COMPLETION_ROUTINE FspIopPostWorkRequestCompletion;
VOID FspIopCompleteIrpEx(PIRP Irp, NTSTATUS Result, BOOLEAN DeviceRelease);
NTSTATUS FspIopDispatchPrepare(PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request);
VOID FspIopDispatchComplete(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspIopCreateRequestEx)
#pragma alloc_text(PAGE, FspIopDeleteRequest)
#pragma alloc_text(PAGE, FspIopRequestContextAddress)
#pragma alloc_text(PAGE, FspIopPostWorkRequest)
#pragma alloc_text(PAGE, FspIopCompleteIrpEx)
#pragma alloc_text(PAGE, FspIopDispatchPrepare)
#pragma alloc_text(PAGE, FspIopDispatchComplete)
#endif

typedef struct
{
    FSP_IOP_REQUEST_FINI *RequestFini;
    PVOID Context[3];
    __declspec(align(MEMORY_ALLOCATION_ALIGNMENT)) UINT8 RequestBuf[];
} FSP_FSCTL_TRANSACT_REQ_HEADER;

NTSTATUS FspIopCreateRequestEx(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_IOP_REQUEST_FINI *RequestFini,
    FSP_FSCTL_TRANSACT_REQ **PRequest)
{
    PAGED_CODE();

    FSP_FSCTL_TRANSACT_REQ_HEADER *RequestHeader;
    FSP_FSCTL_TRANSACT_REQ *Request;

    *PRequest = 0;

    if (0 != FileName)
        ExtraSize += FSP_FSCTL_DEFAULT_ALIGN_UP(FileName->Length + sizeof(WCHAR));

    if (FSP_FSCTL_TRANSACT_REQ_SIZEMAX < sizeof *Request + ExtraSize)
        return STATUS_INVALID_PARAMETER;

    RequestHeader = FspAlloc(sizeof *RequestHeader + sizeof *Request + ExtraSize);
    if (0 == RequestHeader)
        return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(RequestHeader, sizeof *RequestHeader + sizeof *Request + ExtraSize);
    RequestHeader->RequestFini = RequestFini;

    Request = (PVOID)RequestHeader->RequestBuf;
    Request->Size = (UINT16)(sizeof *Request + ExtraSize);
    Request->Hint = (UINT_PTR)Irp;
    if (0 != FileName)
    {
        RtlCopyMemory(Request->Buffer, FileName->Buffer, FileName->Length);
        //Request->Buffer[FileName->Length] = '\0';
        //Request->Buffer[FileName->Length + 1] = '\0';
        //Request->FileName.Offset = 0;
        Request->FileName.Size = FileName->Length + sizeof(WCHAR);
    }

    if (0 != Irp)
        FspIrpRequest(Irp) = Request;
    *PRequest = Request;

    return STATUS_SUCCESS;
}

static VOID FspIopDeleteRequest(FSP_FSCTL_TRANSACT_REQ *Request)
{
    PAGED_CODE();

    FSP_FSCTL_TRANSACT_REQ_HEADER *RequestHeader = (PVOID)((PUINT8)Request - sizeof *RequestHeader);

    if (0 != RequestHeader->RequestFini)
        RequestHeader->RequestFini(RequestHeader->Context);

    FspFree(RequestHeader);
}

PVOID *FspIopRequestContextAddress(FSP_FSCTL_TRANSACT_REQ *Request, ULONG I)
{
    PAGED_CODE();

    FSP_FSCTL_TRANSACT_REQ_HEADER *RequestHeader = (PVOID)((PUINT8)Request - sizeof *RequestHeader);

    return &RequestHeader->Context[I];
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
    PAGED_CODE();

    ASSERT(STATUS_PENDING != Result);

    if (0 != FspIrpRequest(Irp))
    {
        FspIopDeleteRequest(FspIrpRequest(Irp));
        FspIrpRequest(Irp) = 0;
    }

    /* get the device object out of the IRP before completion */
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
