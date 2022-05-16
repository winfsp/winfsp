/**
 * @file dll/debug.c
 *
 * @copyright 2015-2022 Bill Zissimopoulos
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

#include <dll/library.h>

#if !defined(NDEBUG)
ULONG DebugRandom(VOID)
{
    static SRWLOCK SlimLock = SRWLOCK_INIT;
    static ULONG Seed = 1;
    ULONG Result;

    AcquireSRWLockExclusive(&SlimLock);

    /* see ucrt sources */
    Seed = Seed * 214013 + 2531011;
    Result = (Seed >> 16) & 0x7fff;

    ReleaseSRWLockExclusive(&SlimLock);

    return Result;
}
#endif
