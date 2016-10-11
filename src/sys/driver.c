/**
 * @file sys/driver.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.

 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <sys/driver.h>

/*
 * Define the following macro to include FspUnload.
 *
 * Note that this driver is no longer unloadable.
 * See the comments in DriverEntry as to why!
 */
//#define FSP_UNLOAD

DRIVER_INITIALIZE DriverEntry;
static VOID FspDriverMultiVersionInitialize(VOID);
#if defined(FSP_UNLOAD)
DRIVER_UNLOAD FspUnload;
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(INIT, FspDriverMultiVersionInitialize)
#if defined(FSP_UNLOAD)
#pragma alloc_text(PAGE, FspUnload)
#endif
#endif

NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    FSP_ENTER_DRV();

    FspDriverMultiVersionInitialize();

    FspDriverObject = DriverObject;
    ExInitializeResourceLite(&FspDeviceGlobalResource);

    /* create the file system control device objects */
    UNICODE_STRING DeviceSddl;
    UNICODE_STRING DeviceName;
    RtlInitUnicodeString(&DeviceSddl, L"" FSP_FSCTL_DEVICE_SDDL);
    RtlInitUnicodeString(&DeviceName, L"\\Device\\" FSP_FSCTL_DISK_DEVICE_NAME);
    Result = FspDeviceCreateSecure(FspFsctlDeviceExtensionKind, 0,
        &DeviceName, FILE_DEVICE_DISK_FILE_SYSTEM, FILE_DEVICE_SECURE_OPEN,
        &DeviceSddl, &FspFsctlDeviceClassGuid,
        &FspFsctlDiskDeviceObject);
    if (!NT_SUCCESS(Result))
        FSP_RETURN();
    RtlInitUnicodeString(&DeviceName, L"\\Device\\" FSP_FSCTL_NET_DEVICE_NAME);
    Result = FspDeviceCreateSecure(FspFsctlDeviceExtensionKind, 0,
        &DeviceName, FILE_DEVICE_NETWORK_FILE_SYSTEM, FILE_DEVICE_SECURE_OPEN,
        &DeviceSddl, &FspFsctlDeviceClassGuid,
        &FspFsctlNetDeviceObject);
    if (!NT_SUCCESS(Result))
        FSP_RETURN(FspDeviceDelete(FspFsctlDiskDeviceObject));
    Result = FspDeviceInitialize(FspFsctlDiskDeviceObject);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspDeviceInitialize(FspFsctlNetDeviceObject);
    ASSERT(STATUS_SUCCESS == Result);

    /* setup the driver object */
#if defined(FSP_UNLOAD)
    DriverObject->DriverUnload = FspUnload;
