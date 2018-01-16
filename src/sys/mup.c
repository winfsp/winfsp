/**
 * @file sys/mup.c
 *
 * @copyright 2015-2018 Bill Zissimopoulos
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

BOOLEAN FspMupRegister(
    PDEVICE_OBJECT FsmupDeviceObject, PDEVICE_OBJECT FsvolDeviceObject);
VOID FspMupUnregister(
    PDEVICE_OBJECT FsmupDeviceObject, PDEVICE_OBJECT FsvolDeviceObject);
NTSTATUS FspMupHandleIrp(
    PDEVICE_OBJECT FsmupDeviceObject, PIRP Irp);
static NTSTATUS FspMupRedirQueryPathEx(
    PDEVICE_OBJECT FsmupDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspMupRegister)
#pragma alloc_text(PAGE, FspMupUnregister)
#pragma alloc_text(PAGE, FspMupHandleIrp)
#pragma alloc_text(PAGE, FspMupRedirQueryPathEx)
#endif

BOOLEAN FspMupRegister(
    PDEVICE_OBJECT FsmupDeviceObject, PDEVICE_OBJECT FsvolDeviceObject)
{
    PAGED_CODE();

    BOOLEAN Result;
    FSP_FSMUP_DEVICE_EXTENSION *FsmupDeviceExtension = FspFsmupDeviceExtension(FsmupDeviceObject);
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);

    ExAcquireResourceExclusiveLite(&FsmupDeviceExtension->PrefixTableResource, TRUE);
    Result = RtlInsertUnicodePrefix(&FsmupDeviceExtension->PrefixTable,
        &FsvolDeviceExtension->VolumePrefix, &FsvolDeviceExtension->VolumePrefixEntry);
    if (Result)
        FspDeviceReference(FsvolDeviceObject);
    ExReleaseResourceLite(&FsmupDeviceExtension->PrefixTableResource);

    return Result;
}

VOID FspMupUnregister(
    PDEVICE_OBJECT FsmupDeviceObject, PDEVICE_OBJECT FsvolDeviceObject)
{
    PAGED_CODE();

    FSP_FSMUP_DEVICE_EXTENSION *FsmupDeviceExtension = FspFsmupDeviceExtension(FsmupDeviceObject);
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);

    ExAcquireResourceExclusiveLite(&FsmupDeviceExtension->PrefixTableResource, TRUE);
    RtlRemoveUnicodePrefix(&FsmupDeviceExtension->PrefixTable,
        &FsvolDeviceExtension->VolumePrefixEntry);
    FspDeviceDereference(FsvolDeviceObject);
    ExReleaseResourceLite(&FsmupDeviceExtension->PrefixTableResource);
}

NTSTATUS FspMupHandleIrp(
    PDEVICE_OBJECT FsmupDeviceObject, PIRP Irp)
{
    PAGED_CODE();

    FSP_FSMUP_DEVICE_EXTENSION *FsmupDeviceExtension = FspFsmupDeviceExtension(FsmupDeviceObject);
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PDEVICE_OBJECT FsvolDeviceObject = 0;
    PUNICODE_PREFIX_TABLE_ENTRY Entry;
    BOOLEAN DeviceDeref = FALSE;
    NTSTATUS Result;

    FsRtlEnterFileSystem();

    switch (IrpSp->MajorFunction)
    {
    case IRP_MJ_CREATE:
        /*
         * A CREATE request with an empty file name indicates that the fsmup device
         * is being opened. Check for this case and handle it.
         */
        if (0 == FileObject->FileName.Length)
        {
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = FILE_OPENED;
            IoCompleteRequest(Irp, FSP_IO_INCREMENT);
            Result = Irp->IoStatus.Status;
            goto exit;
        }

        /*
         * Every other CREATE request must be forwarded to the appropriate fsvol device.
         */

        if (0 != FileObject->RelatedFileObject)
            FileObject = FileObject->RelatedFileObject;

        ExAcquireResourceExclusiveLite(&FsmupDeviceExtension->PrefixTableResource, TRUE);
        Entry = RtlFindUnicodePrefix(&FsmupDeviceExtension->PrefixTable,
            &FileObject->FileName, 0);
        if (0 != Entry)
        {
            FsvolDeviceObject = CONTAINING_RECORD(Entry,
                FSP_FSVOL_DEVICE_EXTENSION, VolumePrefixEntry)->FsvolDeviceObject;
            FspDeviceReference(FsvolDeviceObject);
            DeviceDeref = TRUE;
        }
        ExReleaseResourceLite(&FsmupDeviceExtension->PrefixTableResource);
        break;

    case IRP_MJ_DEVICE_CONTROL:
        /*
         * A DEVICE_CONTROL request with IOCTL_REDIR_QUERY_PATH_EX must be handled
         * by the fsmup device. Check for this case and handle it.
         */
        if (IOCTL_REDIR_QUERY_PATH_EX == IrpSp->Parameters.DeviceIoControl.IoControlCode)
        {
            Irp->IoStatus.Status = FspMupRedirQueryPathEx(FsmupDeviceObject, Irp, IrpSp);
            IoCompleteRequest(Irp, FSP_IO_INCREMENT);
            Result = Irp->IoStatus.Status;
            goto exit;
        }

        /*
         * Every other DEVICE_CONTROL request must be forwarded to the appropriate fsvol device.
         */

        /* fall through! */

    default:
        /*
         * Every other request must be forwarded to the appropriate fsvol device. If there is no
         * fsvol device, then we must return the appropriate status code (see below).
         *
         * Please note that since we allow the fsmup device to be opened, we must also handle
         * CLEANUP and CLOSE requests for it.
         */

        if (0 != FileObject)
        {
            if (FspFileNodeIsValid(FileObject->FsContext))
                FsvolDeviceObject = ((FSP_FILE_NODE *)FileObject->FsContext)->FsvolDeviceObject;
            else if (0 != FileObject->FsContext2 &&
                3 == ((PDEVICE_OBJECT)FileObject->FsContext2)->Type &&
                0 != ((PDEVICE_OBJECT)FileObject->FsContext2)->DeviceExtension &&
                FspFsvolDeviceExtensionKind == FspDeviceExtension((PDEVICE_OBJECT)FileObject->FsContext2)->Kind)
                FsvolDeviceObject = (PDEVICE_OBJECT)FileObject->FsContext2;
        }
        break;
    }

    if (0 == FsvolDeviceObject)
    {
        /*
         * We were not able to find an fsvol device to forward this IRP to. We will complete
         * the IRP with an appropriate status code.
         */

        switch (IrpSp->MajorFunction)
        {
        case IRP_MJ_CLEANUP:
        case IRP_MJ_CLOSE:
            /*
             * CLEANUP and CLOSE requests ignore their status code (except for STATUS_PENDING).
             * So return STATUS_SUCCESS. This works regardless of whether this is a legitimate
             * fsmup request or an erroneous CLOSE request that we should not have seen.
             */
            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;
        case IRP_MJ_QUERY_INFORMATION:
        case IRP_MJ_SET_INFORMATION:
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            break;
        default:
            Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }

        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, FSP_IO_INCREMENT);
        Result = Irp->IoStatus.Status;
        goto exit;
    }

    ASSERT(FspFsvolDeviceExtensionKind == FspDeviceExtension(FsvolDeviceObject)->Kind);

    /*
     * Forward the IRP to the appropriate fsvol device. The fsvol device will take care
     * to complete the IRP, etc.
     */
    IoSkipCurrentIrpStackLocation(Irp);
    Result = IoCallDriver(FsvolDeviceObject, Irp);

    if (DeviceDeref)
        FspDeviceDereference(FsvolDeviceObject);

