/**
 * @file sys/volume.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspVolumeCreate(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static WORKER_THREAD_ROUTINE FspVolumeCreateRegisterMup;
VOID FspVolumeDelete(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static WORKER_THREAD_ROUTINE FspVolumeDeleteDelayed;
NTSTATUS FspVolumeMount(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeRedirQueryPathEx(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeGetName(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeTransact(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeWork(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspVolumeCreate)
#pragma alloc_text(PAGE, FspVolumeCreateRegisterMup)
// ! #pragma alloc_text(PAGE, FspVolumeDelete)
// ! #pragma alloc_text(PAGE, FspVolumeDeleteDelayed)
// ! #pragma alloc_text(PAGE, FspVolumeMount)
#pragma alloc_text(PAGE, FspVolumeGetName)
#pragma alloc_text(PAGE, FspVolumeTransact)
#pragma alloc_text(PAGE, FspVolumeRedirQueryPathEx)
#pragma alloc_text(PAGE, FspVolumeWork)
#endif

#define PREFIXW                         L"" FSP_FSCTL_VOLUME_PARAMS_PREFIX
#define PREFIXW_SIZE                    (sizeof PREFIXW - sizeof(WCHAR))

typedef struct
{
    PDEVICE_OBJECT FsvolDeviceObject;
    NTSTATUS Result;
    FSP_SYNCHRONOUS_WORK_ITEM SynchronousWorkItem;
} FSP_CREATE_VOLUME_REGISTER_MUP_WORK_ITEM;

NTSTATUS FspVolumeCreate(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_CREATE == IrpSp->MajorFunction);
    ASSERT(0 == IrpSp->FileObject->RelatedFileObject);
    ASSERT(PREFIXW_SIZE <= IrpSp->FileObject->FileName.Length &&
        RtlEqualMemory(PREFIXW, IrpSp->FileObject->FileName.Buffer, PREFIXW_SIZE));
    ASSERT(
        FILE_DEVICE_DISK_FILE_SYSTEM == FsctlDeviceObject->DeviceType ||
        FILE_DEVICE_NETWORK_FILE_SYSTEM == FsctlDeviceObject->DeviceType);

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams = { 0 };
    USHORT PrefixLength = 0;
    GUID Guid;
    UNICODE_STRING DeviceSddl;
    UNICODE_STRING VolumeName;
    WCHAR VolumeNameBuf[FSP_DEVICE_VOLUME_NAME_LENMAX / sizeof(WCHAR)];
    PDEVICE_OBJECT FsvolDeviceObject;
    PDEVICE_OBJECT FsvrtDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension;
    FSP_CREATE_VOLUME_REGISTER_MUP_WORK_ITEM RegisterMupWorkItem;

    /* check parameters */
    if (PREFIXW_SIZE + sizeof(FSP_FSCTL_VOLUME_PARAMS) * sizeof(WCHAR) > FileObject->FileName.Length)
        return STATUS_INVALID_PARAMETER;

    /* copy the VolumeParams */
    for (USHORT Index = 0, Length = sizeof(FSP_FSCTL_VOLUME_PARAMS); Length > Index; Index++)
    {
        WCHAR Value = FileObject->FileName.Buffer[PREFIXW_SIZE / sizeof(WCHAR) + Index];
        if (0xF000 != (Value & 0xFF00))
            return STATUS_INVALID_PARAMETER;
        ((PUINT8)&VolumeParams)[Index] = Value & 0xFF;
    }

    /* check the VolumeParams */
    if (0 == VolumeParams.SectorSize)
        VolumeParams.SectorSize = 512;
    if (0 == VolumeParams.SectorsPerAllocationUnit)
        VolumeParams.SectorsPerAllocationUnit = 1;
    if (0 == VolumeParams.MaxComponentLength)
        VolumeParams.MaxComponentLength = 255;
    if (FspFsctlTransactTimeoutMinimum > VolumeParams.TransactTimeout ||
        VolumeParams.TransactTimeout > FspFsctlTransactTimeoutMaximum)
        VolumeParams.TransactTimeout = FspFsctlTransactTimeoutDefault;
    if (FspFsctlIrpTimeoutMinimum > VolumeParams.IrpTimeout ||
        VolumeParams.IrpTimeout > FspFsctlIrpTimeoutMaximum)
    {
        /* special: allow the debug timeout value on all builds */
        if (FspFsctlIrpTimeoutDebug != VolumeParams.IrpTimeout)
            VolumeParams.IrpTimeout = FspFsctlIrpTimeoutDefault;
    }
    if (FspFsctlIrpCapacityMinimum > VolumeParams.IrpCapacity ||
        VolumeParams.IrpCapacity > FspFsctlIrpCapacityMaximum)
        VolumeParams.IrpCapacity = FspFsctlIrpCapacityDefault;
    if (FILE_DEVICE_NETWORK_FILE_SYSTEM == FsctlDeviceObject->DeviceType)
    {
        VolumeParams.Prefix[sizeof VolumeParams.Prefix / sizeof(WCHAR) - 1] = L'\0';
        for (; L'\0' != VolumeParams.Prefix[PrefixLength]; PrefixLength++)
            ;
        for (; 0 < PrefixLength && L'\\' == VolumeParams.Prefix[PrefixLength - 1]; PrefixLength--)
            ;
        VolumeParams.Prefix[PrefixLength] = L'\0';
        if (0 == PrefixLength)
            return STATUS_INVALID_PARAMETER;
    }

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
            FspDeviceDereference(FsvolDeviceObject);
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
    if (FILE_DEVICE_NETWORK_FILE_SYSTEM == FsctlDeviceObject->DeviceType)
        RtlInitUnicodeString(&FsvolDeviceExtension->VolumePrefix,
            FsvolDeviceExtension->VolumeParams.Prefix);
    RtlInitEmptyUnicodeString(&FsvolDeviceExtension->VolumeName,
        FsvolDeviceExtension->VolumeNameBuf, sizeof FsvolDeviceExtension->VolumeNameBuf);
    RtlCopyUnicodeString(&FsvolDeviceExtension->VolumeName, &VolumeName);
    Result = FspDeviceInitialize(FsvolDeviceObject);
    if (NT_SUCCESS(Result))
    {
        if (0 != FsvrtDeviceObject)
            Result = FspDeviceInitialize(FsvrtDeviceObject);
    }
    if (!NT_SUCCESS(Result))
    {
        if (0 != FsvrtDeviceObject)
            FspDeviceDereference(FsvrtDeviceObject);
        FspDeviceDereference(FsvolDeviceObject);
    }

    /* do we need to register with MUP? */
    if (0 == FsvrtDeviceObject)
    {
        /*
         * Turns out we cannot call FsRtlRegisterUncProviderEx when the PreviousMode
         * is UserMode! So we need to somehow switch to KernelMode prior to issuing
         * the FsRtlRegisterUncProviderEx call. There seems to be no straightforward
         * way to switch the PreviousMode (no ExSetPreviousMode). So we do it indirectly
         * by executing a synchronous work item (FspExecuteSynchronousWorkItem).
         */
        RtlZeroMemory(&RegisterMupWorkItem, sizeof RegisterMupWorkItem);
        RegisterMupWorkItem.FsvolDeviceObject = FsvolDeviceObject;
        FspInitializeSynchronousWorkItem(&RegisterMupWorkItem.SynchronousWorkItem,
            FspVolumeCreateRegisterMup, &RegisterMupWorkItem);
        FspExecuteSynchronousWorkItem(&RegisterMupWorkItem.SynchronousWorkItem);
        Result = RegisterMupWorkItem.Result;
        if (!NT_SUCCESS(Result))
        {
            FspDeviceDereference(FsvolDeviceObject);
            return Result;
        }
    }

    /* associate the new volume device with our file object */
    FileObject->FsContext2 = FsvolDeviceObject;

    Irp->IoStatus.Information = FILE_OPENED;
    return STATUS_SUCCESS;
}

