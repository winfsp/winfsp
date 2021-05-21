/**
 * @file sys/mup.c
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

/*
 * FSP_MUP_PREFIX_CLASS
 *
 * Define the following macro to claim "class" prefixes during prefix
 * resolution. A "class" prefix is of the form \ClassName. The alternative
 * is a "full" prefix, which is of the form \ClassName\InstanceName.
 *
 * Claiming a class prefix has advantages and disadvantages. The main
 * advantage is that by claiming a \ClassName prefix, paths such as
 * \ClassName\IPC$ will be handled by WinFsp, thus speeding up prefix
 * resolution for all \ClassName prefixed names. The disadvantage is
 * it is no longer possible for WinFsp and another redirector to handle
 * instances ("shares") under the same \ClassName prefix.
 */
#define FSP_MUP_PREFIX_CLASS

static NTSTATUS FspMupGetClassName(
    PUNICODE_STRING Prefix, PUNICODE_STRING ClassName);
NTSTATUS FspMupRegister(
    PDEVICE_OBJECT FsmupDeviceObject, PDEVICE_OBJECT FsvolDeviceObject);
VOID FspMupUnregister(
    PDEVICE_OBJECT FsmupDeviceObject, PDEVICE_OBJECT FsvolDeviceObject);
NTSTATUS FspMupHandleIrp(
    PDEVICE_OBJECT FsmupDeviceObject, PIRP Irp);
static NTSTATUS FspMupRedirQueryPathEx(
    PDEVICE_OBJECT FsmupDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspMupGetClassName)
#pragma alloc_text(PAGE, FspMupRegister)
#pragma alloc_text(PAGE, FspMupUnregister)
#pragma alloc_text(PAGE, FspMupHandleIrp)
#pragma alloc_text(PAGE, FspMupRedirQueryPathEx)
#endif

typedef struct _FSP_MUP_CLASS
{
    LONG RefCount;
    UNICODE_STRING Name;
    UNICODE_PREFIX_TABLE_ENTRY Entry;
    WCHAR Buffer[];
} FSP_MUP_CLASS;

static NTSTATUS FspMupGetClassName(
    PUNICODE_STRING VolumePrefix, PUNICODE_STRING ClassName)
{
    PAGED_CODE();

    RtlZeroMemory(ClassName, sizeof *ClassName);

    if (L'\\' == VolumePrefix->Buffer[0])
        for (PWSTR P = VolumePrefix->Buffer + 1,
            EndP = VolumePrefix->Buffer + VolumePrefix->Length / sizeof(WCHAR);
            EndP > P; P++)
        {
            if (L'\\' == *P)
            {
                ClassName->Buffer = VolumePrefix->Buffer;
                ClassName->Length = (USHORT)((P - ClassName->Buffer) * sizeof(WCHAR));
                ClassName->MaximumLength = ClassName->Length;
                return STATUS_SUCCESS;
            }
        }

    return STATUS_INVALID_PARAMETER;
}

NTSTATUS FspMupRegister(
    PDEVICE_OBJECT FsmupDeviceObject, PDEVICE_OBJECT FsvolDeviceObject)
{
    PAGED_CODE();

    NTSTATUS Result;
    BOOLEAN Success;
    FSP_FSMUP_DEVICE_EXTENSION *FsmupDeviceExtension = FspFsmupDeviceExtension(FsmupDeviceObject);
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PUNICODE_PREFIX_TABLE_ENTRY ClassEntry;
    UNICODE_STRING ClassName;
    FSP_MUP_CLASS *Class = 0;

    Result = FspMupGetClassName(&FsvolDeviceExtension->VolumePrefix, &ClassName);
    ASSERT(NT_SUCCESS(Result));

    Class = FspAlloc(sizeof *Class + ClassName.Length);
    if (0 == Class)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    RtlZeroMemory(Class, sizeof *Class);
    Class->RefCount = 1;
    Class->Name.Length = ClassName.Length;
    Class->Name.MaximumLength = ClassName.MaximumLength;
    Class->Name.Buffer = Class->Buffer;
    RtlCopyMemory(Class->Buffer, ClassName.Buffer, ClassName.Length);

    ExAcquireResourceExclusiveLite(&FsmupDeviceExtension->PrefixTableResource, TRUE);
    Success = RtlInsertUnicodePrefix(&FsmupDeviceExtension->PrefixTable,
        &FsvolDeviceExtension->VolumePrefix, &FsvolDeviceExtension->VolumePrefixEntry);
    if (Success)
    {
        FspDeviceReference(FsvolDeviceObject);

        ClassEntry = RtlFindUnicodePrefix(&FsmupDeviceExtension->ClassTable,
            &Class->Name, 0);
        if (0 == ClassEntry)
        {
            Success = RtlInsertUnicodePrefix(&FsmupDeviceExtension->ClassTable,
                &Class->Name, &Class->Entry);
            ASSERT(Success);
            Class = 0;
        }
        else
            CONTAINING_RECORD(ClassEntry, FSP_MUP_CLASS, Entry)->RefCount++;

        Result = STATUS_SUCCESS;
    }
    else
        Result = STATUS_OBJECT_NAME_COLLISION;
    ExReleaseResourceLite(&FsmupDeviceExtension->PrefixTableResource);

exit:
    if (0 != Class)
        FspFree(Class);

    return Result;
}

