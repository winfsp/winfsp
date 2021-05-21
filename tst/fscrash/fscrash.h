/**
 * @file fscrash.h
 *
 * @copyright 2015-2021 Bill Zissimopoulos
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

#ifndef FSCRASH_H_INCLUDED
#define FSCRASH_H_INCLUDED

#include <winfsp/winfsp.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    FspCrashInterceptMask               = 0x0f,
    FspCrashInterceptAccessViolation    = 0x00,
    FspCrashInterceptTerminate          = 0x01,
    FspCrashInterceptHugeAllocationSize = 0x02,
    FspCrashInterceptEnter              = 0x10,
    FspCrashInterceptLeave              = 0x20,
};

VOID FspCrashIntercept(FSP_FILE_SYSTEM *FileSystem,
    ULONG CrashMask, ULONG CrashFlags, ULONG CrashPercent);
VOID FspCrash(FSP_FILE_SYSTEM *FileSystem);

#ifdef __cplusplus
}
#endif

#endif
