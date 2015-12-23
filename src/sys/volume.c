/**
 * @file sys/volume.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspVolumeCreate(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
VOID FspVolumeDelete(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeMount(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeGetName(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeTransact(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeRedirQueryPathEx(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeWork(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspVolumeCreate)
#pragma alloc_text(PAGE, FspVolumeDelete)
#pragma alloc_text(PAGE, FspVolumeMount)
#pragma alloc_text(PAGE, FspVolumeGetName)
#pragma alloc_text(PAGE, FspVolumeTransact)
#pragma alloc_text(PAGE, FspVolumeRedirQueryPathEx)
#pragma alloc_text(PAGE, FspVolumeWork)
#endif

#define PREFIXW                         L"" FSP_FSCTL_VOLUME_PARAMS_PREFIX
#define PREFIXW_SIZE                    (sizeof PREFIXW - sizeof(WCHAR))

NTSTATUS FspVolumeCreate(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams = { 0 };
    GUID Guid;
    UNICODE_STRING DeviceSddl;
    UNICODE_STRING VolumeName;
    WCHAR VolumeNameBuf[FSP_DEVICE_VOLUME_NAME_LENMAX / sizeof(WCHAR)];
    PDEVICE_OBJECT FsvolDeviceObject;
    PDEVICE_OBJECT FsvrtDeviceObject;
    HANDLE MupHandle = 0;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension;

    ASSERT(IRP_MJ_CREATE == IrpSp->MajorFunction);
    ASSERT(0 == FileObject->RelatedFileObject);
    ASSERT(PREFIXW_SIZE <= FileObject->FileName.Length &&
        RtlEqualMemory(PREFIXW, FileObject->FileName.Buffer, PREFIXW_SIZE));

    /* check parameters */
    if (PREFIXW_SIZE + sizeof(FSP_FSCTL_VOLUME_PARAMS) * sizeof(WCHAR) > FileObject->FileName.Length)
        return STATUS_INVALID_PARAMETER;

    /* copy the VolumeParams */
    for (USHORT Index = 0, Length = FileObject->FileName.Length / 2; Length > Index; Index++)
    {
        WCHAR Value = FileObject->FileName.Buffer[Index];
        if (0xF000 != (Value & 0xFF00))
            return STATUS_INVALID_PARAMETER;
        ((PUINT8)&VolumeParams)[Index] = Value & 0xFF;
    }

    /* check the VolumeParams */
    if (FspFsctlIrpTimeoutMinimum > VolumeParams.IrpTimeout ||
        VolumeParams.IrpTimeout > FspFsctlIrpTimeoutMaximum)
    {
#if DBG
        /* allow the debug timeout value on debug builds */
        if (FspFsctlIrpTimeoutDebug != VolumeParams.IrpTimeout)
#endif
        VolumeParams.IrpTimeout = FspFsctlIrpTimeoutDefault;
    }
    if (FspFsctlTransactTimeoutMinimum > VolumeParams.TransactTimeout ||
        VolumeParams.TransactTimeout > FspFsctlTransactTimeoutMaximum)
        VolumeParams.TransactTimeout = FspFsctlTransactTimeoutDefault;

    /* create volume guid */
    Result = FspCreateGuid(&Guid);
    if (!NT_SUCCESS(Result))
        return Result;

    /* prepare the device name and SDDL */
    RtlInitUnicodeString(&DeviceSddl, L"" FSP_FSVRT_DEVICE_SDDL);
    RtlInitEmptyUnicodeString(&VolumeName, VolumeNameBuf, sizeof VolumeNameBuf);
    Result = RtlUnicodeStringPrintf(&VolumeName,
        L"\\Device\\Volume{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        Guid.Data1, Guid.Data2, Guid.Data3,
        Guid.Data4[0], Guid.Data4[1], Guid.Data4[2], Guid.Data4[3],
        Guid.Data4[4], Guid.Data4[5], Guid.Data4[6], Guid.Data4[7]);
    ASSERT(NT_SUCCESS(Result));

    /* create the volume (and virtual disk) device(s) */
    Result = FspDeviceCreate(FspFsvolDeviceExtensionKind, 0,
        FsctlDeviceObject->DeviceType,
        &FsvolDeviceObject);
    if (!NT_SUCCESS(Result))
        return Result;
    if (FILE_DEVICE_DISK_FILE_SYSTEM == FsctlDeviceObject->DeviceType)
    {
        Result = FspDeviceCreateSecure(FspFsvrtDeviceExtensionKind, 0,
            &VolumeName, FILE_DEVICE_VIRTUAL_DISK,
            &DeviceSddl, &FspFsvrtDeviceClassGuid,
            &FsvrtDeviceObject);
        if (!NT_SUCCESS(Result))
        {
            FspDeviceRelease(FsvolDeviceObject);
            return Result;
        }
#pragma prefast(suppress:28175, "We are a filesystem: ok to access SectorSize")
        FsvrtDeviceObject->SectorSize = VolumeParams.SectorSize;
    }
    else
        FsvrtDeviceObject = 0;
