/**
 * @file sys/driver.c
 *
 * @copyright 2015-2022 Bill Zissimopoulos
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
static DRIVER_UNLOAD DriverUnload;
static VOID FspDriverMultiVersionInitialize(VOID);
static NTSTATUS FspDriverInitializeDevices(VOID);
static VOID FspDriverFinalizeDevices(VOID);
static VOID FspDriverFinalizeDevicesForUnload(VOID);
static VOID FspDriverFinalizeDevicesEx(BOOLEAN DeleteDevices);
NTSTATUS FspDriverUnload(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DriverUnload)
#pragma alloc_text(INIT, FspDriverMultiVersionInitialize)
#pragma alloc_text(PAGE, FspDriverInitializeDevices)
#pragma alloc_text(PAGE, FspDriverFinalizeDevices)
#pragma alloc_text(PAGE, FspDriverFinalizeDevicesForUnload)
#pragma alloc_text(PAGE, FspDriverFinalizeDevicesEx)
#pragma alloc_text(PAGE, FspDriverUnload)
#endif

NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    FSP_ENTER_DRV();

    FSP_TRACE_INIT();

    FspSxsIdentInitialize(&DriverObject->DriverName);

    /* setup the driver object */
    DriverObject->DriverUnload = DriverUnload;
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
    FspFastIoDispatch.FastIoRead = FspFastIoRead;
    FspFastIoDispatch.FastIoWrite = FspFastIoWrite;
    FspFastIoDispatch.FastIoQueryBasicInfo = FspFastIoQueryBasicInfo;
    FspFastIoDispatch.FastIoQueryStandardInfo = FspFastIoQueryStandardInfo;
    //FspFastIoDispatch.FastIoLock = 0;
    //FspFastIoDispatch.FastIoUnlockSingle = 0;
    //FspFastIoDispatch.FastIoUnlockAll = 0;
    //FspFastIoDispatch.FastIoUnlockAllByKey = 0;
    FspFastIoDispatch.FastIoDeviceControl = FspFastIoDeviceControl;
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

    BOOLEAN InitDoneSilo = FALSE, InitDonePsBuf = FALSE,
        InitDoneTimers = FALSE, InitDoneDevices = FALSE;

    FspDriverObject = DriverObject;
    FspDriverMultiVersionInitialize();

    ExInitializeFastMutex(&FspDriverUnloadMutex);
    ExInitializeFastMutex(&FspDeviceGlobalMutex);

    Result = FspSiloInitialize(FspDriverInitializeDevices, FspDriverFinalizeDevices);
    if (!NT_SUCCESS(Result))
        goto exit;
    InitDoneSilo = TRUE;

    Result = FspProcessBufferInitialize();
    if (!NT_SUCCESS(Result))
        goto exit;
    InitDonePsBuf = TRUE;

    Result = FspDeviceInitializeAllTimers();
    if (!NT_SUCCESS(Result))
        goto exit;
    InitDoneTimers = TRUE;

    Result = FspDriverInitializeDevices();
    if (!NT_SUCCESS(Result))
        goto exit;
    InitDoneDevices = TRUE;

    Result = FspSiloPostInitialize();
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        if (InitDoneDevices)
            FspDriverFinalizeDevices();
        if (InitDoneTimers)
            FspDeviceFinalizeAllTimers();
        if (InitDonePsBuf)
            FspProcessBufferFinalize();
        if (InitDoneSilo)
            FspSiloFinalize();

        FSP_TRACE_FINI();
    }

#pragma prefast(suppress:28175, "We are in DriverEntry: ok to access DriverName")
    FSP_LEAVE_DRV("DriverName=\"%wZ\", RegistryPath=\"%wZ\"",
        &DriverObject->DriverName, RegistryPath);
}

