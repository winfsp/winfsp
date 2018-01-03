/**
 * @file fscrash.h
 *
 * @copyright 2015-2018 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
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