#pragma prefast(suppress:28175, "We are a filesystem: ok to access SectorSize")
    FsvolDeviceObject->SectorSize = VolumeParams.SectorSize;
    FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    FsvolDeviceExtension->FsctlDeviceObject = FsctlDeviceObject;
    FsvolDeviceExtension->FsvrtDeviceObject = FsvrtDeviceObject;
    FsvolDeviceExtension->VolumeParams = VolumeParams;
    RtlCopyUnicodeString(&FsvolDeviceExtension->VolumeName, &VolumeName);
    FspDeviceInitComplete(FsvolDeviceObject);
    if (0 != FsvrtDeviceObject)
        FspDeviceInitComplete(FsvrtDeviceObject);

    /* do we need to register with MUP? */
    if (0 == FsvrtDeviceObject)
    {
        Result = FsRtlRegisterUncProviderEx(&MupHandle, &VolumeName, FsvolDeviceObject, 0);
        if (!NT_SUCCESS(Result))
        {
            FspDeviceRelease(FsvolDeviceObject);
            return Result;
        }
        FsvolDeviceExtension->MupHandle = MupHandle;
    }

    /* associate the new volume device with our file object */
    FileObject->FsContext2 = FsvolDeviceObject;

    Irp->IoStatus.Information = FILE_OPENED;
    return STATUS_SUCCESS;
}

VOID FspVolumeDelete(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();
}

NTSTATUS FspVolumeMount(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_MOUNT_VOLUME == IrpSp->MinorFunction);

    NTSTATUS Result;
    PVPB Vpb = IrpSp->Parameters.MountVolume.Vpb;
    PDEVICE_OBJECT FsvrtDeviceObject = IrpSp->Parameters.MountVolume.DeviceObject;
    PDEVICE_OBJECT FsvolDeviceObject = 0;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = 0;
    PDEVICE_OBJECT *DeviceObjects = 0;
    ULONG DeviceObjectCount = 0;
    KIRQL Irql;

    /* check the passed in device object; it must be our own and not marked delete pending */
    Result = FspDeviceCopyList(&DeviceObjects, &DeviceObjectCount);
    if (NT_SUCCESS(Result))
    {
        Result = STATUS_UNRECOGNIZED_VOLUME;
        for (ULONG i = 0; DeviceObjectCount > i; i++)
            if (FspDeviceRetain(DeviceObjects[i]))
            {
                if (FspFsvolDeviceExtensionKind == FspDeviceExtension(DeviceObjects[i])->Kind)
                {
                    FsvolDeviceObject = DeviceObjects[i];
                    FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
                    if (FsvolDeviceExtension->FsvrtDeviceObject == FsvrtDeviceObject &&
                        !FsvolDeviceExtension->DeletePending)
                    {
                        Result = STATUS_SUCCESS;
                        /* break out of the loop without FspDeviceRelease on the volume device! */
                        break;
                    }
                }
                FspDeviceRelease(DeviceObjects[i]);
            }
        FspDeviceDeleteList(DeviceObjects, DeviceObjectCount);
    }
    if (!NT_SUCCESS(Result))
        return Result;

    /*
     * At this point the volume device object we are going to use in the VPB has been FspDeviceRetain'ed.
     * We also increment the VPB's ReferenceCount so that we can do a delayed delete of the volume device
     * later on.
     */
    ASSERT(0 != FsvolDeviceObject && 0 != FsvolDeviceExtension);
    IoAcquireVpbSpinLock(&Irql);
    Vpb->ReferenceCount++;
    Vpb->DeviceObject = FsvolDeviceObject;
    Vpb->SerialNumber = FsvolDeviceExtension->VolumeParams.SerialNumber;
    IoReleaseVpbSpinLock(Irql);

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
}

