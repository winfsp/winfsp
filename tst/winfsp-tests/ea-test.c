/**
 * @file ea-test.c
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

#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <strsafe.h>
#include "memfs.h"

#include "winfsp-tests.h"

typedef struct _FILE_GET_EA_INFORMATION
{
    ULONG NextEntryOffset;
    UCHAR EaNameLength;
    CHAR EaName[1];
} FILE_GET_EA_INFORMATION, *PFILE_GET_EA_INFORMATION;

NTSYSAPI NTSTATUS NTAPI NtQueryEaFile(
    IN HANDLE               FileHandle,
    OUT PIO_STATUS_BLOCK    IoStatusBlock,
    OUT PVOID               Buffer,
    IN ULONG                Length,
    IN BOOLEAN              ReturnSingleEntry,
    IN PVOID                EaList OPTIONAL,
    IN ULONG                EaListLength,
    IN PULONG               EaIndex OPTIONAL,
    IN BOOLEAN              RestartScan);
NTSYSAPI NTSTATUS NTAPI NtSetEaFile(
    IN HANDLE               FileHandle,
    OUT PIO_STATUS_BLOCK    IoStatusBlock,
    IN PVOID                EaBuffer,
    IN ULONG                EaBufferSize);

BOOLEAN AddGetEa(PFILE_GET_EA_INFORMATION SingleEa,
    PFILE_GET_EA_INFORMATION Ea, ULONG Length, PULONG PBytesTransferred)
{
    if (0 != SingleEa)
    {
        PUINT8 EaPtr = (PUINT8)Ea + FSP_FSCTL_ALIGN_UP(*PBytesTransferred, sizeof(ULONG));
        PUINT8 EaEnd = (PUINT8)Ea + Length;
        ULONG EaLen = FIELD_OFFSET(FILE_GET_EA_INFORMATION, EaName) +
            SingleEa->EaNameLength + 1;

        if (EaEnd < EaPtr + EaLen)
            return FALSE;

        memcpy(EaPtr, SingleEa, EaLen);
        ((PFILE_GET_EA_INFORMATION)EaPtr)->NextEntryOffset = FSP_FSCTL_ALIGN_UP(EaLen, sizeof(ULONG));
        *PBytesTransferred = (ULONG)(EaPtr + EaLen - (PUINT8)Ea);
    }
    else if ((ULONG)FIELD_OFFSET(FILE_GET_EA_INFORMATION, EaName) <= *PBytesTransferred)
    {
        PUINT8 EaEnd = (PUINT8)Ea + *PBytesTransferred;

        while (EaEnd > (PUINT8)Ea + Ea->NextEntryOffset)
            Ea = (PVOID)((PUINT8)Ea + Ea->NextEntryOffset);

        Ea->NextEntryOffset = 0;
    }

    return TRUE;
}

static void ea_init_ea(
    PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength, PULONG PBytesTransferred)
{
    union
    {
        FILE_FULL_EA_INFORMATION V;
        UINT8 B[128];
    } SingleEa;

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.EaNameLength = (UCHAR)strlen("Aname1");
    SingleEa.V.EaValueLength = (USHORT)strlen("first");
    lstrcpyA(SingleEa.V.EaName, "Aname1");
    memcpy(SingleEa.V.EaName + SingleEa.V.EaNameLength + 1, "first", SingleEa.V.EaValueLength);
    FspFileSystemAddEa(&SingleEa.V, Ea, EaLength, PBytesTransferred);

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.Flags = FILE_NEED_EA;
    SingleEa.V.EaNameLength = (UCHAR)strlen("bnameTwo");
    SingleEa.V.EaValueLength = (USHORT)strlen("second");
    lstrcpyA(SingleEa.V.EaName, "bnameTwo");
    memcpy(SingleEa.V.EaName + SingleEa.V.EaNameLength + 1, "second", SingleEa.V.EaValueLength);
    FspFileSystemAddEa(&SingleEa.V, Ea, EaLength, PBytesTransferred);

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.EaNameLength = (UCHAR)strlen("Cn3");
    SingleEa.V.EaValueLength = (USHORT)strlen("third");
    lstrcpyA(SingleEa.V.EaName, "Cn3");
    memcpy(SingleEa.V.EaName + SingleEa.V.EaNameLength + 1, "third", SingleEa.V.EaValueLength);
    FspFileSystemAddEa(&SingleEa.V, Ea, EaLength, PBytesTransferred);

    FspFileSystemAddEa(0, Ea, EaLength, PBytesTransferred);
}

static void ea_init_bad_ea(
    PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength, PULONG PBytesTransferred)
{
    union
    {
        FILE_FULL_EA_INFORMATION V;
        UINT8 B[128];
    } SingleEa;

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.EaNameLength = (UCHAR)strlen("Aname1");
    SingleEa.V.EaValueLength = (USHORT)strlen("first");
    lstrcpyA(SingleEa.V.EaName, "Aname1");
    memcpy(SingleEa.V.EaName + SingleEa.V.EaNameLength + 1, "first", SingleEa.V.EaValueLength);
    FspFileSystemAddEa(&SingleEa.V, Ea, EaLength, PBytesTransferred);

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.Flags = FILE_NEED_EA;
    SingleEa.V.EaNameLength = (UCHAR)strlen("bnameTwo*");
    SingleEa.V.EaValueLength = (USHORT)strlen("second");
    lstrcpyA(SingleEa.V.EaName, "bnameTwo*");
    memcpy(SingleEa.V.EaName + SingleEa.V.EaNameLength + 1, "second", SingleEa.V.EaValueLength);
    FspFileSystemAddEa(&SingleEa.V, Ea, EaLength, PBytesTransferred);

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.EaNameLength = (UCHAR)strlen("Cn3");
    SingleEa.V.EaValueLength = (USHORT)strlen("third");
    lstrcpyA(SingleEa.V.EaName, "Cn3");
    memcpy(SingleEa.V.EaName + SingleEa.V.EaNameLength + 1, "third", SingleEa.V.EaValueLength);
    FspFileSystemAddEa(&SingleEa.V, Ea, EaLength, PBytesTransferred);

    FspFileSystemAddEa(0, Ea, EaLength, PBytesTransferred);
}

struct ea_check_ea_context
{
    ULONG Count;
    ULONG EaCount[4];
};

static NTSTATUS ea_check_ea_enumerate(
    FSP_FILE_SYSTEM *FileSystem, PVOID Context0,
    PFILE_FULL_EA_INFORMATION SingleEa)
{
    struct ea_check_ea_context *Context = Context0;

    if (0 == strcmp(SingleEa->EaName, "ANAME1"))
    {
        ASSERT(0 == SingleEa->Flags);
        ASSERT(SingleEa->EaNameLength == (UCHAR)strlen("ANAME1"));
        ASSERT(SingleEa->EaValueLength == (UCHAR)strlen("first"));
        ASSERT(0 == memcmp(SingleEa->EaName + SingleEa->EaNameLength + 1, "first", SingleEa->EaValueLength));
        Context->EaCount[0]++;
    }

    if (0 == strcmp(SingleEa->EaName, "BNAMETWO"))
    {
        if (!OptFuseExternal)
        {
            /* FUSE has no concept of FILE_NEED_EA */
            ASSERT(FILE_NEED_EA == SingleEa->Flags);
        }
        ASSERT(SingleEa->EaNameLength == (UCHAR)strlen("BNAMETWO"));
        ASSERT(SingleEa->EaValueLength == (UCHAR)strlen("second"));
        ASSERT(0 == memcmp(SingleEa->EaName + SingleEa->EaNameLength + 1, "second", SingleEa->EaValueLength));
        Context->EaCount[1]++;
    }

    if (0 == strcmp(SingleEa->EaName, "CN3"))
    {
        ASSERT(0 == SingleEa->Flags);
        ASSERT(SingleEa->EaNameLength == (UCHAR)strlen("CN3"));
        ASSERT(SingleEa->EaValueLength == (UCHAR)strlen("third"));
        ASSERT(0 == memcmp(SingleEa->EaName + SingleEa->EaNameLength + 1, "third", SingleEa->EaValueLength));
        Context->EaCount[2]++;
    }

    if (0 == strcmp(SingleEa->EaName, "NONEXISTENT"))
    {
        ASSERT(0 == SingleEa->Flags);
        ASSERT(SingleEa->EaNameLength == (UCHAR)strlen("NONEXISTENT"));
        ASSERT(SingleEa->EaValueLength == 0);
        Context->EaCount[3]++;
    }

    Context->Count++;

    return STATUS_SUCCESS;
}

