/**
 * @file sys/iop.c
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

NTSTATUS FspIopCreateRequestFunnel(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_IOP_REQUEST_FINI *RequestFini,
    ULONG Flags, FSP_FSCTL_TRANSACT_REQ **PRequest);
VOID FspIopDeleteRequest(FSP_FSCTL_TRANSACT_REQ *Request);
VOID FspIopResetRequest(FSP_FSCTL_TRANSACT_REQ *Request, FSP_IOP_REQUEST_FINI *RequestFini);
NTSTATUS FspIopPostWorkRequestFunnel(PDEVICE_OBJECT DeviceObject,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN AllocateIrpMustSucceed);
static IO_COMPLETION_ROUTINE FspIopPostWorkRequestCompletion;
VOID FspIopCompleteIrpEx(PIRP Irp, NTSTATUS Result, BOOLEAN DeviceDereference);
VOID FspIopCompleteCanceledIrp(PIRP Irp);
BOOLEAN FspIopRetryPrepareIrp(PIRP Irp, NTSTATUS *PResult);
BOOLEAN FspIopRetryCompleteIrp(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response, NTSTATUS *PResult);
FSP_FSCTL_TRANSACT_RSP *FspIopIrpResponse(PIRP Irp);
NTSTATUS FspIopDispatchPrepare(PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request);
NTSTATUS FspIopDispatchComplete(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspIopCreateRequestFunnel)
#pragma alloc_text(PAGE, FspIopDeleteRequest)
#pragma alloc_text(PAGE, FspIopResetRequest)
#pragma alloc_text(PAGE, FspIopPostWorkRequestFunnel)
#pragma alloc_text(PAGE, FspIopCompleteIrpEx)
#pragma alloc_text(PAGE, FspIopCompleteCanceledIrp)
#pragma alloc_text(PAGE, FspIopRetryPrepareIrp)
#pragma alloc_text(PAGE, FspIopRetryCompleteIrp)
#pragma alloc_text(PAGE, FspIopIrpResponse)
#pragma alloc_text(PAGE, FspIopDispatchPrepare)
#pragma alloc_text(PAGE, FspIopDispatchComplete)
#endif

/* Requests (and RequestHeaders) must be 16-byte aligned, because we use the low 4 bits for flags */
#if FSP_FSCTL_TRANSACT_REQ_ALIGNMENT <= MEMORY_ALLOCATION_ALIGNMENT
#define REQ_HEADER_ALIGN_MASK           0
#define REQ_HEADER_ALIGN_OVERHEAD       0
#else
#define REQ_HEADER_ALIGN_MASK           (FSP_FSCTL_TRANSACT_REQ_ALIGNMENT - 1)
#define REQ_HEADER_ALIGN_OVERHEAD       (sizeof(PVOID) + REQ_HEADER_ALIGN_MASK)
#endif

NTSTATUS FspIopCreateRequestFunnel(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_IOP_REQUEST_FINI *RequestFini,
    ULONG Flags, FSP_FSCTL_TRANSACT_REQ **PRequest)
{
    PAGED_CODE();

    FSP_FSCTL_TRANSACT_REQ_HEADER *RequestHeader;
    FSP_FSCTL_TRANSACT_REQ *Request;

    *PRequest = 0;

    if (0 != FileName)
        ExtraSize += FSP_FSCTL_DEFAULT_ALIGN_UP(FileName->Length + sizeof(WCHAR));

    if (FSP_FSCTL_TRANSACT_REQ_SIZEMAX < sizeof *Request + ExtraSize)
        return STATUS_INVALID_PARAMETER;

    if (FlagOn(Flags, FspIopRequestMustSucceed))
        RequestHeader = FspAllocatePoolMustSucceed(
            FlagOn(Flags, FspIopRequestNonPaged) ? NonPagedPool : PagedPool,
            sizeof *RequestHeader + sizeof *Request + ExtraSize + REQ_HEADER_ALIGN_OVERHEAD,
            FSP_ALLOC_INTERNAL_TAG);
    else
    {
        RequestHeader = ExAllocatePoolWithTag(
            FlagOn(Flags, FspIopRequestNonPaged) ? NonPagedPool : PagedPool,
            sizeof *RequestHeader + sizeof *Request + ExtraSize + REQ_HEADER_ALIGN_OVERHEAD,
            FSP_ALLOC_INTERNAL_TAG);
        if (0 == RequestHeader)
            return STATUS_INSUFFICIENT_RESOURCES;
    }

#if 0 != REQ_HEADER_ALIGN_MASK
    PVOID Allocation = RequestHeader;
    RequestHeader = (PVOID)(((UINT_PTR)RequestHeader + REQ_HEADER_ALIGN_OVERHEAD) &
        ~REQ_HEADER_ALIGN_MASK);
    ((PVOID *)RequestHeader)[-1] = Allocation;
#endif

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
    {
        ASSERT(0 == FspIrpRequest(Irp));
        FspIrpSetRequest(Irp, Request);
    }
    *PRequest = Request;

    return STATUS_SUCCESS;
}

