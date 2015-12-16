/**
 * @file sys/fsctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

/*
 * Overview
 *
 * The fsctl module provides the IOCTL interface to interact with the
 * user-mode file system. The user-mode file system can use the IOCTL's
 * to create new volumes, delete them (while they are live!) and transact
 * with them.
 *
 *
 * Volume Creation
 *
 * The creation of a new volume is performed using an FSP_FSCTL_CREATE
 * IOCTL code. Creation is simple: a new device \Device\Volume{GUID} is
 * created and its path is returned to the user-mode file system. The
 * user-mode file system also passes a security descriptor to associate
 * with the new virtual volume device so that only the creating user-mode
 * file system can control the new volume.
 *
 *
 * Volume Deletion
 *
 * Deletion of an existing volume is performed using FSP_FSCTL_DELETE and
 * is quite a bit more involved. We must protect against the following two
 * eventualities: (1) that the volume is currently in use and cannot simply
 * go away, and (2) that a simultaneous mount operation is taking place
 * while we are deleting the volume.
 *
 * To protect against the first eventuality we maintain a reference count
 * on all our device extensions. Every time an MJ function is entered,
 * the reference count is incremented (FspDeviceRetain). Every time
 * an IRP is completed, the reference count is decremented (FspDeviceRelease).
 * When the reference count reaches 0 the device is deleted using
 * IoDeleteDevice. This ensures that a device will not go away while an
 * IRP is being pending/processed.
 *
 * To protect against the second eventuality we use the lock (ERESOURCE)
 * on the root Fsctl device to wrap volume deletion and attempts from the
 * system to mount the same volume. We also mark the virtual volume device
 * as Deleted in case we attempt to delete it (FspDeviceRelease) but we
 * cannot because it is currently in use.
 *
 * A sticky point is our use of the Windows VPB. It is not well documented
 * how one should handle this structure during forcible dismount. The fastfat
 * and cdfs samples use a technique where they keep a spare VPB and they swap
 * it with the volume one during forcible dismount. We do something similar.
 * The issue is what to do with the old VPB, because we can delete a volume
 * that is not currently being used. We check the VPB's ReferenceCount and
 * we free the VPB in this case.
 *
 *
 * Volume Transact
 *
 * The user-mode file system's primary interaction with the kernel-mode driver
 * is by using the FSP_FSCTL_TRANSACT IOCTL code. Every virtual volume device
 * maintains an FSP_IOQ (refer to ioq.c for more). When an FSP_FSCTL_TRANSACT
 * arrives it first processes any responses (FSP_FSCTL_TRANSACT_RSP) that the
 * user-mode file system has sent to handle requests sent to it using a prior
 * FSP_FSCTL_TRANSACT. It then proceeds to handle any pending IRP requests by
 * sending the corresponding requests (FSP_FSCTL_TRANSACT_REQ) to the user-
 * mode file system.
 */

static NTSTATUS FspFsctlCreateVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsctlMountVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvrtDeleteVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static WORKER_THREAD_ROUTINE FspFsvrtDeleteVolumeDelayed;
static NTSTATUS FspFsvrtTransact(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsctlFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvrtFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_DRIVER_DISPATCH FspFileSystemControl;
FSP_IOCMPL_DISPATCH FspFileSystemControlComplete;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCreateVolume)
#pragma alloc_text(PAGE, FspFsctlMountVolume)
#pragma alloc_text(PAGE, FspFsvrtDeleteVolume)
#pragma alloc_text(PAGE, FspFsvrtDeleteVolumeDelayed)
#pragma alloc_text(PAGE, FspFsvrtTransact)
#pragma alloc_text(PAGE, FspFsctlFileSystemControl)
#pragma alloc_text(PAGE, FspFsvrtFileSystemControl)
#pragma alloc_text(PAGE, FspFsvolFileSystemControl)
#pragma alloc_text(PAGE, FspFileSystemControl)
#pragma alloc_text(PAGE, FspFileSystemControlComplete)
#endif

