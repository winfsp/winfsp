/**
 * @file sys/fsctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsctlCreateVolume(
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
DRIVER_DISPATCH FspFileSystemControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCreateVolume)
#pragma alloc_text(PAGE, FspFsvrtDeleteVolume)
#pragma alloc_text(PAGE, FspFsvrtTransact)
#pragma alloc_text(PAGE, FspFsctlFileSystemControl)
#pragma alloc_text(PAGE, FspFsvrtFileSystemControl)
#pragma alloc_text(PAGE, FspFsvolFileSystemControl)
#pragma alloc_text(PAGE, FspFileSystemControl)
#endif

static NTSTATUS FspFsctlCreateVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    /* check parameters */
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PVOID SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    if (0 == InputBufferLength || 0 == SystemBuffer ||
        !RtlValidRelativeSecurityDescriptor(SystemBuffer, InputBufferLength,
            OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION))
        return STATUS_INVALID_PARAMETER;
    if (FSP_FSCTL_CREATE_BUFFER_SIZE > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS Result;

    /* create volume guid */
    GUID Guid;
    Result = FspCreateGuid(&Guid);
    if (!NT_SUCCESS(Result))
        return Result;

    /* copy the security descriptor from the system buffer to a temporary one */
    PVOID SecurityDescriptor = ExAllocatePoolWithTag(PagedPool, InputBufferLength, FSP_TAG);
    if (0 == SecurityDescriptor)
        return STATUS_INSUFFICIENT_RESOURCES;
    RtlCopyMemory(SecurityDescriptor, SystemBuffer, InputBufferLength);

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
        sizeof(FSP_FSVRT_DEVICE_EXTENSION) + InputBufferLength, &DeviceName, FILE_DEVICE_VIRTUAL_DISK,
        FILE_DEVICE_SECURE_OPEN, FALSE,
        &DeviceSddl, 0,
        &FsvrtDeviceObject);
    if (NT_SUCCESS(Result))
    {
        FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension = FspFsvrtDeviceExtension(FsvrtDeviceObject);
        FsvrtDeviceExtension->Base.Kind = FspFsvrtDeviceExtensionKind;
        FspIoqInitialize(&FsvrtDeviceExtension->TransactIoq);
        FspIoqInitialize(&FsvrtDeviceExtension->Ioq);
        RtlCopyMemory(FspFsvrtDeviceExtension(FsvrtDeviceObject)->SecurityDescriptorBuf,
            SecurityDescriptor, InputBufferLength);
        Irp->IoStatus.Information = DeviceName.Length + 1;
        Result = FspTransactThreadStart(&FsvrtDeviceExtension->TransactThread,
            &FsvrtDeviceExtension->TransactIoq, &FsvrtDeviceExtension->Ioq);
        if (!NT_SUCCESS(Result))
            IoDeleteDevice(FsvrtDeviceObject);
    }

    /* free the temporary security descriptor */
    ExFreePoolWithTag(SecurityDescriptor, FSP_TAG);

    return Result;
}

static NTSTATUS FspFsvrtDeleteVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    NTSTATUS Result;
    FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension = FspFsvrtDeviceExtension(DeviceObject);

    /* access check */
    Result = FspSecuritySubjectContextAccessCheck(
        FsvrtDeviceExtension->SecurityDescriptorBuf, FILE_WRITE_DATA, Irp->RequestorMode);
    if (!NT_SUCCESS(Result))
        return Result;

    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvrtTransact(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    /* check parameters */
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PVOID SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    if (sizeof(FSP_TRANSACT_RSP) > InputBufferLength || 0 == SystemBuffer)
        return STATUS_INVALID_PARAMETER;
    if (FSP_FSCTL_TRANSACT_BUFFER_SIZE > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS Result;
    FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension = FspFsvrtDeviceExtension(DeviceObject);
    PUINT8 SystemBufferEnd;
    FSP_TRANSACT_RSP *Response;
    PIRP ProcessIrp;

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
        if ((PUINT8)Response + sizeof(Response->Size) > SystemBufferEnd ||
            sizeof(FSP_TRANSACT_RSP) > Response->Size ||
            (PUINT8)Response + Response->Size > SystemBufferEnd)
            break;

        ProcessIrp = FspIoqEndProcessingIrp(&FsvrtDeviceExtension->Ioq, Response->Hint);
        if (0 == ProcessIrp)
            /* either IRP was canceled or a bogus IrpHint was provided */
            continue;

        //FspDispatchProcessedIrp(ProcessIrp, Response);

        Response = (PVOID)((PUINT8)Response + Response->Size);
    }

    if (FspIoqPostIrp(&FsvrtDeviceExtension->TransactIoq, Irp))
        Result = STATUS_PENDING;
    else
        Result = STATUS_ACCESS_DENIED;

    return Result;
}

static NTSTATUS FspFsctlFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
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
    }
    return Result;
}

static NTSTATUS FspFsvrtFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
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
    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        break;
    case IRP_MN_MOUNT_VOLUME:
        break;
    case IRP_MN_VERIFY_VOLUME:
        break;
    }
    return Result;
}

NTSTATUS
FspFileSystemControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
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