VOID FspMupUnregister(
    PDEVICE_OBJECT FsmupDeviceObject, PDEVICE_OBJECT FsvolDeviceObject)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSMUP_DEVICE_EXTENSION *FsmupDeviceExtension = FspFsmupDeviceExtension(FsmupDeviceObject);
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PUNICODE_PREFIX_TABLE_ENTRY PrefixEntry;
    PUNICODE_PREFIX_TABLE_ENTRY ClassEntry;
    UNICODE_STRING ClassName;
    FSP_MUP_CLASS *Class;

    Result = FspMupGetClassName(&FsvolDeviceExtension->VolumePrefix, &ClassName);
    ASSERT(NT_SUCCESS(Result));

    ExAcquireResourceExclusiveLite(&FsmupDeviceExtension->PrefixTableResource, TRUE);
    PrefixEntry = RtlFindUnicodePrefix(&FsmupDeviceExtension->PrefixTable,
        &FsvolDeviceExtension->VolumePrefix, 0);
    if (0 != PrefixEntry)
    {
        RtlRemoveUnicodePrefix(&FsmupDeviceExtension->PrefixTable,
            &FsvolDeviceExtension->VolumePrefixEntry);
        FspDeviceDereference(FsvolDeviceObject);

        ClassEntry = RtlFindUnicodePrefix(&FsmupDeviceExtension->ClassTable,
            &ClassName, 0);
        if (0 != ClassEntry)
        {
            Class = CONTAINING_RECORD(ClassEntry, FSP_MUP_CLASS, Entry);
            if (0 == --Class->RefCount)
            {
                RtlRemoveUnicodePrefix(&FsmupDeviceExtension->ClassTable,
                    ClassEntry);
                FspFree(Class);
            }
        }
    }
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
    PUNICODE_PREFIX_TABLE_ENTRY PrefixEntry;
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
        PrefixEntry = RtlFindUnicodePrefix(&FsmupDeviceExtension->PrefixTable,
            &FileObject->FileName, 0);
        if (0 != PrefixEntry)
        {
            FsvolDeviceObject = CONTAINING_RECORD(PrefixEntry,
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
#pragma prefast(disable:28175, "We are a filesystem: ok to access DeviceObject->Type")
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
        case IRP_MJ_CREATE:
            /*
             * For CREATE requests we return STATUS_BAD_NETWORK_PATH. Here is why.
             *
             * When a file \ClassName\InstanceName\Path is opened by an application, this request
             * first goes to MUP. The MUP gives DFS a first chance to handle the request and if
             * that fails the MUP proceeds with prefix resolution. The DFS attempts to open the
             * file \ClassName\IPC$, this results in a prefix resolution for \ClassName\IPC$
             * through a recursive MUP call! If this resolution fails the DFS returns to the MUP,
             * which now attempts prefix resolution for \ClassName\InstanceName\Path.
             *
             * Under the new fsmup design we respond to IOCTL_REDIR_QUERY_PATH_EX by handling all
             * paths with a \ClassName prefix (that we know). This way we ensure that we will get
             * all opens for paths with a \ClassName prefix and avoid delays for requests of
             * \ClassName\IPC$, which if left unhandled will be forwarded to all network
             * redirectors.
             *
             * In order to successfully short-circuit requests for \ClassName\IPC$ we must also
             * return STATUS_BAD_NETWORK_PATH in CREATE. This makes DFS think that prefix
             * resolution failed and does not complain if it cannot open \ClassName\IPC$. Other
             * error codes cause DFS to completely fail the open issued by the application.
             */
            Irp->IoStatus.Status = STATUS_BAD_NETWORK_PATH;
            break;
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
    PUNICODE_PREFIX_TABLE_ENTRY Entry;

#if defined(FSP_MUP_PREFIX_CLASS)
    UNICODE_STRING ClassName;

    Result = FspMupGetClassName(&QueryPathRequest->PathName, &ClassName);
    if (!NT_SUCCESS(Result))
        return STATUS_BAD_NETWORK_PATH;

    Result = STATUS_BAD_NETWORK_PATH;
    ExAcquireResourceExclusiveLite(&FsmupDeviceExtension->PrefixTableResource, TRUE);
    Entry = RtlFindUnicodePrefix(&FsmupDeviceExtension->ClassTable, &ClassName, 0);
    if (0 != Entry)
    {
        QueryPathResponse->LengthAccepted = ClassName.Length;
        Result = STATUS_SUCCESS;
    }
    ExReleaseResourceLite(&FsmupDeviceExtension->PrefixTableResource);
#else
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension;

    Result = STATUS_BAD_NETWORK_PATH;
    ExAcquireResourceExclusiveLite(&FsmupDeviceExtension->PrefixTableResource, TRUE);
    Entry = RtlFindUnicodePrefix(&FsmupDeviceExtension->PrefixTable,
        &QueryPathRequest->PathName, 0);
    if (0 != Entry)
    {
        FsvolDeviceExtension = CONTAINING_RECORD(Entry, FSP_FSVOL_DEVICE_EXTENSION, VolumePrefixEntry);
        if (!FspIoqStopped(FsvolDeviceExtension->Ioq))
        {
            if (0 < FsvolDeviceExtension->VolumePrefix.Length &&
                FspFsvolDeviceVolumePrefixInString(
                    FsvolDeviceExtension->FsvolDeviceObject, &QueryPathRequest->PathName) &&
                (QueryPathRequest->PathName.Length == FsvolDeviceExtension->VolumePrefix.Length ||
                    '\\' == QueryPathRequest->PathName.Buffer[FsvolDeviceExtension->VolumePrefix.Length / sizeof(WCHAR)]))
            {
                QueryPathResponse->LengthAccepted = FsvolDeviceExtension->VolumePrefix.Length;
                Result = STATUS_SUCCESS;
            }
        }
    }
    ExReleaseResourceLite(&FsmupDeviceExtension->PrefixTableResource);
#endif

    return Result;
}