NTSTATUS FspVolumeGetName(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction);
    ASSERT(FSP_FSCTL_VOLUME_NAME == IrpSp->Parameters.FileSystemControl.FsControlCode);

    /* check parameters */
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PVOID SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    if (FSP_FSCTL_VOLUME_NAME_SIZEMAX > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->FileObject->FsContext2;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    UNICODE_STRING VolumeName;

    ASSERT(FSP_FSCTL_VOLUME_NAME_SIZEMAX >=
        FsvolDeviceExtension->VolumeName.MaximumLength + sizeof(WCHAR));

    RtlInitEmptyUnicodeString(&VolumeName, SystemBuffer, FSP_FSCTL_VOLUME_NAME_SIZEMAX);
    RtlCopyUnicodeString(&VolumeName, &FsvolDeviceExtension->VolumeName);
    VolumeName.Buffer[VolumeName.Length] = L'\0';

    Irp->IoStatus.Information = VolumeName.Length + sizeof(WCHAR);
    return STATUS_SUCCESS;
}

NTSTATUS FspVolumeTransact(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS FspVolumeRedirQueryPathEx(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS FspVolumeWork(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

#if 0
VOID FspFsctlDeleteVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    /* performed during IRP_MJ_CLEANUP! */
    PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension;

    FsvolDeviceObject = FspFsvolDeviceObjectFromFileObject(IrpSp->FileObject);
    if (0 == FsvolDeviceObject)
        return;
    FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);

    /* mark the volume device as pending delete */
    FsvolDeviceExtension->DeletePending = TRUE;

    /* stop the I/O queue */
    FspIoqStop(&FsvolDeviceExtension->Ioq);

    /* do we have a virtual disk device or a MUP handle? */
    if (0 != FsvolDeviceExtension->FsvrtDeviceObject)
    {
        PDEVICE_OBJECT FsvrtDeviceObject = FsvolDeviceExtension->FsvrtDeviceObject;
        PVPB OldVpb;
        KIRQL Irql;
        BOOLEAN DeleteVpb = FALSE;
        BOOLEAN DeleteDly = FALSE;
        LARGE_INTEGER DelayTimeout;

        /* swap the virtual disk device VPB with the preallocated one */
#pragma prefast(push)
#pragma prefast(disable:28175, "We are a filesystem: ok to access Vpb")
        IoAcquireVpbSpinLock(&Irql);
        OldVpb = FsvrtDeviceObject->Vpb;
        if (0 != OldVpb)
        {
            FsvrtDeviceObject->Vpb = FsvolDeviceExtension->SwapVpb;
            FsvrtDeviceObject->Vpb->Size = sizeof *FsvrtDeviceObject->Vpb;
            FsvrtDeviceObject->Vpb->Type = IO_TYPE_VPB;
            FsvrtDeviceObject->Vpb->Flags = FlagOn(OldVpb->Flags, VPB_REMOVE_PENDING);
            FsvrtDeviceObject->Vpb->RealDevice = OldVpb->RealDevice;
            FsvrtDeviceObject->Vpb->RealDevice->Vpb = FsvrtDeviceObject->Vpb;
            DeleteVpb = 0 == OldVpb->ReferenceCount;
            DeleteDly = 2 <= OldVpb->ReferenceCount;
        }
        IoReleaseVpbSpinLock(Irql);
        if (DeleteVpb)
        {
            /* no more references to the old VPB; delete now! */
            FspFreeExternal(OldVpb);
            FsvolDeviceExtension->SwapVpb = 0;
        }
        else if (!DeleteDly)
        {
            /* there is only the reference from IRP_MN_MOUNT_VOLUME */
            FspFreeExternal(OldVpb);
            FsvolDeviceExtension->SwapVpb = 0;
            FspDeviceRelease(FsvolDeviceObject);
        }
        else
            /* keep VPB around for delayed delete */
            FsvolDeviceExtension->SwapVpb = OldVpb;
#pragma prefast(pop)

        /* release the virtual disk and volume device objects */
        FspDeviceRelease(FsvrtDeviceObject);
        FspDeviceRelease(FsvolDeviceObject);

        /* are we doing delayed delete of VPB and volume device object? */
        if (DeleteDly)
        {
            DelayTimeout.QuadPart = 300/*ms*/ * -10000;
            FspInitializeWorkItemWithDelay(&FsvolDeviceExtension->DeleteVolumeWorkItem,
                FspFsctlDeleteVolumeDelayed, FsvolDeviceObject);
            FspQueueWorkItemWithDelay(&FsvolDeviceExtension->DeleteVolumeWorkItem, DelayTimeout);
        }
    }
    else if (0 != FsvolDeviceExtension->MupHandle)
    {
        HANDLE MupHandle = FsvolDeviceExtension->MupHandle;

        FsRtlDeregisterUncProvider(MupHandle);
        FsvolDeviceExtension->MupHandle = 0;

        /* release the volume device object */
        FspDeviceRelease(FsvolDeviceObject);
    }
}

static VOID FspFsctlDeleteVolumeDelayed(PVOID Context)
{
    PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject = Context;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    KIRQL Irql;
    BOOLEAN DeleteVpb = FALSE;
    LARGE_INTEGER DelayTimeout;

    IoAcquireVpbSpinLock(&Irql);
    ASSERT(0 != FsvolDeviceExtension->SwapVpb->ReferenceCount);
    DeleteVpb = 1 == FsvolDeviceExtension->SwapVpb->ReferenceCount;
    if (DeleteVpb)
        FsvolDeviceExtension->SwapVpb->ReferenceCount = 0;
    IoReleaseVpbSpinLock(Irql);
    if (DeleteVpb)
    {
        FspFreeExternal(FsvolDeviceExtension->SwapVpb);
        FsvolDeviceExtension->SwapVpb = 0;
        FspDeviceRelease(FsvolDeviceObject);
    }
    else
    {
        DelayTimeout.QuadPart = 300/*ms*/ * -10000;
        FspQueueWorkItemWithDelay(&FsvolDeviceExtension->DeleteVolumeWorkItem, DelayTimeout);
    }
}

static NTSTATUS FspFsctlTransact(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* check parameters */
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PVOID SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    if (0 != InputBufferLength &&
        FSP_FSCTL_DEFAULT_ALIGN_UP(sizeof(FSP_FSCTL_TRANSACT_RSP)) > InputBufferLength)
        return STATUS_INVALID_PARAMETER;
    if (0 != OutputBufferLength &&
        FSP_FSCTL_TRANSACT_REQ_BUFFER_SIZEMIN > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS Result;
    PDEVICE_OBJECT FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension;
    PVOID MdlBuffer;
    PUINT8 BufferEnd;
    FSP_FSCTL_TRANSACT_RSP *Response, *NextResponse;
    FSP_FSCTL_TRANSACT_REQ *Request, *PendingIrpRequest;
    PIRP ProcessIrp, PendingIrp;
    LARGE_INTEGER Timeout;

    FsvolDeviceObject = FspFsvolDeviceObjectFromFileObject(IrpSp->FileObject);
    if (0 == FsvolDeviceObject)
        return STATUS_ACCESS_DENIED;
    FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);

    /* process any user-mode file system responses */
    Response = SystemBuffer;
    BufferEnd = (PUINT8)SystemBuffer + InputBufferLength;
    for (;;)
    {
        NextResponse = FspFsctlTransactConsumeResponse(Response, BufferEnd);
        if (0 == NextResponse)
            break;

        ProcessIrp = FspIoqEndProcessingIrp(&FsvolDeviceExtension->Ioq, (UINT_PTR)Response->Hint);
        if (0 == ProcessIrp)
            /* either IRP was canceled or a bogus Hint was provided */
            continue;

        FspIopDispatchComplete(ProcessIrp, Response);

        Response = NextResponse;
    }

    /* were we sent an output buffer? */
    if (0 == Irp->MdlAddress)
    {
        Irp->IoStatus.Information = 0;
        return STATUS_SUCCESS;
    }
    MdlBuffer = MmGetMdlVirtualAddress(Irp->MdlAddress);
    ASSERT(0 != MdlBuffer);

    /* wait for an IRP to arrive */
    KeQuerySystemTime(&Timeout);
    Timeout.QuadPart += FsvolDeviceExtension->VolumeParams.TransactTimeout * 10000;
        /* convert millis to nanos and add to absolute time */
    while (0 == (PendingIrp = FspIoqNextPendingIrp(&FsvolDeviceExtension->Ioq, &Timeout)))
    {
        if (FspIoqStopped(&FsvolDeviceExtension->Ioq))
            return STATUS_CANCELLED;
    }
    if (FspIoqTimeout == PendingIrp)
    {
        Irp->IoStatus.Information = 0;
        return STATUS_SUCCESS;
    }

    /* send any pending IRP's to the user-mode file system */
    Request = MdlBuffer;
    BufferEnd = (PUINT8)MdlBuffer + OutputBufferLength;
    ASSERT(FspFsctlTransactCanProduceRequest(Request, BufferEnd));
    for (;;)
    {
        PendingIrpRequest = FspIrpRequest(PendingIrp);

        Result = FspIopDispatchPrepare(PendingIrp, PendingIrpRequest);
        if (!NT_SUCCESS(Result))
            FspIopCompleteIrp(PendingIrp, Result);
        else
        {
            RtlCopyMemory(Request, PendingIrpRequest, PendingIrpRequest->Size);
            Request = FspFsctlTransactProduceRequest(Request, PendingIrpRequest->Size);

            if (!FspIoqStartProcessingIrp(&FsvolDeviceExtension->Ioq, PendingIrp))
            {
                /*
                 * This can only happen if the Ioq was stopped. Abandon everything
                 * and return STATUS_CANCELLED. Any IRP's in the Pending and Process
                 * queues of the Ioq will be cancelled during FspIoqStop(). We must
                 * also cancel the PendingIrp we have in our hands.
                 */
                ASSERT(FspIoqStopped(&FsvolDeviceExtension->Ioq));
                FspIopCompleteIrp(PendingIrp, STATUS_CANCELLED);
                return STATUS_CANCELLED;
            }

            /* check that we have enough space before pulling the next pending IRP off the queue */
            if (!FspFsctlTransactCanProduceRequest(Request, BufferEnd))
                break;
        }

        PendingIrp = FspIoqNextPendingIrp(&FsvolDeviceExtension->Ioq, 0);
        if (0 == PendingIrp)
            break;

    }

    Irp->IoStatus.Information = (PUINT8)Request - (PUINT8)MdlBuffer;
    Result = STATUS_SUCCESS;

    return Result;
}

static NTSTATUS FspFsvolWork(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    if (KernelMode != Irp->RequestorMode)
        return STATUS_ACCESS_DENIED;

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_FSCTL_TRANSACT_REQ *Request = IrpSp->Parameters.FileSystemControl.Type3InputBuffer;

    ASSERT(0 == Request->Hint);

    /* associate the passed Request with our Irp; acquire ownership of the Request */
    Request->Hint = (UINT_PTR)Irp;
    FspIrpRequest(Irp) = Request;

    /*
     * Post the IRP to our Ioq; we do this here instead of at IRP_LEAVE_MJ time,
     * so that we can disassociate the Request on failure and release ownership
     * back to the caller.
     */
    if (!FspIoqPostIrp(&FsvolDeviceExtension->Ioq, Irp))
    {
        /* this can only happen if the Ioq was stopped */
        ASSERT(FspIoqStopped(&FsvolDeviceExtension->Ioq));

        Request->Hint = 0;
        FspIrpRequest(Irp) = 0;

        Result = STATUS_CANCELLED;
    }
    else
        Result = STATUS_PENDING;

    return Result;
}
#endif
