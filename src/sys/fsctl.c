/**
 * @file sys/fsctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsctlCreateVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsctlMountVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvrtDeleteVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
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
    const FSP_FSCTL_VOLUME_PARAMS *Params = SystemBuffer;
    PSECURITY_DESCRIPTOR SecurityDescriptor = (PVOID)(Params + 1);
    DWORD SecurityDescriptorSize = InputBufferLength - sizeof *Params;
    if (sizeof *Params >= InputBufferLength || 0 == SystemBuffer ||
        !RtlValidRelativeSecurityDescriptor(SecurityDescriptor, SecurityDescriptorSize,
            OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION))
        return STATUS_INVALID_PARAMETER;
    if (FSP_FSCTL_CREATE_BUFFER_SIZE > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS Result;
    PVOID SecurityDescriptorBuf = 0;
    PVPB SwapVpb = 0;

    /* create volume guid */
    GUID Guid;
    Result = FspCreateGuid(&Guid);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* copy the security descriptor from the system buffer to a temporary one */
    SecurityDescriptorBuf = ExAllocatePoolWithTag(PagedPool, SecurityDescriptorSize, FSP_TAG);
    if (0 == SecurityDescriptorBuf)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    RtlCopyMemory(SecurityDescriptorBuf, SecurityDescriptor, SecurityDescriptorSize);

    /* preallocate swap VPB */
    SwapVpb = ExAllocatePoolWithTag(NonPagedPool, sizeof *SwapVpb, FSP_TAG);
    if (0 == SwapVpb)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    RtlZeroMemory(SwapVpb, sizeof *SwapVpb);

    /* create the virtual volume device */
    PDEVICE_OBJECT FsvrtDeviceObject;
    UNICODE_STRING DeviceSddl;
    UNICODE_STRING DeviceName;
    RtlInitUnicodeString(&DeviceSddl, L"" FSP_FSVRT_DEVICE_SDDL);
    RtlInitEmptyUnicodeString(&DeviceName, SystemBuffer, FSP_FSCTL_CREATE_BUFFER_SIZE);
    Result = RtlUnicodeStringPrintf(&DeviceName,
        L"\\Device\\Volume{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        Guid.Data1, Guid.Data2, Guid.Data3,
        Guid.Data4[0], Guid.Data4[1], Guid.Data4[2], Guid.Data4[3],
        Guid.Data4[4], Guid.Data4[5], Guid.Data4[6], Guid.Data4[7]);
    ASSERT(NT_SUCCESS(Result));
    Result = IoCreateDeviceSecure(DeviceObject->DriverObject,
        sizeof(FSP_FSVRT_DEVICE_EXTENSION) + SecurityDescriptorSize, &DeviceName, FILE_DEVICE_VIRTUAL_DISK,
        FILE_DEVICE_SECURE_OPEN, FALSE,
        &DeviceSddl, 0,
        &FsvrtDeviceObject);
    if (NT_SUCCESS(Result))
    {
        FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension = FspFsvrtDeviceExtension(FsvrtDeviceObject);
        FsvrtDeviceExtension->Base.Kind = FspFsvrtDeviceExtensionKind;
        FsvrtDeviceExtension->FsctlDeviceObject = DeviceObject;
        FsvrtDeviceExtension->VolumeParams = *Params;
        FspIoqInitialize(&FsvrtDeviceExtension->Ioq);
        FsvrtDeviceExtension->SwapVpb = SwapVpb;
        RtlCopyMemory(FspFsvrtDeviceExtension(FsvrtDeviceObject)->SecurityDescriptorBuf,
            SecurityDescriptorBuf, SecurityDescriptorSize);
        ClearFlag(FsvrtDeviceObject->Flags, DO_DEVICE_INITIALIZING);
        Irp->IoStatus.Information = DeviceName.Length + 1;
        SwapVpb = 0;
    }

