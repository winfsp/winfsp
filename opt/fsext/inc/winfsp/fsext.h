/**
 * @file winfsp/fsext.h
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
    NTSTATUS (*DeviceInit)(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_PARAMS *VolumeParams);
    VOID (*DeviceFini)(PDEVICE_OBJECT DeviceObject);
    VOID (*DeviceExpirationRoutine)(PDEVICE_OBJECT DeviceObject, UINT64 ExpirationTime);
    NTSTATUS (*DeviceTransact)(PDEVICE_OBJECT DeviceObject, PIRP Irp);
    /* out */
    UINT32 DeviceExtensionOffset;
} FSP_FSEXT_PROVIDER;

FSP_DDI_DEF(NTSTATUS, FspFsextProviderRegister,
    FSP_FSEXT_PROVIDER *Provider)
FSP_DDI_DEF(NTSTATUS, FspFsextProviderTransact,
    PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject,
    FSP_FSCTL_TRANSACT_RSP *Response, FSP_FSCTL_TRANSACT_REQ **PRequest)

FSP_DDI_DEF(NTSTATUS, FspPosixMapUidToSid,
    UINT32 Uid,
    PSID *PSid)
FSP_DDI_DEF(NTSTATUS, FspPosixMapSidToUid,
    PSID Sid,
    PUINT32 PUid)
FSP_DDI_DEF(VOID, FspDeleteSid,
    PSID Sid,
    NTSTATUS (*CreateFunc)())
FSP_DDI_DEF(NTSTATUS, FspPosixMapPermissionsToSecurityDescriptor,
    UINT32 Uid,
    UINT32 Gid,
    UINT32 Mode,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
FSP_DDI_DEF(NTSTATUS, FspPosixMapSecurityDescriptorToPermissions,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    PUINT32 PUid,
    PUINT32 PGid,
    PUINT32 PMode)
FSP_DDI_DEF(NTSTATUS, FspPosixMapWindowsToPosixPathEx,
    PWSTR WindowsPath,
    char **PPosixPath,
    BOOLEAN Translate)
FSP_DDI_DEF(NTSTATUS, FspPosixMapPosixToWindowsPathEx,
    const char *PosixPath,
    PWSTR *PWindowsPath,
    BOOLEAN Translate)
FSP_DDI_DEF(VOID, FspPosixDeletePath,
    void *Path)
FSP_DDI_DEF(VOID, FspPosixEncodeWindowsPath,
    PWSTR WindowsPath,
    ULONG Size)
FSP_DDI_DEF(VOID, FspPosixDecodeWindowsPath,
    PWSTR WindowsPath,
    ULONG Size)

#endif
