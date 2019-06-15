/**
 * @file winfsp/fsext.h
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

#ifndef WINFSP_FSEXT_H_INCLUDED
#define WINFSP_FSEXT_H_INCLUDED

#if !defined(_KERNEL_MODE)
#error This file can only be included when compiling for kernel mode.
#endif

#include <winfsp/fsctl.h>

#if defined(WINFSP_SYS_INTERNAL)
#define FSP_DDI                         __declspec(dllexport)
#else
#define FSP_DDI                         __declspec(dllimport)
#endif
#if !defined(FSP_DDI_DEF)
#define FSP_DDI_DEF(RetType, Name, ...) FSP_DDI RetType NTAPI Name ( __VA_ARGS__ );
#endif

typedef struct
{
    UINT32 Version;
    /* in */
    UINT32 DeviceTransactCode;
    UINT32 DeviceExtensionSize;
    NTSTATUS (*DeviceInit)(PDEVICE_OBJECT DeviceObject);
    VOID (*DeviceFini)(PDEVICE_OBJECT DeviceObject);
    VOID (*DeviceExpirationRoutine)(PDEVICE_OBJECT DeviceObject, UINT64 ExpirationTime);
    NTSTATUS (*DeviceTransact)(PIRP Irp, PDEVICE_OBJECT DeviceObject);
    /* out */
    UINT32 DeviceExtensionOffset;
} FSP_FSEXT_PROVIDER;

FSP_DDI_DEF(NTSTATUS, FspFsextRegisterProvider, FSP_FSEXT_PROVIDER *Provider)

FSP_DDI_DEF(NTSTATUS, FspPosixMapSidToUid, PSID Sid, PUINT32 PUid)
FSP_DDI_DEF(NTSTATUS, FspPosixMapWindowsToPosixPathEx, PWSTR WindowsPath, char **PPosixPath,
    BOOLEAN Translate)

#endif