static void ea_check_ea(HANDLE Handle)
{
    NTSTATUS Result;
    IO_STATUS_BLOCK Iosb;
    union
    {
        FILE_FULL_EA_INFORMATION V;
        UINT8 B[512];
    } Ea;
    union
    {
        FILE_GET_EA_INFORMATION V;
        UINT8 B[512];
    } GetEa;
    union
    {
        FILE_GET_EA_INFORMATION V;
        UINT8 B[128];
    } SingleGetEa;
    ULONG EaLength = 0;
    ULONG EaIndex;
    struct ea_check_ea_context Context;

    EaLength = 0;
    memset(&SingleGetEa, 0, sizeof SingleGetEa);
    SingleGetEa.V.EaNameLength = (UCHAR)strlen("bnameTwo");
    lstrcpyA(SingleGetEa.V.EaName, "bnameTwo");
    AddGetEa(&SingleGetEa.V, &GetEa.V, sizeof GetEa, &EaLength);
    AddGetEa(0, &GetEa.V, sizeof GetEa, &EaLength);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb, &Ea, sizeof Ea, FALSE, &GetEa.V, EaLength, 0, FALSE);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(1 == Context.Count);
    ASSERT(0 == Context.EaCount[0]);
    ASSERT(1 == Context.EaCount[1]);
    ASSERT(0 == Context.EaCount[2]);
    ASSERT(0 == Context.EaCount[3]);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("bnameTwo") + 1),
        FALSE, &GetEa.V, EaLength, 0, FALSE);
    ASSERT(STATUS_BUFFER_OVERFLOW == Result);
    ASSERT(0 == Iosb.Information);

    EaLength = 0;
    memset(&SingleGetEa, 0, sizeof SingleGetEa);
    SingleGetEa.V.EaNameLength = (UCHAR)strlen("nonexistent");
    lstrcpyA(SingleGetEa.V.EaName, "nonexistent");
    AddGetEa(&SingleGetEa.V, &GetEa.V, sizeof GetEa, &EaLength);
    AddGetEa(0, &GetEa.V, sizeof GetEa, &EaLength);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb, &Ea, sizeof Ea, FALSE, &GetEa.V, EaLength, 0, FALSE);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(1 == Context.Count);
    ASSERT(0 == Context.EaCount[0]);
    ASSERT(0 == Context.EaCount[1]);
    ASSERT(0 == Context.EaCount[2]);
    ASSERT(1 == Context.EaCount[3]);

    EaLength = 0;
    memset(&SingleGetEa, 0, sizeof SingleGetEa);
    SingleGetEa.V.EaNameLength = (UCHAR)strlen("bnameTwo");
    lstrcpyA(SingleGetEa.V.EaName, "bnameTwo");
    AddGetEa(&SingleGetEa.V, &GetEa.V, sizeof GetEa, &EaLength);
    memset(&SingleGetEa, 0, sizeof SingleGetEa);
    SingleGetEa.V.EaNameLength = (UCHAR)strlen("nonexistent");
    lstrcpyA(SingleGetEa.V.EaName, "nonexistent");
    AddGetEa(&SingleGetEa.V, &GetEa.V, sizeof GetEa, &EaLength);
    AddGetEa(0, &GetEa.V, sizeof GetEa, &EaLength);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb, &Ea, sizeof Ea, FALSE, &GetEa.V, EaLength, 0, FALSE);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(2 == Context.Count);
    ASSERT(0 == Context.EaCount[0]);
    ASSERT(1 == Context.EaCount[1]);
    ASSERT(0 == Context.EaCount[2]);
    ASSERT(1 == Context.EaCount[3]);

    EaLength = 0;
    memset(&SingleGetEa, 0, sizeof SingleGetEa);
    SingleGetEa.V.EaNameLength = (UCHAR)strlen("Aname1");
    lstrcpyA(SingleGetEa.V.EaName, "Aname1");
    AddGetEa(&SingleGetEa.V, &GetEa.V, sizeof GetEa, &EaLength);
    memset(&SingleGetEa, 0, sizeof SingleGetEa);
    SingleGetEa.V.EaNameLength = (UCHAR)strlen("bnameTwo");
    lstrcpyA(SingleGetEa.V.EaName, "bnameTwo");
    AddGetEa(&SingleGetEa.V, &GetEa.V, sizeof GetEa, &EaLength);
    AddGetEa(0, &GetEa.V, sizeof GetEa, &EaLength);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb, &Ea, sizeof Ea, FALSE, &GetEa.V, EaLength, 0, FALSE);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(2 == Context.Count);
    ASSERT(1 == Context.EaCount[0]);
    ASSERT(1 == Context.EaCount[1]);
    ASSERT(0 == Context.EaCount[2]);
    ASSERT(0 == Context.EaCount[3]);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("Aname1") + 1 + strlen("first")),
        FALSE, &GetEa.V, EaLength, 0, FALSE);
    ASSERT(STATUS_BUFFER_OVERFLOW == Result);
    ASSERT(0 == Iosb.Information);

    EaLength = 0;
    memset(&SingleGetEa, 0, sizeof SingleGetEa);
    SingleGetEa.V.EaNameLength = (UCHAR)strlen("bnameTwo");
    lstrcpyA(SingleGetEa.V.EaName, "bnameTwo");
    AddGetEa(&SingleGetEa.V, &GetEa.V, sizeof GetEa, &EaLength);
    memset(&SingleGetEa, 0, sizeof SingleGetEa);
    SingleGetEa.V.EaNameLength = (UCHAR)strlen("bnameTwo");
    lstrcpyA(SingleGetEa.V.EaName, "bnameTwo");
    AddGetEa(&SingleGetEa.V, &GetEa.V, sizeof GetEa, &EaLength);
    AddGetEa(0, &GetEa.V, sizeof GetEa, &EaLength);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb, &Ea, sizeof Ea, FALSE, &GetEa.V, EaLength, 0, FALSE);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(1 == Context.Count);
    ASSERT(0 == Context.EaCount[0]);
    ASSERT(1 == Context.EaCount[1]);
    ASSERT(0 == Context.EaCount[2]);
    ASSERT(0 == Context.EaCount[3]);

    EaLength = 0;
    memset(&SingleGetEa, 0, sizeof SingleGetEa);
    SingleGetEa.V.EaNameLength = (UCHAR)strlen("bnameTwo");
    lstrcpyA(SingleGetEa.V.EaName, "bnameTwo");
    AddGetEa(&SingleGetEa.V, &GetEa.V, sizeof GetEa, &EaLength);
    memset(&SingleGetEa, 0, sizeof SingleGetEa);
    SingleGetEa.V.EaNameLength = (UCHAR)strlen("bnameTwo*");
    lstrcpyA(SingleGetEa.V.EaName, "bnameTwo*");
    AddGetEa(&SingleGetEa.V, &GetEa.V, sizeof GetEa, &EaLength);
    AddGetEa(0, &GetEa.V, sizeof GetEa, &EaLength);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb, &Ea, sizeof Ea, FALSE, &GetEa.V, EaLength, 0, FALSE);
    ASSERT(STATUS_INVALID_EA_NAME == Result);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb, &Ea, sizeof Ea, FALSE, 0, 0, 0, FALSE);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(3 == Context.Count);
    ASSERT(1 == Context.EaCount[0]);
    ASSERT(1 == Context.EaCount[1]);
    ASSERT(1 == Context.EaCount[2]);
    ASSERT(0 == Context.EaCount[3]);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb, &Ea, sizeof Ea, FALSE, 0, 0, 0, FALSE);
    ASSERT(STATUS_NO_MORE_EAS == Result);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb, &Ea, sizeof Ea, TRUE, 0, 0, 0, TRUE);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(1 == Context.Count);
    ASSERT(1 == Context.EaCount[0]);
    ASSERT(0 == Context.EaCount[1]);
    ASSERT(0 == Context.EaCount[2]);
    ASSERT(0 == Context.EaCount[3]);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb, &Ea, sizeof Ea, TRUE, 0, 0, 0, FALSE);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(1 == Context.Count);
    ASSERT(0 == Context.EaCount[0]);
    ASSERT(1 == Context.EaCount[1]);
    ASSERT(0 == Context.EaCount[2]);
    ASSERT(0 == Context.EaCount[3]);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("Aname1") + 1),
        FALSE, 0, 0, 0, TRUE);
    ASSERT(STATUS_BUFFER_TOO_SMALL == Result);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("Aname1") + 1 + strlen("first")),
        FALSE, 0, 0, 0, TRUE);
    ASSERT(STATUS_BUFFER_OVERFLOW == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(1 == Context.Count);
    ASSERT(1 == Context.EaCount[0]);
    ASSERT(0 == Context.EaCount[1]);
    ASSERT(0 == Context.EaCount[2]);
    ASSERT(0 == Context.EaCount[3]);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("Aname1") + 1 + strlen("first")),
        FALSE, 0, 0, 0, TRUE);
    ASSERT(STATUS_BUFFER_OVERFLOW == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(1 == Context.Count);
    ASSERT(1 == Context.EaCount[0]);
    ASSERT(0 == Context.EaCount[1]);
    ASSERT(0 == Context.EaCount[2]);
    ASSERT(0 == Context.EaCount[3]);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("bnameTwo") + 1),
        FALSE, 0, 0, 0, FALSE);
    ASSERT(STATUS_BUFFER_TOO_SMALL == Result);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("bnameTwo") + 1 + strlen("bnameTwo")),
        FALSE, 0, 0, 0, FALSE);
    ASSERT(STATUS_BUFFER_OVERFLOW == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(1 == Context.Count);
    ASSERT(0 == Context.EaCount[0]);
    ASSERT(1 == Context.EaCount[1]);
    ASSERT(0 == Context.EaCount[2]);
    ASSERT(0 == Context.EaCount[3]);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("Cn3") + 1),
        FALSE, 0, 0, 0, FALSE);
    ASSERT(STATUS_BUFFER_TOO_SMALL == Result);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("Cn3") + 1 + strlen("third")),
        FALSE, 0, 0, 0, FALSE);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(1 == Context.Count);
    ASSERT(0 == Context.EaCount[0]);
    ASSERT(0 == Context.EaCount[1]);
    ASSERT(1 == Context.EaCount[2]);
    ASSERT(0 == Context.EaCount[3]);

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb, &Ea, sizeof Ea, FALSE, 0, 0, 0, FALSE);
    ASSERT(STATUS_NO_MORE_EAS == Result);

    memset(&Context, 0, sizeof Context);
    EaIndex = 0;
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, sizeof Ea,
        FALSE, 0, 0, &EaIndex, FALSE);
    ASSERT(STATUS_NONEXISTENT_EA_ENTRY == Result);

    memset(&Context, 0, sizeof Context);
    EaIndex = 1;
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("Aname1") + 1),
        FALSE, 0, 0, &EaIndex, FALSE);
    ASSERT(STATUS_BUFFER_TOO_SMALL == Result);

    memset(&Context, 0, sizeof Context);
    EaIndex = 1;
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("Aname1") + 1 + strlen("first")),
        FALSE, 0, 0, &EaIndex, FALSE);
    ASSERT(STATUS_BUFFER_OVERFLOW == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(1 == Context.Count);
    ASSERT(1 == Context.EaCount[0]);
    ASSERT(0 == Context.EaCount[1]);
    ASSERT(0 == Context.EaCount[2]);
    ASSERT(0 == Context.EaCount[3]);

    memset(&Context, 0, sizeof Context);
    EaIndex = 2;
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("bnameTwo") + 1),
        FALSE, 0, 0, &EaIndex, FALSE);
    ASSERT(STATUS_BUFFER_TOO_SMALL == Result);

    memset(&Context, 0, sizeof Context);
    EaIndex = 2;
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("bnameTwo") + 1 + strlen("bnameTwo")),
        FALSE, 0, 0, &EaIndex, FALSE);
    ASSERT(STATUS_BUFFER_OVERFLOW == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(1 == Context.Count);
    ASSERT(0 == Context.EaCount[0]);
    ASSERT(1 == Context.EaCount[1]);
    ASSERT(0 == Context.EaCount[2]);
    ASSERT(0 == Context.EaCount[3]);

    memset(&Context, 0, sizeof Context);
    EaIndex = 3;
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("Cn3") + 1),
        FALSE, 0, 0, &EaIndex, FALSE);
    ASSERT(STATUS_BUFFER_TOO_SMALL == Result);

    memset(&Context, 0, sizeof Context);
    EaIndex = 3;
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, (ULONG)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + strlen("Cn3") + 1 + strlen("third")),
        FALSE, 0, 0, &EaIndex, FALSE);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(1 == Context.Count);
    ASSERT(0 == Context.EaCount[0]);
    ASSERT(0 == Context.EaCount[1]);
    ASSERT(1 == Context.EaCount[2]);
    ASSERT(0 == Context.EaCount[3]);

    memset(&Context, 0, sizeof Context);
    EaIndex = 4;
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, sizeof Ea,
        FALSE, 0, 0, &EaIndex, FALSE);
    ASSERT(STATUS_NO_MORE_EAS == Result);

    memset(&Context, 0, sizeof Context);
    EaIndex = 5;
    Result = NtQueryEaFile(Handle, &Iosb,
        &Ea, sizeof Ea,
        FALSE, 0, 0, &EaIndex, FALSE);
    ASSERT(STATUS_NONEXISTENT_EA_ENTRY == Result);
}

