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
        UINT8 B[128];
    } SingleEa;
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

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.EaNameLength = (UCHAR)strlen("name1");
    SingleEa.V.EaValueLength = (USHORT)strlen("first");
    lstrcpyA(SingleEa.V.EaName, "name1");
    memcpy(SingleEa.V.EaName + SingleEa.V.EaNameLength + 1, "first", SingleEa.V.EaValueLength);
    FspFileSystemAddEa(&SingleEa.V, &Ea.V, sizeof Ea, &EaLength);

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.EaNameLength = (UCHAR)strlen("nameTwo");
    SingleEa.V.EaValueLength = (USHORT)strlen("second");
    lstrcpyA(SingleEa.V.EaName, "nameTwo");
    memcpy(SingleEa.V.EaName + SingleEa.V.EaNameLength + 1, "second", SingleEa.V.EaValueLength);
    FspFileSystemAddEa(&SingleEa.V, &Ea.V, sizeof Ea, &EaLength);

    memset(&SingleEa, 0, sizeof SingleEa);
    SingleEa.V.EaNameLength = (UCHAR)strlen("n3");
    SingleEa.V.EaValueLength = (USHORT)strlen("third");
    lstrcpyA(SingleEa.V.EaName, "n3");
    memcpy(SingleEa.V.EaName + SingleEa.V.EaNameLength + 1, "third", SingleEa.V.EaValueLength);
    FspFileSystemAddEa(&SingleEa.V, &Ea.V, sizeof Ea, &EaLength);

    FspFileSystemAddEa(0, &Ea.V, sizeof Ea, &EaLength);

    UnicodePath.Length = (USHORT)wcslen(UnicodePathBuf) * sizeof(WCHAR);
    UnicodePath.MaximumLength = sizeof UnicodePathBuf;
    UnicodePath.Buffer = UnicodePathBuf;
    InitializeObjectAttributes(&Obja, &UnicodePath, 0, DirHandle, 0);
    Result = NtCreateFile(&FileHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, &Obja, &Iosb,
        &LargeZero, FILE_ATTRIBUTE_NORMAL, 0,
        FILE_CREATE, FILE_DELETE_ON_CLOSE,
        &Ea, EaLength);
    ASSERT(STATUS_SUCCESS == Result);
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
