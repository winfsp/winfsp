/**
 * @file sys/volume.c
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

NTSTATUS FspVolumeCreate(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspVolumeCreateNoLock(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    FSP_SILO_GLOBALS *Globals);
VOID FspVolumeDelete(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static VOID FspVolumeDeleteNoLock(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    FSP_SILO_GLOBALS *Globals);
static WORKER_THREAD_ROUTINE FspVolumeDeleteDelayed;
NTSTATUS FspVolumeMount(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspVolumeMountNoLock(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeMakeMountdev(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeGetName(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeGetNameList(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspVolumeGetNameListNoLock(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeTransact(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeTransactFsext(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeStop(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeNotify(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspVolumeNotifyLock(
    PDEVICE_OBJECT FsvolDeviceObject);
static WORKER_THREAD_ROUTINE FspVolumeNotifyWork;
NTSTATUS FspVolumeWork(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspVolumeCreate)
#pragma alloc_text(PAGE, FspVolumeCreateNoLock)
// ! #pragma alloc_text(PAGE, FspVolumeDelete)
// ! #pragma alloc_text(PAGE, FspVolumeDeleteNoLock)
// ! #pragma alloc_text(PAGE, FspVolumeDeleteDelayed)
// ! #pragma alloc_text(PAGE, FspVolumeMount)
// ! #pragma alloc_text(PAGE, FspVolumeMountNoLock)
#pragma alloc_text(PAGE, FspVolumeMakeMountdev)
#pragma alloc_text(PAGE, FspVolumeGetName)
#pragma alloc_text(PAGE, FspVolumeGetNameList)
#pragma alloc_text(PAGE, FspVolumeGetNameListNoLock)
#pragma alloc_text(PAGE, FspVolumeTransact)
#pragma alloc_text(PAGE, FspVolumeTransactFsext)
#pragma alloc_text(PAGE, FspVolumeStop)
#pragma alloc_text(PAGE, FspVolumeNotify)
#pragma alloc_text(PAGE, FspVolumeNotifyLock)
#pragma alloc_text(PAGE, FspVolumeNotifyWork)
#pragma alloc_text(PAGE, FspVolumeWork)
#endif

#define PREFIXW                         L"" FSP_FSCTL_VOLUME_PARAMS_PREFIX
#define PREFIXW_SIZE                    (sizeof PREFIXW - sizeof(WCHAR))

NTSTATUS FspVolumeCreate(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    FSP_SILO_GLOBALS *Globals;
    NTSTATUS Result;

    FspSiloGetGlobals(&Globals);
    ASSERT(0 != Globals);

    FspDeviceGlobalLock();
    Result = FspVolumeCreateNoLock(FsctlDeviceObject, Irp, IrpSp,
        Globals);
    FspDeviceGlobalUnlock();

    FspSiloDereferenceGlobals(Globals);

    return Result;
}

static NTSTATUS FspVolumeCreateNoLock(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    FSP_SILO_GLOBALS *Globals)
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
    UNICODE_STRING FsmupDeviceName;
    WCHAR VolumeNameBuf[FSP_FSCTL_VOLUME_NAME_SIZE / sizeof(WCHAR)];
    FSP_FSEXT_PROVIDER *Provider = 0;
    PDEVICE_OBJECT FsvolDeviceObject;
    PDEVICE_OBJECT FsvrtDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension;

    /* check parameters */
    if (PREFIXW_SIZE + sizeof(FSP_FSCTL_VOLUME_PARAMS_V0) * sizeof(WCHAR) > FileObject->FileName.Length)
        return STATUS_INVALID_PARAMETER;

    /* copy the VolumeParams */
    for (USHORT Index = 0, Length = sizeof(FSP_FSCTL_VOLUME_PARAMS); Length > Index; Index++)
    {
        if (PREFIXW_SIZE / sizeof(WCHAR) + Index >= FileObject->FileName.Length / sizeof(WCHAR))
            break;

        WCHAR Value = FileObject->FileName.Buffer[PREFIXW_SIZE / sizeof(WCHAR) + Index];
        if (0xF000 != (Value & 0xFF00))
            return STATUS_INVALID_PARAMETER;
        ((PUINT8)&VolumeParams)[Index] = Value & 0xFF;
    }

    /* check VolumeParams size */
    if (0 != VolumeParams.Version &&
        PREFIXW_SIZE + VolumeParams.Version * sizeof(WCHAR) != FileObject->FileName.Length)
        return STATUS_INVALID_PARAMETER;

    /* check the VolumeParams */
    if (0 == VolumeParams.SectorSize)
        VolumeParams.SectorSize = 512;
    if (0 == VolumeParams.SectorsPerAllocationUnit)
        VolumeParams.SectorsPerAllocationUnit = 1;
    if (0 == VolumeParams.MaxComponentLength)
        VolumeParams.MaxComponentLength = 255;
    if (0 == VolumeParams.TransactTimeout)
        VolumeParams.TransactTimeout = 24 * 60 * 60 * 1000; /* 1 day */
    else if (FspFsctlTransactTimeoutMinimum > VolumeParams.TransactTimeout ||
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
    if (sizeof(FSP_FSCTL_VOLUME_PARAMS_V0) >= VolumeParams.Version)
    {
        VolumeParams.VolumeInfoTimeout = VolumeParams.FileInfoTimeout;
        VolumeParams.DirInfoTimeout = VolumeParams.FileInfoTimeout;
        VolumeParams.SecurityTimeout = VolumeParams.FileInfoTimeout;
        VolumeParams.StreamInfoTimeout = VolumeParams.FileInfoTimeout;
        VolumeParams.EaTimeout = VolumeParams.FileInfoTimeout;
    }
    else
    {
        if (!VolumeParams.VolumeInfoTimeoutValid)
            VolumeParams.VolumeInfoTimeout = VolumeParams.FileInfoTimeout;
        if (!VolumeParams.DirInfoTimeoutValid)
            VolumeParams.DirInfoTimeout = VolumeParams.FileInfoTimeout;
        if (!VolumeParams.SecurityTimeoutValid)
            VolumeParams.SecurityTimeout = VolumeParams.FileInfoTimeout;
        if (!VolumeParams.StreamInfoTimeoutValid)
            VolumeParams.StreamInfoTimeout = VolumeParams.FileInfoTimeout;
        if (!VolumeParams.EaTimeoutValid)
            VolumeParams.EaTimeout = VolumeParams.FileInfoTimeout;
    }
    VolumeParams.VolumeInfoTimeoutValid = 1;
    VolumeParams.DirInfoTimeoutValid = 1;
    VolumeParams.SecurityTimeoutValid = 1;
    VolumeParams.StreamInfoTimeoutValid = 1;
    VolumeParams.EaTimeoutValid = 1;
    if (FILE_DEVICE_NETWORK_FILE_SYSTEM == FsctlDeviceObject->DeviceType)
    {
        VolumeParams.Prefix[sizeof VolumeParams.Prefix / sizeof(WCHAR) - 1] = L'\0';
        for (; L'\0' != VolumeParams.Prefix[PrefixLength]; PrefixLength++)
            ;
        for (; 0 < PrefixLength && L'\\' == VolumeParams.Prefix[PrefixLength - 1]; PrefixLength--)
            ;
        VolumeParams.Prefix[PrefixLength] = L'\0';

        /* volume prefix cannot be the empty string */
        if (0 == PrefixLength)
            return STATUS_INVALID_PARAMETER;

        /* volume prefix must start with exactly one backslash */
        if (L'\\' != VolumeParams.Prefix[0] || L'\\' == VolumeParams.Prefix[1])
            return STATUS_INVALID_PARAMETER;

        /* volume prefix must have at least one other backslash */
        USHORT I;
        for (I = 1; L'\0' != VolumeParams.Prefix[I] && L'\\' != VolumeParams.Prefix[I]; I++)
            ;
        if (I == PrefixLength)
            return STATUS_INVALID_PARAMETER;
    }
    VolumeParams.FileSystemName[sizeof VolumeParams.FileSystemName / sizeof(WCHAR) - 1] = L'\0';

#if !DBG
    /*
     * In Release builds we hardcode AlwaysUseDoubleBuffering for Reads as we do not want someone
     * to use WinFsp to crash Windows.
     *
     * See http://www.osronline.com/showthread.cfm?link=282037
     */
    VolumeParams.AlwaysUseDoubleBuffering = 1;
#endif

    /* load any fsext provider */
    if (0 != VolumeParams.FsextControlCode)
    {
        Provider = FspFsextProvider(VolumeParams.FsextControlCode, &Result);
        if (0 == Provider)
            return Result;
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
    VolumeName.MaximumLength = VolumeName.Length;

    /* create the volume (and virtual disk) device(s) */
    Result = FspDeviceCreate(FspFsvolDeviceExtensionKind,
        0 == Provider ? 0 : Provider->DeviceExtensionSize,
        FsctlDeviceObject->DeviceType,
        FILE_DEVICE_DISK_FILE_SYSTEM == FsctlDeviceObject->DeviceType ? 0 : FILE_REMOTE_DEVICE,
        &FsvolDeviceObject);
    if (!NT_SUCCESS(Result))
        return Result;
    if (FILE_DEVICE_DISK_FILE_SYSTEM == FsctlDeviceObject->DeviceType)
    {
        Result = FspDeviceCreateSecure(FspFsvrtDeviceExtensionKind, 0,
            &VolumeName, FILE_DEVICE_DISK, 0,
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
    FsvolDeviceExtension->FsvolDeviceObject = FsvolDeviceObject;
    FsvolDeviceExtension->VolumeParams = VolumeParams;
    if (FILE_DEVICE_NETWORK_FILE_SYSTEM == FsctlDeviceObject->DeviceType)
        RtlInitUnicodeString(&FsvolDeviceExtension->VolumePrefix,
            FsvolDeviceExtension->VolumeParams.Prefix);
    RtlInitEmptyUnicodeString(&FsvolDeviceExtension->VolumeName,
        FsvolDeviceExtension->VolumeNameBuf, sizeof FsvolDeviceExtension->VolumeNameBuf);
    RtlCopyUnicodeString(&FsvolDeviceExtension->VolumeName, &VolumeName);
#if defined(FSP_CFG_REJECT_EARLY_IRP)
    if (!FsvolDeviceExtension->VolumeParams.RejectIrpPriorToTransact0)
        FsvolDeviceExtension->ReadyToAcceptIrp = 1;
#endif
    Result = FspDeviceInitialize(FsvolDeviceObject);
    if (NT_SUCCESS(Result))
    {
        if (0 != FsvrtDeviceObject)
        {
            FspFsvrtDeviceExtension(FsvrtDeviceObject)->SectorSize =
                FsvolDeviceExtension->VolumeParams.SectorSize;
            Result = FspDeviceInitialize(FsvrtDeviceObject);
        }
    }
    if (!NT_SUCCESS(Result))
    {
        if (0 != FsvrtDeviceObject)
            FspDeviceDereference(FsvrtDeviceObject);
        FspDeviceDereference(FsvolDeviceObject);
        return Result;
    }

    /* do we need to register with fsmup? */
    if (0 == FsvrtDeviceObject)
    {
        Result = FspMupRegister(Globals->FsmupDeviceObject, FsvolDeviceObject);
        if (!NT_SUCCESS(Result))
        {
            FspDeviceDereference(FsvolDeviceObject);
            return Result;
        }

        RtlInitUnicodeString(&FsmupDeviceName, Globals->FsmupDeviceNameBuf);
        Result = IoCreateSymbolicLink(&FsvolDeviceExtension->VolumeName, &FsmupDeviceName);
        if (!NT_SUCCESS(Result))
        {
            FspMupUnregister(Globals->FsmupDeviceObject, FsvolDeviceObject);
            FspDeviceDereference(FsvolDeviceObject);
            return Result;
        }
    }

    /* associate the new volume device with our file object */
    FileObject->FsContext2 = FsvolDeviceObject;

    Irp->IoStatus.Information = FILE_OPENED;
    return STATUS_SUCCESS;
}

VOID FspVolumeDelete(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    // !PAGED_CODE();

    FSP_SILO_GLOBALS *Globals;
    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->FileObject->FsContext2;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PDEVICE_OBJECT FsvrtDeviceObject = FsvolDeviceExtension->FsvrtDeviceObject;
    FSP_FILE_NODE **FileNodes;
    ULONG FileNodeCount, Index;
    NTSTATUS Result;

    /*
     * If we have an fsvrt that is a mountdev, finalize it now! Finalizing a mountdev
     * involves interaction with the MountManager, which tries to open our devices.
     * So if we delay this interaction and we do it during final fsvrt teardown (i.e.
     * FspDeviceDelete time) we will fail such opens with STATUS_CANCELLED, which will
     * confuse the MountManager.
     */
    if (0 != FsvrtDeviceObject)
        FspMountdevFini(FsvrtDeviceObject);

    FspDeviceReference(FsvolDeviceObject);

    FspSiloGetGlobals(&Globals);
    ASSERT(0 != Globals);

    FspDeviceGlobalLock();
    FspVolumeDeleteNoLock(FsctlDeviceObject, Irp, IrpSp,
        Globals);
    FspDeviceGlobalUnlock();

    FspSiloDereferenceGlobals(Globals);

    /*
     * Call MmForceSectionClosed on active files to ensure that Mm removes them from Standby List.
     */
    Result = FspFileNodeCopyActiveList(FsvolDeviceObject, &FileNodes, &FileNodeCount);
    if (NT_SUCCESS(Result))
    {
        for (Index = FileNodeCount - 1; FileNodeCount > Index; Index--)
            MmForceSectionClosed(&FileNodes[Index]->NonPaged->SectionObjectPointers, TRUE);

        FspFileNodeDeleteList(FileNodes, FileNodeCount);
    }

    FspDeviceDereference(FsvolDeviceObject);
}

static VOID FspVolumeDeleteNoLock(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    FSP_SILO_GLOBALS *Globals)
{
    // !PAGED_CODE();

    ASSERT(IRP_MJ_CLEANUP == IrpSp->MajorFunction);
    ASSERT(0 != IrpSp->FileObject->FsContext2);

    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->FileObject->FsContext2;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    IrpSp->FileObject->FsContext2 = 0;

    /* stop the I/O queue */
    FspIoqStop(FsvolDeviceExtension->Ioq, TRUE);

    /* do we have a virtual disk device or are we registered with fsmup? */
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
    else
    {
        IoDeleteSymbolicLink(&FsvolDeviceExtension->VolumeName);
        FspMupUnregister(Globals->FsmupDeviceObject, FsvolDeviceObject);
    }

    /* release the volume notify lock if held (so that any pending rename will abort) */
    FspWgroupSignalPermanently(&FsvolDeviceExtension->VolumeNotifyWgroup);
    if (1 == InterlockedCompareExchange(&FsvolDeviceExtension->VolumeNotifyLock, 0, 1))
        FspFsvolDeviceFileRenameReleaseOwner(FsvolDeviceObject, FsvolDeviceObject);

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
        FspDeviceGlobalLock();
        FspFreeExternal(FsvolDeviceExtension->SwapVpb);
        FsvolDeviceExtension->SwapVpb = 0;
        FspDeviceDereference(FsvolDeviceObject);
        FspDeviceGlobalUnlock();
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

    NTSTATUS Result;
    FspDeviceGlobalLock();
    Result = FspVolumeMountNoLock(FsctlDeviceObject, Irp, IrpSp);
    FspDeviceGlobalUnlock();
    return Result;
}

static NTSTATUS FspVolumeMountNoLock(
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
                        if (!FspIoqStopped(FsvolDeviceExtension->Ioq))
                        {
                            Result = STATUS_SUCCESS;
                            /* break out of the loop without FspDeviceDereference */
                            break;
                        }
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
     * has been FspDeviceReference'd.
     *
     * We will increment the VPB's ReferenceCount so that we can do a delayed delete
     * of the volume device later on.
     */
    ASSERT(0 != FsvolDeviceObject && 0 != FsvolDeviceExtension);
    IoAcquireVpbSpinLock(&Irql);
    Vpb->ReferenceCount++;
    Vpb->DeviceObject = FsvolDeviceObject;
    Vpb->SerialNumber = FsvolDeviceExtension->VolumeParams.VolumeSerialNumber;
    IoReleaseVpbSpinLock(Irql);

    /*
     * Argh! Turns out that the IrpSp->Parameters.MountVolume.DeviceObject is
     * passed to us with an extra reference, which is not removed on SUCCESS.
     * So go ahead and dereference it now!
     */
    ObDereferenceObject(FsvrtDeviceObject);

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
}

NTSTATUS FspVolumeMakeMountdev(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction);
    ASSERT(FSP_FSCTL_MOUNTDEV == IrpSp->Parameters.FileSystemControl.FsControlCode);
    ASSERT(0 != IrpSp->FileObject->FsContext2);

    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->FileObject->FsContext2;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PDEVICE_OBJECT FsvrtDeviceObject = FsvolDeviceExtension->FsvrtDeviceObject;
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    BOOLEAN Persistent = 0 < InputBufferLength ? !!*(PBOOLEAN)Irp->AssociatedIrp.SystemBuffer : FALSE;
    NTSTATUS Result;

    if (0 == FsvrtDeviceObject)
        return STATUS_INVALID_PARAMETER;
    if (sizeof(GUID) > OutputBufferLength)
        return STATUS_INVALID_PARAMETER;

    FspDeviceGlobalLock();

    Result = FspMountdevMake(FsvrtDeviceObject, FsvolDeviceObject, Persistent);
    if (!NT_SUCCESS(Result))
    {
        if (STATUS_TOO_LATE != Result)
            goto exit;
    }

    RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer,
        &FspFsvrtDeviceExtension(FsvrtDeviceObject)->UniqueId, sizeof(GUID));

    Irp->IoStatus.Information = sizeof(GUID);
    Result = STATUS_SUCCESS;

exit:
    FspDeviceGlobalUnlock();

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
        FsvolDeviceExtension->VolumeName.MaximumLength +
        FsvolDeviceExtension->VolumePrefix.MaximumLength +
        sizeof(WCHAR));

    RtlInitEmptyUnicodeString(&VolumeName, SystemBuffer, FSP_FSCTL_VOLUME_NAME_SIZEMAX);
    RtlCopyUnicodeString(&VolumeName, &FsvolDeviceExtension->VolumeName);
    if (FILE_DEVICE_NETWORK_FILE_SYSTEM == FsctlDeviceObject->DeviceType)
        RtlAppendUnicodeStringToString(&VolumeName, &FsvolDeviceExtension->VolumePrefix);
    VolumeName.Buffer[VolumeName.Length / sizeof(WCHAR)] = L'\0';

    Irp->IoStatus.Information = VolumeName.Length + sizeof(WCHAR);
    return STATUS_SUCCESS;
}