static void ea_init_ea2(
    PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength, PULONG PBytesTransferred)
{
    union
    {
        FILE_FULL_EA_INFORMATION V;
        UINT8 B[128];
    } SingleEa;

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.EaNameLength = (UCHAR)strlen("Aname1");
    SingleEa.V.EaValueLength = (USHORT)strlen("ValueForAname1");
    lstrcpyA(SingleEa.V.EaName, "Aname1");
    memcpy(SingleEa.V.EaName + SingleEa.V.EaNameLength + 1, "ValueForAname1", SingleEa.V.EaValueLength);
    FspFileSystemAddEa(&SingleEa.V, Ea, EaLength, PBytesTransferred);

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.EaNameLength = (UCHAR)strlen("bnameTwo");
    SingleEa.V.EaValueLength = (USHORT)strlen("ValueForBNameTwo");
    lstrcpyA(SingleEa.V.EaName, "bnameTwo");
    memcpy(SingleEa.V.EaName + SingleEa.V.EaNameLength + 1, "ValueForBNameTwo", SingleEa.V.EaValueLength);
    FspFileSystemAddEa(&SingleEa.V, Ea, EaLength, PBytesTransferred);

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.EaNameLength = (UCHAR)strlen("Cn3");
    SingleEa.V.EaValueLength = 0;
    lstrcpyA(SingleEa.V.EaName, "Cn3");
    FspFileSystemAddEa(&SingleEa.V, Ea, EaLength, PBytesTransferred);

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.EaNameLength = (UCHAR)strlen("dn4");
    SingleEa.V.EaValueLength = (USHORT)strlen("ValueForDn4");
    lstrcpyA(SingleEa.V.EaName, "dn4");
    memcpy(SingleEa.V.EaName + SingleEa.V.EaNameLength + 1, "ValueForDn4", SingleEa.V.EaValueLength);
    FspFileSystemAddEa(&SingleEa.V, Ea, EaLength, PBytesTransferred);

    FspFileSystemAddEa(0, Ea, EaLength, PBytesTransferred);
}

