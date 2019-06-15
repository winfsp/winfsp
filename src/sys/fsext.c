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

FSP_FSEXT_PROVIDER *FspFsextProvider(VOID)
{
    FSP_FSEXT_PROVIDER *Provider;
    KIRQL Irql;

    KeAcquireSpinLock(&FsextSpinLock, &Irql);
    Provider = FsextProvider;
    KeReleaseSpinLock(&FsextSpinLock, Irql);

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