static NTSTATUS FspFsctlCreateVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* check parameters */
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PVOID SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    PSECURITY_DESCRIPTOR SecurityDescriptor =
        (PVOID)((PUINT8)SystemBuffer + FSP_FSCTL_VOLUME_PARAMS_SIZE);
    DWORD SecurityDescriptorSize = InputBufferLength - FSP_FSCTL_VOLUME_PARAMS_SIZE;
    if (FSP_FSCTL_VOLUME_PARAMS_SIZE >= InputBufferLength || 0 == SystemBuffer ||
        !FspValidRelativeSecurityDescriptor(SecurityDescriptor, SecurityDescriptorSize,
            DACL_SECURITY_INFORMATION))
        return STATUS_INVALID_PARAMETER;
    if (FSP_FSCTL_CREATE_BUFFER_SIZEMIN > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS Result;
    FSP_FSCTL_VOLUME_PARAMS Params = *(FSP_FSCTL_VOLUME_PARAMS *)SystemBuffer;
    PVOID SecurityDescriptorBuf = 0;
    FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension;

    /* create volume guid */
    GUID Guid;
    Result = FspCreateGuid(&Guid);
    if (!NT_SUCCESS(Result))
        return Result;

    /* copy the security descriptor from the system buffer to a temporary one */
    SecurityDescriptorBuf = FspAlloc(SecurityDescriptorSize);
    if (0 == SecurityDescriptorBuf)
        return STATUS_INSUFFICIENT_RESOURCES;
    RtlCopyMemory(SecurityDescriptorBuf, SecurityDescriptor, SecurityDescriptorSize);

    /* prepare the device name and SDDL */
    PDEVICE_OBJECT FsvrtDeviceObject;
    UNICODE_STRING DeviceSddl;
    UNICODE_STRING DeviceName;
    RtlInitUnicodeString(&DeviceSddl, L"" FSP_FSVRT_DEVICE_SDDL);
    RtlInitEmptyUnicodeString(&DeviceName, SystemBuffer, FSP_FSCTL_CREATE_BUFFER_SIZEMIN);
    Result = RtlUnicodeStringPrintf(&DeviceName,
        L"\\Device\\Volume{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        Guid.Data1, Guid.Data2, Guid.Data3,
        Guid.Data4[0], Guid.Data4[1], Guid.Data4[2], Guid.Data4[3],
        Guid.Data4[4], Guid.Data4[5], Guid.Data4[6], Guid.Data4[7]);
    ASSERT(NT_SUCCESS(Result));

    /* create the virtual volume device */
    FSP_FSCTL_DEVICE_EXTENSION *FsctlDeviceExtension = FspFsctlDeviceExtension(DeviceObject);
    ExAcquireResourceExclusiveLite(&FsctlDeviceExtension->Base.Resource, TRUE);
    try
    {
        Result = FspDeviceCreateSecure(FspFsvrtDeviceExtensionKind, SecurityDescriptorSize,
            &DeviceName, FILE_DEVICE_VIRTUAL_DISK,
            &DeviceSddl, &FspFsvrtDeviceClassGuid,
            &FsvrtDeviceObject);
        if (NT_SUCCESS(Result))
        {
#pragma prefast(suppress:28175, "We are a filesystem: ok to access SectorSize")
            FsvrtDeviceObject->SectorSize = Params.SectorSize;
            FsvrtDeviceExtension = FspFsvrtDeviceExtension(FsvrtDeviceObject);
            FsvrtDeviceExtension->FsctlDeviceObject = DeviceObject;
            FsvrtDeviceExtension->VolumeParams = Params;
            RtlCopyMemory(FsvrtDeviceExtension->SecurityDescriptorBuf,
                SecurityDescriptorBuf, SecurityDescriptorSize);
            ClearFlag(FsvrtDeviceObject->Flags, DO_DEVICE_INITIALIZING);
            Irp->IoStatus.Information = DeviceName.Length + sizeof(WCHAR);
            FspFsctlDeviceVolumeCreated(DeviceObject);
        }
    }
    finally
    {
        ExReleaseResourceLite(&FsctlDeviceExtension->Base.Resource);
    }

    /* free the temporary security descriptor */
    if (0 != SecurityDescriptorBuf)
        FspFree(SecurityDescriptorBuf);

    return Result;
}