static NTSTATUS ea_check_ea2_enumerate(
    FSP_FILE_SYSTEM *FileSystem, PVOID Context0,
    PFILE_FULL_EA_INFORMATION SingleEa)
{
    struct ea_check_ea_context *Context = Context0;

    if (0 == strcmp(SingleEa->EaName, "ANAME1"))
    {
        ASSERT(0 == SingleEa->Flags);
        ASSERT(SingleEa->EaNameLength == (UCHAR)strlen("ANAME1"));
        ASSERT(SingleEa->EaValueLength == (UCHAR)strlen("ValueForAname1"));
        ASSERT(0 == memcmp(SingleEa->EaName + SingleEa->EaNameLength + 1, "ValueForAname1", SingleEa->EaValueLength));
        Context->EaCount[0]++;
    }

    if (0 == strcmp(SingleEa->EaName, "BNAMETWO"))
    {
        ASSERT(0 == SingleEa->Flags);
        ASSERT(SingleEa->EaNameLength == (UCHAR)strlen("BNAMETWO"));
        ASSERT(SingleEa->EaValueLength == (UCHAR)strlen("ValueForBNameTwo"));
        ASSERT(0 == memcmp(SingleEa->EaName + SingleEa->EaNameLength + 1, "ValueForBNameTwo", SingleEa->EaValueLength));
        Context->EaCount[1]++;
    }

    if (0 == strcmp(SingleEa->EaName, "CN3"))
    {
        ASSERT(0 == SingleEa->Flags);
        ASSERT(SingleEa->EaNameLength == (UCHAR)strlen("CN3"));
        ASSERT(SingleEa->EaValueLength == 0);
        Context->EaCount[2]++;
    }

    if (0 == strcmp(SingleEa->EaName, "DN4"))
    {
        ASSERT(0 == SingleEa->Flags);
        ASSERT(SingleEa->EaNameLength == (UCHAR)strlen("DN4"));
        ASSERT(SingleEa->EaValueLength == (UCHAR)strlen("ValueForDn4"));
        ASSERT(0 == memcmp(SingleEa->EaName + SingleEa->EaNameLength + 1, "ValueForDn4", SingleEa->EaValueLength));
        Context->EaCount[3]++;
    }

    Context->Count++;

    return STATUS_SUCCESS;
}

