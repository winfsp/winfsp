/**
 * @file winfsp/fsctl.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef WINFSP_FSCTL_H_INCLUDED
#define WINFSP_FSCTL_H_INCLUDED

#include <devioctl.h>

// {6F9D25FA-6DEE-4A9D-80F5-E98E14F35E54}
extern const __declspec(selectany) GUID FspFsctlDeviceClassGuid =
    { 0x6f9d25fa, 0x6dee, 0x4a9d, { 0x80, 0xf5, 0xe9, 0x8e, 0x14, 0xf3, 0x5e, 0x54 } };
// {B48171C3-DD50-4852-83A3-344C50D93B17}
extern const __declspec(selectany) GUID FspFsvrtDeviceClassGuid = 
    { 0xb48171c3, 0xdd50, 0x4852, { 0x83, 0xa3, 0x34, 0x4c, 0x50, 0xd9, 0x3b, 0x17 } };

#define FSP_FSCTL_DISK_DEVICE_NAME      "WinFsp.Disk"
#define FSP_FSCTL_NET_DEVICE_NAME       "WinFsp.Net"

/* fsctl device codes */
#define FSP_FSCTL_CREATE                \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'C', METHOD_BUFFERED, FILE_ANY_ACCESS)

/* fsvrt device codes */
#define FSP_FSCTL_DELETE                \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'D', METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSP_FSCTL_TRANSACT              \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'T', METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FSP_FSCTL_CREATE_BUFFER_SIZEMAX 64

#endif