static NTSTATUS FspFsctlMountVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSCTL_DEVICE_EXTENSION *FsctlDeviceExtension = FspFsctlDeviceExtension(DeviceObject);

    ExAcquireResourceExclusiveLite(&FsctlDeviceExtension->Base.Resource, TRUE);
    try
    {
        PDEVICE_OBJECT *DeviceObjects = 0;
        ULONG DeviceObjectCount = 0;
        PVPB Vpb = IrpSp->Parameters.MountVolume.Vpb;
        PDEVICE_OBJECT FsvrtDeviceObject = IrpSp->Parameters.MountVolume.DeviceObject;
        PDEVICE_OBJECT FsvolDeviceObject;
        FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension =
            FspFsvrtDeviceExtension(FsvrtDeviceObject);
        FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension;

        /* check the passed in volume object; it must be one of our own and not marked Deleted */
        Result = FspDeviceCopyList(&DeviceObjects, &DeviceObjectCount);
        if (NT_SUCCESS(Result))
        {
            Result = STATUS_UNRECOGNIZED_VOLUME;
            for (ULONG i = 0; DeviceObjectCount > i; i++)
                if (DeviceObjects[i] == FsvrtDeviceObject)
                {
                    if (FspDeviceRetain(FsvrtDeviceObject))
                    {
                        if (!FsvrtDeviceExtension->Deleted &&
                            FILE_DEVICE_VIRTUAL_DISK == FsvrtDeviceObject->DeviceType)
                            Result = STATUS_SUCCESS;
                        else
                            FspDeviceRelease(FsvrtDeviceObject);
                    }
                    break;
                }
            FspDeviceDeleteList(DeviceObjects, DeviceObjectCount);
        }
        if (!NT_SUCCESS(Result))
            goto exit;

        /* create the file system device object */
        Result = FspDeviceCreate(FspFsvolDeviceExtensionKind, 0,
            DeviceObject->DeviceType,
            &FsvolDeviceObject);
        if (NT_SUCCESS(Result))
        {
            /*
             * Reference the virtual volume device so that it will not go away while the
             * file system device object is alive!
             */
            ObReferenceObject(FsvrtDeviceObject);

#pragma prefast(suppress:28175, "We are a filesystem: ok to access SectorSize")
            FsvolDeviceObject->SectorSize = FsvrtDeviceExtension->VolumeParams.SectorSize;
            FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
            FsvolDeviceExtension->FsvrtDeviceObject = FsvrtDeviceObject;
            FsvrtDeviceExtension->FsvolDeviceObject = FsvolDeviceObject;
            ClearFlag(FsvolDeviceObject->Flags, DO_DEVICE_INITIALIZING);
            Vpb->DeviceObject = FsvolDeviceObject;
            Vpb->SerialNumber = FsvrtDeviceExtension->VolumeParams.SerialNumber;
            Irp->IoStatus.Information = 0;
        }

        FspDeviceRelease(FsvrtDeviceObject);

    exit:;
    }
    finally
    {
        ExReleaseResourceLite(&FsctlDeviceExtension->Base.Resource);
    }

    return Result;
}

typedef struct
{
    PDEVICE_OBJECT FsvolDeviceObject;
    PVPB OldVpb;
    FSP_WORK_ITEM_WITH_DELAY WorkItemWithDelay;
} FSP_FSVRT_DELETE_VOLUME_WORK_ITEM;