static void ea_check_ea2(HANDLE Handle)
{
    NTSTATUS Result;
    IO_STATUS_BLOCK Iosb;
    union
    {
        FILE_FULL_EA_INFORMATION V;
        UINT8 B[512];
    } Ea;
    struct ea_check_ea_context Context;

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb, &Ea, sizeof Ea, FALSE, 0, 0, 0, TRUE);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea2_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(3 == Context.Count);
    ASSERT(1 == Context.EaCount[0]);
    ASSERT(1 == Context.EaCount[1]);
    ASSERT(0 == Context.EaCount[2]);
    ASSERT(1 == Context.EaCount[3]);
}

static void ea_create_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE DirHandle, FileHandle;
    NTSTATUS Result;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];
    WCHAR UnicodePathBuf[MAX_PATH] = L"file2";
    UNICODE_STRING UnicodePath;
    OBJECT_ATTRIBUTES Obja;
    IO_STATUS_BLOCK Iosb;
    LARGE_INTEGER LargeZero = { 0 };
    union
    {
        FILE_FULL_EA_INFORMATION V;
        UINT8 B[512];
    } Ea;
    ULONG EaLength;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    DirHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != DirHandle);

    UnicodePath.Length = (USHORT)wcslen(UnicodePathBuf) * sizeof(WCHAR);
    UnicodePath.MaximumLength = sizeof UnicodePathBuf;
    UnicodePath.Buffer = UnicodePathBuf;
    InitializeObjectAttributes(&Obja, &UnicodePath, 0, DirHandle, 0);

    EaLength = 0;
    ea_init_bad_ea(&Ea.V, sizeof Ea, &EaLength);
    Result = NtCreateFile(&FileHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, &Obja, &Iosb,
        &LargeZero, FILE_ATTRIBUTE_NORMAL, 0,
        FILE_CREATE, 0,
        &Ea, EaLength);
    ASSERT(STATUS_INVALID_EA_NAME == Result);

    EaLength = 0;
    ea_init_ea(&Ea.V, sizeof Ea, &EaLength);

    Result = NtCreateFile(&FileHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, &Obja, &Iosb,
        &LargeZero, FILE_ATTRIBUTE_NORMAL, 0,
        FILE_CREATE, FILE_NO_EA_KNOWLEDGE,
        &Ea, EaLength);
    ASSERT(STATUS_ACCESS_DENIED == Result);

    Result = NtCreateFile(&FileHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, &Obja, &Iosb,
        &LargeZero, FILE_ATTRIBUTE_NORMAL, 0,
        FILE_CREATE, 0,
        &Ea, EaLength);
    ASSERT(STATUS_SUCCESS == Result);
    CloseHandle(FileHandle);

    if (!OptFuseExternal)
    {
        /* FUSE has no concept of FILE_NEED_EA */

        Result = NtCreateFile(&FileHandle,
            FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, &Obja, &Iosb,
            &LargeZero, FILE_ATTRIBUTE_NORMAL, 0,
            FILE_OPEN, FILE_NO_EA_KNOWLEDGE,
            0, 0);
        ASSERT(STATUS_ACCESS_DENIED == Result);
    }

    Result = NtCreateFile(&FileHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, &Obja, &Iosb,
        &LargeZero, FILE_ATTRIBUTE_NORMAL, 0,
        FILE_OPEN, FILE_DELETE_ON_CLOSE,
        0, 0);
    ASSERT(STATUS_SUCCESS == Result);
    ea_check_ea(FileHandle);
    CloseHandle(FileHandle);

    CloseHandle(DirHandle);

    DirHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == DirHandle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    memfs_stop(memfs);
}