NTSTATUS FspVolumeGetNameList(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    FspDeviceGlobalLock();
    Result = FspVolumeGetNameListNoLock(FsctlDeviceObject, Irp, IrpSp);
    FspDeviceGlobalUnlock();
    return Result;
}

static NTSTATUS FspVolumeGetNameListNoLock(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction);
    ASSERT(FSP_FSCTL_VOLUME_LIST == IrpSp->Parameters.FileSystemControl.FsControlCode);

    NTSTATUS Result;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PVOID SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    PDEVICE_OBJECT *DeviceObjects = 0;
    ULONG DeviceObjectCount = 0;
    PDEVICE_OBJECT FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension;
    UNICODE_STRING VolumeList;
    USHORT Length;

    if (65535/*USHRT_MAX*/ < OutputBufferLength)
        return STATUS_INVALID_PARAMETER;

    Result = FspDeviceCopyList(&DeviceObjects, &DeviceObjectCount);
    if (!NT_SUCCESS(Result))
        return Result;

    Result = STATUS_SUCCESS;
    RtlInitEmptyUnicodeString(&VolumeList, SystemBuffer, (USHORT)OutputBufferLength);
    for (ULONG i = 0; NT_SUCCESS(Result) && DeviceObjectCount > i; i++)
        if (FspDeviceReference(DeviceObjects[i]))
        {
            if (FspFsvolDeviceExtensionKind == FspDeviceExtension(DeviceObjects[i])->Kind)
            {
                FsvolDeviceObject = DeviceObjects[i];
                FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
                if (FsvolDeviceExtension->FsctlDeviceObject == FsctlDeviceObject &&
                    !FspIoqStopped(FsvolDeviceExtension->Ioq))
                {
                    Length =
                        FsvolDeviceExtension->VolumeName.Length +
                        FsvolDeviceExtension->VolumePrefix.Length +
                        sizeof(WCHAR);

                    if (VolumeList.Length + Length <= VolumeList.MaximumLength)
                    {
                        RtlAppendUnicodeStringToString(&VolumeList, &FsvolDeviceExtension->VolumeName);
                        if (FILE_DEVICE_NETWORK_FILE_SYSTEM == FsctlDeviceObject->DeviceType)
                            RtlAppendUnicodeStringToString(&VolumeList, &FsvolDeviceExtension->VolumePrefix);
                        VolumeList.Buffer[VolumeList.Length / sizeof(WCHAR)] = L'\0';
                        VolumeList.Length += sizeof(WCHAR);
                    }
                    else
                    {
                        VolumeList.Length = 0;
                        Result = STATUS_BUFFER_TOO_SMALL;
                    }
                }
            }
            FspDeviceDereference(DeviceObjects[i]);
        }

    FspDeviceDeleteList(DeviceObjects, DeviceObjectCount);

    Irp->IoStatus.Information = VolumeList.Length;
    return Result;
}

