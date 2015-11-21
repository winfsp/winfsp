/**
 * @file sys/fsctl.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef WINFSP_FSCTL_H_INCLUDED
#define WINFSP_FSCTL_H_INCLUDED

#include <devioctl.h>

#define FSP_FSCTL_DISK_DEVICE_NAME      "WinFsp.Disk"
#define FSP_FSCTL_NET_DEVICE_NAME       "WinFsp.Net"

/* fsctl device codes */
#define FSP_FSCTL_CREATE                \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'C', METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define FSP_FSCTL_DELETE                \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'D', METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

/* fsvrt device codes */
#define FSP_FSCTL_TRANSACT              \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'T', METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

#endif