static void ea_create_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        ea_create_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        ea_create_dotest(MemfsDisk, 0, 0);
        ea_create_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        ea_create_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        ea_create_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void ea_overwrite_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE DirHandle, FileHandle;
    NTSTATUS Result;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];
    WCHAR UnicodePathBuf[MAX_PATH] = L"file2";
    UNICODE_STRING UnicodePath;
    OBJECT_ATTRIBUTES Obja;
    IO_STATUS_BLOCK Iosb;
    LARGE_INTEGER LargeZero = { 0 };
    union
    {
        FILE_FULL_EA_INFORMATION V;
        UINT8 B[512];
    } Ea;
    ULONG EaLength;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    DirHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != DirHandle);

    UnicodePath.Length = (USHORT)wcslen(UnicodePathBuf) * sizeof(WCHAR);
    UnicodePath.MaximumLength = sizeof UnicodePathBuf;
    UnicodePath.Buffer = UnicodePathBuf;
    InitializeObjectAttributes(&Obja, &UnicodePath, 0, DirHandle, 0);

    Result = NtCreateFile(&FileHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, &Obja, &Iosb,
        &LargeZero, FILE_ATTRIBUTE_NORMAL, 0,
        FILE_CREATE, 0,
        0, 0);
    ASSERT(STATUS_SUCCESS == Result);
    CloseHandle(FileHandle);

    EaLength = 0;
    ea_init_ea(&Ea.V, sizeof Ea, &EaLength);
    Result = NtCreateFile(&FileHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, &Obja, &Iosb,
        &LargeZero, FILE_ATTRIBUTE_NORMAL, 0,
        FILE_OVERWRITE, 0,
        &Ea, EaLength);
    ASSERT(STATUS_SUCCESS == Result);
    ea_check_ea(FileHandle);
    CloseHandle(FileHandle);

    EaLength = 0;
    ea_init_ea2(&Ea.V, sizeof Ea, &EaLength);
    Result = NtCreateFile(&FileHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, &Obja, &Iosb,
        &LargeZero, FILE_ATTRIBUTE_NORMAL, 0,
        FILE_OVERWRITE, FILE_DELETE_ON_CLOSE,
        &Ea, EaLength);
    ASSERT(STATUS_SUCCESS == Result);
    ea_check_ea2(FileHandle);
    CloseHandle(FileHandle);

    CloseHandle(DirHandle);

    DirHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == DirHandle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    memfs_stop(memfs);
}

