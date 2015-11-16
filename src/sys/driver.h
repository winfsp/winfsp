/**
 * @file sys/driver.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef WINFSP_SYS_DRIVER_H_INCLUDED
#define WINFSP_SYS_DRIVER_H_INCLUDED

#include <wdm.h>

#if DBG
#define DEBUGLOG(fmt, ...)              DbgPrint(__FUNCTION__ ": " fmt "\n", __VA_ARGS__)
#else
#define DEBUGLOG(fmt, ...)              ((void)0)
#endif

#endif
