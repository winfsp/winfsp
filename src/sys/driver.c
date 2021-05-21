/**
 * @file sys/driver.c
 *
 * @copyright 2015-2021 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#include <sys/driver.h>

DRIVER_INITIALIZE DriverEntry;
static VOID FspDriverMultiVersionInitialize(VOID);
static NTSTATUS FspDriverInitializeDevices(VOID);
static VOID FspDriverFinalizeDevices(VOID);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(INIT, FspDriverMultiVersionInitialize)
#pragma alloc_text(PAGE, FspDriverInitializeDevices)
#pragma alloc_text(PAGE, FspDriverFinalizeDevices)
#endif

NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    FSP_ENTER_DRV();

    FSP_TRACE_INIT();

    /* setup the driver object */
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
    FspIopCompleteFunction[IRP_MJ_LOCK_CONTROL] = FspFsvolLockControlComplete;
    FspIopCompleteFunction[IRP_MJ_CLEANUP] = FspFsvolCleanupComplete;
    FspIopCompleteFunction[IRP_MJ_QUERY_SECURITY] = FspFsvolQuerySecurityComplete;
    FspIopCompleteFunction[IRP_MJ_SET_SECURITY] = FspFsvolSetSecurityComplete;

    /* setup fast I/O and resource acquisition */
    FspFastIoDispatch.SizeOfFastIoDispatch = sizeof FspFastIoDispatch;
    FspFastIoDispatch.FastIoCheckIfPossible = FspFastIoCheckIfPossible;
    //FspFastIoDispatch.FastIoRead = 0;
    //FspFastIoDispatch.FastIoWrite = 0;
    FspFastIoDispatch.FastIoQueryBasicInfo = FspFastIoQueryBasicInfo;
    FspFastIoDispatch.FastIoQueryStandardInfo = FspFastIoQueryStandardInfo;
    //FspFastIoDispatch.FastIoLock = 0;
    //FspFastIoDispatch.FastIoUnlockSingle = 0;
    //FspFastIoDispatch.FastIoUnlockAll = 0;
    //FspFastIoDispatch.FastIoUnlockAllByKey = 0;
    //FspFastIoDispatch.FastIoDeviceControl = 0;
    FspFastIoDispatch.AcquireFileForNtCreateSection = FspAcquireFileForNtCreateSection;
    FspFastIoDispatch.ReleaseFileForNtCreateSection = FspReleaseFileForNtCreateSection;
    //FspFastIoDispatch.FastIoDetachDevice = 0;
    FspFastIoDispatch.FastIoQueryNetworkOpenInfo = FspFastIoQueryNetworkOpenInfo;
    FspFastIoDispatch.AcquireForModWrite = FspAcquireForModWrite;
    //FspFastIoDispatch.MdlRead = 0;
    //FspFastIoDispatch.MdlReadComplete = 0;
    //FspFastIoDispatch.PrepareMdlWrite = 0;
    //FspFastIoDispatch.MdlWriteComplete = 0;
    //FspFastIoDispatch.FastIoReadCompressed = 0;
    //FspFastIoDispatch.FastIoWriteCompressed = 0;
    //FspFastIoDispatch.MdlReadCompleteCompressed = 0;
    //FspFastIoDispatch.MdlWriteCompleteCompressed = 0;
    FspFastIoDispatch.FastIoQueryOpen = FspFastIoQueryOpen;
    FspFastIoDispatch.ReleaseForModWrite = FspReleaseForModWrite;
    FspFastIoDispatch.AcquireForCcFlush = FspAcquireForCcFlush;
    FspFastIoDispatch.ReleaseForCcFlush = FspReleaseForCcFlush;
    FspCacheManagerCallbacks.AcquireForLazyWrite = FspAcquireForLazyWrite;
    FspCacheManagerCallbacks.ReleaseFromLazyWrite = FspReleaseFromLazyWrite;
    FspCacheManagerCallbacks.AcquireForReadAhead = FspAcquireForReadAhead;
    FspCacheManagerCallbacks.ReleaseFromReadAhead = FspReleaseFromReadAhead;