NTSTATUS FspVolumeTransact(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction);
    ASSERT(
        FSP_FSCTL_TRANSACT == IrpSp->Parameters.FileSystemControl.FsControlCode ||
        FSP_FSCTL_TRANSACT_BATCH == IrpSp->Parameters.FileSystemControl.FsControlCode ||
        FSP_FSCTL_TRANSACT_INTERNAL == IrpSp->Parameters.FileSystemControl.FsControlCode);
    ASSERT(0 != IrpSp->FileObject->FsContext2);

    /* check parameters */
    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->FileObject->FsContext2;
    ULONG ControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PVOID InputBuffer = 0;
    PVOID OutputBuffer = 0;
    if (FSP_FSCTL_TRANSACT_INTERNAL == ControlCode)
    {
        InputBuffer = IrpSp->Parameters.FileSystemControl.Type3InputBuffer;
        if (KernelMode != Irp->RequestorMode)
            return STATUS_INVALID_DEVICE_REQUEST;
        ASSERT(0 == InputBufferLength ||
            FSP_FSCTL_DEFAULT_ALIGN_UP(sizeof(FSP_FSCTL_TRANSACT_RSP)) <= InputBufferLength);
        ASSERT(0 == OutputBufferLength ||
            sizeof(PVOID) <= OutputBufferLength);
    }
    else
    {
        InputBuffer = Irp->AssociatedIrp.SystemBuffer;
        if (0 != InputBufferLength &&
            FSP_FSCTL_DEFAULT_ALIGN_UP(sizeof(FSP_FSCTL_TRANSACT_RSP)) > InputBufferLength)
            return STATUS_INVALID_PARAMETER;
        if (0 != OutputBufferLength &&
            ((FSP_FSCTL_TRANSACT == ControlCode &&
                FSP_FSCTL_TRANSACT_BUFFER_SIZEMIN > OutputBufferLength) ||
            (FSP_FSCTL_TRANSACT_BATCH == ControlCode &&
                FSP_FSCTL_TRANSACT_BATCH_BUFFER_SIZEMIN > OutputBufferLength)))
            return STATUS_BUFFER_TOO_SMALL;
    }

    if (!FspDeviceReference(FsvolDeviceObject))
        return STATUS_CANCELLED;