VOID FspIopDeleteRequest(FSP_FSCTL_TRANSACT_REQ *Request)
{
    PAGED_CODE();

    FSP_FSCTL_TRANSACT_REQ_HEADER *RequestHeader = (PVOID)((PUINT8)Request - sizeof *RequestHeader);

    if (0 != RequestHeader->RequestFini)
        RequestHeader->RequestFini(Request, RequestHeader->Context);

    if (0 != RequestHeader->Response)
        FspFree(RequestHeader->Response);

#if 0 != REQ_HEADER_ALIGN_MASK
    RequestHeader = ((PVOID *)RequestHeader)[-1];
#endif

    FspFree(RequestHeader);
}

VOID FspIopResetRequest(FSP_FSCTL_TRANSACT_REQ *Request, FSP_IOP_REQUEST_FINI *RequestFini)
{
    PAGED_CODE();

    FSP_FSCTL_TRANSACT_REQ_HEADER *RequestHeader = (PVOID)((PUINT8)Request - sizeof *RequestHeader);

    if (0 != RequestHeader->RequestFini)
        RequestHeader->RequestFini(Request, RequestHeader->Context);

    RtlZeroMemory(&RequestHeader->Context, sizeof RequestHeader->Context);
    RequestHeader->RequestFini = RequestFini;
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

VOID FspIopCompleteIrpEx(PIRP Irp, NTSTATUS Result, BOOLEAN DeviceDereference)
{
    PAGED_CODE();

    ASSERT(STATUS_PENDING != Result);
    ASSERT(0 == (FSP_STATUS_PRIVATE_BIT & Result));

    if (0 != FspIrpRequest(Irp))
    {
        FspIopDeleteRequest(FspIrpRequest(Irp));
        FspIrpSetRequest(Irp, 0);
    }

    /* get the device object out of the IRP before completion */
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_OBJECT DeviceObject = IrpSp->DeviceObject;

    /*
     * HACK:
     *
     * Turns out that SRV2 sends an undocumented flavor of IRP_MJ_DIRECTORY_CONTROL /
     * IRP_MN_QUERY_DIRECTORY. These IRP's have a non-NULL Irp->MdlAddress. They expect
     * the FSD to fill the buffer pointed by Irp->MdlAddress and they cannot handle
     * completed IRP's with a non-NULL Irp->AssociatedIrp.SystemBuffer. So we have to
     * provide special support for these IRPs.
     *
     * While this processing is IRP_MJ_DIRECTORY_CONTROL specific, we do this here for
     * these reasons:
     *
     * 1.  There may be other IRP's that have similar completion requirements under SRV2.
     *     If/when such IRP's are discovered the completion processing can be centralized
     *     here.
     * 2.  IRP_MJ_DIRECTORY_CONTROL has a few different ways that it can complete IRP's.
     *     It is far simpler to do this processing here, even if not academically correct.
     *
     * This will have to be revisited if IRP_MJ_DIRECTORY_CONTROL processing changes
     * substantially (e.g. to no longer use Irp->AssociatedIrp.SystemBuffer).
     */
    if (IRP_MJ_DIRECTORY_CONTROL == IrpSp->MajorFunction &&
        IRP_MN_QUERY_DIRECTORY == IrpSp->MinorFunction &&
        0 != Irp->MdlAddress && /* SRV2 queries have this set */
        0 != Irp->AssociatedIrp.SystemBuffer &&
        FlagOn(Irp->Flags, IRP_BUFFERED_IO))
    {
        if (STATUS_SUCCESS == Result)
        {
            PVOID Address = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
            if (0 != Address)
                RtlCopyMemory(Address, Irp->AssociatedIrp.SystemBuffer, Irp->IoStatus.Information);
            else
                Result = STATUS_INSUFFICIENT_RESOURCES;
        }

        FspFreeExternal(Irp->AssociatedIrp.SystemBuffer);
        Irp->AssociatedIrp.SystemBuffer = 0;
        ClearFlag(Irp->Flags, IRP_INPUT_OPERATION | IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER);
    }

    if (STATUS_SUCCESS != Result && STATUS_REPARSE != Result && STATUS_BUFFER_OVERFLOW != Result)
        Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Result;
    IoCompleteRequest(Irp, FSP_IO_INCREMENT);

    if (DeviceDereference)
        FspDeviceDereference(DeviceObject);
}

VOID FspIopCompleteCanceledIrp(PIRP Irp)
{
    PAGED_CODE();

    DEBUGLOGIRP(Irp, STATUS_CANCELLED);

    /*
     * An IRP cancel may happen at any time including when APC's are still enabled.
     * For this reason we execute FsRtlEnterFileSystem/FsRtlExitFileSystem here.
     * This will protect ERESOURCE operations during Request finalizations.
     */
    FsRtlEnterFileSystem();

    PIRP TopLevelIrp = IoGetTopLevelIrp();
    IoSetTopLevelIrp(Irp);

    FspIopCompleteIrpEx(Irp, STATUS_CANCELLED, TRUE);

    IoSetTopLevelIrp(TopLevelIrp);

    FsRtlExitFileSystem();
}

BOOLEAN FspIopRetryPrepareIrp(PIRP Irp, NTSTATUS *PResult)
{
    PAGED_CODE();

    PDEVICE_OBJECT DeviceObject = IoGetCurrentIrpStackLocation(Irp)->DeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    return FspIoqPostIrpBestEffort(FsvolDeviceExtension->Ioq, Irp, PResult);
}

BOOLEAN FspIopRetryCompleteIrp(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response, NTSTATUS *PResult)
{
    PAGED_CODE();

    PDEVICE_OBJECT DeviceObject = IoGetCurrentIrpStackLocation(Irp)->DeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    FSP_FSCTL_TRANSACT_REQ_HEADER *RequestHeader = (PVOID)((PUINT8)Request - sizeof *RequestHeader);

    ASSERT(0 != Request);

    if (0 != Response && RequestHeader->Response != Response)
    {
        if (0 != RequestHeader->Response)
            FspFree(RequestHeader->Response);
        RequestHeader->Response = FspAllocMustSucceed(Response->Size);
        RtlCopyMemory(RequestHeader->Response, Response, Response->Size);
        Response = RequestHeader->Response;
    }

    return FspIoqRetryCompleteIrp(FsvolDeviceExtension->Ioq, Irp, PResult);
}

FSP_FSCTL_TRANSACT_RSP *FspIopIrpResponse(PIRP Irp)
{
    PAGED_CODE();

    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    FSP_FSCTL_TRANSACT_REQ_HEADER *RequestHeader = (PVOID)((PUINT8)Request - sizeof *RequestHeader);

    return RequestHeader->Response;
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

NTSTATUS FspIopDispatchComplete(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    ASSERT(IRP_MJ_MAXIMUM_FUNCTION >= IrpSp->MajorFunction);
    ASSERT(0 != FspIopCompleteFunction[IrpSp->MajorFunction]);

    return FspIopCompleteFunction[IrpSp->MajorFunction](Irp, Response);
}

FSP_IOPREP_DISPATCH *FspIopPrepareFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
FSP_IOCMPL_DISPATCH *FspIopCompleteFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
