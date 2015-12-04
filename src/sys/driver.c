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

NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    FSP_ENTER();

    FspDriverObject = DriverObject;

    /* create the file system control device objects */
    UNICODE_STRING DeviceSddl;
    UNICODE_STRING DeviceName;
    RtlInitUnicodeString(&DeviceSddl, L"" FSP_FSCTL_DEVICE_SDDL);
    RtlInitUnicodeString(&DeviceName, L"\\Device\\" FSP_FSCTL_DISK_DEVICE_NAME);
    Result = FspDeviceCreateSecure(FspFsctlDeviceExtensionKind, 0,
        &DeviceName, FILE_DEVICE_DISK_FILE_SYSTEM,
        &DeviceSddl, &FspFsctlDeviceClassGuid,
        &FspFsctlDiskDeviceObject);
    if (!NT_SUCCESS(Result))
        FSP_RETURN();
    RtlInitUnicodeString(&DeviceName, L"\\Device\\" FSP_FSCTL_NET_DEVICE_NAME);
    Result = FspDeviceCreateSecure(FspFsctlDeviceExtensionKind, 0,
        &DeviceName, FILE_DEVICE_NETWORK_FILE_SYSTEM,
        &DeviceSddl, &FspFsctlDeviceClassGuid,
        &FspFsctlNetDeviceObject);
    if (!NT_SUCCESS(Result))
        FSP_RETURN(FspDeviceDelete(FspFsctlDiskDeviceObject));

    /* setup the driver object */
    DriverObject->DriverUnload = FspUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = FspCreate;
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
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN] = FspShutdown;
    DriverObject->MajorFunction[IRP_MJ_LOCK_CONTROL] = FspLockControl;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = FspCleanup;
    DriverObject->MajorFunction[IRP_MJ_QUERY_SECURITY] = FspQuerySecurity;
    DriverObject->MajorFunction[IRP_MJ_SET_SECURITY] = FspSetSecurity;

    /* setup the I/O preparation functions */
    FspIopPrepareFunction[IRP_MJ_CREATE] = FspFsvolCreatePrepare;

    /* setup the I/O completion functions */
    FspIopCompleteFunction[IRP_MJ_CREATE] = FspFsvolCreateComplete;
    FspIopCompleteFunction[IRP_MJ_CLOSE] = FspCloseComplete;
    FspIopCompleteFunction[IRP_MJ_READ] = FspReadComplete;
    FspIopCompleteFunction[IRP_MJ_WRITE] = FspWriteComplete;
    FspIopCompleteFunction[IRP_MJ_QUERY_INFORMATION] = FspQueryInformationComplete;
    FspIopCompleteFunction[IRP_MJ_SET_INFORMATION] = FspSetInformationComplete;
    FspIopCompleteFunction[IRP_MJ_QUERY_EA] = FspQueryEaComplete;
    FspIopCompleteFunction[IRP_MJ_SET_EA] = FspSetEaComplete;
    FspIopCompleteFunction[IRP_MJ_FLUSH_BUFFERS] = FspFlushBuffersComplete;
    FspIopCompleteFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] = FspQueryVolumeInformationComplete;
    FspIopCompleteFunction[IRP_MJ_SET_VOLUME_INFORMATION] = FspSetVolumeInformationComplete;
    FspIopCompleteFunction[IRP_MJ_DIRECTORY_CONTROL] = FspDirectoryControlComplete;
    FspIopCompleteFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = FspFileSystemControlComplete;
    FspIopCompleteFunction[IRP_MJ_DEVICE_CONTROL] = FspDeviceControlComplete;
    FspIopCompleteFunction[IRP_MJ_SHUTDOWN] = FspShutdownComplete;
    FspIopCompleteFunction[IRP_MJ_LOCK_CONTROL] = FspLockControlComplete;
    FspIopCompleteFunction[IRP_MJ_CLEANUP] = FspCleanupComplete;
    FspIopCompleteFunction[IRP_MJ_QUERY_SECURITY] = FspQuerySecurityComplete;
    FspIopCompleteFunction[IRP_MJ_SET_SECURITY] = FspSetSecurityComplete;

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
#pragma prefast(suppress:28175, "We are a filesystem: ok to access FastIoDispatch")
    DriverObject->FastIoDispatch = &FspFastIoDispatch;

#pragma prefast(suppress:28175, "We are in DriverEntry: ok to access DriverName")
    FSP_LEAVE("DriverName=\"%wZ\", RegistryPath=\"%wZ\"",
        &DriverObject->DriverName, RegistryPath);
}

VOID FspUnload(
    PDRIVER_OBJECT DriverObject)
{
    FSP_ENTER_VOID(PAGED_CODE());

    FspFsctlDiskDeviceObject = 0;
    FspFsctlNetDeviceObject = 0;
    FspDeviceDeleteAll();

    FspDriverObject = 0;

#pragma prefast(suppress:28175, "We are in DriverUnload: ok to access DriverName")
    FSP_LEAVE_VOID("DriverName=\"%wZ\"",
        &DriverObject->DriverName);
}

PDRIVER_OBJECT FspDriverObject;
PDEVICE_OBJECT FspFsctlDiskDeviceObject;
PDEVICE_OBJECT FspFsctlNetDeviceObject;