static VOID FspVolumeCreateRegisterMup(PVOID Context)
{
    PAGED_CODE();

    FSP_CREATE_VOLUME_REGISTER_MUP_WORK_ITEM *RegisterMupWorkItem = Context;
    PDEVICE_OBJECT FsvolDeviceObject = RegisterMupWorkItem->FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);

    RegisterMupWorkItem->Result = FsRtlRegisterUncProviderEx(&FsvolDeviceExtension->MupHandle,
        &FsvolDeviceExtension->VolumeName, FsvolDeviceObject, 0);
}

VOID FspVolumeDelete(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    // !PAGED_CODE();

    ASSERT(IRP_MJ_CLEANUP == IrpSp->MajorFunction);
    ASSERT(0 != IrpSp->FileObject->FsContext2);

    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->FileObject->FsContext2;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    IrpSp->FileObject->FsContext2 = 0;

    /* acquire the volume device DeleteResource */
    ExAcquireResourceExclusiveLite(&FsvolDeviceExtension->DeleteResource, TRUE);

    /* stop the I/O queue */
    FspIoqStop(FsvolDeviceExtension->Ioq);

    /* do we have a virtual disk device or a MUP handle? */
    if (0 != FsvolDeviceExtension->FsvrtDeviceObject)
    {
        PDEVICE_OBJECT FsvrtDeviceObject = FsvolDeviceExtension->FsvrtDeviceObject;
        PVPB OldVpb;
        KIRQL Irql;
        BOOLEAN DeleteVpb = FALSE;
        BOOLEAN DeleteDly = FALSE;
        LARGE_INTEGER Delay;

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
#pragma prefast(pop)

        /*
         * Release the virtual disk object. This is safe to do here because the volume device
         * keeps an extra reference to the virtual disk object using ObReferenceObject.
         */
        FspDeviceDereference(FsvrtDeviceObject);

        if (DeleteVpb)
        {
            /* no more references to the old VPB; delete now! */
            FspFreeExternal(OldVpb);
            FsvolDeviceExtension->SwapVpb = 0;
        }
        else if (!DeleteDly)
        {
            /* there is only the reference from FspVolumeMount; release it! */
            FspFreeExternal(OldVpb);
            FsvolDeviceExtension->SwapVpb = 0;
            FspDeviceDereference(FsvolDeviceObject);
        }
        else
        {
            /* VPB has extra references; we must do a delayed delete of the volume device */
            FsvolDeviceExtension->SwapVpb = OldVpb;
            Delay.QuadPart = 300/*ms*/ * -10000;
            FspInitializeDelayedWorkItem(&FsvolDeviceExtension->DeleteVolumeDelayedWorkItem,
                FspVolumeDeleteDelayed, FsvolDeviceObject);
            FspQueueDelayedWorkItem(&FsvolDeviceExtension->DeleteVolumeDelayedWorkItem, Delay);
        }
    }
    else if (0 != FsvolDeviceExtension->MupHandle)
    {
        FsRtlDeregisterUncProvider(FsvolDeviceExtension->MupHandle);
        FsvolDeviceExtension->MupHandle = 0;
    }

    /* release the volume device DeleteResource */
    ExReleaseResourceLite(&FsvolDeviceExtension->DeleteResource);

    /* release the volume device object */
    FspDeviceDereference(FsvolDeviceObject);
}