#if defined(FSP_CFG_REJECT_EARLY_IRP)
    if (0 == InputBufferLength && 0 == OutputBufferLength)
        FspFsvolDeviceSetReadyToAcceptIrp(FsvolDeviceObject);
#endif

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PUINT8 BufferEnd;
    FSP_FSCTL_TRANSACT_RSP *Response, *NextResponse;
    FSP_FSCTL_TRANSACT_REQ *Request, *PendingIrpRequest;
    PVOID InternalBuffer = 0;
    PIRP ProcessIrp, PendingIrp, RetriedIrp, RepostedIrp;
    ULONG LoopCount;
    LARGE_INTEGER Timeout;
    PIRP TopLevelIrp = IoGetTopLevelIrp();

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
            DEBUGLOG("BOGUS(Kind=%d, Hint=%p)", Response->Kind, (PVOID)(UINT_PTR)Response->Hint);
            Response = NextResponse;
            continue;
        }

        ASSERT((UINT_PTR)ProcessIrp == (UINT_PTR)Response->Hint);
        ASSERT(FspIrpRequest(ProcessIrp)->Hint == Response->Hint);

        IoSetTopLevelIrp(ProcessIrp);
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
    LoopCount = FspIoqRetriedIrpCount(FsvolDeviceExtension->Ioq);
    while (0 < LoopCount--) /* upper bound on loop guarantees forward progress! */
    {
        /* get the next retried IRP, but do not go beyond the first reposted IRP! */
        RetriedIrp = FspIoqNextCompleteIrp(FsvolDeviceExtension->Ioq, RepostedIrp);
        if (0 == RetriedIrp)
            break;

        IoSetTopLevelIrp(RetriedIrp);
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
    case METHOD_NEITHER:
        OutputBuffer = Irp->UserBuffer;
        break;
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
    if (0 == (PendingIrp = FspIoqNextPendingIrp(FsvolDeviceExtension->Ioq, 0, &Timeout, Irp)))
    {
        if (FspIoqStopped(FsvolDeviceExtension->Ioq))
        {
            Result = STATUS_CANCELLED;
            goto exit;
        }

        PendingIrp = FspIoqTimeout;
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
    ASSERT(FSP_FSCTL_TRANSACT_INTERNAL == ControlCode ?
        TRUE :
        FspFsctlTransactCanProduceRequest(Request, BufferEnd));
    LoopCount = FspIoqPendingIrpCount(FsvolDeviceExtension->Ioq);
    for (;;)
    {
        PendingIrpRequest = FspIrpRequest(PendingIrp);

        IoSetTopLevelIrp(PendingIrp);
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
            if (FSP_FSCTL_TRANSACT_INTERNAL == ControlCode)
            {
                InternalBuffer = FspAllocatePoolMustSucceed(
                    PagedPool, PendingIrpRequest->Size, FSP_ALLOC_EXTERNAL_TAG);
                RtlCopyMemory(InternalBuffer, PendingIrpRequest, PendingIrpRequest->Size);
                *(PVOID *)OutputBuffer = InternalBuffer;
            }
            else
            {
                RtlCopyMemory(Request, PendingIrpRequest, PendingIrpRequest->Size);
                Request = FspFsctlTransactProduceRequest(Request, PendingIrpRequest->Size);
            }

            if (!FspIoqStartProcessingIrp(FsvolDeviceExtension->Ioq, PendingIrp))
            {
                /*
                 * This can only happen if the Ioq was stopped. Abandon everything
                 * and return STATUS_CANCELLED. Any IRP's in the Pending and Process
                 * queues of the Ioq will be cancelled during FspIoqStop(). We must
                 * also cancel the PendingIrp we have in our hands.
                 */
                ASSERT(FspIoqStopped(FsvolDeviceExtension->Ioq));
                if (0 != InternalBuffer)
                {
                    ASSERT(FSP_FSCTL_TRANSACT_INTERNAL == ControlCode);
                    *(PVOID *)OutputBuffer = 0;
                    FspFree(InternalBuffer);
                }
                FspIopCompleteCanceledIrp(PendingIrp);
                Result = STATUS_CANCELLED;
                goto exit;
            }

            /* are we doing single request or batch mode? */
            if (FSP_FSCTL_TRANSACT_INTERNAL == ControlCode)
            {
                Irp->IoStatus.Information = sizeof(PVOID);
                Result = STATUS_SUCCESS;
                goto exit;
            }
            else
            if (FSP_FSCTL_TRANSACT == ControlCode)
                break;

            /* check that we have enough space before pulling the next pending IRP off the queue */
            if (!FspFsctlTransactCanProduceRequest(Request, BufferEnd))
                break;
        }

        if (0 >= LoopCount--) /* upper bound on loop guarantees forward progress! */
            break;

        /* get the next pending IRP, but do not go beyond the first reposted IRP! */
        PendingIrp = FspIoqNextPendingIrp(FsvolDeviceExtension->Ioq, RepostedIrp, 0, Irp);
        if (0 == PendingIrp)
            break;
    }

    Irp->IoStatus.Information = (PUINT8)Request - (PUINT8)OutputBuffer;
    Result = STATUS_SUCCESS;

exit:
    IoSetTopLevelIrp(TopLevelIrp);
    FspDeviceDereference(FsvolDeviceObject);
    return Result;
}

