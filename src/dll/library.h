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

static inline PVOID MemAlloc(SIZE_T Size)
{
    extern HANDLE ProcessHeap;
    return HeapAlloc(ProcessHeap, 0, Size);
}
static inline PVOID MemAllocSLE(SIZE_T Size)
{
    extern HANDLE ProcessHeap;
    PVOID Pointer = HeapAlloc(ProcessHeap, 0, Size);
    if (0 == Pointer)
        SetLastError(ERROR_NO_SYSTEM_RESOURCES);
    return Pointer;
}
static inline VOID MemFree(PVOID Pointer)
{
    extern HANDLE ProcessHeap;
    if (0 != Pointer)
        HeapFree(ProcessHeap, 0, Pointer);
}

#endif
