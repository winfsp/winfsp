/**
 * @file dll/library.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef WINFSP_DLL_LIBRARY_H_INCLUDED
#define WINFSP_DLL_LIBRARY_H_INCLUDED

#define WINFSP_DLL_INTERNAL
#include <winfsp/winfsp.h>
#include <strsafe.h>

#define LIBRARY_NAME                    "WinFsp"

/* DEBUGLOG */
#if !defined(NDEBUG)
#define DEBUGLOG(fmt, ...)              \
    FspDebugLog("[U] " LIBRARY_NAME "!" __FUNCTION__ ": " fmt "\n", __VA_ARGS__)
#else
#define DEBUGLOG(fmt, ...)              ((void)0)
#endif

#endif
