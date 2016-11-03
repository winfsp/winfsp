/**
 * @file fscrash.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
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

#include "fscrash.h"

static SRWLOCK FspCrashLock = SRWLOCK_INIT;
static FSP_FILE_SYSTEM *FspCrashFileSystem;
static ULONG FspCrashPercent;
static FSP_FILE_SYSTEM_OPERATION *FspCrashInterceptedOperations
    [ARRAYSIZE(((FSP_FILE_SYSTEM *)0)->Operations)];
static volatile PULONG FspCrashNullPointer;

static unsigned FspCrashRandSeed = 1;
static int FspCrashRand(void)
{
    /*
     * This mimics MSVCRT rand(); we need our own version
     * as to not interfere with the program's rand().
     */

    FspCrashRandSeed = FspCrashRandSeed * 214013 + 2531011;
    return (FspCrashRandSeed >> 16) & RAND_MAX;
}

static __forceinline BOOLEAN FspCrashInterceptTest(FSP_FILE_SYSTEM *FileSystem)
{
    BOOLEAN Result;
    AcquireSRWLockShared(&FspCrashLock);
    Result = FileSystem == FspCrashFileSystem &&
        FspCrashRand() < (LONG)FspCrashPercent * 0x7fff / 100;
    ReleaseSRWLockShared(&FspCrashLock);
    return Result;
}

#define DefineInterceptor(NAME, CRASH, ENTER, LEAVE)\
    static NTSTATUS NAME(FSP_FILE_SYSTEM *FileSystem,\
        FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)\
    {\
        NTSTATUS Result;\
        if (ENTER)\
        {\
            if (FspCrashInterceptTest(FileSystem))\
            {\
                if (CRASH)\
                    *FspCrashNullPointer = 0x42424242;\
                else\
                    TerminateProcess(GetCurrentProcess(), STATUS_UNSUCCESSFUL);\
            }\
        }\
        Result = FspCrashInterceptedOperations[Request->Kind](FileSystem, Request, Response);\
        if (LEAVE)\
        {\
            if (FspCrashInterceptTest(FileSystem))\
            {\
                if (CRASH)\
                    *FspCrashNullPointer = 0x42424242;\
                else\
                    TerminateProcess(GetCurrentProcess(), STATUS_UNSUCCESSFUL);\
            }\
        }\
        return Result;\
    }

DefineInterceptor(FspCrashInterceptorCE, TRUE, TRUE, FALSE)
DefineInterceptor(FspCrashInterceptorCL, TRUE, FALSE, TRUE)
DefineInterceptor(FspCrashInterceptorCB, TRUE, TRUE, TRUE)
DefineInterceptor(FspCrashInterceptorTE, FALSE, TRUE, FALSE)
DefineInterceptor(FspCrashInterceptorTL, FALSE, FALSE, TRUE)
DefineInterceptor(FspCrashInterceptorTB, FALSE, TRUE, TRUE)

VOID FspCrashIntercept(FSP_FILE_SYSTEM *FileSystem,
    ULONG CrashMask, ULONG CrashFlags, ULONG CrashPercent)
{
    FSP_FILE_SYSTEM_OPERATION *Interceptor = 0;

    if (CrashFlags & FspCrashInterceptAccessViolation)
    {
        if (FspCrashInterceptEnter == (CrashFlags & FspCrashInterceptEnter))
            Interceptor = FspCrashInterceptorCE;
        else
        if (FspCrashInterceptLeave == (CrashFlags & FspCrashInterceptLeave))
            Interceptor = FspCrashInterceptorCL;
        else
            Interceptor = FspCrashInterceptorCB;
    }
    else
    {
        if (FspCrashInterceptEnter == (CrashFlags & FspCrashInterceptEnter))
            Interceptor = FspCrashInterceptorTE;
        else
        if (FspCrashInterceptLeave == (CrashFlags & FspCrashInterceptLeave))
            Interceptor = FspCrashInterceptorTL;
        else
            Interceptor = FspCrashInterceptorTB;
    }

    RtlCopyMemory(FspCrashInterceptedOperations,
        FileSystem->Operations, sizeof FileSystem->Operations);

    for (ULONG Index = 0; ARRAYSIZE(FileSystem->Operations) > Index; Index++)
        if (0 != ((1 << Index) & CrashMask) && 0 != FileSystem->Operations[Index])
            FileSystem->Operations[Index] = Interceptor;

    FspCrashPercent = CrashPercent;

    FspCrashRandSeed = GetTickCount();
    if (0 == FspCrashRandSeed)
        FspCrashRandSeed = 1;

    MemoryBarrier();
}

VOID FspCrash(FSP_FILE_SYSTEM *FileSystem)
{
    AcquireSRWLockExclusive(&FspCrashLock);
    FspCrashFileSystem = FileSystem;
    ReleaseSRWLockExclusive(&FspCrashLock);
}
