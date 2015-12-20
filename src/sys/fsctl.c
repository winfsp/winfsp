/**
 * @file sys/fsctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsctlFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsctlCreateVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsctlMountVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
VOID FspFsctlDeleteVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static WORKER_THREAD_ROUTINE FspFsctlDeleteVolumeDelayed;
static NTSTATUS FspFsctlTransact(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolFileSystemControlComplete;
FSP_DRIVER_DISPATCH FspFileSystemControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlFileSystemControl)
#pragma alloc_text(PAGE, FspFsctlCreateVolume)
#pragma alloc_text(PAGE, FspFsctlMountVolume)
#pragma alloc_text(PAGE, FspFsctlDeleteVolume)
#pragma alloc_text(PAGE, FspFsctlDeleteVolumeDelayed)
#pragma alloc_text(PAGE, FspFsctlTransact)
#pragma alloc_text(PAGE, FspFsvolFileSystemControl)
#pragma alloc_text(PAGE, FspFsvolFileSystemControlComplete)
#pragma alloc_text(PAGE, FspFileSystemControl)
#endif

static NTSTATUS FspFsctlFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        switch (IrpSp->Parameters.FileSystemControl.FsControlCode)
        {
        case FSP_FSCTL_CREATE:
            Result = FspFsctlCreateVolume(DeviceObject, Irp, IrpSp);
            break;
        case FSP_FSCTL_TRANSACT:
            Result = FspFsctlTransact(DeviceObject, Irp, IrpSp);
            break;
        }
        break;
    case IRP_MN_MOUNT_VOLUME:
        Result = FspFsctlMountVolume(DeviceObject, Irp, IrpSp);
        break;
#if 0
    case IRP_MN_VERIFY_VOLUME:
        break;
#endif
    }
    return Result;
}

static NTSTATUS FspFsctlCreateVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* check parameters */
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PVOID SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    if (0 == SystemBuffer || sizeof(FSP_FSCTL_VOLUME_PARAMS) > InputBufferLength)
        return STATUS_INVALID_PARAMETER;
    if (FSP_FSCTL_CREATE_BUFFER_SIZEMIN > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS Result;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams = *(FSP_FSCTL_VOLUME_PARAMS *)SystemBuffer;
    GUID Guid;
    UNICODE_STRING DeviceSddl;
    UNICODE_STRING DeviceName;
    FSP_FSCTL_FILE_CONTEXT2 *FsContext2;
    HANDLE MupHandle = 0;
    PDEVICE_OBJECT FsvrtDeviceObject = 0;
    PDEVICE_OBJECT FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension;

    /* check the passed in VolumeParams */
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
    RtlInitEmptyUnicodeString(&DeviceName, SystemBuffer, FSP_FSCTL_CREATE_BUFFER_SIZEMIN);
    Result = RtlUnicodeStringPrintf(&DeviceName,
        L"\\Device\\Volume{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        Guid.Data1, Guid.Data2, Guid.Data3,
        Guid.Data4[0], Guid.Data4[1], Guid.Data4[2], Guid.Data4[3],
        Guid.Data4[4], Guid.Data4[5], Guid.Data4[6], Guid.Data4[7]);
    ASSERT(NT_SUCCESS(Result));

    FsContext2 = IrpSp->FileObject->FsContext2;
    ExAcquireFastMutex(&FsContext2->FastMutex);
    try
    {
        /* check to see if we already have a volume */
        if (0 != FsContext2->FsvolDeviceObject)
        {
            Result = STATUS_ACCESS_DENIED;
            goto exit;
        }

        /* create the volume (and virtual disk) device(s) */
        Result = FspDeviceCreate(FspFsvolDeviceExtensionKind, 0,
            DeviceObject->DeviceType,
            &FsvolDeviceObject);
        if (!NT_SUCCESS(Result))
            goto exit;
        if (FILE_DEVICE_DISK_FILE_SYSTEM == DeviceObject->DeviceType)
        {
            Result = FspDeviceCreateSecure(FspFsvrtDeviceExtensionKind, 0,
                &DeviceName, FILE_DEVICE_VIRTUAL_DISK,
                &DeviceSddl, &FspFsvrtDeviceClassGuid,
                &FsvrtDeviceObject);
            if (!NT_SUCCESS(Result))
            {
                FspDeviceRelease(FsvolDeviceObject);
                goto exit;
            }
#pragma prefast(suppress:28175, "We are a filesystem: ok to access SectorSize")
            FsvrtDeviceObject->SectorSize = VolumeParams.SectorSize;
        }
#pragma prefast(suppress:28175, "We are a filesystem: ok to access SectorSize")
        FsvolDeviceObject->SectorSize = VolumeParams.SectorSize;
        FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
        FsvolDeviceExtension->FsctlDeviceObject = DeviceObject;
        FsvolDeviceExtension->FsvrtDeviceObject = FsvrtDeviceObject;
        FsvolDeviceExtension->VolumeParams = VolumeParams;
        if (0 != FsvrtDeviceObject)
            FspDeviceInitComplete(FsvrtDeviceObject);
        FspDeviceInitComplete(FsvolDeviceObject);

        /* do we need to register with MUP? */
        if (0 == FsvrtDeviceObject)
        {
            Result = FsRtlRegisterUncProviderEx(&MupHandle, &DeviceName, FsvolDeviceObject, 0);
            if (!NT_SUCCESS(Result))
            {
                FspDeviceRelease(FsvolDeviceObject);
                goto exit;
            }
            FsvolDeviceExtension->MupHandle = MupHandle;
        }

        /* associate the new volume device with our file object */
        FsContext2->FsvolDeviceObject = FsvolDeviceObject;

        Irp->IoStatus.Information = DeviceName.Length + sizeof(WCHAR);
        Result = STATUS_SUCCESS;

    exit:;
    }
    finally
    {
        ExReleaseFastMutex(&FsContext2->FastMutex);
    }

    return Result;
}

