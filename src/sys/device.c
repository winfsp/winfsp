/**
 * @file sys/device.c
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

NTSTATUS FspDeviceCreateSecure(UINT32 Kind, ULONG ExtraSize,
    PUNICODE_STRING DeviceName, DEVICE_TYPE DeviceType, ULONG DeviceCharacteristics,
    PUNICODE_STRING DeviceSddl, LPCGUID DeviceClassGuid,
    PDEVICE_OBJECT *PDeviceObject);
NTSTATUS FspDeviceCreate(UINT32 Kind, ULONG ExtraSize,
    DEVICE_TYPE DeviceType, ULONG DeviceCharacteristics,
    PDEVICE_OBJECT *PDeviceObject);
NTSTATUS FspDeviceInitialize(PDEVICE_OBJECT DeviceObject);
VOID FspDeviceDelete(PDEVICE_OBJECT DeviceObject);
BOOLEAN FspDeviceReference(PDEVICE_OBJECT DeviceObject);
VOID FspDeviceDereference(PDEVICE_OBJECT DeviceObject);
_IRQL_requires_(DISPATCH_LEVEL)
static BOOLEAN FspDeviceReferenceAtDpcLevel(PDEVICE_OBJECT DeviceObject);
_IRQL_requires_(DISPATCH_LEVEL)
static VOID FspDeviceDereferenceFromDpcLevel(PDEVICE_OBJECT DeviceObject);
static NTSTATUS FspFsvolDeviceInit(PDEVICE_OBJECT DeviceObject);
static VOID FspFsvolDeviceFini(PDEVICE_OBJECT DeviceObject);
static IO_TIMER_ROUTINE FspFsvolDeviceTimerRoutine;
static WORKER_THREAD_ROUTINE FspFsvolDeviceExpirationRoutine;
VOID FspFsvolDeviceFileRenameAcquireShared(PDEVICE_OBJECT DeviceObject);
BOOLEAN FspFsvolDeviceFileRenameTryAcquireShared(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceFileRenameAcquireExclusive(PDEVICE_OBJECT DeviceObject);
BOOLEAN FspFsvolDeviceFileRenameTryAcquireExclusive(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceFileRenameSetOwner(PDEVICE_OBJECT DeviceObject, PVOID Owner);
VOID FspFsvolDeviceFileRenameRelease(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceFileRenameReleaseOwner(PDEVICE_OBJECT DeviceObject, PVOID Owner);
BOOLEAN FspFsvolDeviceFileRenameIsAcquiredExclusive(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceLockContextTable(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceUnlockContextTable(PDEVICE_OBJECT DeviceObject);
NTSTATUS FspFsvolDeviceCopyContextList(PDEVICE_OBJECT DeviceObject,
    PVOID **PContexts, PULONG PContextCount);
NTSTATUS FspFsvolDeviceCopyContextByNameList(PDEVICE_OBJECT DeviceObject,
    PVOID **PContexts, PULONG PContextCount);
VOID FspFsvolDeviceDeleteContextList(PVOID *Contexts, ULONG ContextCount);
PVOID FspFsvolDeviceEnumerateContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName,
    BOOLEAN NextFlag, FSP_DEVICE_CONTEXT_BY_NAME_TABLE_RESTART_KEY *RestartKey);
PVOID FspFsvolDeviceLookupContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName);
PVOID FspFsvolDeviceInsertContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName, PVOID Context,
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT *ElementStorage, PBOOLEAN PInserted);
VOID FspFsvolDeviceDeleteContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName,
    PBOOLEAN PDeleted);
static RTL_AVL_COMPARE_ROUTINE FspFsvolDeviceCompareContextByName;
static RTL_AVL_ALLOCATE_ROUTINE FspFsvolDeviceAllocateContextByName;
static RTL_AVL_FREE_ROUTINE FspFsvolDeviceFreeContextByName;
VOID FspFsvolDeviceGetVolumeInfo(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_INFO *VolumeInfo);
BOOLEAN FspFsvolDeviceTryGetVolumeInfo(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_INFO *VolumeInfo);
VOID FspFsvolDeviceSetVolumeInfo(PDEVICE_OBJECT DeviceObject, const FSP_FSCTL_VOLUME_INFO *VolumeInfo);
VOID FspFsvolDeviceInvalidateVolumeInfo(PDEVICE_OBJECT DeviceObject);
static NTSTATUS FspFsmupDeviceInit(PDEVICE_OBJECT DeviceObject);
static VOID FspFsmupDeviceFini(PDEVICE_OBJECT DeviceObject);
NTSTATUS FspDeviceCopyList(
    PDEVICE_OBJECT **PDeviceObjects, PULONG PDeviceObjectCount);
VOID FspDeviceDeleteList(
    PDEVICE_OBJECT *DeviceObjects, ULONG DeviceObjectCount);
VOID FspDeviceDeleteAll(VOID);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspDeviceCreateSecure)
#pragma alloc_text(PAGE, FspDeviceCreate)
#pragma alloc_text(PAGE, FspDeviceInitialize)
#pragma alloc_text(PAGE, FspDeviceDelete)
#pragma alloc_text(PAGE, FspFsvolDeviceInit)
#pragma alloc_text(PAGE, FspFsvolDeviceFini)
#pragma alloc_text(PAGE, FspFsvolDeviceFileRenameAcquireShared)
#pragma alloc_text(PAGE, FspFsvolDeviceFileRenameTryAcquireShared)
#pragma alloc_text(PAGE, FspFsvolDeviceFileRenameAcquireExclusive)
#pragma alloc_text(PAGE, FspFsvolDeviceFileRenameTryAcquireExclusive)
#pragma alloc_text(PAGE, FspFsvolDeviceFileRenameSetOwner)
#pragma alloc_text(PAGE, FspFsvolDeviceFileRenameRelease)
#pragma alloc_text(PAGE, FspFsvolDeviceFileRenameReleaseOwner)
#pragma alloc_text(PAGE, FspFsvolDeviceFileRenameIsAcquiredExclusive)
#pragma alloc_text(PAGE, FspFsvolDeviceLockContextTable)
#pragma alloc_text(PAGE, FspFsvolDeviceUnlockContextTable)
#pragma alloc_text(PAGE, FspFsvolDeviceCopyContextList)
#pragma alloc_text(PAGE, FspFsvolDeviceCopyContextByNameList)
#pragma alloc_text(PAGE, FspFsvolDeviceDeleteContextList)
#pragma alloc_text(PAGE, FspFsvolDeviceEnumerateContextByName)
#pragma alloc_text(PAGE, FspFsvolDeviceLookupContextByName)
#pragma alloc_text(PAGE, FspFsvolDeviceInsertContextByName)
#pragma alloc_text(PAGE, FspFsvolDeviceDeleteContextByName)
#pragma alloc_text(PAGE, FspFsvolDeviceCompareContextByName)
#pragma alloc_text(PAGE, FspFsvolDeviceAllocateContextByName)
#pragma alloc_text(PAGE, FspFsvolDeviceFreeContextByName)
#pragma alloc_text(PAGE, FspFsmupDeviceInit)
#pragma alloc_text(PAGE, FspFsmupDeviceFini)
#pragma alloc_text(PAGE, FspDeviceCopyList)
#pragma alloc_text(PAGE, FspDeviceDeleteList)
#pragma alloc_text(PAGE, FspDeviceDeleteAll)
#endif

NTSTATUS FspDeviceCreateSecure(UINT32 Kind, ULONG ExtraSize,
    PUNICODE_STRING DeviceName, DEVICE_TYPE DeviceType, ULONG DeviceCharacteristics,
    PUNICODE_STRING DeviceSddl, LPCGUID DeviceClassGuid,
    PDEVICE_OBJECT *PDeviceObject)
{
    PAGED_CODE();

    NTSTATUS Result;
    ULONG DeviceExtensionSize;
    PDEVICE_OBJECT DeviceObject;
    FSP_DEVICE_EXTENSION *DeviceExtension;

    *PDeviceObject = 0;

    switch (Kind)
    {
    case FspFsvolDeviceExtensionKind:
        DeviceExtensionSize = sizeof(FSP_FSVOL_DEVICE_EXTENSION);
        break;
    case FspFsvrtDeviceExtensionKind:
        DeviceExtensionSize = sizeof(FSP_FSVRT_DEVICE_EXTENSION);
        break;
    case FspFsmupDeviceExtensionKind:
        DeviceExtensionSize = sizeof(FSP_FSMUP_DEVICE_EXTENSION);
        break;
    case FspFsctlDeviceExtensionKind:
        DeviceExtensionSize = sizeof(FSP_DEVICE_EXTENSION);
        break;
    default:
        ASSERT(0);
        return STATUS_INVALID_PARAMETER;
    }

    if (0 != DeviceSddl)
        Result = IoCreateDeviceSecure(FspDriverObject,
            DeviceExtensionSize + ExtraSize, DeviceName, DeviceType,
            DeviceCharacteristics, FALSE,
            DeviceSddl, DeviceClassGuid,
            &DeviceObject);
    else
        Result = IoCreateDevice(FspDriverObject,
            DeviceExtensionSize + ExtraSize, DeviceName, DeviceType,
            DeviceCharacteristics, FALSE,
            &DeviceObject);
    if (!NT_SUCCESS(Result))
        return Result;

    DeviceExtension = FspDeviceExtension(DeviceObject);
    KeInitializeSpinLock(&DeviceExtension->SpinLock);
    DeviceExtension->RefCount = 1;
    DeviceExtension->Kind = Kind;

    *PDeviceObject = DeviceObject;

    return Result;
}

NTSTATUS FspDeviceCreate(UINT32 Kind, ULONG ExtraSize,
    DEVICE_TYPE DeviceType, ULONG DeviceCharacteristics,
    PDEVICE_OBJECT *PDeviceObject)
{
    PAGED_CODE();

    return FspDeviceCreateSecure(Kind, ExtraSize, 0, DeviceType, DeviceCharacteristics,
        0, 0, PDeviceObject);
}

NTSTATUS FspDeviceInitialize(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_DEVICE_EXTENSION *DeviceExtension = FspDeviceExtension(DeviceObject);

    switch (DeviceExtension->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        Result = FspFsvolDeviceInit(DeviceObject);
        break;
    case FspFsvrtDeviceExtensionKind:
        Result = STATUS_SUCCESS;
        break;
    case FspFsmupDeviceExtensionKind:
        Result = FspFsmupDeviceInit(DeviceObject);
        break;
    case FspFsctlDeviceExtensionKind:
        Result = STATUS_SUCCESS;
        break;
    default:
        ASSERT(0);
        return STATUS_INVALID_PARAMETER;
    }

    if (NT_SUCCESS(Result))
        ClearFlag(DeviceObject->Flags, DO_DEVICE_INITIALIZING);

    return Result;
}

VOID FspDeviceDelete(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_DEVICE_EXTENSION *DeviceExtension = FspDeviceExtension(DeviceObject);

    switch (DeviceExtension->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FspFsvolDeviceFini(DeviceObject);
        break;
    case FspFsvrtDeviceExtensionKind:
        break;
    case FspFsmupDeviceExtensionKind:
        FspFsmupDeviceFini(DeviceObject);
        break;
    case FspFsctlDeviceExtensionKind:
        break;
    default:
        ASSERT(0);
        return;
    }

#if DBG
#pragma prefast(suppress:28175, "Debugging only: ok to access DeviceObject->Size")
    RtlFillMemory(&DeviceExtension->Kind,
        (PUINT8)DeviceObject + DeviceObject->Size - (PUINT8)&DeviceExtension->Kind, 0xBD);
#endif

    IoDeleteDevice(DeviceObject);
}

BOOLEAN FspDeviceReference(PDEVICE_OBJECT DeviceObject)
{
    // !PAGED_CODE();

    BOOLEAN Result;
    FSP_DEVICE_EXTENSION *DeviceExtension;
    KIRQL Irql;

    DeviceExtension = FspDeviceExtension(DeviceObject);
    KeAcquireSpinLock(&DeviceExtension->SpinLock, &Irql);
    Result = 0 != DeviceExtension->RefCount;
    if (Result)
        DeviceExtension->RefCount++;
    KeReleaseSpinLock(&DeviceExtension->SpinLock, Irql);

    return Result;
}

VOID FspDeviceDereference(PDEVICE_OBJECT DeviceObject)
{
    // !PAGED_CODE();

    BOOLEAN Delete = FALSE;
    FSP_DEVICE_EXTENSION *DeviceExtension;
    KIRQL Irql;

    DeviceExtension = FspDeviceExtension(DeviceObject);
    KeAcquireSpinLock(&DeviceExtension->SpinLock, &Irql);
    if (0 != DeviceExtension->RefCount)
    {
        DeviceExtension->RefCount--;
        Delete = 0 == DeviceExtension->RefCount;
    }
    KeReleaseSpinLock(&DeviceExtension->SpinLock, Irql);

    if (Delete)
        FspDeviceDelete(DeviceObject);
}

_IRQL_requires_(DISPATCH_LEVEL)
static BOOLEAN FspDeviceReferenceAtDpcLevel(PDEVICE_OBJECT DeviceObject)
{
    // !PAGED_CODE();

    BOOLEAN Result;
    FSP_DEVICE_EXTENSION *DeviceExtension;

    DeviceExtension = FspDeviceExtension(DeviceObject);
    KeAcquireSpinLockAtDpcLevel(&DeviceExtension->SpinLock);
    Result = 0 != DeviceExtension->RefCount;
    if (Result)
        DeviceExtension->RefCount++;
    KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

    return Result;
}

_IRQL_requires_(DISPATCH_LEVEL)
static VOID FspDeviceDereferenceFromDpcLevel(PDEVICE_OBJECT DeviceObject)
{
    // !PAGED_CODE();

    BOOLEAN Delete = FALSE;
    FSP_DEVICE_EXTENSION *DeviceExtension;

    DeviceExtension = FspDeviceExtension(DeviceObject);
    KeAcquireSpinLockAtDpcLevel(&DeviceExtension->SpinLock);
    if (0 != DeviceExtension->RefCount)
    {
        DeviceExtension->RefCount--;
        Delete = 0 == DeviceExtension->RefCount;
    }
    KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

    ASSERT(!Delete);
}

static NTSTATUS FspFsvolDeviceInit(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    LARGE_INTEGER IrpTimeout;
    LARGE_INTEGER SecurityTimeout, DirInfoTimeout, StreamInfoTimeout, EaTimeout;

    /*
     * Volume device initialization is a mess, because of the different ways of
     * creating/initializing different resources. So we will use some bits just
     * to track what has been initialized!
     */

    /* initialize any fsext provider */
    if (0 != FsvolDeviceExtension->VolumeParams.FsextControlCode)
    {
        FSP_FSEXT_PROVIDER *Provider = FspFsextProvider(
            FsvolDeviceExtension->VolumeParams.FsextControlCode, 0);
        if (0 != Provider)
        {
            Result = Provider->DeviceInit(DeviceObject, &FsvolDeviceExtension->VolumeParams);
            if (!NT_SUCCESS(Result))
                return Result;
            FsvolDeviceExtension->Provider = Provider;
            FsvolDeviceExtension->InitDoneFsext = 1;
        }
        else
            return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    /* is there a virtual disk? */
    if (0 != FsvolDeviceExtension->FsvrtDeviceObject)
    {
        /* allocate a spare VPB so that we can be mounted on the virtual disk */
        FsvolDeviceExtension->SwapVpb = FspAllocNonPagedExternal(sizeof *FsvolDeviceExtension->SwapVpb);
        if (0 == FsvolDeviceExtension->SwapVpb)
            return STATUS_INSUFFICIENT_RESOURCES;
        RtlZeroMemory(FsvolDeviceExtension->SwapVpb, sizeof *FsvolDeviceExtension->SwapVpb);

        /* reference the virtual disk device so that it will not go away while we are using it */
        ObReferenceObject(FsvolDeviceExtension->FsvrtDeviceObject);
        FsvolDeviceExtension->InitDoneFsvrt = 1;
    }

    /* create our Ioq */
    IrpTimeout.QuadPart = FsvolDeviceExtension->VolumeParams.IrpTimeout * 10000ULL;
        /* convert millis to nanos */
    Result = FspIoqCreate(
        FsvolDeviceExtension->VolumeParams.IrpCapacity, &IrpTimeout, FspIopCompleteCanceledIrp,
        &FsvolDeviceExtension->Ioq);
    if (!NT_SUCCESS(Result))
        return Result;
    FsvolDeviceExtension->InitDoneIoq = 1;

    /* create our security meta cache */
    SecurityTimeout.QuadPart = FspTimeoutFromMillis(FsvolDeviceExtension->VolumeParams.SecurityTimeout);
        /* convert millis to nanos */
    Result = FspMetaCacheCreate(
        FspFsvolDeviceSecurityCacheCapacity, FspFsvolDeviceSecurityCacheItemSizeMax, &SecurityTimeout,
        &FsvolDeviceExtension->SecurityCache);
    if (!NT_SUCCESS(Result))
        return Result;
    FsvolDeviceExtension->InitDoneSec = 1;

    /* create our directory meta cache */
    DirInfoTimeout.QuadPart = FspTimeoutFromMillis(FsvolDeviceExtension->VolumeParams.DirInfoTimeout);
        /* convert millis to nanos */
    Result = FspMetaCacheCreate(
        FspFsvolDeviceDirInfoCacheCapacity, FspFsvolDeviceDirInfoCacheItemSizeMax, &DirInfoTimeout,
        &FsvolDeviceExtension->DirInfoCache);
    if (!NT_SUCCESS(Result))
        return Result;
    FsvolDeviceExtension->InitDoneDir = 1;

    /* create our stream info meta cache */
    StreamInfoTimeout.QuadPart = FspTimeoutFromMillis(FsvolDeviceExtension->VolumeParams.StreamInfoTimeout);
        /* convert millis to nanos */
    Result = FspMetaCacheCreate(
        FspFsvolDeviceStreamInfoCacheCapacity, FspFsvolDeviceStreamInfoCacheItemSizeMax, &StreamInfoTimeout,
        &FsvolDeviceExtension->StreamInfoCache);
    if (!NT_SUCCESS(Result))
        return Result;
    FsvolDeviceExtension->InitDoneStrm = 1;

    /* create our EA meta cache */
    EaTimeout.QuadPart = FspTimeoutFromMillis(FsvolDeviceExtension->VolumeParams.EaTimeout);
        /* convert millis to nanos */
    Result = FspMetaCacheCreate(
        FspFsvolDeviceEaCacheCapacity, FspFsvolDeviceEaCacheItemSizeMax, &EaTimeout,
        &FsvolDeviceExtension->EaCache);
    if (!NT_SUCCESS(Result))
        return Result;
    FsvolDeviceExtension->InitDoneEa = 1;

    /* initialize the Volume Notify and FSRTL Notify mechanisms */
    Result = FspNotifyInitializeSync(&FsvolDeviceExtension->NotifySync);
    if (!NT_SUCCESS(Result))
        return Result;
    FspWgroupInitialize(&FsvolDeviceExtension->VolumeNotifyWgroup);
    InitializeListHead(&FsvolDeviceExtension->NotifyList);
    FsvolDeviceExtension->InitDoneNotify = 1;

    /* create file system statistics */
    Result = FspStatisticsCreate(&FsvolDeviceExtension->Statistics);
    if (!NT_SUCCESS(Result))
        return Result;
    FsvolDeviceExtension->InitDoneStat = 1;

    /* initialize our context table */
    ExInitializeResourceLite(&FsvolDeviceExtension->FileRenameResource);
    ExInitializeResourceLite(&FsvolDeviceExtension->ContextTableResource);
    InitializeListHead(&FsvolDeviceExtension->ContextList);
    RtlInitializeGenericTableAvl(&FsvolDeviceExtension->ContextByNameTable,
        FspFsvolDeviceCompareContextByName,
        FspFsvolDeviceAllocateContextByName,
        FspFsvolDeviceFreeContextByName,
        0);
    FsvolDeviceExtension->InitDoneCtxTab = 1;

    /* initialize our timer routine and start our expiration timer */
#pragma prefast(suppress:28133, "We are a filesystem: we do not have AddDevice")
    Result = IoInitializeTimer(DeviceObject, FspFsvolDeviceTimerRoutine, 0);
    if (!NT_SUCCESS(Result))
        return Result;
    KeInitializeSpinLock(&FsvolDeviceExtension->ExpirationLock);
    ExInitializeWorkItem(&FsvolDeviceExtension->ExpirationWorkItem,
        FspFsvolDeviceExpirationRoutine, DeviceObject);
    IoStartTimer(DeviceObject);
    FsvolDeviceExtension->InitDoneTimer = 1;

    /* initialize the volume information */
    KeInitializeSpinLock(&FsvolDeviceExtension->InfoSpinLock);
    FsvolDeviceExtension->InitDoneInfo = 1;

    return STATUS_SUCCESS;
}

