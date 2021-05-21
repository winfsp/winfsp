/**
 * @file sys/mountdev.c
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

NTSTATUS FspMountdevQueryDeviceName(
    PDEVICE_OBJECT FsvrtDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspMountdevQueryUniqueId(
    PDEVICE_OBJECT FsvrtDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
BOOLEAN FspMountdevDeviceControl(
    PDEVICE_OBJECT FsvrtDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    PNTSTATUS PResult);
NTSTATUS FspMountdevMake(
    PDEVICE_OBJECT FsvrtDeviceObject, PDEVICE_OBJECT FsvolDeviceObject,
    BOOLEAN Persistent);
VOID FspMountdevFini(
    PDEVICE_OBJECT FsvrtDeviceObject);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspMountdevQueryDeviceName)
#pragma alloc_text(PAGE, FspMountdevQueryUniqueId)
#pragma alloc_text(PAGE, FspMountdevDeviceControl)
#pragma alloc_text(PAGE, FspMountdevMake)
#pragma alloc_text(PAGE, FspMountdevFini)
#endif

NTSTATUS FspMountdevQueryDeviceName(
    PDEVICE_OBJECT FsvrtDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension = FspFsvrtDeviceExtension(FsvrtDeviceObject);
    ULONG OutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
    PMOUNTDEV_NAME OutputBuffer = Irp->AssociatedIrp.SystemBuffer;

    if (sizeof(MOUNTDEV_NAME) > OutputBufferLength)
        /* NOTE: Windows storage samples also set: Irp->IoStatus.Information = sizeof(MOUNTDEV_NAME) */
        return STATUS_BUFFER_TOO_SMALL;

    RtlZeroMemory(OutputBuffer, sizeof(MOUNTDEV_NAME));
    OutputBuffer->NameLength = FsvrtDeviceExtension->VolumeName.Length;

    Irp->IoStatus.Information =
        FIELD_OFFSET(MOUNTDEV_NAME, Name) + OutputBuffer->NameLength;
    if (Irp->IoStatus.Information > OutputBufferLength)
    {
        Irp->IoStatus.Information = sizeof(MOUNTDEV_NAME);
        return STATUS_BUFFER_OVERFLOW;
    }

    RtlCopyMemory(OutputBuffer->Name,
        FsvrtDeviceExtension->VolumeName.Buffer, OutputBuffer->NameLength);

    return STATUS_SUCCESS;
}

NTSTATUS FspMountdevQueryUniqueId(
    PDEVICE_OBJECT FsvrtDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension = FspFsvrtDeviceExtension(FsvrtDeviceObject);
    ULONG OutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
    PMOUNTDEV_UNIQUE_ID OutputBuffer = Irp->AssociatedIrp.SystemBuffer;

    if (sizeof(MOUNTDEV_UNIQUE_ID) > OutputBufferLength)
        /* NOTE: Windows storage samples also set: Irp->IoStatus.Information = sizeof(MOUNTDEV_UNIQUE_ID) */
        return STATUS_BUFFER_TOO_SMALL;

    RtlZeroMemory(OutputBuffer, sizeof(MOUNTDEV_UNIQUE_ID));
    OutputBuffer->UniqueIdLength = sizeof FsvrtDeviceExtension->UniqueId;

    Irp->IoStatus.Information =
        FIELD_OFFSET(MOUNTDEV_UNIQUE_ID, UniqueId) + OutputBuffer->UniqueIdLength;
    if (Irp->IoStatus.Information > OutputBufferLength)
    {
        Irp->IoStatus.Information = sizeof(MOUNTDEV_UNIQUE_ID);
        return STATUS_BUFFER_OVERFLOW;
    }

    RtlCopyMemory(OutputBuffer->UniqueId,
        &FsvrtDeviceExtension->UniqueId, OutputBuffer->UniqueIdLength);

    return STATUS_SUCCESS;
}

BOOLEAN FspMountdevDeviceControl(
    PDEVICE_OBJECT FsvrtDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    PNTSTATUS PResult)
{
    PAGED_CODE();

    if (0 != FsvrtDeviceObject)
    {
        FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension = FspFsvrtDeviceExtension(FsvrtDeviceObject);
        if (0 != InterlockedCompareExchange(&FsvrtDeviceExtension->IsMountdev, 0, 0))
        {
            switch (IrpSp->Parameters.DeviceIoControl.IoControlCode)
            {
            case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME:
                *PResult = FspMountdevQueryDeviceName(FsvrtDeviceObject, Irp, IrpSp);
                return TRUE;
            case IOCTL_MOUNTDEV_QUERY_UNIQUE_ID:
                *PResult = FspMountdevQueryUniqueId(FsvrtDeviceObject, Irp, IrpSp);
                return TRUE;
            }
        }
    }

    return FALSE;
}

