/**
 * @file sys/iop.c
 *
 * @copyright 2015-2021 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#include <sys/driver.h>

NTSTATUS FspIopCreateRequestFunnel(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_IOP_REQUEST_FINI *RequestFini,
    ULONG Flags, FSP_FSCTL_TRANSACT_REQ **PRequest);
NTSTATUS FspIopCreateRequestWorkItem(FSP_FSCTL_TRANSACT_REQ *Request);
VOID FspIopDeleteRequest(FSP_FSCTL_TRANSACT_REQ *Request);
VOID FspIopResetRequest(FSP_FSCTL_TRANSACT_REQ *Request, FSP_IOP_REQUEST_FINI *RequestFini);
NTSTATUS FspIopPostWorkRequestFunnel(PDEVICE_OBJECT DeviceObject,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN AllocateIrpMustSucceed);
static IO_COMPLETION_ROUTINE FspIopPostWorkRequestCompletion;
VOID FspIopCompleteIrpEx(PIRP Irp, NTSTATUS Result, BOOLEAN DeviceDereference);
VOID FspIopCompleteCanceledIrp(PIRP Irp);
BOOLEAN FspIopRetryPrepareIrp(PIRP Irp, NTSTATUS *PResult);
BOOLEAN FspIopRetryCompleteIrp(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response, NTSTATUS *PResult);
VOID FspIopSetIrpResponse(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);
FSP_FSCTL_TRANSACT_RSP *FspIopIrpResponse(PIRP Irp);
NTSTATUS FspIopDispatchPrepare(PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request);
NTSTATUS FspIopDispatchComplete(PIRP Irp, FSP_FSCTL_TRANSACT_RSP *Response);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspIopCreateRequestFunnel)
#pragma alloc_text(PAGE, FspIopCreateRequestWorkItem)
#pragma alloc_text(PAGE, FspIopDeleteRequest)
#pragma alloc_text(PAGE, FspIopResetRequest)
#pragma alloc_text(PAGE, FspIopPostWorkRequestFunnel)
#pragma alloc_text(PAGE, FspIopCompleteIrpEx)
#pragma alloc_text(PAGE, FspIopCompleteCanceledIrp)
#pragma alloc_text(PAGE, FspIopRetryPrepareIrp)
#pragma alloc_text(PAGE, FspIopRetryCompleteIrp)
#pragma alloc_text(PAGE, FspIopSetIrpResponse)
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
    FSP_FSCTL_TRANSACT_REQ_WORK_ITEM *RequestWorkItem = 0;
    FSP_FSCTL_TRANSACT_REQ *Request;

    *PRequest = 0;

    if (0 != FileName)
        ExtraSize += FSP_FSCTL_DEFAULT_ALIGN_UP(FileName->Length + sizeof(WCHAR));

    if (FSP_FSCTL_TRANSACT_REQ_SIZEMAX < sizeof *Request + ExtraSize)
        return STATUS_INVALID_PARAMETER;

    if (FlagOn(Flags, FspIopCreateRequestMustSucceedFlag))
    {
        RequestHeader = FspAllocatePoolMustSucceed(
            FlagOn(Flags, FspIopCreateRequestNonPagedFlag) ? NonPagedPool : PagedPool,
            sizeof *RequestHeader + sizeof *Request + ExtraSize + REQ_HEADER_ALIGN_OVERHEAD,
            FSP_ALLOC_INTERNAL_TAG);

        if (FlagOn(Flags, FspIopCreateRequestWorkItemFlag))
        {
            RequestWorkItem = FspAllocatePoolMustSucceed(
                NonPagedPool, sizeof *RequestWorkItem, FSP_ALLOC_INTERNAL_TAG);

            RtlZeroMemory(RequestWorkItem, sizeof *RequestWorkItem);
        }
    }
    else
    {
        RequestHeader = ExAllocatePoolWithTag(
            FlagOn(Flags, FspIopCreateRequestNonPagedFlag) ? NonPagedPool : PagedPool,
            sizeof *RequestHeader + sizeof *Request + ExtraSize + REQ_HEADER_ALIGN_OVERHEAD,
            FSP_ALLOC_INTERNAL_TAG);
        if (0 == RequestHeader)
            return STATUS_INSUFFICIENT_RESOURCES;

        if (FlagOn(Flags, FspIopCreateRequestWorkItemFlag))
        {
            RequestWorkItem = FspAllocNonPaged(sizeof *RequestWorkItem);
            if (0 == RequestWorkItem)
            {
                FspFree(RequestHeader);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            RtlZeroMemory(RequestWorkItem, sizeof *RequestWorkItem);
        }
    }

#if 0 != REQ_HEADER_ALIGN_MASK
    PVOID Allocation = RequestHeader;
    RequestHeader = (PVOID)(((UINT_PTR)RequestHeader + REQ_HEADER_ALIGN_OVERHEAD) &
        ~REQ_HEADER_ALIGN_MASK);
    ((PVOID *)RequestHeader)[-1] = Allocation;
#endif

    RtlZeroMemory(RequestHeader, sizeof *RequestHeader + sizeof *Request + ExtraSize);
    RequestHeader->RequestFini = RequestFini;
    RequestHeader->WorkItem = RequestWorkItem;

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

    ASSERT(0 == ((UINT_PTR)Request & (FSP_FSCTL_TRANSACT_REQ_ALIGNMENT - 1)));

    if (0 != Irp)
    {
        ASSERT(0 == FspIrpRequest(Irp));
        FspIrpSetRequest(Irp, Request);
    }
    *PRequest = Request;

    return STATUS_SUCCESS;
}

NTSTATUS FspIopCreateRequestWorkItem(FSP_FSCTL_TRANSACT_REQ *Request)
{
    PAGED_CODE();

    FSP_FSCTL_TRANSACT_REQ_HEADER *RequestHeader = (PVOID)((PUINT8)Request - sizeof *RequestHeader);
    FSP_FSCTL_TRANSACT_REQ_WORK_ITEM *RequestWorkItem;

    if (0 == RequestHeader->WorkItem)
    {
        RequestWorkItem = FspAllocNonPaged(sizeof *RequestWorkItem);
        if (0 == RequestWorkItem)
            return STATUS_INSUFFICIENT_RESOURCES;

        RtlZeroMemory(RequestWorkItem, sizeof *RequestWorkItem);
        RequestHeader->WorkItem = RequestWorkItem;
    }

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

    if (0 != RequestHeader->WorkItem)
        FspFree(RequestHeader->WorkItem);

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
     * We update the Create statistics here to avoid doing it in multiple places.
     */
    if (IRP_MJ_CREATE == IrpSp->MajorFunction)
    {
        /* only update statistics if we actually have a reference to the DeviceObject */
        if (DeviceDereference)
        {
            FSP_DEVICE_EXTENSION *DeviceExtension = FspDeviceExtension(DeviceObject);

            if (FspFsvolDeviceExtensionKind == FspDeviceExtension(DeviceObject)->Kind)
            {
                FSP_STATISTICS *Statistics = FspStatistics(
                    ((FSP_FSVOL_DEVICE_EXTENSION *)DeviceExtension)->Statistics);

                FspStatisticsInc(Statistics, Specific.CreateHits);
                if (STATUS_SUCCESS == Result)
                    FspStatisticsInc(Statistics, Specific.SuccessfulCreates);
                else
                    FspStatisticsInc(Statistics, Specific.FailedCreates);
            }
        }
    }

    if (STATUS_SUCCESS != Result &&
        STATUS_REPARSE != Result &&
        STATUS_OPLOCK_BREAK_IN_PROGRESS != Result &&
        STATUS_BUFFER_OVERFLOW != Result &&
        STATUS_SHARING_VIOLATION != Result)
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

    FspIopSetIrpResponse(Irp, Response);

    return FspIoqRetryCompleteIrp(FsvolDeviceExtension->Ioq, Irp, PResult);
}

VOID FspIopSetIrpResponse(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

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

NTSTATUS FspIopDispatchComplete(PIRP Irp, FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    ASSERT(IRP_MJ_MAXIMUM_FUNCTION >= IrpSp->MajorFunction);
    ASSERT(0 != FspIopCompleteFunction[IrpSp->MajorFunction]);

    if (STATUS_PENDING == Response->IoStatus.Status ||
        FlagOn(Response->IoStatus.Status, FSP_STATUS_PRIVATE_BIT | FSP_STATUS_IGNORE_BIT))
        Response->IoStatus.Status = (UINT32)STATUS_INTERNAL_ERROR;
    return FspIopCompleteFunction[IrpSp->MajorFunction](Irp, Response);
}

FSP_IOPREP_DISPATCH *FspIopPrepareFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
FSP_IOCMPL_DISPATCH *FspIopCompleteFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