NTSTATUS FspVolumeTransactFsext(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction);
    ASSERT(CTL_CODE(0, 0xC00, 0, 0) ==
        (IrpSp->Parameters.FileSystemControl.FsControlCode & CTL_CODE(0, 0xC00, 0, 0)));
    ASSERT(0 != IrpSp->FileObject->FsContext2);

    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->FileObject->FsContext2;
    if (!FspDeviceReference(FsvolDeviceObject))
        return STATUS_CANCELLED;

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    if (IrpSp->Parameters.FileSystemControl.FsControlCode ==
        FsvolDeviceExtension->VolumeParams.FsextControlCode)
    {
        if (0 != FsvolDeviceExtension->Provider)
            Result = FsvolDeviceExtension->Provider->DeviceTransact(FsvolDeviceObject, Irp);
    }

    FspDeviceDereference(FsvolDeviceObject);

    return Result;
}

NTSTATUS FspVolumeStop(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction);
    ASSERT(
        FSP_FSCTL_STOP == IrpSp->Parameters.FileSystemControl.FsControlCode ||
        FSP_FSCTL_STOP0 == IrpSp->Parameters.FileSystemControl.FsControlCode);
    ASSERT(0 != IrpSp->FileObject->FsContext2);

    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->FileObject->FsContext2;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);

    /* Fix GitHub issue #369
     *
     * The original WinFsp protocol for shutting down a file system was to issue
     * an FSP_FSCTL_STOP control code to the fsctl device. This would set the IOQ
     * to the "stopped" state and would also cancel all active IRP's. Cancelation
     * of IRP's would sometimes free buffers that may have still been in use by
     * the user mode file system threads; hence access violation.
     *
     * To fix this problem a new control code FSP_FSCTL_STOP0 is introduced. The
     * new file system shutdown protocol is backwards compatible with the original
     * one and works as follows:
     *
     * - First the file system process issues an FSP_FSCTL_STOP0 control code which
     * sets the IOQ to the "stopped" state but does NOT cancel IRP's.
     *
     * - Then the file system process waits for its dispatcher threads to complete
     * (see FspFileSystemStopDispatcher).
     *
     * - Finally the file system process issues an FSP_FSCTL_STOP control code
     * which stops the (already stopped) IOQ and cancels all IRP's.
     */
    FspIoqStop(FsvolDeviceExtension->Ioq,
        FSP_FSCTL_STOP == IrpSp->Parameters.FileSystemControl.FsControlCode);

    return STATUS_SUCCESS;
}