static VOID FspFsvolDeviceFini(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    /*
     * First things first: stop our timer.
     *
     * Our IoTimer routine will NOT be called again after IoStopTimer() returns.
     * However a work item may be in flight. For this reason our IoTimer routine
     * references our DeviceObject before queueing work items.
     */
    if (FsvolDeviceExtension->InitDoneTimer)
        IoStopTimer(DeviceObject);

    /* delete the file system statistics */
    if (FsvolDeviceExtension->InitDoneStat)
        FspStatisticsDelete(FsvolDeviceExtension->Statistics);

    /* uninitialize the Volume Notify and FSRTL Notify mechanisms */
    if (FsvolDeviceExtension->InitDoneNotify)
    {
        FspNotifyCleanupAll(
            FsvolDeviceExtension->NotifySync, &FsvolDeviceExtension->NotifyList);
        FspNotifyUninitializeSync(&FsvolDeviceExtension->NotifySync);
    }

    /* delete the EA meta cache */
    if (FsvolDeviceExtension->InitDoneEa)
        FspMetaCacheDelete(FsvolDeviceExtension->EaCache);

    /* delete the stream info meta cache */
    if (FsvolDeviceExtension->InitDoneStrm)
        FspMetaCacheDelete(FsvolDeviceExtension->StreamInfoCache);

    /* delete the directory meta cache */
    if (FsvolDeviceExtension->InitDoneDir)
        FspMetaCacheDelete(FsvolDeviceExtension->DirInfoCache);

    /* delete the security meta cache */
    if (FsvolDeviceExtension->InitDoneSec)
        FspMetaCacheDelete(FsvolDeviceExtension->SecurityCache);

    /* delete the Ioq */
    if (FsvolDeviceExtension->InitDoneIoq)
        FspIoqDelete(FsvolDeviceExtension->Ioq);

    if (FsvolDeviceExtension->InitDoneCtxTab)
    {
        /*
         * FspDeviceFreeContext/FspDeviceFreeContextByName is a no-op, so it is not necessary
         * to enumerate and delete all entries in the ContextTable.
         */

        ExDeleteResourceLite(&FsvolDeviceExtension->ContextTableResource);
        ExDeleteResourceLite(&FsvolDeviceExtension->FileRenameResource);
    }

    /* is there a virtual disk? */
    if (FsvolDeviceExtension->InitDoneFsvrt)
    {
        /* dereference the virtual volume device so that it can now go away */
        ObDereferenceObject(FsvolDeviceExtension->FsvrtDeviceObject);

        /* free the spare VPB if we still have it */
        if (0 != FsvolDeviceExtension->SwapVpb)
            FspFreeExternal(FsvolDeviceExtension->SwapVpb);
    }

    /* finalize any fsext provider */
    if (FsvolDeviceExtension->InitDoneFsext)
    {
        if (0 != FsvolDeviceExtension->Provider)
            FsvolDeviceExtension->Provider->DeviceFini(DeviceObject);
    }
}

