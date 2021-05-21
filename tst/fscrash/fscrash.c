/**
 * @file fscrash.c
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

#include "fscrash.h"

static SRWLOCK FspCrashLock = SRWLOCK_INIT;
static FSP_FILE_SYSTEM *FspCrashFileSystem;
static BOOLEAN FspCrashInterceptFlag;
static ULONG FspCrashPercent;
static FSP_FILE_SYSTEM_OPERATION *FspCrashInterceptedOperations
    [ARRAYSIZE(((FSP_FILE_SYSTEM *)0)->Operations)];
static volatile PULONG FspCrashNullPointer;

static unsigned FspCrashRandSeed = 1;
static __forceinline int FspCrashRand(void)
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
    Result = FspCrashInterceptFlag ||
        (FileSystem == FspCrashFileSystem &&
            FspCrashRand() < (LONG)FspCrashPercent * 0x7fff / 100);
    FspCrashInterceptFlag = FspCrashInterceptFlag || Result;
    ReleaseSRWLockShared(&FspCrashLock);
    return Result;
}

#define DefineInterceptor(NAME, ACTION, ENTER, LEAVE)\
    static NTSTATUS NAME(FSP_FILE_SYSTEM *FileSystem,\
        FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)\
    {\
        NTSTATUS Result;\
        if (ENTER)\
        {\
            switch (ACTION)\
            {\
            default:\
            case FspCrashInterceptAccessViolation:\
                if (FspCrashInterceptTest(FileSystem))\
                    *FspCrashNullPointer = 0x42424242;\
                break;\
            case FspCrashInterceptTerminate:\
                if (FspCrashInterceptTest(FileSystem))\
                    TerminateProcess(GetCurrentProcess(), STATUS_UNSUCCESSFUL);\
                break;\
            case FspCrashInterceptHugeAllocationSize:\
                break;\
            }\
        }\
        Result = FspCrashInterceptedOperations[Request->Kind](FileSystem, Request, Response);\
        if (LEAVE)\
        {\
            switch (ACTION)\
            {\
            default:\
            case FspCrashInterceptAccessViolation:\
                if (FspCrashInterceptTest(FileSystem))\
                    *FspCrashNullPointer = 0x42424242;\
                break;\
            case FspCrashInterceptTerminate:\
                if (FspCrashInterceptTest(FileSystem))\
                    TerminateProcess(GetCurrentProcess(), STATUS_UNSUCCESSFUL);\
                break;\
            case FspCrashInterceptHugeAllocationSize:\
                if (FspCrashInterceptTest(FileSystem))\
                    if (STATUS_SUCCESS == Response->IoStatus.Status)\
                    {\
                        if (FspFsctlTransactCreateKind == Request->Kind)\
                            Response->Rsp.Create.Opened.FileInfo.AllocationSize = HugeAllocationSize;\
                        else if (FspFsctlTransactOverwriteKind == Request->Kind)\
                            Response->Rsp.Overwrite.FileInfo.AllocationSize = HugeAllocationSize;\
                        else if (FspFsctlTransactQueryInformationKind == Request->Kind)\
                            Response->Rsp.QueryInformation.FileInfo.AllocationSize = HugeAllocationSize;\
                        else if (FspFsctlTransactSetInformationKind == Request->Kind &&\
                            (4/*Basic*/ == Request->Req.SetInformation.FileInformationClass ||\
                            19/*Allocation*/ == Request->Req.SetInformation.FileInformationClass ||\
                            20/*EndOfFile*/ == Request->Req.SetInformation.FileInformationClass))\
                            Response->Rsp.SetInformation.FileInfo.AllocationSize = HugeAllocationSize;\
                        else if (FspFsctlTransactWriteKind == Request->Kind &&\
                            !Request->Req.Write.ConstrainedIo)\
                            Response->Rsp.Write.FileInfo.AllocationSize = HugeAllocationSize;\
                    }\
                break;\
            }\
        }\
        return Result;\
    }

#define HugeAllocationSize              0x1000000000000000ULL
DefineInterceptor(FspCrashInterceptorCE, FspCrashInterceptAccessViolation, TRUE, FALSE)
DefineInterceptor(FspCrashInterceptorCL, FspCrashInterceptAccessViolation, FALSE, TRUE)
DefineInterceptor(FspCrashInterceptorCB, FspCrashInterceptAccessViolation, TRUE, TRUE)
DefineInterceptor(FspCrashInterceptorTE, FspCrashInterceptTerminate, TRUE, FALSE)
DefineInterceptor(FspCrashInterceptorTL, FspCrashInterceptTerminate, FALSE, TRUE)
DefineInterceptor(FspCrashInterceptorTB, FspCrashInterceptTerminate, TRUE, TRUE)
DefineInterceptor(FspCrashInterceptorAB, FspCrashInterceptHugeAllocationSize, TRUE, TRUE)

VOID FspCrashIntercept(FSP_FILE_SYSTEM *FileSystem,
    ULONG CrashMask, ULONG CrashFlags, ULONG CrashPercent)
{
    FSP_FILE_SYSTEM_OPERATION *Interceptor = 0;

    switch (CrashFlags & FspCrashInterceptMask)
    {
    default:
    case FspCrashInterceptAccessViolation:
        if (FspCrashInterceptEnter == (CrashFlags & FspCrashInterceptEnter))
            Interceptor = FspCrashInterceptorCE;
        else
        if (FspCrashInterceptLeave == (CrashFlags & FspCrashInterceptLeave))
            Interceptor = FspCrashInterceptorCL;
        else
            Interceptor = FspCrashInterceptorCB;
        break;
    case FspCrashInterceptTerminate:
        if (FspCrashInterceptEnter == (CrashFlags & FspCrashInterceptEnter))
            Interceptor = FspCrashInterceptorTE;
        else
        if (FspCrashInterceptLeave == (CrashFlags & FspCrashInterceptLeave))
            Interceptor = FspCrashInterceptorTL;
        else
            Interceptor = FspCrashInterceptorTB;
        break;
    case FspCrashInterceptHugeAllocationSize:
        Interceptor = FspCrashInterceptorAB;
        break;
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