typedef struct
{
    WORK_QUEUE_ITEM WorkItem;
    PDEVICE_OBJECT FsvolDeviceObject;
    ULONG InputBufferLength;
    FSP_FSCTL_DECLSPEC_ALIGN UINT8 InputBuffer[];
} FSP_VOLUME_NOTIFY_WORK_ITEM;

NTSTATUS FspVolumeNotify(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /*
     * FspVolumeNotify processing requires multiple locks that cannot be acquired
     * synchronously or deadlocks are possible. (The reason is that FspVolumeNotify
     * may be called by the user mode file system while servicing a request that
     * has already acquired one of the required locks.)
     *
     * For this reason FspVolumeNotify does its processing asynchronously; it ships
     * its payload as a work item to a system worker thread, which will perform the
     * actual processing. See FspVolumeNotifyWork.
     */

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);
    ASSERT(IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction);
    ASSERT(FSP_FSCTL_NOTIFY == IrpSp->Parameters.FileSystemControl.FsControlCode);
    ASSERT(METHOD_NEITHER == (IrpSp->Parameters.FileSystemControl.FsControlCode & 3));
    ASSERT(0 != IrpSp->FileObject->FsContext2);

    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->FileObject->FsContext2;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PVOID InputBuffer = IrpSp->Parameters.FileSystemControl.Type3InputBuffer;
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    FSP_VOLUME_NOTIFY_WORK_ITEM *NotifyWorkItem = 0;
    NTSTATUS Result;

    if (0 == InputBufferLength)
        return FspVolumeNotifyLock(FsvolDeviceObject);

    if (!FspDeviceReference(FsvolDeviceObject))
        return STATUS_CANCELLED;

    NotifyWorkItem = FspAllocNonPaged(
        FIELD_OFFSET(FSP_VOLUME_NOTIFY_WORK_ITEM, InputBuffer) + InputBufferLength);
    if (0 == NotifyWorkItem)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto fail;
    }

    try
    {
        ProbeForRead(InputBuffer, InputBufferLength, 1);
        RtlCopyMemory(NotifyWorkItem->InputBuffer, InputBuffer, InputBufferLength);
        NotifyWorkItem->InputBufferLength = InputBufferLength;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Result = GetExceptionCode();
        Result = FsRtlIsNtstatusExpected(Result) ? STATUS_INVALID_USER_BUFFER : Result;
        goto fail;
    }

    ExInitializeWorkItem(&NotifyWorkItem->WorkItem, FspVolumeNotifyWork, NotifyWorkItem);
    NotifyWorkItem->FsvolDeviceObject = FsvolDeviceObject;

    FspWgroupIncrement(&FsvolDeviceExtension->VolumeNotifyWgroup);
    ExQueueWorkItem(&NotifyWorkItem->WorkItem, DelayedWorkQueue);

    return STATUS_SUCCESS;