exit:
    FsRtlExitFileSystem();

    return Result;
}

static NTSTATUS FspMupRedirQueryPathEx(
    PDEVICE_OBJECT FsmupDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    ASSERT(IRP_MJ_DEVICE_CONTROL == IrpSp->MajorFunction);
    ASSERT(IOCTL_REDIR_QUERY_PATH_EX == IrpSp->Parameters.DeviceIoControl.IoControlCode);

    Irp->IoStatus.Information = 0;

    if (KernelMode != Irp->RequestorMode)
        return STATUS_INVALID_DEVICE_REQUEST;

    /* check parameters */
    ULONG InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
    QUERY_PATH_REQUEST_EX *QueryPathRequest = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;
    QUERY_PATH_RESPONSE *QueryPathResponse = Irp->UserBuffer;
    if (sizeof(QUERY_PATH_REQUEST_EX) > InputBufferLength ||
        0 == QueryPathRequest || 0 == QueryPathResponse)
        return STATUS_INVALID_PARAMETER;
    if (sizeof(QUERY_PATH_RESPONSE) > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS Result;
    FSP_FSMUP_DEVICE_EXTENSION *FsmupDeviceExtension = FspFsmupDeviceExtension(FsmupDeviceObject);
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension;
    PUNICODE_PREFIX_TABLE_ENTRY Entry;
    PDEVICE_OBJECT FsvolDeviceObject = 0;

    Result = STATUS_BAD_NETWORK_PATH;
    ExAcquireResourceExclusiveLite(&FsmupDeviceExtension->PrefixTableResource, TRUE);
    Entry = RtlFindUnicodePrefix(&FsmupDeviceExtension->PrefixTable,
        &QueryPathRequest->PathName, 0);
    if (0 != Entry)
    {
        FsvolDeviceExtension = CONTAINING_RECORD(Entry, FSP_FSVOL_DEVICE_EXTENSION, VolumePrefixEntry);
        FsvolDeviceObject = FsvolDeviceExtension->FsvolDeviceObject;
        if (!FspIoqStopped(FsvolDeviceExtension->Ioq))
        {
            if (0 < FsvolDeviceExtension->VolumePrefix.Length &&
                FspFsvolDeviceVolumePrefixInString(FsvolDeviceObject, &QueryPathRequest->PathName) &&
                (QueryPathRequest->PathName.Length == FsvolDeviceExtension->VolumePrefix.Length ||
                    '\\' == QueryPathRequest->PathName.Buffer[FsvolDeviceExtension->VolumePrefix.Length / sizeof(WCHAR)]))
            {
                QueryPathResponse->LengthAccepted = FsvolDeviceExtension->VolumePrefix.Length;
                Result = STATUS_SUCCESS;
            }
        }
    }
    ExReleaseResourceLite(&FsmupDeviceExtension->PrefixTableResource);

    return Result;
}