NTSTATUS FspMountdevMake(
    PDEVICE_OBJECT FsvrtDeviceObject, PDEVICE_OBJECT FsvolDeviceObject,
    BOOLEAN Persistent)
{
    /*
     * This function converts the fsvrt device into a mountdev device that
     * responds to MountManager IOCTL's. This allows the fsvrt device to
     * be mounted using the MountManager.
     *
     * This function requires protection against concurrency. In general this
     * is achieved by acquiring the GlobalDeviceLock.
     */

    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension = FspFsvrtDeviceExtension(FsvrtDeviceObject);
    UNICODE_STRING String;
    WCHAR StringBuf[FSP_FSCTL_VOLUME_FSNAME_SIZE / sizeof(WCHAR) + 18];
    GUID Guid;
    NTSTATUS Result;

    ASSERT(FsvolDeviceExtension->FsvrtDeviceObject == FsvrtDeviceObject);

    if (0 != InterlockedCompareExchange(&FsvrtDeviceExtension->IsMountdev, 0, 0))
        return Persistent == FsvrtDeviceExtension->Persistent ?
            STATUS_TOO_LATE : STATUS_ACCESS_DENIED;

    FsvrtDeviceExtension->Persistent = Persistent;

    if (Persistent)
    {
        /* make UUID v5 from the fsvrt device GUID and a unique string derived from VolumeParams */
        RtlInitEmptyUnicodeString(&String, StringBuf, sizeof StringBuf);
        Result = RtlUnicodeStringPrintf(&String,
            L"%s:%08lx:%08lx",
            FsvolDeviceExtension->VolumeParams.FileSystemName,
            FsvolDeviceExtension->VolumeParams.VolumeSerialNumber,
            FsvolDeviceExtension->VolumeParams.VolumeCreationTime);
        ASSERT(NT_SUCCESS(Result));
        Result = FspUuid5Make(&FspFsvrtDeviceClassGuid, String.Buffer, String.Length, &Guid);
    }
    else
        /* create volume guid */
        Result = FspCreateGuid(&Guid);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* initialize the fsvrt device extension */
    RtlCopyMemory(&FsvrtDeviceExtension->UniqueId, &Guid, sizeof Guid);
    RtlInitEmptyUnicodeString(&FsvrtDeviceExtension->VolumeName,
        FsvrtDeviceExtension->VolumeNameBuf, sizeof FsvrtDeviceExtension->VolumeNameBuf);
    RtlCopyUnicodeString(&FsvrtDeviceExtension->VolumeName, &FsvolDeviceExtension->VolumeName);

    /* mark the fsvrt device as initialized */
    InterlockedIncrement(&FspFsvrtDeviceExtension(FsvrtDeviceObject)->IsMountdev);

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

VOID FspMountdevFini(
    PDEVICE_OBJECT FsvrtDeviceObject)
{
    PAGED_CODE();

    FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension = FspFsvrtDeviceExtension(FsvrtDeviceObject);
    PVOID Buffer = 0;
    ULONG Length = 4096;
    MOUNTMGR_MOUNT_POINT *MountPoint;
    NTSTATUS Result;

    if (0 == InterlockedCompareExchange(&FsvrtDeviceExtension->IsMountdev, 0, 0))
        return;

    if (FsvrtDeviceExtension->Persistent)
        /* if the mountdev is marked as persistent do not purge the MountManager */
        return;

    Buffer = FspAllocNonPaged(Length);
    if (0 == Buffer)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    MountPoint = Buffer;
    RtlZeroMemory(MountPoint, sizeof *MountPoint);
    MountPoint->UniqueIdOffset = sizeof(MOUNTMGR_MOUNT_POINT);
    MountPoint->UniqueIdLength = sizeof FsvrtDeviceExtension->UniqueId;
    RtlCopyMemory((PUINT8)MountPoint + MountPoint->UniqueIdOffset,
        &FsvrtDeviceExtension->UniqueId, MountPoint->UniqueIdLength);

    Result = FspSendMountmgrDeviceControlIrp(IOCTL_MOUNTMGR_DELETE_POINTS,
        Buffer, MountPoint->UniqueIdOffset + MountPoint->UniqueIdLength, &Length);

exit:
    if (0 != Buffer)
        FspFree(Buffer);
}