static VOID FspFsvolDeviceTimerRoutine(PDEVICE_OBJECT DeviceObject, PVOID Context)
{
    // !PAGED_CODE();

    /*
     * This routine runs at DPC level. Reference our DeviceObject and queue a work item
     * so that we can do our processing at Passive level. Only do so if the work item
     * is not already in flight (otherwise we could requeue the same work item).
     */

    ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);
    
    if (!FspDeviceReferenceAtDpcLevel(DeviceObject))
        return;

    BOOLEAN ExpirationInProgress;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    KeAcquireSpinLockAtDpcLevel(&FsvolDeviceExtension->ExpirationLock);
    ExpirationInProgress = FsvolDeviceExtension->ExpirationInProgress;
    if (!ExpirationInProgress)
    {
        FsvolDeviceExtension->ExpirationInProgress = TRUE;
        ExQueueWorkItem(&FsvolDeviceExtension->ExpirationWorkItem, DelayedWorkQueue);
    }
    KeReleaseSpinLockFromDpcLevel(&FsvolDeviceExtension->ExpirationLock);

    if (ExpirationInProgress)
        FspDeviceDereferenceFromDpcLevel(DeviceObject);
}

static VOID FspFsvolDeviceExpirationRoutine(PVOID Context)
{
    // !PAGED_CODE();

    PDEVICE_OBJECT DeviceObject = Context;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    UINT64 InterruptTime;
    KIRQL Irql;

    InterruptTime = KeQueryInterruptTime();
    FspMetaCacheInvalidateExpired(FsvolDeviceExtension->SecurityCache, InterruptTime);
    FspMetaCacheInvalidateExpired(FsvolDeviceExtension->DirInfoCache, InterruptTime);
    FspMetaCacheInvalidateExpired(FsvolDeviceExtension->StreamInfoCache, InterruptTime);
    /* run any fsext provider expiration routine */
    if (0 != FsvolDeviceExtension->Provider)
        FsvolDeviceExtension->Provider->DeviceExpirationRoutine(DeviceObject, InterruptTime);
    FspIoqRemoveExpired(FsvolDeviceExtension->Ioq, InterruptTime);

    KeAcquireSpinLock(&FsvolDeviceExtension->ExpirationLock, &Irql);
    FsvolDeviceExtension->ExpirationInProgress = FALSE;
    KeReleaseSpinLock(&FsvolDeviceExtension->ExpirationLock, Irql);

    FspDeviceDereference(DeviceObject);
}