#pragma prefast(suppress:28175, "We are a filesystem: ok to access FastIoDispatch")
    DriverObject->FastIoDispatch = &FspFastIoDispatch;

    BOOLEAN InitDoneGRes = FALSE, InitDoneSilo = FALSE, InitDonePsBuf = FALSE,
        InitDoneDevices = FALSE;

    FspDriverObject = DriverObject;
    FspDriverMultiVersionInitialize();

    ExInitializeResourceLite(&FspDeviceGlobalResource);
    InitDoneGRes = TRUE;

    Result = FspSiloInitialize(FspDriverInitializeDevices, FspDriverFinalizeDevices);
    if (!NT_SUCCESS(Result))
        goto exit;
    InitDoneSilo = TRUE;

    Result = FspProcessBufferInitialize();
    if (!NT_SUCCESS(Result))
        goto exit;
    InitDonePsBuf = TRUE;

    Result = FspDriverInitializeDevices();
    if (!NT_SUCCESS(Result))
        goto exit;
    InitDoneDevices = TRUE;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        if (InitDoneDevices)
            FspDriverFinalizeDevices();
        if (InitDonePsBuf)
            FspProcessBufferFinalize();
        if (InitDoneSilo)
            FspSiloFinalize();
        if (InitDoneGRes)
            ExDeleteResourceLite(&FspDeviceGlobalResource);

        FSP_TRACE_FINI();
    }

#pragma prefast(suppress:28175, "We are in DriverEntry: ok to access DriverName")
    FSP_LEAVE_DRV("DriverName=\"%wZ\", RegistryPath=\"%wZ\"",
        &DriverObject->DriverName, RegistryPath);
}

static VOID FspDriverMultiVersionInitialize(VOID)
{
    FspProcessorCount = KeQueryActiveProcessorCount(0);

#pragma prefast(suppress:30035, "FspDriverMultiVersionInitialize is called from DriverEntry")
    ExInitializeDriverRuntime(DrvRtPoolNxOptIn);

    if (FspIsNtDdiVersionAvailable(NTDDI_WIN7))
    {
        UNICODE_STRING Name;

        RtlInitUnicodeString(&Name, L"CcCoherencyFlushAndPurgeCache");
        FspMvCcCoherencyFlushAndPurgeCache =
            (FSP_MV_CcCoherencyFlushAndPurgeCache *)(UINT_PTR)MmGetSystemRoutineAddress(&Name);
    }

    if (FspIsNtDdiVersionAvailable(NTDDI_WIN8))
        FspMvMdlMappingNoWrite = MdlMappingNoWrite;

    if (FspIsNtDdiVersionAvailable(NTDDI_WIN10_RS4))
        FspHasReparsePointCaseSensitivityFix = TRUE;
}