VOID DriverUnload(
    PDRIVER_OBJECT DriverObject)
{
    FSP_ENTER_VOID(PAGED_CODE());

    FspDriverFinalizeDevices();

    FspDeviceFinalizeAllTimers();

    FspProcessBufferFinalize();

    FspSiloFinalize();

    FSP_TRACE_FINI();

#pragma prefast(suppress:28175, "We are in DriverUnload: ok to access DriverName")
    FSP_LEAVE_VOID("DriverName=\"%wZ\"",
        &DriverObject->DriverName);
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
    WCHAR DeviceNameBuf[128];
    UNICODE_STRING SymlinkName;
    GUID Guid;
    NTSTATUS Result;

    FspSiloGetGlobals(&Globals);
    ASSERT(0 != Globals);

    /* create the file system control device objects */
    RtlInitUnicodeString(&DeviceSddl, L"" FSP_FSCTL_DEVICE_SDDL);
    RtlInitEmptyUnicodeString(&DeviceName, DeviceNameBuf, sizeof DeviceNameBuf);
    Result = RtlUnicodeStringPrintf(&DeviceName,
        L"\\Device\\" FSP_FSCTL_DISK_DEVICE_NAME "%wZ",
        FspSxsSuffix());
    ASSERT(NT_SUCCESS(Result));
    Result = FspDeviceCreateSecure(FspFsctlDeviceExtensionKind, 0,
        &DeviceName, FILE_DEVICE_DISK_FILE_SYSTEM, FILE_DEVICE_SECURE_OPEN,
        &DeviceSddl, &FspFsctlDeviceClassGuid,
        &Globals->FsctlDiskDeviceObject);
    if (!NT_SUCCESS(Result))
        goto exit;
    if (0 != FspSxsIdent()->Length)
    {
        /* \Device\WinFsp.Disk SxS symlink */
        RtlInitUnicodeString(&SymlinkName, L"\\Device\\" FSP_FSCTL_DISK_DEVICE_NAME);
        Result = IoCreateSymbolicLink(&SymlinkName, &DeviceName);
        if (!NT_SUCCESS(Result))
            goto exit;
        Globals->InitDoneSymlinkDisk = 1;
    }
    RtlInitEmptyUnicodeString(&DeviceName, DeviceNameBuf, sizeof DeviceNameBuf);
    Result = RtlUnicodeStringPrintf(&DeviceName,
        L"\\Device\\" FSP_FSCTL_NET_DEVICE_NAME "%wZ",
        FspSxsSuffix());
    ASSERT(NT_SUCCESS(Result));
    Result = FspDeviceCreateSecure(FspFsctlDeviceExtensionKind, 0,
        &DeviceName, FILE_DEVICE_NETWORK_FILE_SYSTEM, FILE_DEVICE_SECURE_OPEN,
        &DeviceSddl, &FspFsctlDeviceClassGuid,
        &Globals->FsctlNetDeviceObject);
    if (!NT_SUCCESS(Result))
        goto exit;
    if (0 != FspSxsIdent()->Length)
    {
        /* \Device\WinFsp.Net SxS symlink */
        RtlInitUnicodeString(&SymlinkName, L"\\Device\\" FSP_FSCTL_NET_DEVICE_NAME);
        Result = IoCreateSymbolicLink(&SymlinkName, &DeviceName);
        if (!NT_SUCCESS(Result))
            goto exit;
        Globals->InitDoneSymlinkNet = 1;
    }
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
            L"\\Device\\" FSP_FSCTL_MUP_DEVICE_NAME "%wZ":
            L"\\Device\\" FSP_FSCTL_MUP_DEVICE_NAME "%wZ{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        FspSxsSuffix(),
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
    Globals->InitDoneRegisterDisk = 1;

    /*
     * Reference primary device objects to allow for IoDeleteDevice during FspDriverUnload.
     */
    ObReferenceObject(Globals->FsctlDiskDeviceObject);
    ObReferenceObject(Globals->FsctlNetDeviceObject);
    ObReferenceObject(Globals->FsmupDeviceObject);

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        if (Globals->InitDoneRegisterDisk)
        {
            IoUnregisterFileSystem(Globals->FsctlDiskDeviceObject);
            Globals->InitDoneRegisterDisk = 0;
        }
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
        if (Globals->InitDoneSymlinkNet)
        {
            RtlInitUnicodeString(&SymlinkName, L"\\Device\\" FSP_FSCTL_NET_DEVICE_NAME);
            IoDeleteSymbolicLink(&SymlinkName);
            Globals->InitDoneSymlinkNet = 0;
        }
        if (0 != Globals->FsctlNetDeviceObject)
        {
            FspDeviceDelete(Globals->FsctlNetDeviceObject);
            Globals->FsctlNetDeviceObject = 0;
        }
        if (Globals->InitDoneSymlinkDisk)
        {
            RtlInitUnicodeString(&SymlinkName, L"\\Device\\" FSP_FSCTL_DISK_DEVICE_NAME);
            IoDeleteSymbolicLink(&SymlinkName);
            Globals->InitDoneSymlinkDisk = 0;
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

    FspDriverFinalizeDevicesEx(TRUE);
}

static VOID FspDriverFinalizeDevicesForUnload(VOID)
{
    PAGED_CODE();

    FspDriverFinalizeDevicesEx(FALSE);
}

static VOID FspDriverFinalizeDevicesEx(BOOLEAN DeleteDevices)
{
    PAGED_CODE();

    FSP_SILO_GLOBALS *Globals;
    UNICODE_STRING SymlinkName;

    FspSiloGetGlobals(&Globals);
    ASSERT(0 != Globals);

    if (Globals->InitDoneRegisterDisk)
    {
        IoUnregisterFileSystem(Globals->FsctlDiskDeviceObject);
        Globals->InitDoneRegisterDisk = 0;
    }
    if (0 != Globals->MupHandle)
    {
        FsRtlDeregisterUncProvider(Globals->MupHandle);
        Globals->MupHandle = 0;
    }
    if (0 != Globals->FsmupDeviceObject)
    {
        if (DeleteDevices)
        {
            FspDeviceDelete(Globals->FsmupDeviceObject);
            ObDereferenceObject(Globals->FsmupDeviceObject);
            Globals->FsmupDeviceObject = 0;
        }
        else
            FspDeviceDoIoDeleteDevice(Globals->FsmupDeviceObject);
    }
    if (Globals->InitDoneSymlinkNet)
    {
        RtlInitUnicodeString(&SymlinkName, L"\\Device\\" FSP_FSCTL_NET_DEVICE_NAME);
        IoDeleteSymbolicLink(&SymlinkName);
        Globals->InitDoneSymlinkNet = 0;
    }
    if (0 != Globals->FsctlNetDeviceObject)
    {
        if (DeleteDevices)
        {
            FspDeviceDelete(Globals->FsctlNetDeviceObject);
            ObDereferenceObject(Globals->FsctlNetDeviceObject);
            Globals->FsctlNetDeviceObject = 0;
        }
        else
            FspDeviceDoIoDeleteDevice(Globals->FsctlNetDeviceObject);
    }
    if (Globals->InitDoneSymlinkDisk)
    {
        RtlInitUnicodeString(&SymlinkName, L"\\Device\\" FSP_FSCTL_DISK_DEVICE_NAME);
        IoDeleteSymbolicLink(&SymlinkName);
        Globals->InitDoneSymlinkDisk = 0;
    }
    if (0 != Globals->FsctlDiskDeviceObject)
    {
        if (DeleteDevices)
        {
            FspDeviceDelete(Globals->FsctlDiskDeviceObject);
            ObDereferenceObject(Globals->FsctlDiskDeviceObject);
            Globals->FsctlDiskDeviceObject = 0;
        }
        else
            FspDeviceDoIoDeleteDevice(Globals->FsctlDiskDeviceObject);
    }

    FspSiloDereferenceGlobals(Globals);
}

NTSTATUS FspDriverUnload(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction);
    ASSERT(FSP_FSCTL_UNLOAD == IrpSp->Parameters.FileSystemControl.FsControlCode);

    NTSTATUS Result;
    UNICODE_STRING DriverServiceName, DriverName, Remain;
    WCHAR DriverServiceNameBuf[64 + 256];
    PDEVICE_OBJECT *DeviceObjects = 0;
    ULONG DeviceObjectCount = 0;

    if (!FspSiloIsHost())
        return STATUS_INVALID_DEVICE_REQUEST;

    if (!SeSinglePrivilegeCheck(RtlConvertLongToLuid(SE_LOAD_DRIVER_PRIVILEGE), UserMode))
        return STATUS_PRIVILEGE_NOT_HELD;

    ExAcquireFastMutexUnsafe(&FspDriverUnloadMutex);

    if (!FspDriverUnloadDone)
    {
        FspFileNameSuffix(&FspDriverObject->DriverName, &Remain, &DriverName);
        RtlInitEmptyUnicodeString(&DriverServiceName, DriverServiceNameBuf, sizeof DriverServiceNameBuf);
        Result = RtlUnicodeStringPrintf(&DriverServiceName,
            L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\%wZ", &DriverName);
        if (!NT_SUCCESS(Result))
            goto exit;
        Result = ZwUnloadDriver(&DriverServiceName);
        if (!NT_SUCCESS(Result))
            goto exit;

        FspSiloEnumerate(FspDriverFinalizeDevicesForUnload);
        FspDriverFinalizeDevicesForUnload();

        Result = FspDeviceCopyList(&DeviceObjects, &DeviceObjectCount);
        if (NT_SUCCESS(Result))
        {
            for (ULONG I = 0; DeviceObjectCount > I; I++)
            {
                FSP_DEVICE_EXTENSION *DeviceExtension = FspDeviceExtension(DeviceObjects[I]);
                if (FspFsvolDeviceExtensionKind == DeviceExtension->Kind)
                    FspIoqStop(((FSP_FSVOL_DEVICE_EXTENSION *)DeviceExtension)->Ioq, FALSE);
            }
            FspDeviceDeleteList(DeviceObjects, DeviceObjectCount);
        }

        FspDriverUnloadDone = TRUE;
    }

    Result = STATUS_SUCCESS;

exit:
    ExReleaseFastMutexUnsafe(&FspDriverUnloadMutex);

    return Result;
}

PDRIVER_OBJECT FspDriverObject;
FAST_IO_DISPATCH FspFastIoDispatch;
CACHE_MANAGER_CALLBACKS FspCacheManagerCallbacks;
FAST_MUTEX FspDriverUnloadMutex;
BOOLEAN FspDriverUnloadDone;

ULONG FspProcessorCount;
FSP_MV_CcCoherencyFlushAndPurgeCache *FspMvCcCoherencyFlushAndPurgeCache;
ULONG FspMvMdlMappingNoWrite = 0;
BOOLEAN FspHasReparsePointCaseSensitivityFix = FALSE;