static NTSTATUS FspFsctlMountVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

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
                        break;
                    }
                }
                FspDeviceRelease(DeviceObjects[i]);
            }
        FspDeviceDeleteList(DeviceObjects, DeviceObjectCount);
    }
    if (!NT_SUCCESS(Result))
        return Result;

    /* our volume device object has been FspDeviceRetain'ed */
    ASSERT(0 != FsvolDeviceObject && 0 != FsvolDeviceExtension);
    IoAcquireVpbSpinLock(&Irql);
    Vpb->ReferenceCount++;
    Vpb->DeviceObject = FsvolDeviceObject;
    Vpb->SerialNumber = FsvolDeviceExtension->VolumeParams.SerialNumber;
    IoReleaseVpbSpinLock(Irql);

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
}

VOID FspFsctlDeleteVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    /* performed during IRP_MJ_CLEANUP! */
    PAGED_CODE();

    FSP_FSCTL_FILE_CONTEXT2 *FsContext2 = IrpSp->FileObject->FsContext2;
    PDEVICE_OBJECT FsvolDeviceObject = 0;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension;

    /*
     * Check to see if we have a volume. There is no need to protect this
     * access in FsContext2->FastMutex, because we are called during IRP_MJ_CLEANUP.
     */
    FsvolDeviceObject = FsContext2->FsvolDeviceObject;
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
    if (FSP_FSCTL_TRANSACT_REQ_BUFFER_SIZEMIN > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS Result;
    FSP_FSCTL_FILE_CONTEXT2 *FsContext2;
    PDEVICE_OBJECT FsvolDeviceObject = 0;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension;
    PVOID MdlBuffer;
    PUINT8 BufferEnd;
    FSP_FSCTL_TRANSACT_RSP *Response, *NextResponse;
    FSP_FSCTL_TRANSACT_REQ *Request, *PendingIrpRequest;
    PIRP ProcessIrp, PendingIrp;
    LARGE_INTEGER Timeout;

    FsContext2 = IrpSp->FileObject->FsContext2;
    ExAcquireFastMutex(&FsContext2->FastMutex);
    try
    {
        /* check to see if we already have a volume */
        FsvolDeviceObject = FsContext2->FsvolDeviceObject;
        if (0 != FsvolDeviceObject)
        {
            BOOLEAN Success; (VOID)Success;

            /* this must succeed because our volume device exists until IRP_MJ_CLEANUP */
            Success = FspDeviceRetain(FsvolDeviceObject);
            ASSERT(Success);

            Result = STATUS_SUCCESS;
        }
        else
            Result = STATUS_ACCESS_DENIED;
    }
    finally
    {
        ExReleaseFastMutex(&FsContext2->FastMutex);
    }
    if (!NT_SUCCESS(Result))
        return Result;

    try
    {
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

        /* try to get a pointer to the output buffer */
        MdlBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
        if (0 == MdlBuffer)
        {
            Irp->IoStatus.Information = 0;
            Result = STATUS_SUCCESS;
            goto exit;
        }

        /* wait for an IRP to arrive */
        KeQuerySystemTime(&Timeout);
        Timeout.QuadPart += FsvolDeviceExtension->VolumeParams.TransactTimeout * 10000;
            /* convert millis to nanos and add to absolute time */
        while (0 == (PendingIrp = FspIoqNextPendingIrp(&FsvolDeviceExtension->Ioq, &Timeout)))
        {
            if (FspIoqStopped(&FsvolDeviceExtension->Ioq))
            {
                Result = STATUS_CANCELLED;
                goto exit;
            }
        }
        if (FspIoqTimeout == PendingIrp)
        {
            Irp->IoStatus.Information = 0;
            Result = STATUS_SUCCESS;
            goto exit;
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
                    Result = STATUS_CANCELLED;
                    goto exit;
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

    exit:;
    }
    finally
    {
        FspDeviceRelease(FsvolDeviceObject);
    }

    return Result;
}

static NTSTATUS FspFsvolFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        break;
    }
    return Result;
}

VOID FspFsvolFileSystemControlComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC(
        "FileObject=%p%s%s",
        IrpSp->FileObject,
        IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction ? ", " : "",
        IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction ?
            IoctlCodeSym(IrpSp->Parameters.FileSystemControl.FsControlCode) : "");
}

NTSTATUS FspFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolFileSystemControl(DeviceObject, Irp, IrpSp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlFileSystemControl(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ(
        "FileObject=%p%s%s",
        IrpSp->FileObject,
        IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction ? ", " : "",
        IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction ?
            IoctlCodeSym(IrpSp->Parameters.FileSystemControl.FsControlCode) : "");
}