static void ea_overwrite_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        ea_overwrite_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        ea_overwrite_dotest(MemfsDisk, 0, 0);
        ea_overwrite_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        ea_overwrite_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        ea_overwrite_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void ea_getset_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE DirHandle, FileHandle;
    NTSTATUS Result;
    BOOLEAN Success;
    IO_STATUS_BLOCK Iosb;
    WCHAR FilePath[MAX_PATH];
    union
    {
        FILE_FULL_EA_INFORMATION V;
        UINT8 B[512];
    } Ea;
    ULONG EaLength;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    DirHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != DirHandle);

    EaLength = 0;
    ea_init_bad_ea(&Ea.V, sizeof Ea, &EaLength);
    Result = NtSetEaFile(DirHandle, &Iosb, &Ea, EaLength);
    ASSERT(STATUS_INVALID_EA_NAME == Result);

    EaLength = 0;
    ea_init_ea(&Ea.V, sizeof Ea, &EaLength);
    Result = NtSetEaFile(DirHandle, &Iosb, &Ea, EaLength);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == Iosb.Information);
    ea_check_ea(DirHandle);

    EaLength = 0;
    ea_init_ea2(&Ea.V, sizeof Ea, &EaLength);
    Result = NtSetEaFile(DirHandle, &Iosb, &Ea, EaLength);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == Iosb.Information);
    Result = NtQueryEaFile(DirHandle, &Iosb, &Ea, sizeof Ea, FALSE, 0, 0, 0, FALSE);
    ASSERT(STATUS_EA_CORRUPT_ERROR == Result);
    ea_check_ea2(DirHandle);

    CloseHandle(DirHandle);

    DirHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == DirHandle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    FileHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != FileHandle);

    EaLength = 0;
    ea_init_bad_ea(&Ea.V, sizeof Ea, &EaLength);
    Result = NtSetEaFile(FileHandle, &Iosb, &Ea, EaLength);
    ASSERT(STATUS_INVALID_EA_NAME == Result);

    EaLength = 0;
    ea_init_ea(&Ea.V, sizeof Ea, &EaLength);
    Result = NtSetEaFile(FileHandle, &Iosb, &Ea, EaLength);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == Iosb.Information);
    ea_check_ea(FileHandle);

    EaLength = 0;
    ea_init_ea2(&Ea.V, sizeof Ea, &EaLength);
    Result = NtSetEaFile(FileHandle, &Iosb, &Ea, EaLength);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == Iosb.Information);
    Result = NtQueryEaFile(FileHandle, &Iosb, &Ea, sizeof Ea, FALSE, 0, 0, 0, FALSE);
    ASSERT(STATUS_EA_CORRUPT_ERROR == Result);
    ea_check_ea2(FileHandle);

    CloseHandle(FileHandle);

    FileHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == FileHandle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    memfs_stop(memfs);
}

static void ea_getset_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        ea_getset_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        ea_getset_dotest(MemfsDisk, 0, 0);
        ea_getset_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        ea_getset_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        ea_getset_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

void ea_tests(void)
{
    TEST_OPT(ea_create_test);
    TEST_OPT(ea_overwrite_test);
    TEST_OPT(ea_getset_test);
}
