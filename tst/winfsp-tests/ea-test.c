/**
 * @file ea-test.c
 *
 * @copyright 2015-2019 Bill Zissimopoulos
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

static void ea_init_ea(
    PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength, PULONG PBytesTransferred)
{
    union
    {
        FILE_FULL_EA_INFORMATION V;
        UINT8 B[128];
    } SingleEa;

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.EaNameLength = (UCHAR)strlen("name1");
    SingleEa.V.EaValueLength = (USHORT)strlen("first");
    lstrcpyA(SingleEa.V.EaName, "name1");
    memcpy(SingleEa.V.EaName + SingleEa.V.EaNameLength + 1, "first", SingleEa.V.EaValueLength);
    FspFileSystemAddEa(&SingleEa.V, Ea, EaLength, PBytesTransferred);

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.Flags = FILE_NEED_EA;
    SingleEa.V.EaNameLength = (UCHAR)strlen("nameTwo");
    SingleEa.V.EaValueLength = (USHORT)strlen("second");
    lstrcpyA(SingleEa.V.EaName, "nameTwo");
    memcpy(SingleEa.V.EaName + SingleEa.V.EaNameLength + 1, "second", SingleEa.V.EaValueLength);
    FspFileSystemAddEa(&SingleEa.V, Ea, EaLength, PBytesTransferred);

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.EaNameLength = (UCHAR)strlen("n3");
    SingleEa.V.EaValueLength = (USHORT)strlen("third");
    lstrcpyA(SingleEa.V.EaName, "n3");
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

    if (0 == strcmp(SingleEa->EaName, "NAME1"))
    {
        ASSERT(SingleEa->EaNameLength == (UCHAR)strlen("NAME1"));
        ASSERT(SingleEa->EaValueLength == (UCHAR)strlen("first"));
        ASSERT(0 == memcmp(SingleEa->EaName + SingleEa->EaNameLength + 1, "first", SingleEa->EaValueLength));
        Context->EaCount[0]++;
    }

    if (0 == strcmp(SingleEa->EaName, "NAMETWO"))
    {
        ASSERT(SingleEa->EaNameLength == (UCHAR)strlen("NAMETWO"));
        ASSERT(SingleEa->EaValueLength == (UCHAR)strlen("second"));
        ASSERT(0 == memcmp(SingleEa->EaName + SingleEa->EaNameLength + 1, "second", SingleEa->EaValueLength));
        Context->EaCount[1]++;
    }

    if (0 == strcmp(SingleEa->EaName, "N3"))
    {
        ASSERT(SingleEa->EaNameLength == (UCHAR)strlen("N3"));
        ASSERT(SingleEa->EaValueLength == (UCHAR)strlen("third"));
        ASSERT(0 == memcmp(SingleEa->EaName + SingleEa->EaNameLength + 1, "third", SingleEa->EaValueLength));
        Context->EaCount[2]++;
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
        UINT8 B[128];
    } GetEa;
    ULONG EaLength = 0;
    struct ea_check_ea_context Context;

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb, &Ea, sizeof Ea, FALSE, 0, 0, 0, FALSE);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(3 == Context.Count);
    ASSERT(1 == Context.EaCount[0]);
    ASSERT(1 == Context.EaCount[1]);
    ASSERT(1 == Context.EaCount[2]);

    memset(&GetEa, 0, sizeof GetEa);
    GetEa.V.EaNameLength = (UCHAR)strlen("nameTwo");
    lstrcpyA(GetEa.V.EaName, "nameTwo");
    EaLength = FIELD_OFFSET(FILE_GET_EA_INFORMATION, EaName) + GetEa.V.EaNameLength + 1;

    memset(&Context, 0, sizeof Context);
    Result = NtQueryEaFile(Handle, &Iosb, &Ea, sizeof Ea, FALSE, &GetEa.V, EaLength, 0, FALSE);
    ASSERT(STATUS_SUCCESS == Result);
    Result = FspFileSystemEnumerateEa(0, ea_check_ea_enumerate, &Context, &Ea.V, (ULONG)Iosb.Information);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(1 == Context.Count);
    ASSERT(0 == Context.EaCount[0]);
    ASSERT(1 == Context.EaCount[1]);
    ASSERT(0 == Context.EaCount[2]);
}

static void ea_create_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

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
    ULONG EaLength = 0;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    DirHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != DirHandle);

    ea_init_ea(&Ea.V, sizeof Ea, &EaLength);

    UnicodePath.Length = (USHORT)wcslen(UnicodePathBuf) * sizeof(WCHAR);
    UnicodePath.MaximumLength = sizeof UnicodePathBuf;
    UnicodePath.Buffer = UnicodePathBuf;
    InitializeObjectAttributes(&Obja, &UnicodePath, 0, DirHandle, 0);

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

    Result = NtCreateFile(&FileHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, &Obja, &Iosb,
        &LargeZero, FILE_ATTRIBUTE_NORMAL, 0,
        FILE_OPEN, FILE_NO_EA_KNOWLEDGE,
        0, 0);
    ASSERT(STATUS_ACCESS_DENIED == Result);

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
        ea_create_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        ea_create_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        ea_create_dotest(MemfsNet, L"\\\\memfs\\share");
}

void ea_tests(void)
{
    TEST(ea_create_test);
}
