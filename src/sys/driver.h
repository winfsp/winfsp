/**
 * @file sys/driver.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef WINFSP_SYS_DRIVER_H_INCLUDED
#define WINFSP_SYS_DRIVER_H_INCLUDED

#include <wdm.h>

#define DRIVER_NAME                     "winfsp"

#if DBG
#define DEBUGLOG(fmt, ...)              \
    DbgPrint(DRIVER_NAME ": " __FUNCTION__ ": " fmt "\n", __VA_ARGS__)
#else
#define DEBUGLOG(fmt, ...)              ((void)0)
#endif

/* driver major functions */
DRIVER_DISPATCH FspCleanup;
DRIVER_DISPATCH FspClose;
DRIVER_DISPATCH FspCreate;
DRIVER_DISPATCH FspDeviceControl;
DRIVER_DISPATCH FspDirectoryControl;
DRIVER_DISPATCH FspFileSystemControl;
DRIVER_DISPATCH FspFlushBuffers;
DRIVER_DISPATCH FspLockControl;
DRIVER_DISPATCH FspQueryEa;
DRIVER_DISPATCH FspQueryInformation;
DRIVER_DISPATCH FspQuerySecurity;
DRIVER_DISPATCH FspQueryVolumeInformation;
DRIVER_DISPATCH FspRead;
DRIVER_DISPATCH FspSetEa;
DRIVER_DISPATCH FspSetInformation;
DRIVER_DISPATCH FspSetSecurity;
DRIVER_DISPATCH FspSetVolumeInformation;
DRIVER_DISPATCH FspShutdown;
DRIVER_DISPATCH FspWrite;

#endif