exit:
    /* free the temporary security descriptor */
    if (0 != SecurityDescriptorBuf)
        ExFreePoolWithTag(SecurityDescriptorBuf, FSP_TAG);
    /* free swap VPB if we failed */
    if (0 != SwapVpb)
        ExFreePoolWithTag(SwapVpb, FSP_TAG);

    return Result;
}

static NTSTATUS FspFsctlMountVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSCTL_DEVICE_EXTENSION *FsctlDeviceExtension = FspFsctlDeviceExtension(DeviceObject);
    PVPB Vpb = IrpSp->Parameters.MountVolume.Vpb;
    PDEVICE_OBJECT FsvrtDeviceObject = Vpb->RealDevice;
    PDEVICE_OBJECT FsvolDeviceObject;

    /* check the passed in volume object; it must be one of our own */
    Result = FspDeviceOwned(DeviceObject->DriverObject, FsvrtDeviceObject);
    if (!NT_SUCCESS(Result))
    {
        if (STATUS_NO_SUCH_DEVICE == Result)
            return STATUS_UNRECOGNIZED_VOLUME;
        else
            return Result;
    }
    if (FILE_DEVICE_VIRTUAL_DISK != FsvrtDeviceObject->DeviceType)
        return STATUS_UNRECOGNIZED_VOLUME;

    ExAcquireResourceExclusiveLite(&FsctlDeviceExtension->Resource, TRUE);

    /* create the file system device object */
    Result = IoCreateDevice(DeviceObject->DriverObject,
        sizeof(FSP_FSVOL_DEVICE_EXTENSION), 0, DeviceObject->DeviceType,
        0, FALSE,
        &FsvolDeviceObject);
    if (NT_SUCCESS(Result))
    {
#pragma prefast(suppress:28175, "We are a filesystem: ok to access SectorSize")
        FsvolDeviceObject->SectorSize =
            FspFsvrtDeviceExtension(FsvrtDeviceObject)->VolumeParams.SectorSize;
        FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
        FsvolDeviceExtension->Base.Kind = FspFsvolDeviceExtensionKind;
        FsvolDeviceExtension->FsvrtDeviceObject = FsvrtDeviceObject;
        ClearFlag(FsvolDeviceObject->Flags, DO_DEVICE_INITIALIZING);
        Vpb->DeviceObject = FsvolDeviceObject;
        Irp->IoStatus.Information = 0;
    }

    ExReleaseResourceLite(&FsctlDeviceExtension->Resource);

    return Result;
}