#endif
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
    FspIopPrepareFunction[IRP_MJ_READ] = FspFsvolReadPrepare;
    FspIopCompleteFunction[IRP_MJ_READ] = FspFsvolReadComplete;
    FspIopPrepareFunction[IRP_MJ_WRITE] = FspFsvolWritePrepare;
    FspIopCompleteFunction[IRP_MJ_WRITE] = FspFsvolWriteComplete;
    FspIopCompleteFunction[IRP_MJ_QUERY_INFORMATION] = FspFsvolQueryInformationComplete;
    FspIopPrepareFunction[IRP_MJ_SET_INFORMATION] = FspFsvolSetInformationPrepare;
    FspIopCompleteFunction[IRP_MJ_SET_INFORMATION] = FspFsvolSetInformationComplete;
    FspIopCompleteFunction[IRP_MJ_QUERY_EA] = FspFsvolQueryEaComplete;
    FspIopCompleteFunction[IRP_MJ_SET_EA] = FspFsvolSetEaComplete;
    FspIopCompleteFunction[IRP_MJ_FLUSH_BUFFERS] = FspFsvolFlushBuffersComplete;
    FspIopCompleteFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] = FspFsvolQueryVolumeInformationComplete;
    FspIopCompleteFunction[IRP_MJ_SET_VOLUME_INFORMATION] = FspFsvolSetVolumeInformationComplete;
    FspIopPrepareFunction[IRP_MJ_DIRECTORY_CONTROL] = FspFsvolDirectoryControlPrepare;
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
    FspFastIoDispatch.SizeOfFastIoDispatch = sizeof FspFastIoDispatch;
    FspFastIoDispatch.FastIoCheckIfPossible = FspFastIoCheckIfPossible;
    //FspFastIoDispatch.FastIoRead = 0;
    //FspFastIoDispatch.FastIoWrite = 0;
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
    //FspFastIoDispatch.MdlRead = 0;
    //FspFastIoDispatch.MdlReadComplete = 0;
    //FspFastIoDispatch.PrepareMdlWrite = 0;
    //FspFastIoDispatch.MdlWriteComplete = 0;
    //FspFastIoDispatch.FastIoReadCompressed = 0;
    //FspFastIoDispatch.FastIoWriteCompressed = 0;
    //FspFastIoDispatch.MdlReadCompleteCompressed = 0;
    //FspFastIoDispatch.MdlWriteCompleteCompressed = 0;
    //FspFastIoDispatch.FastIoQueryOpen = 0;
    FspFastIoDispatch.ReleaseForModWrite = FspReleaseForModWrite;
    FspFastIoDispatch.AcquireForCcFlush = FspAcquireForCcFlush;
    FspFastIoDispatch.ReleaseForCcFlush = FspReleaseForCcFlush;
    FspCacheManagerCallbacks.AcquireForLazyWrite = FspAcquireForLazyWrite;
    FspCacheManagerCallbacks.ReleaseFromLazyWrite = FspReleaseFromLazyWrite;
    FspCacheManagerCallbacks.AcquireForReadAhead = FspAcquireForReadAhead;
    FspCacheManagerCallbacks.ReleaseFromReadAhead = FspReleaseFromReadAhead;
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
    FSP_LEAVE_DRV("DriverName=\"%wZ\", RegistryPath=\"%wZ\"",
        &DriverObject->DriverName, RegistryPath);
}

static VOID FspDriverMultiVersionInitialize(VOID)
{
    if (RtlIsNtDdiVersionAvailable(NTDDI_WIN7))
    {
        UNICODE_STRING Name;

        RtlInitUnicodeString(&Name, L"CcCoherencyFlushAndPurgeCache");
        FspMvCcCoherencyFlushAndPurgeCache =
            (FSP_MV_CcCoherencyFlushAndPurgeCache *)(UINT_PTR)MmGetSystemRoutineAddress(&Name);
    }

    if (RtlIsNtDdiVersionAvailable(NTDDI_WIN8))
        FspMvMdlMappingNoWrite = MdlMappingNoWrite;
}

#if defined(FSP_UNLOAD)
VOID FspUnload(
    PDRIVER_OBJECT DriverObject)
{
    FSP_ENTER_VOID(PAGED_CODE());

    FspFsctlDiskDeviceObject = 0;
    FspFsctlNetDeviceObject = 0;
    //FspDeviceDeleteAll();

    ExDeleteResourceLite(&FspDeviceGlobalResource);
    FspDriverObject = 0;

#pragma prefast(suppress:28175, "We are in DriverUnload: ok to access DriverName")
    FSP_LEAVE_VOID("DriverName=\"%wZ\"",
        &DriverObject->DriverName);
}
#endif

PDRIVER_OBJECT FspDriverObject;
PDEVICE_OBJECT FspFsctlDiskDeviceObject;
PDEVICE_OBJECT FspFsctlNetDeviceObject;
FAST_IO_DISPATCH FspFastIoDispatch;
CACHE_MANAGER_CALLBACKS FspCacheManagerCallbacks;

FSP_MV_CcCoherencyFlushAndPurgeCache *FspMvCcCoherencyFlushAndPurgeCache;
ULONG FspMvMdlMappingNoWrite = 0;