static NTSTATUS FspFsvrtDeleteVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension = FspFsvrtDeviceExtension(DeviceObject);
    FSP_FSCTL_DEVICE_EXTENSION *FsctlDeviceExtension =
        FspFsctlDeviceExtension(FsvrtDeviceExtension->FsctlDeviceObject);

    ExAcquireResourceExclusiveLite(&FsctlDeviceExtension->Base.Resource, TRUE);
    try
    {
        PDEVICE_OBJECT FsctlDeviceObject = FsvrtDeviceExtension->FsctlDeviceObject;
        PDEVICE_OBJECT FsvolDeviceObject = FsvrtDeviceExtension->FsvolDeviceObject;
        PVPB OldVpb;
        BOOLEAN DeleteVpb = FALSE;
        BOOLEAN DeleteDelayed = FALSE;
        LARGE_INTEGER DelayTimeout;
        FSP_FSVRT_DELETE_VOLUME_WORK_ITEM *WorkItem = 0;
        KIRQL Irql;

        /* access check */
        Result = FspSecuritySubjectContextAccessCheck(
            FsvrtDeviceExtension->SecurityDescriptorBuf, FILE_WRITE_DATA, Irp->RequestorMode);
        if (!NT_SUCCESS(Result))
            goto exit;

        /* pre-allocate a work item in case we need it for delayed delete */
        WorkItem = FspAllocNonPaged(sizeof *WorkItem);
        if (0 == WorkItem)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        /* mark the virtual volume device as deleted */
        FsvrtDeviceExtension->Deleted = TRUE;

        /* stop the I/O queue */
        FspIoqStop(&FsvrtDeviceExtension->Ioq);

        /* swap the preallocated VPB */
#pragma prefast(push)
#pragma prefast(disable:28175, "We are a filesystem: ok to access Vpb")
        IoAcquireVpbSpinLock(&Irql);
        OldVpb = DeviceObject->Vpb;
        if (0 != OldVpb && 0 != FsvrtDeviceExtension->SwapVpb)
        {
            DeviceObject->Vpb = FsvrtDeviceExtension->SwapVpb;
            DeviceObject->Vpb->Size = sizeof *DeviceObject->Vpb;
            DeviceObject->Vpb->Type = IO_TYPE_VPB;
            DeviceObject->Vpb->Flags = FlagOn(OldVpb->Flags, VPB_REMOVE_PENDING);
            DeviceObject->Vpb->RealDevice = OldVpb->RealDevice;
            DeviceObject->Vpb->RealDevice->Vpb = DeviceObject->Vpb;
            FsvrtDeviceExtension->SwapVpb = 0;
            DeleteVpb = 0 == OldVpb->ReferenceCount;
            DeleteDelayed = !DeleteVpb && 0 != FsvolDeviceObject;
            if (DeleteDelayed)
                /* keep VPB around for delayed delete */
                OldVpb->ReferenceCount++;
        }
        IoReleaseVpbSpinLock(Irql);
        if (DeleteDelayed)
            /* keep fsvol around for delayed delete */
            FspDeviceRetain(FsvolDeviceObject);
        else if (DeleteVpb)
            FspFreeExternal(OldVpb);
#pragma prefast(pop)

        /* release the file system device and virtual volume objects */
        FsvrtDeviceExtension->FsvolDeviceObject = 0;
        if (0 != FsvolDeviceObject)
            FspDeviceRelease(FsvolDeviceObject);
        FspDeviceRelease(DeviceObject);

        FspFsctlDeviceVolumeDeleted(FsctlDeviceObject);

        /* are we doing delayed delete of VPB and fsvol? */
        if (DeleteDelayed)
        {
            DelayTimeout.QuadPart = 300/*ms*/ * -10000;
            WorkItem->FsvolDeviceObject = FsvolDeviceObject;
            WorkItem->OldVpb = OldVpb;
            FspInitializeWorkItemWithDelay(&WorkItem->WorkItemWithDelay,
                FspFsvrtDeleteVolumeDelayed, WorkItem);
            FspQueueWorkItemWithDelay(&WorkItem->WorkItemWithDelay, DelayTimeout);
            WorkItem = 0;
        }

        Result = STATUS_SUCCESS;

    exit:
        if (0 != WorkItem)
            FspFree(WorkItem);
    }
    finally
    {
        ExReleaseResourceLite(&FsctlDeviceExtension->Base.Resource);
    }

    return Result;
}

static VOID FspFsvrtDeleteVolumeDelayed(PVOID Context)
{
    PAGED_CODE();

    FSP_FSVRT_DELETE_VOLUME_WORK_ITEM *WorkItem = Context;
    BOOLEAN DeleteVpb = FALSE;
    LARGE_INTEGER DelayTimeout;
    KIRQL Irql;

    IoAcquireVpbSpinLock(&Irql);
    ASSERT(0 != WorkItem->OldVpb->ReferenceCount);
    DeleteVpb = 1 == WorkItem->OldVpb->ReferenceCount;
    if (DeleteVpb)
        WorkItem->OldVpb->ReferenceCount = 0;
    IoReleaseVpbSpinLock(Irql);
    if (DeleteVpb)
    {
        FspFreeExternal(WorkItem->OldVpb);
        FspDeviceRelease(WorkItem->FsvolDeviceObject);
        FspFree(WorkItem);
    }
    else
    {
        DelayTimeout.QuadPart = 300/*ms*/ * -10000;
        FspQueueWorkItemWithDelay(&WorkItem->WorkItemWithDelay, DelayTimeout);
    }
}

