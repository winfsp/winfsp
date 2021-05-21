/**
 * @file sys/fsext.c
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
#include <winfsp/fsext.h>

/*
 * Maximum number of allowed fsext providers. This must be kept small,
 * because we do a linear search to find the provider. If this changes
 * the data structure used to store providers (currently 2 parallel
 * arrays) must be revisited.
 */
#define FSP_FSEXT_PROVIDER_COUNTMAX     16

static KSPIN_LOCK FsextSpinLock = 0;
static UINT32 FsextControlCodes[FSP_FSEXT_PROVIDER_COUNTMAX];
static FSP_FSEXT_PROVIDER *FsextProviders[FSP_FSEXT_PROVIDER_COUNTMAX];

static inline
FSP_FSEXT_PROVIDER *FspFsextProviderGet(UINT32 FsextControlCode)
{
    for (ULONG I = 0; FSP_FSEXT_PROVIDER_COUNTMAX > I; I++)
        if (FsextControlCode == FsextControlCodes[I])
            return FsextProviders[I];

    return 0;
}

FSP_FSEXT_PROVIDER *FspFsextProvider(UINT32 FsextControlCode, PNTSTATUS PLoadResult)
{
    FSP_FSEXT_PROVIDER *Provider;
    KIRQL Irql;

    KeAcquireSpinLock(&FsextSpinLock, &Irql);
    Provider = FspFsextProviderGet(FsextControlCode);
    KeReleaseSpinLock(&FsextSpinLock, Irql);
    ASSERT(0 == Provider || FsextControlCode == Provider->DeviceTransactCode);

    if (0 != PLoadResult)
    {
        if (0 == Provider)
        {
            WCHAR Buf[64 + 256];
            UNICODE_STRING Path;
            UNICODE_STRING Name;
            union
            {
                KEY_VALUE_PARTIAL_INFORMATION V;
                UINT8 B[FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data) + 256];
            } Value;
            ULONG Length;
            NTSTATUS Result;

            RtlInitUnicodeString(&Path, L"" FSP_REGKEY "\\Fsext");
            RtlInitEmptyUnicodeString(&Name, Buf, sizeof Buf);
            Result = RtlUnicodeStringPrintf(&Name, L"%08x", FsextControlCode);
            ASSERT(NT_SUCCESS(Result));
            Length = sizeof Value;
            Result = FspRegistryGetValue(&Path, &Name, &Value.V, &Length);
            if (!NT_SUCCESS(Result))
            {
                *PLoadResult = Result;
                return 0;
            }
            if (REG_SZ != Value.V.Type)
            {
                *PLoadResult = STATUS_OBJECT_NAME_NOT_FOUND;
                return 0;
            }

            RtlInitEmptyUnicodeString(&Path, Buf, sizeof Buf);
            Result = RtlUnicodeStringPrintf(&Path,
                L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\%s", (PWSTR)Value.V.Data);
            ASSERT(NT_SUCCESS(Result));

            Result = ZwLoadDriver(&Path);
            if (!NT_SUCCESS(Result) && STATUS_IMAGE_ALREADY_LOADED != Result)
            {
                *PLoadResult = Result;
                return 0;
            }

            KeAcquireSpinLock(&FsextSpinLock, &Irql);
            Provider = FspFsextProviderGet(FsextControlCode);
            KeReleaseSpinLock(&FsextSpinLock, Irql);
            ASSERT(0 == Provider || FsextControlCode == Provider->DeviceTransactCode);
        }

        *PLoadResult = 0 != Provider ? STATUS_SUCCESS : STATUS_OBJECT_NAME_NOT_FOUND;
    }

    return Provider;
}

NTSTATUS FspFsextProviderRegister(FSP_FSEXT_PROVIDER *Provider)
{
    NTSTATUS Result;
    KIRQL Irql;

    KeAcquireSpinLock(&FsextSpinLock, &Irql);

    Result = STATUS_TOO_LATE;
    for (ULONG I = 0; FSP_FSEXT_PROVIDER_COUNTMAX > I; I++)
        if (0 == FsextControlCodes[I])
        {
            Provider->DeviceExtensionOffset = FIELD_OFFSET(FSP_FSVOL_DEVICE_EXTENSION, FsextData);
            FsextControlCodes[I] = Provider->DeviceTransactCode;
            FsextProviders[I] = Provider;
            Result = STATUS_SUCCESS;
            break;
        }

    KeReleaseSpinLock(&FsextSpinLock, Irql);

    return Result;
}

NTSTATUS FspFsextProviderTransact(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject,
    FSP_FSCTL_TRANSACT_RSP *Response, FSP_FSCTL_TRANSACT_REQ **PRequest)
{
    /*
     * This function uses IoBuildDeviceIoControlRequest to build an IRP and then send it
     * to the WinFsp driver. IoBuildDeviceIoControlRequest places the IRP in the IRP queue
     * thus allowing it to be cancelled with CancelSynchronousIo.
     *
     * Special kernel APC's must be enabled:
     * See https://www.osr.com/blog/2018/02/14/beware-iobuilddeviceiocontrolrequest/
     */
    ASSERT(!KeAreAllApcsDisabled());

    NTSTATUS Result;
    IO_STATUS_BLOCK IoStatus;
    PIRP Irp;
    PIO_STACK_LOCATION IrpSp;

    if (0 != PRequest)
        *PRequest = 0;

    if (0 == DeviceObject)
        DeviceObject = IoGetRelatedDeviceObject(FileObject);

    Irp = IoBuildDeviceIoControlRequest(FSP_FSCTL_TRANSACT_INTERNAL,
        DeviceObject,
        Response,
        0 != Response ? Response->Size : 0,
        PRequest,
        0 != PRequest ? sizeof(PVOID) : 0,
        FALSE,
        0,
        &IoStatus);
    if (0 == Irp)
        return STATUS_INSUFFICIENT_RESOURCES;

    /*
     * IoBuildDeviceIoControlRequest builds an IOCTL IRP without a FileObject.
     * Patch it so that it is an FSCTL IRP with a FileObject. Mark it as
     * IRP_SYNCHRONOUS_API so that CancelSynchronousIo can cancel it.
     */
    Irp->Flags |= IRP_SYNCHRONOUS_API;
    IrpSp = IoGetNextIrpStackLocation(Irp);
    IrpSp->MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL;
    IrpSp->MinorFunction = IRP_MN_USER_FS_REQUEST;
    IrpSp->FileObject = FileObject;

    Result = IoCallDriver(DeviceObject, Irp);
    ASSERT(STATUS_PENDING != Result);

    return Result;
}
