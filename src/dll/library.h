/**
 * @file dll/library.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef WINFSP_DLL_LIBRARY_H_INCLUDED
#define WINFSP_DLL_LIBRARY_H_INCLUDED

#define WINFSP_DLL_INTERNAL
#define WINFSP_DLL_NODEFAULTLIB
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
static inline VOID MemFree(PVOID Pointer)
{
    extern HANDLE ProcessHeap;
    if (0 != Pointer)
        HeapFree(ProcessHeap, 0, Pointer);
}

/*
 * Define WINFSP_DLL_NODEFAULTLIB to eliminate dependency on the MSVCRT libraries.
 *
 * For this to work the following project settings must be set:
 * - "C/C++ > General > SDL checks" must be empty (not "Yes" or "No").
 * - "C/C++ > Code Generation > Basic Runtime Checks" must be set to "Default"
 * - "C/C++ > Code Generation > Security Check" must be disabled (/GS-).
 * - "Linker > Input > Ignore All Default Libraries" must be "Yes".
 */
#if defined(WINFSP_DLL_NODEFAULTLIB)
#undef RtlFillMemory
#undef RtlMoveMemory
NTSYSAPI VOID NTAPI RtlFillMemory(VOID *Destination, DWORD Length, BYTE Fill);
NTSYSAPI VOID NTAPI RtlMoveMemory(VOID *Destination, CONST VOID *Source, DWORD Length);

#pragma function(memcpy)
#pragma function(memset)
static inline
void *memcpy(void *dst, const void *src, size_t siz)
{
    RtlMoveMemory(dst, src, (DWORD)siz);
    return dst;
}
static inline
void *memset(void *dst, int val, size_t siz)
{
    RtlFillMemory(dst, (DWORD)siz, val);
    return dst;
}
#endif

#endif