fail:
    if (0 != NotifyWorkItem)
        FspFree(NotifyWorkItem);

    FspDeviceDereference(FsvolDeviceObject);

    return Result;
}

static NTSTATUS FspVolumeNotifyLock(
    PDEVICE_OBJECT FsvolDeviceObject)
{
    PAGED_CODE();

    NTSTATUS Result;

    if (!FspDeviceReference(FsvolDeviceObject))
        return STATUS_CANCELLED;

    /*
     * Acquire the rename lock shared to disallow concurrent RENAME's.
     *
     * This guards against the race where a file that we want to invalidate
     * is being concurrently renamed to a different name. Thus we may think
     * that the file is not open and not invalidate its caches, whereas the
     * file has simply changed name.
     */
    Result = STATUS_CANT_WAIT;
    if (FspFsvolDeviceFileRenameTryAcquireShared(FsvolDeviceObject))
    {
        FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);

        if (0 == InterlockedCompareExchange(&FsvolDeviceExtension->VolumeNotifyLock, 1, 0))
        {
            FspFsvolDeviceFileRenameSetOwner(FsvolDeviceObject, FsvolDeviceObject);
            Result = STATUS_SUCCESS;
        }
        else
            FspFsvolDeviceFileRenameRelease(FsvolDeviceObject);
    }

    FspDeviceDereference(FsvolDeviceObject);

    return Result;
}