VOID FspFsvolDeviceFileRenameAcquireShared(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    ExAcquireResourceSharedLite(&FsvolDeviceExtension->FileRenameResource, TRUE);
}

BOOLEAN FspFsvolDeviceFileRenameTryAcquireShared(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    return ExAcquireResourceSharedLite(&FsvolDeviceExtension->FileRenameResource, FALSE);
}

VOID FspFsvolDeviceFileRenameAcquireExclusive(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    ExAcquireResourceExclusiveLite(&FsvolDeviceExtension->FileRenameResource, TRUE);
}

BOOLEAN FspFsvolDeviceFileRenameTryAcquireExclusive(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    return ExAcquireResourceExclusiveLite(&FsvolDeviceExtension->FileRenameResource, FALSE);
}

VOID FspFsvolDeviceFileRenameSetOwner(PDEVICE_OBJECT DeviceObject, PVOID Owner)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    Owner = (PVOID)((UINT_PTR)Owner | 3);

    ExSetResourceOwnerPointer(&FsvolDeviceExtension->FileRenameResource, Owner);
}

VOID FspFsvolDeviceFileRenameRelease(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    ExReleaseResourceLite(&FsvolDeviceExtension->FileRenameResource);
}

VOID FspFsvolDeviceFileRenameReleaseOwner(PDEVICE_OBJECT DeviceObject, PVOID Owner)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    Owner = (PVOID)((UINT_PTR)Owner | 3);

    if (ExIsResourceAcquiredLite(&FsvolDeviceExtension->FileRenameResource))
        ExReleaseResourceLite(&FsvolDeviceExtension->FileRenameResource);
    else
        ExReleaseResourceForThreadLite(&FsvolDeviceExtension->FileRenameResource, (ERESOURCE_THREAD)Owner);
}

