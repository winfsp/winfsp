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
    Result = FspDeviceInitialize(FspFsctlDiskDeviceObject);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspDeviceInitialize(FspFsctlNetDeviceObject);
    ASSERT(STATUS_SUCCESS == Result);

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

    /* setup the I/O prepare/complete functions */
    FspIopPrepareFunction[IRP_MJ_CREATE] = FspFsvolCreatePrepare;
    FspIopCompleteFunction[IRP_MJ_CREATE] = FspFsvolCreateComplete;
    FspIopCompleteFunction[IRP_MJ_CLOSE] = FspFsvolCloseComplete;
    FspIopCompleteFunction[IRP_MJ_READ] = FspFsvolReadComplete;
    FspIopCompleteFunction[IRP_MJ_WRITE] = FspFsvolWriteComplete;
    FspIopCompleteFunction[IRP_MJ_QUERY_INFORMATION] = FspFsvolQueryInformationComplete;
    FspIopCompleteFunction[IRP_MJ_SET_INFORMATION] = FspFsvolSetInformationComplete;
    FspIopCompleteFunction[IRP_MJ_QUERY_EA] = FspFsvolQueryEaComplete;
    FspIopCompleteFunction[IRP_MJ_SET_EA] = FspFsvolSetEaComplete;
    FspIopCompleteFunction[IRP_MJ_FLUSH_BUFFERS] = FspFsvolFlushBuffersComplete;
    FspIopCompleteFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] = FspFsvolQueryVolumeInformationComplete;
    FspIopCompleteFunction[IRP_MJ_SET_VOLUME_INFORMATION] = FspFsvolSetVolumeInformationComplete;
    FspIopCompleteFunction[IRP_MJ_DIRECTORY_CONTROL] = FspFsvolDirectoryControlComplete;
    FspIopCompleteFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = FspFsvolFileSystemControlComplete;
    FspIopCompleteFunction[IRP_MJ_DEVICE_CONTROL] = FspFsvolDeviceControlComplete;
    FspIopCompleteFunction[IRP_MJ_SHUTDOWN] = FspFsvolShutdownComplete;
    FspIopCompleteFunction[IRP_MJ_LOCK_CONTROL] = FspFsvolLockControlComplete;
    FspIopCompleteFunction[IRP_MJ_CLEANUP] = FspFsvolCleanupComplete;
    FspIopCompleteFunction[IRP_MJ_QUERY_SECURITY] = FspFsvolQuerySecurityComplete;
    FspIopPrepareFunction[IRP_MJ_SET_SECURITY] = FspFsvolSetSecurityPrepare;
    FspIopCompleteFunction[IRP_MJ_SET_SECURITY] = FspFsvolSetSecurityComplete;

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

    /*
     * Register our "disk" device as a file system. We do not register our "net" device
     * as a file system, but we register with the MUP instead at a later time.
     *
     * Please note that the call below makes our driver unloadable. In fact the driver
     * remains unloadable even if we issue an IoUnregisterFileSystem() call immediately
     * after our IoRegisterFileSystem() call! Some system component appears to keep an
     * extra reference to our device somewhere.
     */
    IoRegisterFileSystem(FspFsctlDiskDeviceObject);

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
