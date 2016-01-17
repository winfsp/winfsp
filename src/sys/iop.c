/**
 * @file sys/iop.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspIopCreateRequestFunnel(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_IOP_REQUEST_FINI *RequestFini,
    BOOLEAN MustSucceed,
    FSP_FSCTL_TRANSACT_REQ **PRequest);
VOID FspIopDeleteRequest(FSP_FSCTL_TRANSACT_REQ *Request);
PVOID *FspIopRequestContextAddress(FSP_FSCTL_TRANSACT_REQ *Request, ULONG I);
NTSTATUS FspIopPostWorkRequestFunnel(PDEVICE_OBJECT DeviceObject,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN AllocateIrpMustSucceed);
static IO_COMPLETION_ROUTINE FspIopPostWorkRequestCompletion;
VOID FspIopCompleteIrpEx(PIRP Irp, NTSTATUS Result, BOOLEAN DeviceRelease);
VOID FspIopCompleteCanceledIrp(PIRP Irp);
NTSTATUS FspIopDispatchPrepare(PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request);
VOID FspIopDispatchComplete(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspIopCreateRequestFunnel)
#pragma alloc_text(PAGE, FspIopDeleteRequest)
#pragma alloc_text(PAGE, FspIopRequestContextAddress)
#pragma alloc_text(PAGE, FspIopPostWorkRequestFunnel)
#pragma alloc_text(PAGE, FspIopCompleteIrpEx)
#pragma alloc_text(PAGE, FspIopCompleteCanceledIrp)
#pragma alloc_text(PAGE, FspIopDispatchPrepare)
#pragma alloc_text(PAGE, FspIopDispatchComplete)
#endif

static const LONG Delays[] =
{
    -100,
    -200,
    -300,
    -400,
    -500,
    -1000,
};

static PVOID FspAllocMustSucceed(SIZE_T Size)
{
    PVOID Result;
    LARGE_INTEGER Delay;

    for (ULONG i = 0, n = sizeof(Delays) / sizeof(Delays[0]);; i++)
    {
        Result = FspAlloc(Size);
        if (0 != Result)
            return Result;

        Delay.QuadPart = n > i ? Delays[i] : Delays[n - 1];
        KeDelayExecutionThread(KernelMode, FALSE, &Delay);
    }
}

static PVOID FspAllocateIrpMustSucceed(CCHAR StackSize)
{
    PIRP Result;
    LARGE_INTEGER Delay;

    for (ULONG i = 0, n = sizeof(Delays) / sizeof(Delays[0]);; i++)
    {
        Result = IoAllocateIrp(StackSize, FALSE);
        if (0 != Result)
            return Result;

        Delay.QuadPart = n > i ? Delays[i] : Delays[n - 1];
        KeDelayExecutionThread(KernelMode, FALSE, &Delay);
    }
}

typedef struct
{
    FSP_IOP_REQUEST_FINI *RequestFini;
    PVOID Context[3];
    __declspec(align(MEMORY_ALLOCATION_ALIGNMENT)) UINT8 RequestBuf[];
} FSP_FSCTL_TRANSACT_REQ_HEADER;

NTSTATUS FspIopCreateRequestFunnel(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_IOP_REQUEST_FINI *RequestFini,
    BOOLEAN MustSucceed,
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

    if (MustSucceed)
        RequestHeader = FspAllocMustSucceed(sizeof *RequestHeader + sizeof *Request + ExtraSize);
    else
    {
        RequestHeader = FspAlloc(sizeof *RequestHeader + sizeof *Request + ExtraSize);
        if (0 == RequestHeader)
            return STATUS_INSUFFICIENT_RESOURCES;
    }

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

VOID FspIopDeleteRequest(FSP_FSCTL_TRANSACT_REQ *Request)
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

NTSTATUS FspIopPostWorkRequestFunnel(PDEVICE_OBJECT DeviceObject,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN BestEffort)
{
    PAGED_CODE();

    ASSERT(0 == Request->Hint);

    NTSTATUS Result;
    PIRP Irp;

    if (BestEffort)
        Irp = FspAllocateIrpMustSucceed(DeviceObject->StackSize);
    else
    {
        Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);
        if (0 == Irp)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }
    }

    PIO_STACK_LOCATION IrpSp = IoGetNextIrpStackLocation(Irp);
    Irp->RequestorMode = KernelMode;
    IrpSp->MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL;
    IrpSp->MinorFunction = IRP_MN_USER_FS_REQUEST;
    IrpSp->Parameters.FileSystemControl.FsControlCode = BestEffort ?
        FSP_FSCTL_WORK_BEST_EFFORT : FSP_FSCTL_WORK;
    IrpSp->Parameters.FileSystemControl.InputBufferLength = Request->Size;
    IrpSp->Parameters.FileSystemControl.Type3InputBuffer = Request;

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
    ASSERT(0 == (FSP_STATUS_PRIVATE_BIT & Result));

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

VOID FspIopCompleteCanceledIrp(PIRP Irp)
{
    PAGED_CODE();

#if DBG
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    DEBUGLOG("IRP=%p, %s%c, %s%s",
        Irp,
        (const char *)&FspDeviceExtension(IrpSp->DeviceObject)->Kind,
        Irp->RequestorMode == KernelMode ? 'K' : 'U',
        IrpMajorFunctionSym(IrpSp->MajorFunction),
        IrpMinorFunctionSym(IrpSp->MajorFunction, IrpSp->MinorFunction));
#endif

    FspIopCompleteIrpEx(Irp, STATUS_CANCELLED, TRUE);
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
