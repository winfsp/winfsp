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

    /* create the file system control device objects */
    UNICODE_STRING DeviceSddl;
    UNICODE_STRING DeviceName;
    RtlInitUnicodeString(&DeviceSddl, L"" FSP_FSCTL_DEVICE_SDDL);
    RtlInitUnicodeString(&DeviceName, L"\\Device\\" FSP_FSCTL_DISK_DEVICE_NAME);
    Result = IoCreateDeviceSecure(DriverObject,
        sizeof(FSP_FSCTL_DEVICE_EXTENSION), &DeviceName, FILE_DEVICE_DISK_FILE_SYSTEM,
        FILE_DEVICE_SECURE_OPEN, FALSE,
        &DeviceSddl, &FspFsctlDeviceClassGuid,
        &FspFsctlDiskDeviceObject);
    if (!NT_SUCCESS(Result))
        FSP_RETURN();
    RtlInitUnicodeString(&DeviceName, L"\\Device\\" FSP_FSCTL_NET_DEVICE_NAME);
    Result = IoCreateDeviceSecure(DriverObject,
        sizeof(FSP_FSCTL_DEVICE_EXTENSION), &DeviceName, FILE_DEVICE_NETWORK_FILE_SYSTEM,
        FILE_DEVICE_SECURE_OPEN, FALSE,
        &DeviceSddl, &FspFsctlDeviceClassGuid,
        &FspFsctlNetDeviceObject);
    if (!NT_SUCCESS(Result))
        FSP_RETURN(IoDeleteDevice(FspFsctlDiskDeviceObject));
    FspDeviceExtension(FspFsctlDiskDeviceObject)->Kind = FspFsctlDeviceExtensionKind;
    FspDeviceExtension(FspFsctlNetDeviceObject)->Kind = FspFsctlDeviceExtensionKind;

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

    /* setup the I/O completion functions */
    FspIoProcessFunction[IRP_MJ_CREATE] = FspCreateComplete;
    FspIoProcessFunction[IRP_MJ_CLOSE] = FspCloseComplete;
    FspIoProcessFunction[IRP_MJ_READ] = FspReadComplete;
    FspIoProcessFunction[IRP_MJ_WRITE] = FspWriteComplete;
    FspIoProcessFunction[IRP_MJ_QUERY_INFORMATION] = FspQueryInformationComplete;
    FspIoProcessFunction[IRP_MJ_SET_INFORMATION] = FspSetInformationComplete;
    FspIoProcessFunction[IRP_MJ_QUERY_EA] = FspQueryEaComplete;
    FspIoProcessFunction[IRP_MJ_SET_EA] = FspSetEaComplete;
    FspIoProcessFunction[IRP_MJ_FLUSH_BUFFERS] = FspFlushBuffersComplete;
    FspIoProcessFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] = FspQueryVolumeInformationComplete;
    FspIoProcessFunction[IRP_MJ_SET_VOLUME_INFORMATION] = FspSetVolumeInformationComplete;
    FspIoProcessFunction[IRP_MJ_DIRECTORY_CONTROL] = FspDirectoryControlComplete;
    FspIoProcessFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = FspFileSystemControlComplete;
    FspIoProcessFunction[IRP_MJ_DEVICE_CONTROL] = FspDeviceControlComplete;
    FspIoProcessFunction[IRP_MJ_SHUTDOWN] = FspShutdownComplete;
    FspIoProcessFunction[IRP_MJ_LOCK_CONTROL] = FspLockControlComplete;
    FspIoProcessFunction[IRP_MJ_CLEANUP] = FspCleanupComplete;
    FspIoProcessFunction[IRP_MJ_QUERY_SECURITY] = FspQuerySecurityComplete;
    FspIoProcessFunction[IRP_MJ_SET_SECURITY] = FspSetSecurityComplete;

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
#pragma prefast(suppress:28175, "We are a filesystem: ok to touch FastIoDispatch")
    DriverObject->FastIoDispatch = &FspFastIoDispatch;

    /* register our device objects as file systems */
    IoRegisterFileSystem(FspFsctlDiskDeviceObject);
    IoRegisterFileSystem(FspFsctlNetDeviceObject);

    FSP_LEAVE("DriverName=\"%wZ\", RegistryPath=\"%wZ\"",
        &DriverObject->DriverName, RegistryPath);
}

VOID FspUnload(
    PDRIVER_OBJECT DriverObject)
{
    FSP_ENTER_VOID(PAGED_CODE());

    if (0 != FspFsctlDiskDeviceObject)
    {
        IoDeleteDevice(FspFsctlDiskDeviceObject);
        FspFsctlDiskDeviceObject = 0;
    }
    if (0 != FspFsctlNetDeviceObject)
    {
        IoDeleteDevice(FspFsctlNetDeviceObject);
        FspFsctlNetDeviceObject = 0;
    }

    FSP_LEAVE_VOID("DriverName=\"%wZ\"",
        &DriverObject->DriverName);
}

PDEVICE_OBJECT FspFsctlDiskDeviceObject;
PDEVICE_OBJECT FspFsctlNetDeviceObject;
