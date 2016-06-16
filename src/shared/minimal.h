/**
 * @file shared/minimal.h
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the
 * GNU Affero General Public License version 3 as published by the
 * Free Software Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#ifndef WINFSP_SHARED_MINIMAL_H_INCLUDED
#define WINFSP_SHARED_MINIMAL_H_INCLUDED

/*
 * Eliminate dependency on the MSVCRT libraries.
 *
 * For this to work the following project settings must be set:
 * - "C/C++ > General > SDL checks" must be empty (not "Yes" or "No").
 * - "C/C++ > Code Generation > Basic Runtime Checks" must be set to "Default"
 * - "C/C++ > Code Generation > Runtime Library" must be set to "Multi-threaded (/MT)".
 * - "C/C++ > Code Generation > Security Check" must be disabled (/GS-).
 * - "Linker > Input > Ignore All Default Libraries" must be "Yes".
 *
 *
 * Update:
 *
 * It is possible to have the "Linker > Input > Ignore All Default Libraries"
 * setting to "No" and still eliminate most dependencies on the MSVCRT libraries.
 * For example, the WinFsp DLL does this on 32-bit builds (only) to include the
 * __allmul symbol that is used when doing int64_t multiplications.
 *
 * The following project setting must be changed:
 * - "Linker > Input > Ignore All Default Libraries" must be "No".
 *
 * Extreme care must be taken to ensure that the linker does not pull in symbols
 * that are not required (or worse create a half-baked CRT). For example, the WinFsp
 * DLL ensures this by setting the "Linker > Input > Ignore All Default Libraries"
 * to "Yes" on 64-bit builds and "No" on 32-bit builds.
 */

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

static inline void *MemAlloc(size_t Size)
{
    return HeapAlloc(GetProcessHeap(), 0, Size);
}
static inline void MemFree(void *Pointer)
{
    if (0 != Pointer)
        HeapFree(GetProcessHeap(), 0, Pointer);
}

static FORCEINLINE
VOID InsertTailList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry)
{
    PLIST_ENTRY Blink;

    Blink = ListHead->Blink;
    Entry->Flink = ListHead;
    Entry->Blink = Blink;
    Blink->Flink = Entry;
    ListHead->Blink = Entry;
}
static FORCEINLINE
BOOLEAN RemoveEntryList(PLIST_ENTRY Entry)
{
    PLIST_ENTRY Blink;
    PLIST_ENTRY Flink;

    Flink = Entry->Flink;
    Blink = Entry->Blink;
    Blink->Flink = Flink;
    Flink->Blink = Blink;
    return Flink == Blink;
}

#endif