BOOLEAN FspFsvolDeviceFileRenameIsAcquiredExclusive(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    return ExIsResourceAcquiredExclusiveLite(&FsvolDeviceExtension->FileRenameResource);
}

VOID FspFsvolDeviceLockContextTable(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    ExAcquireResourceExclusiveLite(&FsvolDeviceExtension->ContextTableResource, TRUE);
}

VOID FspFsvolDeviceUnlockContextTable(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    ExReleaseResourceLite(&FsvolDeviceExtension->ContextTableResource);
}

NTSTATUS FspFsvolDeviceCopyContextList(PDEVICE_OBJECT DeviceObject,
    PVOID **PContexts, PULONG PContextCount)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    PVOID *Contexts;
    ULONG ContextCount, Index;

    *PContexts = 0;
    *PContextCount = 0;

    ContextCount = 0;
    for (
        PLIST_ENTRY Head = &FsvolDeviceExtension->ContextList, Entry = Head->Flink;
        Head != Entry;
        Entry = Entry->Flink)
    {
        ContextCount++;
    }

    /* if ContextCount == 0 allocate an empty Context list */
    Contexts = FspAlloc(sizeof(PVOID) * (0 != ContextCount ? ContextCount : 1));
    if (0 == Contexts)
        return STATUS_INSUFFICIENT_RESOURCES;

    Index = 0;
    for (
        PLIST_ENTRY Head = &FsvolDeviceExtension->ContextList, Entry = Head->Flink;
        Head != Entry;
        Entry = Entry->Flink)
    {
        ASSERT(Index < ContextCount);
        Contexts[Index++] = CONTAINING_RECORD(Entry, FSP_FILE_NODE, ActiveEntry);
            /* assume that Contexts can only be FSP_FILE_NODE's */
    }
    ASSERT(Index == ContextCount);

    *PContexts = Contexts;
    *PContextCount = Index;

    return STATUS_SUCCESS;
}