static NTSTATUS FspDriverInitializeDevices(VOID)
{
    PAGED_CODE();

    FSP_SILO_GLOBALS *Globals;
    UNICODE_STRING DeviceSddl;
    UNICODE_STRING DeviceName;
    GUID Guid;
    NTSTATUS Result;

    FspSiloGetGlobals(&Globals);
    ASSERT(0 != Globals);

    /* create the file system control device objects */
    RtlInitUnicodeString(&DeviceSddl, L"" FSP_FSCTL_DEVICE_SDDL);
    RtlInitUnicodeString(&DeviceName, L"\\Device\\" FSP_FSCTL_DISK_DEVICE_NAME);
    Result = FspDeviceCreateSecure(FspFsctlDeviceExtensionKind, 0,
        &DeviceName, FILE_DEVICE_DISK_FILE_SYSTEM, FILE_DEVICE_SECURE_OPEN,
        &DeviceSddl, &FspFsctlDeviceClassGuid,
        &Globals->FsctlDiskDeviceObject);
    if (!NT_SUCCESS(Result))
        goto exit;
    RtlInitUnicodeString(&DeviceName, L"\\Device\\" FSP_FSCTL_NET_DEVICE_NAME);
    Result = FspDeviceCreateSecure(FspFsctlDeviceExtensionKind, 0,
        &DeviceName, FILE_DEVICE_NETWORK_FILE_SYSTEM, FILE_DEVICE_SECURE_OPEN,
        &DeviceSddl, &FspFsctlDeviceClassGuid,
        &Globals->FsctlNetDeviceObject);
    if (!NT_SUCCESS(Result))
        goto exit;
    Result = FspDeviceCreate(FspFsmupDeviceExtensionKind, 0,
        FILE_DEVICE_NETWORK_FILE_SYSTEM, FILE_REMOTE_DEVICE,
        &Globals->FsmupDeviceObject);
    if (!NT_SUCCESS(Result))
        goto exit;

#if DBG
    /*
     * Fix GitHub issue #177. All credit for the investigation of this issue
     * and the suggested steps to reproduce and work around the problem goes
     * to GitHub user @thinkport.
     *
     * On debug builds set DO_LOW_PRIORITY_FILESYSTEM to place the file system
     * at the end of the file system list during IoRegisterFileSystem below.
     * This allows us to test the behavior of our Fsvrt devices when foreign
     * file systems attempt to use them for mounting.
     */
    SetFlag(Globals->FsctlDiskDeviceObject->Flags, DO_LOW_PRIORITY_FILESYSTEM);
#endif

    Result = FspDeviceInitialize(Globals->FsctlDiskDeviceObject);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspDeviceInitialize(Globals->FsctlNetDeviceObject);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspDeviceInitialize(Globals->FsmupDeviceObject);
    ASSERT(STATUS_SUCCESS == Result);

    FspSiloGetContainerId(&Guid);
    RtlInitEmptyUnicodeString(&DeviceName,
        Globals->FsmupDeviceNameBuf, sizeof Globals->FsmupDeviceNameBuf);
    Result = RtlUnicodeStringPrintf(&DeviceName,
        0 == ((PULONG)&Guid)[0] && 0 == ((PULONG)&Guid)[1] &&
        0 == ((PULONG)&Guid)[2] && 0 == ((PULONG)&Guid)[3] ?
            L"\\Device\\" FSP_FSCTL_MUP_DEVICE_NAME :
            L"\\Device\\" FSP_FSCTL_MUP_DEVICE_NAME "{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        Guid.Data1, Guid.Data2, Guid.Data3,
        Guid.Data4[0], Guid.Data4[1], Guid.Data4[2], Guid.Data4[3],
        Guid.Data4[4], Guid.Data4[5], Guid.Data4[6], Guid.Data4[7]);
    ASSERT(NT_SUCCESS(Result));
    DeviceName.MaximumLength = DeviceName.Length;
    Result = FsRtlRegisterUncProviderEx(&Globals->MupHandle,
        &DeviceName, Globals->FsmupDeviceObject, 0);
    if (!NT_SUCCESS(Result))
    {
        Globals->MupHandle = 0;
        goto exit;
    }

    /*
     * Register our "disk" device as a file system. We do not register our "net" device
     * as a file system; we register with the MUP instead.
     */
    IoRegisterFileSystem(Globals->FsctlDiskDeviceObject);

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        if (0 != Globals->MupHandle)
        {
            FsRtlDeregisterUncProvider(Globals->MupHandle);
            Globals->MupHandle = 0;
        }
        if (0 != Globals->FsmupDeviceObject)
        {
            FspDeviceDelete(Globals->FsmupDeviceObject);
            Globals->FsmupDeviceObject = 0;
        }
        if (0 != Globals->FsctlNetDeviceObject)
        {
            FspDeviceDelete(Globals->FsctlNetDeviceObject);
            Globals->FsctlNetDeviceObject = 0;
        }
        if (0 != Globals->FsctlDiskDeviceObject)
        {
            FspDeviceDelete(Globals->FsctlDiskDeviceObject);
            Globals->FsctlDiskDeviceObject = 0;
        }
    }

    FspSiloDereferenceGlobals(Globals);

    return Result;
}

static VOID FspDriverFinalizeDevices(VOID)
{
    PAGED_CODE();

    FSP_SILO_GLOBALS *Globals;

    FspSiloGetGlobals(&Globals);
    ASSERT(0 != Globals);

    IoUnregisterFileSystem(Globals->FsctlDiskDeviceObject);

    if (0 != Globals->MupHandle)
    {
        FsRtlDeregisterUncProvider(Globals->MupHandle);
        Globals->MupHandle = 0;
    }
    if (0 != Globals->FsmupDeviceObject)
    {
        FspDeviceDelete(Globals->FsmupDeviceObject);
        Globals->FsmupDeviceObject = 0;
    }
    if (0 != Globals->FsctlNetDeviceObject)
    {
        FspDeviceDelete(Globals->FsctlNetDeviceObject);
        Globals->FsctlNetDeviceObject = 0;
    }
    if (0 != Globals->FsctlDiskDeviceObject)
    {
        FspDeviceDelete(Globals->FsctlDiskDeviceObject);
        Globals->FsctlDiskDeviceObject = 0;
    }

    FspSiloDereferenceGlobals(Globals);
}

PDRIVER_OBJECT FspDriverObject;
FAST_IO_DISPATCH FspFastIoDispatch;
CACHE_MANAGER_CALLBACKS FspCacheManagerCallbacks;

ULONG FspProcessorCount;
FSP_MV_CcCoherencyFlushAndPurgeCache *FspMvCcCoherencyFlushAndPurgeCache;
ULONG FspMvMdlMappingNoWrite = 0;
BOOLEAN FspHasReparsePointCaseSensitivityFix = FALSE;