static VOID FspVolumeDeleteDelayed(PVOID Context)
{
    // !PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject = Context;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    KIRQL Irql;
    BOOLEAN DeleteVpb = FALSE;
    LARGE_INTEGER Delay;

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
        FspDeviceDereference(FsvolDeviceObject);
    }
    else
    {
        Delay.QuadPart = 300/*ms*/ * -10000;
        FspQueueDelayedWorkItem(&FsvolDeviceExtension->DeleteVolumeDelayedWorkItem, Delay);
    }
}

NTSTATUS FspVolumeMount(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    // !PAGED_CODE();

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

    /* check the passed in device object; it must be our own and not stopped */
    Result = FspDeviceCopyList(&DeviceObjects, &DeviceObjectCount);
    if (NT_SUCCESS(Result))
    {
        Result = STATUS_UNRECOGNIZED_VOLUME;
        for (ULONG i = 0; DeviceObjectCount > i; i++)
            if (FspDeviceReference(DeviceObjects[i]))
            {
                if (FspFsvolDeviceExtensionKind == FspDeviceExtension(DeviceObjects[i])->Kind)
                {
                    FsvolDeviceObject = DeviceObjects[i];
                    FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
                    if (FsvolDeviceExtension->FsvrtDeviceObject == FsvrtDeviceObject)
                    {
                        ExAcquireResourceExclusiveLite(&FsvolDeviceExtension->DeleteResource, TRUE);
                        if (!FspIoqStopped(FsvolDeviceExtension->Ioq))
                        {
                            Result = STATUS_SUCCESS;
                            /* break out of the loop without FspDeviceDereference or DeleteResource release! */
                            break;
                        }
                        ExReleaseResourceLite(&FsvolDeviceExtension->DeleteResource);
                    }
                }
                FspDeviceDereference(DeviceObjects[i]);
            }
        FspDeviceDeleteList(DeviceObjects, DeviceObjectCount);
    }
    if (!NT_SUCCESS(Result))
        return Result;

    /*
     * At this point the volume device object we are going to use in the VPB
     * has been FspDeviceReference'ed and the volume DeleteResource has been acquired.
     * We will increment the VPB's ReferenceCount so that we can do a delayed delete
     * of the volume device later on. Once done with the VPB we can release the
     * DeleteResource. [The volume device remains FspDeviceReference'ed!]
     */
    ASSERT(0 != FsvolDeviceObject && 0 != FsvolDeviceExtension);
    IoAcquireVpbSpinLock(&Irql);
    Vpb->ReferenceCount++;
    Vpb->DeviceObject = FsvolDeviceObject;
    Vpb->SerialNumber = FsvolDeviceExtension->VolumeParams.VolumeSerialNumber;
    IoReleaseVpbSpinLock(Irql);

    /* done with mounting; release the DeleteResource */
    ExReleaseResourceLite(&FsvolDeviceExtension->DeleteResource);

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
}