NTSTATUS FspFsvolDeviceCopyContextByNameList(PDEVICE_OBJECT DeviceObject,
    PVOID **PContexts, PULONG PContextCount)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT_DATA *Data;
    PVOID *Contexts;
    ULONG ContextCount, Index;

    *PContexts = 0;
    *PContextCount = 0;

    ContextCount = RtlNumberGenericTableElementsAvl(&FsvolDeviceExtension->ContextByNameTable);

    /* if ContextCount == 0 allocate an empty Context list */
    Contexts = FspAlloc(sizeof(PVOID) * (0 != ContextCount ? ContextCount : 1));
    if (0 == Contexts)
        return STATUS_INSUFFICIENT_RESOURCES;

    Index = 0;
    Data = RtlEnumerateGenericTableAvl(&FsvolDeviceExtension->ContextByNameTable, TRUE);
    while (Index < ContextCount && 0 != Data)
    {
        Contexts[Index++] = Data->Context;
        Data = RtlEnumerateGenericTableAvl(&FsvolDeviceExtension->ContextByNameTable, FALSE);
    }

    *PContexts = Contexts;
    *PContextCount = Index;

    return STATUS_SUCCESS;
}

VOID FspFsvolDeviceDeleteContextList(PVOID *Contexts, ULONG ContextCount)
{
    PAGED_CODE();

    FspFree(Contexts);
}

PVOID FspFsvolDeviceEnumerateContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName,
    BOOLEAN NextFlag, FSP_DEVICE_CONTEXT_BY_NAME_TABLE_RESTART_KEY *RestartKey)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    BOOLEAN CaseInsensitive = 0 == FsvolDeviceExtension->VolumeParams.CaseSensitiveSearch;
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT_DATA *Result;

    if (0 != RestartKey->RestartKey)
        NextFlag = TRUE;

    Result = RtlEnumerateGenericTableLikeADirectory(&FsvolDeviceExtension->ContextByNameTable,
        0, 0, NextFlag, &RestartKey->RestartKey, &RestartKey->DeleteCount, &FileName);

    if (0 != Result &&
        FspFileNameIsPrefix(FileName, Result->FileName, CaseInsensitive, 0) &&
        (FileName->Length == Result->FileName->Length ||
            (L'\\' == Result->FileName->Buffer[FileName->Length / sizeof(WCHAR)] ||
            L':' == Result->FileName->Buffer[FileName->Length / sizeof(WCHAR)])))
        return Result->Context;
    else
        return 0;
}

PVOID FspFsvolDeviceLookupContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT_DATA *Result;

    Result = RtlLookupElementGenericTableAvl(&FsvolDeviceExtension->ContextByNameTable, &FileName);

    return 0 != Result ? Result->Context : 0;
}

PVOID FspFsvolDeviceInsertContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName, PVOID Context,
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT *ElementStorage, PBOOLEAN PInserted)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT_DATA *Result, Element = { 0 };

    ASSERT(0 != ElementStorage);
    Element.FileName = FileName;
    Element.Context = Context;

    FsvolDeviceExtension->ContextByNameTableElementStorage = ElementStorage;
    Result = RtlInsertElementGenericTableAvl(&FsvolDeviceExtension->ContextByNameTable,
        &Element, sizeof Element, PInserted);
    FsvolDeviceExtension->ContextByNameTableElementStorage = 0;

    ASSERT(0 != Result);

    return Result->Context;
}

VOID FspFsvolDeviceDeleteContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName,
    PBOOLEAN PDeleted)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    BOOLEAN Deleted;

    Deleted = RtlDeleteElementGenericTableAvl(&FsvolDeviceExtension->ContextByNameTable, &FileName);

    if (0 != PDeleted)
        *PDeleted = Deleted;
}

static RTL_GENERIC_COMPARE_RESULTS NTAPI FspFsvolDeviceCompareContextByName(
    PRTL_AVL_TABLE Table, PVOID FirstElement, PVOID SecondElement)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        CONTAINING_RECORD(Table, FSP_FSVOL_DEVICE_EXTENSION, ContextByNameTable);
    BOOLEAN CaseInsensitive = 0 == FsvolDeviceExtension->VolumeParams.CaseSensitiveSearch;
    PUNICODE_STRING FirstFileName = *(PUNICODE_STRING *)FirstElement;
    PUNICODE_STRING SecondFileName = *(PUNICODE_STRING *)SecondElement;
    LONG ComparisonResult;

    /*
     * Since FileNode FileName's are now always normalized, we could perhaps get away
     * with using CaseInsensitive == FALSE at all times. For safety reasons we avoid
     * doing so here.
     */
    ComparisonResult = FspFileNameCompare(FirstFileName, SecondFileName, CaseInsensitive, 0);

    if (0 > ComparisonResult)
        return GenericLessThan;
    else
    if (0 < ComparisonResult)
        return GenericGreaterThan;
    else
        return GenericEqual;
}

static PVOID NTAPI FspFsvolDeviceAllocateContextByName(
    PRTL_AVL_TABLE Table, CLONG ByteSize)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        CONTAINING_RECORD(Table, FSP_FSVOL_DEVICE_EXTENSION, ContextByNameTable);

    ASSERT(sizeof(FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT) == ByteSize);

    return FsvolDeviceExtension->ContextByNameTableElementStorage;
}

static VOID NTAPI FspFsvolDeviceFreeContextByName(
    PRTL_AVL_TABLE Table, PVOID Buffer)
{
    PAGED_CODE();
}

VOID FspFsvolDeviceGetVolumeInfo(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    // !PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_FSCTL_VOLUME_INFO VolumeInfoNp;
    KIRQL Irql;

    KeAcquireSpinLock(&FsvolDeviceExtension->InfoSpinLock, &Irql);
    VolumeInfoNp = FsvolDeviceExtension->VolumeInfo;
    KeReleaseSpinLock(&FsvolDeviceExtension->InfoSpinLock, Irql);

    *VolumeInfo = VolumeInfoNp;
}

