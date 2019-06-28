/**
 * @file sys/fsext.c
 *
 * @copyright 2015-2019 Bill Zissimopoulos
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

static KSPIN_LOCK FsextSpinLock = 0;
FSP_FSEXT_PROVIDER *FsextProvider;

FSP_FSEXT_PROVIDER *FspFsextProvider(UINT32 FsextControlCode, PNTSTATUS PLoadResult)
{
    FSP_FSEXT_PROVIDER *Provider;
    KIRQL Irql;

    KeAcquireSpinLock(&FsextSpinLock, &Irql);
    Provider = FsextProvider;
    KeReleaseSpinLock(&FsextSpinLock, Irql);
    if (0 != Provider && FsextControlCode != Provider->DeviceTransactCode)
        Provider = 0;

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
            Provider = FsextProvider;
            KeReleaseSpinLock(&FsextSpinLock, Irql);
            if (0 != Provider && FsextControlCode != Provider->DeviceTransactCode)
                Provider = 0;
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

    if (0 != FsextProvider)
    {
        Result = STATUS_TOO_LATE;
        goto exit;
    }

    Provider->DeviceExtensionOffset = FIELD_OFFSET(FSP_FSVOL_DEVICE_EXTENSION, FsextData);
    FsextProvider = Provider;

    Result = STATUS_SUCCESS;

exit:
    KeReleaseSpinLock(&FsextSpinLock, Irql);

    return Result;
}