NTSTATUS FspVolumeRedirQueryPathEx(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_DEVICE_CONTROL == IrpSp->MajorFunction);
    ASSERT(IOCTL_REDIR_QUERY_PATH_EX == IrpSp->Parameters.DeviceIoControl.IoControlCode);

    if (KernelMode != Irp->RequestorMode)
        return STATUS_INVALID_DEVICE_REQUEST;

    /* check parameters */
    ULONG InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
    QUERY_PATH_REQUEST_EX *QueryPathRequest = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;
    QUERY_PATH_RESPONSE *QueryPathResponse = Irp->UserBuffer;
    if (sizeof(QUERY_PATH_REQUEST_EX) > InputBufferLength ||
        0 == QueryPathRequest || 0 == QueryPathResponse)
        return STATUS_INVALID_PARAMETER;
    if (sizeof(QUERY_PATH_RESPONSE) > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);

    /* acquire our DeleteResource */
    ExAcquireResourceExclusiveLite(&FsvolDeviceExtension->DeleteResource, TRUE);

    Result = STATUS_BAD_NETWORK_PATH;
    if (!FspIoqStopped(FsvolDeviceExtension->Ioq))
    {
        if (0 < FsvolDeviceExtension->VolumePrefix.Length &&
            QueryPathRequest->PathName.Length >= FsvolDeviceExtension->VolumePrefix.Length &&
            RtlEqualMemory(QueryPathRequest->PathName.Buffer,
                FsvolDeviceExtension->VolumePrefix.Buffer, FsvolDeviceExtension->VolumePrefix.Length) &&
            (QueryPathRequest->PathName.Length == FsvolDeviceExtension->VolumePrefix.Length ||
                '\\' == QueryPathRequest->PathName.Buffer[FsvolDeviceExtension->VolumePrefix.Length / sizeof(WCHAR)]))
        {
            QueryPathResponse->LengthAccepted = FsvolDeviceExtension->VolumePrefix.Length;

            Irp->IoStatus.Information = 0;
            Result = STATUS_SUCCESS;
        }
    }

    /* release the DeleteResource */
    ExReleaseResourceLite(&FsvolDeviceExtension->DeleteResource);

    return Result;
}

NTSTATUS FspVolumeGetName(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction);
    ASSERT(FSP_FSCTL_VOLUME_NAME == IrpSp->Parameters.FileSystemControl.FsControlCode);
    ASSERT(0 != IrpSp->FileObject->FsContext2);

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
    VolumeName.Buffer[VolumeName.Length / sizeof(WCHAR)] = L'\0';

    Irp->IoStatus.Information = VolumeName.Length + sizeof(WCHAR);
    return STATUS_SUCCESS;
}