static NTSTATUS FspFsvrtTransact(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* check parameters */
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PVOID SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    if (0 == SystemBuffer ||
        (0 != InputBufferLength &&
            FSP_FSCTL_DEFAULT_ALIGN_UP(sizeof(FSP_FSCTL_TRANSACT_RSP)) > InputBufferLength))
        return STATUS_INVALID_PARAMETER;
    if (FSP_FSCTL_TRANSACT_REQ_BUFFER_SIZEMIN > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS Result;
    FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension = FspFsvrtDeviceExtension(DeviceObject);
    PUINT8 SystemBufferEnd;
    FSP_FSCTL_TRANSACT_RSP *Response, *NextResponse;
    FSP_FSCTL_TRANSACT_REQ *Request, *PendingIrpRequest;
    PIRP ProcessIrp, PendingIrp;
    ULONG TransactTimeout;
    LARGE_INTEGER Timeout;

    /* access check */
    Result = FspSecuritySubjectContextAccessCheck(
        FsvrtDeviceExtension->SecurityDescriptorBuf, FILE_WRITE_DATA, Irp->RequestorMode);
    if (!NT_SUCCESS(Result))
        return Result;

    /* process any user-mode file system responses */
    Response = SystemBuffer;
    SystemBufferEnd = (PUINT8)SystemBuffer + InputBufferLength;
    for (;;)
    {
        NextResponse = FspFsctlTransactConsumeResponse(Response, SystemBufferEnd);
        if (0 == NextResponse)
            break;

        ProcessIrp = FspIoqEndProcessingIrp(&FsvrtDeviceExtension->Ioq, (UINT_PTR)Response->Hint);
        if (0 == ProcessIrp)
            /* either IRP was canceled or a bogus Hint was provided */
            continue;

        FspIopDispatchComplete(ProcessIrp, Response);

        Response = NextResponse;
    }

    /* wait for an IRP to arrive */
    TransactTimeout = FsvrtDeviceExtension->VolumeParams.TransactTimeout;
    if (FspFsctlTransactTimeoutMinimum > TransactTimeout || TransactTimeout > FspFsctlTransactTimeoutMaximum)
        TransactTimeout = FspFsctlTransactTimeoutDefault;
    KeQuerySystemTime(&Timeout);
    Timeout.QuadPart += TransactTimeout * 10000; /* convert millis to nanos and add to absolute time */
    while (0 == (PendingIrp = FspIoqNextPendingIrp(&FsvrtDeviceExtension->Ioq, &Timeout)))
    {
        if (FspIoqStopped(&FsvrtDeviceExtension->Ioq))
            return STATUS_CANCELLED;
    }
    if (FspIoqTimeout == PendingIrp)
    {
        Irp->IoStatus.Information = 0;
        return STATUS_SUCCESS;
    }

    /* send any pending IRP's to the user-mode file system */
    Request = SystemBuffer;
    SystemBufferEnd = (PUINT8)SystemBuffer + OutputBufferLength;
    ASSERT(FspFsctlTransactCanProduceRequest(Request, SystemBufferEnd));
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

            if (!FspIoqStartProcessingIrp(&FsvrtDeviceExtension->Ioq, PendingIrp))
            {
                /*
                 * This can only happen if the Ioq was stopped. Abandon everything
                 * and return STATUS_CANCELLED. Any IRP's in the Pending and Process
                 * queues of the Ioq will be cancelled during FspIoqStop(). We must
                 * also cancel the PendingIrp we have in our hands.
                 */
                ASSERT(FspIoqStopped(&FsvrtDeviceExtension->Ioq));
                FspIopCompleteIrp(PendingIrp, STATUS_CANCELLED);
                return STATUS_CANCELLED;
            }

            /* check that we have enough space before pulling the next pending IRP off the queue */
            if (!FspFsctlTransactCanProduceRequest(Request, SystemBufferEnd))
                break;
        }

        PendingIrp = FspIoqNextPendingIrp(&FsvrtDeviceExtension->Ioq, 0);
        if (0 == PendingIrp)
            break;

    }
    Irp->IoStatus.Information = (PUINT8)Request - (PUINT8)SystemBuffer;

    return STATUS_SUCCESS;
}

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

static NTSTATUS FspFsvrtFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        switch (IrpSp->Parameters.FileSystemControl.FsControlCode)
        {
        case FSP_FSCTL_DELETE:
            Result = FspFsvrtDeleteVolume(DeviceObject, Irp, IrpSp);
            break;
        case FSP_FSCTL_TRANSACT:
            Result = FspFsvrtTransact(DeviceObject, Irp, IrpSp);
            break;
        }
        break;
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

NTSTATUS FspFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolFileSystemControl(DeviceObject, Irp, IrpSp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtFileSystemControl(DeviceObject, Irp, IrpSp));
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

VOID FspFileSystemControlComplete(
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