static NTSTATUS FspFsvrtDeleteVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension =
        FspFsvrtDeviceExtension(DeviceObject);
    FSP_FSCTL_DEVICE_EXTENSION *FsctlDeviceExtension =
        FspFsctlDeviceExtension(FsvrtDeviceExtension->FsctlDeviceObject);

    /* access check */
    Result = FspSecuritySubjectContextAccessCheck(
        FsvrtDeviceExtension->SecurityDescriptorBuf, FILE_WRITE_DATA, Irp->RequestorMode);
    if (!NT_SUCCESS(Result))
        return Result;

    ExAcquireResourceExclusiveLite(&FsctlDeviceExtension->Resource, TRUE);

    /* stop the I/O queue */
    FspIoqStop(&FsvrtDeviceExtension->Ioq);

    /* swap the preallocated VPB */
    PVPB OldVpb;
    KIRQL Irql;
    IoAcquireVpbSpinLock(&Irql);
    OldVpb = DeviceObject->Vpb;
    if (0 != OldVpb)
    {
        DeviceObject->Vpb = FsvrtDeviceExtension->SwapVpb;
        DeviceObject->Vpb->Size = sizeof *DeviceObject->Vpb;
        DeviceObject->Vpb->Type = IO_TYPE_VPB;
        DeviceObject->Vpb->Flags = FlagOn(OldVpb->Flags, VPB_REMOVE_PENDING);
        DeviceObject->Vpb->RealDevice = OldVpb->RealDevice;
        DeviceObject->Vpb->RealDevice->Vpb = DeviceObject->Vpb;
        FsvrtDeviceExtension->SwapVpb = 0;
    }
    IoReleaseVpbSpinLock(Irql);

    /* delete the file system device object */
    if (0 != OldVpb && 0 != OldVpb->DeviceObject &&
        FspDeviceOwned(DeviceObject->DriverObject, OldVpb->DeviceObject))
    {
        ASSERT(FspFsvolDeviceExtensionKind == FspDeviceExtension(OldVpb->DeviceObject)->Kind);
        FspDeviceDeleteObject(OldVpb->DeviceObject);
    }

    /* delete the virtual volume device */
    FspDeviceDeleteObject(DeviceObject);

    ExReleaseResourceLite(&FsctlDeviceExtension->Resource);

    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvrtTransact(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* check parameters */
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PVOID SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    if (sizeof(FSP_FSCTL_TRANSACT_RSP) > InputBufferLength || 0 == SystemBuffer)
        return STATUS_INVALID_PARAMETER;
    if (FSP_FSCTL_TRANSACT_BUFFER_SIZE > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS Result;
    FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension = FspFsvrtDeviceExtension(DeviceObject);
    PUINT8 SystemBufferEnd;
    const FSP_FSCTL_TRANSACT_RSP *Response, *NextResponse;
    FSP_FSCTL_TRANSACT_REQ *Request, *NextRequest, *PendingIrpRequest;
    PIRP ProcessIrp, PendingIrp;

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
    while (0 == (PendingIrp = FspIoqNextPendingIrp(&FsvrtDeviceExtension->Ioq, (ULONG)-1L)))
    {
        if (FspIoqStopped(&FsvrtDeviceExtension->Ioq))
            return STATUS_CANCELLED;
    }

    /* send any pending IRP's to the user-mode file system */
    Request = SystemBuffer;
    SystemBufferEnd = (PUINT8)SystemBuffer + OutputBufferLength;
    ASSERT((PUINT8)Request + FSP_FSCTL_TRANSACT_REQ_SIZEMAX <= SystemBufferEnd);
    for (;;)
    {
        PendingIrpRequest = PendingIrp->Tail.Overlay.DriverContext[0];

        NextRequest = FspFsctlTransactProduceRequest(
            Request, PendingIrpRequest->Size, SystemBufferEnd);
            /* this should not fail as we have already checked that we have enough space */
        ASSERT(0 != NextRequest);

        RtlCopyMemory(Request, PendingIrpRequest, PendingIrpRequest->Size);
        Request = NextRequest;

        if (!FspIoqStartProcessingIrp(&FsvrtDeviceExtension->Ioq, PendingIrp))
        {
            /*
             * This can only happen if the Ioq was stopped. Abandon everything
             * and return STATUS_CANCELLED. Any IRP's in the Pending and Process
             * queues of the Ioq will be cancelled during FspIoqStop(). We must
             * also cancel the PendingIrp we have in our hands.
             */
            ASSERT(FspIoqStopped(&FsvrtDeviceExtension->Ioq));
            FspIopCompleteRequest(PendingIrp, STATUS_CANCELLED);
            return STATUS_CANCELLED;
        }

        /* check that we have enough space before pulling the next pending IRP off the queue */
        if ((PUINT8)Request + FSP_FSCTL_TRANSACT_REQ_SIZEMAX > SystemBufferEnd)
            break;

        PendingIrp = FspIoqNextPendingIrp(&FsvrtDeviceExtension->Ioq, 0);
        if (0 == PendingIrp)
            break;

    }
    RtlZeroMemory(Request, SystemBufferEnd - (PUINT8)Request);
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
        FSP_RETURN(Result = FspFsctlFileSystemControl(DeviceObject, Irp, IrpSp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtFileSystemControl(DeviceObject, Irp, IrpSp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolFileSystemControl(DeviceObject, Irp, IrpSp));
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