NTSTATUS FspVolumeTransact(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction);
    ASSERT(
        FSP_FSCTL_TRANSACT == IrpSp->Parameters.FileSystemControl.FsControlCode ||
        FSP_FSCTL_TRANSACT_BATCH == IrpSp->Parameters.FileSystemControl.FsControlCode);
    ASSERT(
        METHOD_BUFFERED == (IrpSp->Parameters.FileSystemControl.FsControlCode & 3) ||
        METHOD_OUT_DIRECT == (IrpSp->Parameters.FileSystemControl.FsControlCode & 3));
    ASSERT(0 != IrpSp->FileObject->FsContext2);

    /* check parameters */
    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->FileObject->FsContext2;
    ULONG ControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PVOID InputBuffer = Irp->AssociatedIrp.SystemBuffer;
    PVOID OutputBuffer = 0;
    if (0 != InputBufferLength &&
        FSP_FSCTL_DEFAULT_ALIGN_UP(sizeof(FSP_FSCTL_TRANSACT_RSP)) > InputBufferLength)
        return STATUS_INVALID_PARAMETER;
    if (0 != OutputBufferLength &&
        ((FSP_FSCTL_TRANSACT == ControlCode &&
            FSP_FSCTL_TRANSACT_BUFFER_SIZEMIN > OutputBufferLength) ||
        (FSP_FSCTL_TRANSACT_BATCH == ControlCode &&
            FSP_FSCTL_TRANSACT_BATCH_BUFFER_SIZEMIN > OutputBufferLength)))
        return STATUS_BUFFER_TOO_SMALL;

    if (!FspDeviceReference(FsvolDeviceObject))
        return STATUS_CANCELLED;

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PUINT8 BufferEnd;
    FSP_FSCTL_TRANSACT_RSP *Response, *NextResponse;
    FSP_FSCTL_TRANSACT_REQ *Request, *PendingIrpRequest;
    PIRP ProcessIrp, PendingIrp, RetriedIrp, RepostedIrp;
    LARGE_INTEGER Timeout;

    /* process any user-mode file system responses */
    RepostedIrp = 0;
    Response = InputBuffer;
    BufferEnd = (PUINT8)InputBuffer + InputBufferLength;
    for (;;)
    {
        NextResponse = FspFsctlTransactConsumeResponse(Response, BufferEnd);
        if (0 == NextResponse)
            break;

        ProcessIrp = FspIoqEndProcessingIrp(FsvolDeviceExtension->Ioq, (UINT_PTR)Response->Hint);
        if (0 == ProcessIrp)
        {
            /* either IRP was canceled or a bogus Hint was provided */
            DEBUGLOG("BOGUS(Kind=%d, Hint=%p)", Response->Kind, (PVOID)Response->Hint);
            Response = NextResponse;
            continue;
        }

        ASSERT((UINT_PTR)ProcessIrp == (UINT_PTR)Response->Hint);
        ASSERT(FspIrpRequest(ProcessIrp)->Hint == Response->Hint);

        Result = FspIopDispatchComplete(ProcessIrp, Response);
        if (STATUS_PENDING == Result)
        {
            /*
             * The IRP has been reposted to our Ioq. Remember the first such IRP,
             * so that we know to break the loop if we see it again.
             */
            if (0 == RepostedIrp)
                RepostedIrp = ProcessIrp;
        }

        Response = NextResponse;
    }

    /* process any retried IRP's */
    for (;;)
    {
        /* get the next retried IRP, but do not go beyond the first reposted IRP! */
        RetriedIrp = FspIoqNextCompleteIrp(FsvolDeviceExtension->Ioq, RepostedIrp);
        if (0 == RetriedIrp)
            break;

        Response = FspIopIrpResponse(RetriedIrp);
        Result = FspIopDispatchComplete(RetriedIrp, Response);
        if (STATUS_PENDING == Result)
        {
            /*
             * The IRP has been reposted to our Ioq. Remember the first such IRP,
             * so that we know to break the loop if we see it again.
             */
            if (0 == RepostedIrp)
                RepostedIrp = RetriedIrp;
        }
    }

    /* were we sent an output buffer? */
    switch (ControlCode & 3)
    {
    case METHOD_OUT_DIRECT:
        if (0 != Irp->MdlAddress)
            OutputBuffer = MmGetMdlVirtualAddress(Irp->MdlAddress);
        break;
    case METHOD_BUFFERED:
        if (0 != OutputBufferLength)
            OutputBuffer = Irp->AssociatedIrp.SystemBuffer;
        break;
    default:
        ASSERT(0);
        break;
    }
    if (0 == OutputBuffer)
    {
        Irp->IoStatus.Information = 0;
        Result = STATUS_SUCCESS;
        goto exit;
    }

    /* wait for an IRP to arrive */
    KeQuerySystemTime(&Timeout);
    Timeout.QuadPart += FsvolDeviceExtension->VolumeParams.TransactTimeout * 10000ULL;
        /* convert millis to nanos and add to absolute time */
    while (0 == (PendingIrp = FspIoqNextPendingIrp(FsvolDeviceExtension->Ioq, 0, &Timeout, Irp)))
    {
        if (FspIoqStopped(FsvolDeviceExtension->Ioq))
        {
            Result = STATUS_CANCELLED;
            goto exit;
        }
    }
    if (FspIoqTimeout == PendingIrp || FspIoqCancelled == PendingIrp)
    {
        Irp->IoStatus.Information = 0;
        Result = FspIoqTimeout == PendingIrp ? STATUS_SUCCESS : STATUS_CANCELLED;
        goto exit;
    }

    /* send any pending IRP's to the user-mode file system */
    RepostedIrp = 0;
    Request = OutputBuffer;
    BufferEnd = (PUINT8)OutputBuffer + OutputBufferLength;
    ASSERT(FspFsctlTransactCanProduceRequest(Request, BufferEnd));
    for (;;)
    {
        PendingIrpRequest = FspIrpRequest(PendingIrp);

        Result = FspIopDispatchPrepare(PendingIrp, PendingIrpRequest);
        if (STATUS_PENDING == Result)
        {
            /*
             * The IRP has been reposted to our Ioq. Remember the first such IRP,
             * so that we know to break the loop if we see it again.
             */
            if (0 == RepostedIrp)
                RepostedIrp = PendingIrp;
        }
        else if (!NT_SUCCESS(Result))
            FspIopCompleteIrp(PendingIrp, Result);
        else
        {
            RtlCopyMemory(Request, PendingIrpRequest, PendingIrpRequest->Size);
            Request = FspFsctlTransactProduceRequest(Request, PendingIrpRequest->Size);

            if (!FspIoqStartProcessingIrp(FsvolDeviceExtension->Ioq, PendingIrp))
            {
                /*
                 * This can only happen if the Ioq was stopped. Abandon everything
                 * and return STATUS_CANCELLED. Any IRP's in the Pending and Process
                 * queues of the Ioq will be cancelled during FspIoqStop(). We must
                 * also cancel the PendingIrp we have in our hands.
                 */
                ASSERT(FspIoqStopped(FsvolDeviceExtension->Ioq));
                FspIopCompleteCanceledIrp(PendingIrp);
                Result = STATUS_CANCELLED;
                goto exit;
            }

            /* are we doing single request or batch mode? */
            if (FSP_FSCTL_TRANSACT == ControlCode)
                break;

            /* check that we have enough space before pulling the next pending IRP off the queue */
            if (!FspFsctlTransactCanProduceRequest(Request, BufferEnd))
                break;
        }

        /* get the next pending IRP, but do not go beyond the first reposted IRP! */
        PendingIrp = FspIoqNextPendingIrp(FsvolDeviceExtension->Ioq, RepostedIrp, 0, Irp);
        if (0 == PendingIrp)
            break;
    }

    Irp->IoStatus.Information = (PUINT8)Request - (PUINT8)OutputBuffer;
    Result = STATUS_SUCCESS;