static VOID FspVolumeNotifyWork(PVOID NotifyWorkItem0)
{
    PAGED_CODE();

    FsRtlEnterFileSystem();
    IoSetTopLevelIrp(0);

    FSP_VOLUME_NOTIFY_WORK_ITEM *NotifyWorkItem = NotifyWorkItem0;
    PDEVICE_OBJECT FsvolDeviceObject = NotifyWorkItem->FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    FSP_FSCTL_NOTIFY_INFO *NotifyInfo = (PVOID)NotifyWorkItem->InputBuffer;
    PUINT8 NotifyInfoEnd = (PUINT8)NotifyInfo + NotifyWorkItem->InputBufferLength;
    ULONG NotifyInfoSize;
    UNICODE_STRING FileName = { 0 }, StreamPart = { 0 }, AbsFileName = { 0 }, FullFileName = { 0 };
    ULONG StreamType = FspFileNameStreamTypeNone;
    BOOLEAN Unlock = FALSE;
    NTSTATUS Result;

    /* iterate over notify information and invalidate/notify each file */
    for (; (PUINT8)NotifyInfo + sizeof(NotifyInfo->Size) <= NotifyInfoEnd;
        NotifyInfo = (PVOID)((PUINT8)NotifyInfo + FSP_FSCTL_DEFAULT_ALIGN_UP(NotifyInfoSize)))
    {
        NotifyInfoSize = NotifyInfo->Size;

        if (sizeof(FSP_FSCTL_NOTIFY_INFO) > NotifyInfoSize)
        {
            Unlock = TRUE;
            break;
        }

        FileName.Length =
        FileName.MaximumLength = (USHORT)(NotifyInfoSize - sizeof(FSP_FSCTL_NOTIFY_INFO));
        FileName.Buffer = NotifyInfo->FileNameBuf;
        if (sizeof(WCHAR) * 2/* not empty or root */ <= FileName.Length &&
            L'\\' == FileName.Buffer[FileName.Length / sizeof(WCHAR) - 1])
            FileName.Length -= sizeof(WCHAR);

        if (!FspFileNameIsValid(&FileName, FsvolDeviceExtension->VolumeParams.MaxComponentLength,
            FsvolDeviceExtension->VolumeParams.NamedStreams ? &StreamPart : 0,
            &StreamType))
            continue;

        if (sizeof(WCHAR) <= FileName.Length && L'\\' == FileName.Buffer[0])
        {
            /* absolute file names are used as-is */

            AbsFileName = FileName;

            FspFileNodeInvalidateCachesAndNotifyChangeByName(FsvolDeviceObject,
                &FileName, NotifyInfo->Filter, NotifyInfo->Action,
                TRUE);
        }
        else if (0 != AbsFileName.Length)
        {
            /* relative file names are considered relative to the last absolute file name */

            if (0 == FullFileName.Buffer)
            {
                FullFileName.Buffer = FspAllocatePoolMustSucceed(
                    NonPagedPool, FSP_FSCTL_TRANSACT_PATH_SIZEMAX, FSP_ALLOC_INTERNAL_TAG);
                FullFileName.MaximumLength = FSP_FSCTL_TRANSACT_PATH_SIZEMAX;
            }

            FullFileName.Length = 0;
            Result = RtlAppendUnicodeStringToString(&FullFileName, &AbsFileName);
            if (NT_SUCCESS(Result))
            {
                if (sizeof(WCHAR) * 2/* not empty or root */ <= AbsFileName.Length)
                    Result = RtlAppendUnicodeToString(&FullFileName, L"\\");
            }
            if (NT_SUCCESS(Result))
                Result = RtlAppendUnicodeStringToString(&FullFileName, &FileName);

            if (NT_SUCCESS(Result))
                FspFileNodeInvalidateCachesAndNotifyChangeByName(FsvolDeviceObject,
                    &FullFileName, NotifyInfo->Filter, NotifyInfo->Action,
                    FALSE);
        }
    }

    if (0 != FullFileName.Buffer)
        FspFree(FullFileName.Buffer);

    FspFree(NotifyWorkItem);

    FspWgroupDecrement(&FsvolDeviceExtension->VolumeNotifyWgroup);
    if (Unlock)
    {
        FspWgroupWait(&FsvolDeviceExtension->VolumeNotifyWgroup, KernelMode, FALSE, 0);
        if (1 == InterlockedCompareExchange(&FsvolDeviceExtension->VolumeNotifyLock, 0, 1))
            FspFsvolDeviceFileRenameReleaseOwner(FsvolDeviceObject, FsvolDeviceObject);
    }

    FspDeviceDereference(FsvolDeviceObject);

    FsRtlExitFileSystem();
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
    FspIrpSetRequest(Irp, Request);

    /*
     * Post the IRP to our Ioq; we do this here instead of at FSP_LEAVE_MJ time,
     * so that we can disassociate the Request on failure and release ownership
     * back to the caller.
     */
    if (!FspIoqPostIrpEx(FsvolDeviceExtension->Ioq, Irp, BestEffort, &Result))
    {
        Request->Hint = 0;
        FspIrpSetRequest(Irp, 0);
    }

    DEBUGLOG("%s(Irp=%p) = %s",
        IoctlCodeSym(BestEffort ? FSP_FSCTL_WORK_BEST_EFFORT : FSP_FSCTL_WORK),
        Irp, /* referencing pointer value, which is safe despite FspIoqPostIrpEx above! */
        NtStatusSym(Result));

    return Result;
}