#pragma warning(push)
#pragma warning(disable:4701) /* disable idiotic warning! */
BOOLEAN FspFsvolDeviceTryGetVolumeInfo(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    // !PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_FSCTL_VOLUME_INFO VolumeInfoNp;
    KIRQL Irql;
    BOOLEAN Result;

    KeAcquireSpinLock(&FsvolDeviceExtension->InfoSpinLock, &Irql);
    if (FspExpirationTimeValid(FsvolDeviceExtension->InfoExpirationTime))
    {
        VolumeInfoNp = FsvolDeviceExtension->VolumeInfo;
        Result = TRUE;
    }
    else
        Result = FALSE;
    KeReleaseSpinLock(&FsvolDeviceExtension->InfoSpinLock, Irql);

    if (Result)
        *VolumeInfo = VolumeInfoNp;

    return Result;
}
#pragma warning(pop)

VOID FspFsvolDeviceSetVolumeInfo(PDEVICE_OBJECT DeviceObject, const FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    // !PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_FSCTL_VOLUME_INFO VolumeInfoNp = *VolumeInfo;
    KIRQL Irql;

    KeAcquireSpinLock(&FsvolDeviceExtension->InfoSpinLock, &Irql);
    FsvolDeviceExtension->VolumeInfo = VolumeInfoNp;
    FsvolDeviceExtension->InfoExpirationTime = FspExpirationTimeFromMillis(
        FsvolDeviceExtension->VolumeParams.VolumeInfoTimeout);
    KeReleaseSpinLock(&FsvolDeviceExtension->InfoSpinLock, Irql);
}

VOID FspFsvolDeviceInvalidateVolumeInfo(PDEVICE_OBJECT DeviceObject)
{
    // !PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    KIRQL Irql;

    KeAcquireSpinLock(&FsvolDeviceExtension->InfoSpinLock, &Irql);
    FsvolDeviceExtension->InfoExpirationTime = 0;
    KeReleaseSpinLock(&FsvolDeviceExtension->InfoSpinLock, Irql);
}

static NTSTATUS FspFsmupDeviceInit(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSMUP_DEVICE_EXTENSION *FsmupDeviceExtension = FspFsmupDeviceExtension(DeviceObject);

    /* initialize our prefix table */
    ExInitializeResourceLite(&FsmupDeviceExtension->PrefixTableResource);
    RtlInitializeUnicodePrefix(&FsmupDeviceExtension->PrefixTable);
    RtlInitializeUnicodePrefix(&FsmupDeviceExtension->ClassTable);
    FsmupDeviceExtension->InitDonePfxTab = 1;

    return STATUS_SUCCESS;
}

static VOID FspFsmupDeviceFini(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSMUP_DEVICE_EXTENSION *FsmupDeviceExtension = FspFsmupDeviceExtension(DeviceObject);

    if (FsmupDeviceExtension->InitDonePfxTab)
    {
        /*
         * Normally we would have to finalize our prefix table. This is not necessary as all
         * prefixes will be gone if this code ever gets reached.
         */
        ASSERT(0 == RtlNextUnicodePrefix(&FsmupDeviceExtension->PrefixTable, TRUE));
        ASSERT(0 == RtlNextUnicodePrefix(&FsmupDeviceExtension->ClassTable, TRUE));
        ExDeleteResourceLite(&FsmupDeviceExtension->PrefixTableResource);
    }
}

NTSTATUS FspDeviceCopyList(
    PDEVICE_OBJECT **PDeviceObjects, PULONG PDeviceObjectCount)
{
    PAGED_CODE();

    PDEVICE_OBJECT *DeviceObjects = 0;
    ULONG DeviceObjectCount = 0;

    while (STATUS_BUFFER_TOO_SMALL == IoEnumerateDeviceObjectList(FspDriverObject,
        DeviceObjects, sizeof *DeviceObjects * DeviceObjectCount, &DeviceObjectCount))
    {
        if (0 != DeviceObjects)
            FspFree(DeviceObjects);
        DeviceObjects = FspAllocNonPaged(sizeof *DeviceObjects * DeviceObjectCount);
        if (0 == DeviceObjects)
            return STATUS_INSUFFICIENT_RESOURCES;
        RtlZeroMemory(DeviceObjects, sizeof *DeviceObjects * DeviceObjectCount);
    }

    *PDeviceObjects = DeviceObjects;
    *PDeviceObjectCount = DeviceObjectCount;

    return STATUS_SUCCESS;
}

VOID FspDeviceDeleteList(
    PDEVICE_OBJECT *DeviceObjects, ULONG DeviceObjectCount)
{
    PAGED_CODE();

    for (ULONG i = 0; DeviceObjectCount > i; i++)
        ObDereferenceObject(DeviceObjects[i]);

    FspFree(DeviceObjects);
}

VOID FspDeviceDeleteAll(VOID)
{
    PAGED_CODE();

    NTSTATUS Result;
    PDEVICE_OBJECT *DeviceObjects = 0;
    ULONG DeviceObjectCount = 0;

    Result = FspDeviceCopyList(&DeviceObjects, &DeviceObjectCount);
    if (!NT_SUCCESS(Result))
        return;

    for (ULONG i = 0; DeviceObjectCount > i; i++)
        FspDeviceDelete(DeviceObjects[i]);

    FspDeviceDeleteList(DeviceObjects, DeviceObjectCount);
}

ERESOURCE FspDeviceGlobalResource;
