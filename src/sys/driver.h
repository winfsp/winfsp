/**
 * @file sys/driver.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef WINFSP_SYS_DRIVER_H_INCLUDED
#define WINFSP_SYS_DRIVER_H_INCLUDED

#include <wdm.h>

#if DBG
#define DEBUGLOG(...)                   DbgPrint(__FUNCTION__ ": " __VA_ARGS__)
#else
#define DEBUGLOG(...)                   ((void)0)
#endif

#endif