exit:
    FspDeviceDereference(FsvolDeviceObject);
    return Result;
}

NTSTATUS FspVolumeWork(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction);
    ASSERT(
        FSP_FSCTL_WORK == IrpSp->Parameters.FileSystemControl.FsControlCode ||
        FSP_FSCTL_WORK_BEST_EFFORT == IrpSp->Parameters.FileSystemControl.FsControlCode);

    if (KernelMode != Irp->RequestorMode)
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    FSP_FSCTL_TRANSACT_REQ *Request = IrpSp->Parameters.FileSystemControl.Type3InputBuffer;
    BOOLEAN BestEffort = FSP_FSCTL_WORK_BEST_EFFORT == IrpSp->Parameters.FileSystemControl.FsControlCode;

    ASSERT(0 == Request->Hint);

    /* associate the passed Request with our Irp; acquire ownership of the Request */
    Request->Hint = (UINT_PTR)Irp;
    FspIrpRequest(Irp) = Request;

    /*
     * Post the IRP to our Ioq; we do this here instead of at FSP_LEAVE_MJ time,
     * so that we can disassociate the Request on failure and release ownership
     * back to the caller.
     */
    if (!FspIoqPostIrpEx(FsvolDeviceExtension->Ioq, Irp, BestEffort, &Result))
    {
        Request->Hint = 0;
        FspIrpRequest(Irp) = 0;
    }

    DEBUGLOG("%s(Irp=%p) = %s",
        IoctlCodeSym(BestEffort ? FSP_FSCTL_WORK_BEST_EFFORT : FSP_FSCTL_WORK),
        Irp, /* referencing pointer value, which is safe despite FspIoqPostIrpEx above! */
        NtStatusSym(Result));

    return Result;
}
