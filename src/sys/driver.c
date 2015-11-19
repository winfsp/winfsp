/**
 * @file sys/driver.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD FspUnload;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, FspUnload)
#endif

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    FSP_ENTER();

    /* create the file system device object */
    UNICODE_STRING DeviceName;
    RtlInitUnicodeString(&DeviceName, L"\\Device\\" DEVICE_NAME);
    Result = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_FILE_SYSTEM, 0, FALSE,
        &FspDeviceObject);
    if (!NT_SUCCESS(Result))
        FSP_RETURN();

    /* setup the driver object */
    DriverObject->DriverUnload = FspUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = FspCreate;
    //DriverObject->MajorFunction[IRP_MJ_CREATE_NAMED_PIPE] = IopInvalidDeviceRequest;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = FspClose;
    DriverObject->MajorFunction[IRP_MJ_READ] = FspRead;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = FspWrite;
    DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION] = FspQueryInformation;
    DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] = FspSetInformation;
    DriverObject->MajorFunction[IRP_MJ_QUERY_EA] = FspQueryEa;
    DriverObject->MajorFunction[IRP_MJ_SET_EA] = FspSetEa;
    DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS] = FspFlushBuffers;
    DriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] = FspQueryVolumeInformation;
    DriverObject->MajorFunction[IRP_MJ_SET_VOLUME_INFORMATION] = FspSetVolumeInformation;
    DriverObject->MajorFunction[IRP_MJ_DIRECTORY_CONTROL] = FspDirectoryControl;
    DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = FspFileSystemControl;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = FspDeviceControl;
    //DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = IopInvalidDeviceRequest;
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN] = FspShutdown;
    DriverObject->MajorFunction[IRP_MJ_LOCK_CONTROL] = FspLockControl;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = FspCleanup;
    //DriverObject->MajorFunction[IRP_MJ_CREATE_MAILSLOT] = IopInvalidDeviceRequest;
    DriverObject->MajorFunction[IRP_MJ_QUERY_SECURITY] = FspQuerySecurity;
    DriverObject->MajorFunction[IRP_MJ_SET_SECURITY] = FspSetSecurity;
    //DriverObject->MajorFunction[IRP_MJ_POWER] = IopInvalidDeviceRequest;
    //DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = IopInvalidDeviceRequest;
    //DriverObject->MajorFunction[IRP_MJ_DEVICE_CHANGE] = IopInvalidDeviceRequest;
    //DriverObject->MajorFunction[IRP_MJ_QUERY_QUOTA] = IopInvalidDeviceRequest;
    //DriverObject->MajorFunction[IRP_MJ_SET_QUOTA] = IopInvalidDeviceRequest;
    //DriverObject->MajorFunction[IRP_MJ_PNP] = IopInvalidDeviceRequest;

    /* setup fast I/O and resource acquisition */
    static FAST_IO_DISPATCH FspFastIoDispatch = { 0 };
    FspFastIoDispatch.SizeOfFastIoDispatch = sizeof FspFastIoDispatch;
    FspFastIoDispatch.FastIoCheckIfPossible = FspFastIoCheckIfPossible;
    FspFastIoDispatch.FastIoRead = FsRtlCopyRead;
    FspFastIoDispatch.FastIoWrite = FsRtlCopyWrite;
    //FspFastIoDispatch.FastIoQueryBasicInfo = 0;
    //FspFastIoDispatch.FastIoQueryStandardInfo = 0;
    //FspFastIoDispatch.FastIoLock = 0;
    //FspFastIoDispatch.FastIoUnlockSingle = 0;
    //FspFastIoDispatch.FastIoUnlockAll = 0;
    //FspFastIoDispatch.FastIoUnlockAllByKey = 0;
    //FspFastIoDispatch.FastIoDeviceControl = 0;
    FspFastIoDispatch.AcquireFileForNtCreateSection = FspAcquireFileForNtCreateSection;
    FspFastIoDispatch.ReleaseFileForNtCreateSection = FspReleaseFileForNtCreateSection;
    //FspFastIoDispatch.FastIoDetachDevice = 0;
    //FspFastIoDispatch.FastIoQueryNetworkOpenInfo = 0;
    FspFastIoDispatch.AcquireForModWrite = FspAcquireForModWrite;
    FspFastIoDispatch.MdlRead = FsRtlMdlReadDev;
    FspFastIoDispatch.MdlReadComplete = FsRtlMdlReadCompleteDev;
    FspFastIoDispatch.PrepareMdlWrite = FsRtlPrepareMdlWriteDev;
    FspFastIoDispatch.MdlWriteComplete = FsRtlMdlWriteCompleteDev;
    //FspFastIoDispatch.FastIoReadCompressed = 0;
    //FspFastIoDispatch.FastIoWriteCompressed = 0;
    //FspFastIoDispatch.MdlReadCompleteCompressed = 0;
    //FspFastIoDispatch.MdlWriteCompleteCompressed = 0;
    //FspFastIoDispatch.FastIoQueryOpen = 0;
    FspFastIoDispatch.ReleaseForModWrite = FspReleaseForModWrite;
    FspFastIoDispatch.AcquireForCcFlush = FspAcquireForCcFlush;
    FspFastIoDispatch.ReleaseForCcFlush = FspReleaseForCcFlush;
    DriverObject->FastIoDispatch = &FspFastIoDispatch;

    /*
     * Register as a file system; this informs all filter drivers.
     * Future drivers will *not* be informed because we are a FILE_DEVICE_FILE_SYSTEM!
     */
    IoRegisterFileSystem(FspDeviceObject);

    FSP_LEAVE("DriverObject->DriverName=\"%wZ\", RegistryPath=\"%wZ\"",
        &DriverObject->DriverName, RegistryPath);
}

VOID
FspUnload(
    _In_ PDRIVER_OBJECT DriverObject)
{
    FSP_ENTER_VOID(PAGED_CODE());

    if (0 != FspDeviceObject)
    {
        IoDeleteDevice(FspDeviceObject);
        FspDeviceObject = 0;
    }

    FSP_LEAVE_VOID("DriverObject->DriverName=\"%wZ\"",
        &DriverObject->DriverName);
}

PDEVICE_OBJECT FspDeviceObject;
