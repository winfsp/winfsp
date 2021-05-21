/**
 * @file launch-test.c
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

#include <winfsp/launch.h>
#include <tlib/testsuite.h>

#include "winfsp-tests.h"

static void launch_reg_test(void)
{
    NTSTATUS Result;
    PWSTR ClassName = L"winfsp-tests-launch-reg-test";
    FSP_LAUNCH_REG_RECORD RecordBuf = { 0 }, *Record;

    Result = FspLaunchRegGetRecord(ClassName, 0, &Record);
    ASSERT(STATUS_OBJECT_NAME_NOT_FOUND == Result);
    ASSERT(0 == Record);

    RecordBuf.Executable = 0;
    Result = FspLaunchRegSetRecord(ClassName, &RecordBuf);
    ASSERT(STATUS_INVALID_PARAMETER == Result);

    RecordBuf.Executable = L"Executable";
    RecordBuf.CommandLine = L"CommandLine";
    RecordBuf.WorkDirectory = L"WorkDirectory";
    RecordBuf.RunAs = L"RunAs";
    RecordBuf.Security = L"Security";
    RecordBuf.JobControl = 1;
    RecordBuf.Credentials = 42;
    Result = FspLaunchRegSetRecord(ClassName, &RecordBuf);
    if (STATUS_ACCESS_DENIED == Result)
    {
        FspDebugLog(__FUNCTION__ ": need Administrator\n");
        return;
    }
    ASSERT(NT_SUCCESS(Result));

    Result = FspLaunchRegGetRecord(ClassName, 0, &Record);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 != Record);
    ASSERT(0 == Record->Agent);
    ASSERT(0 == wcscmp(RecordBuf.Executable, Record->Executable));
    ASSERT(0 == wcscmp(RecordBuf.CommandLine, Record->CommandLine));
    ASSERT(0 == wcscmp(RecordBuf.WorkDirectory, Record->WorkDirectory));
    ASSERT(0 == wcscmp(RecordBuf.RunAs, Record->RunAs));
    ASSERT(0 == wcscmp(RecordBuf.Security, Record->Security));
    ASSERT(RecordBuf.JobControl == Record->JobControl);
    ASSERT(RecordBuf.Credentials == Record->Credentials);
    FspLaunchRegFreeRecord(Record);

    Result = FspLaunchRegSetRecord(ClassName, &RecordBuf);
    ASSERT(NT_SUCCESS(Result));

    Result = FspLaunchRegGetRecord(ClassName, 0, &Record);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 != Record);
    ASSERT(0 == Record->Agent);
    ASSERT(0 == wcscmp(RecordBuf.Executable, Record->Executable));
    ASSERT(0 == wcscmp(RecordBuf.CommandLine, Record->CommandLine));
    ASSERT(0 == wcscmp(RecordBuf.WorkDirectory, Record->WorkDirectory));
    ASSERT(0 == wcscmp(RecordBuf.RunAs, Record->RunAs));
    ASSERT(0 == wcscmp(RecordBuf.Security, Record->Security));
    ASSERT(RecordBuf.JobControl == Record->JobControl);
    ASSERT(RecordBuf.Credentials == Record->Credentials);
    FspLaunchRegFreeRecord(Record);

    RecordBuf.Security = 0;
    RecordBuf.Credentials = 0;
    Result = FspLaunchRegSetRecord(ClassName, &RecordBuf);
    ASSERT(NT_SUCCESS(Result));

    Result = FspLaunchRegGetRecord(ClassName, 0, &Record);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 != Record);
    ASSERT(0 == Record->Agent);
    ASSERT(0 == wcscmp(RecordBuf.Executable, Record->Executable));
    ASSERT(0 == wcscmp(RecordBuf.CommandLine, Record->CommandLine));
    ASSERT(0 == wcscmp(RecordBuf.WorkDirectory, Record->WorkDirectory));
    ASSERT(0 == wcscmp(RecordBuf.RunAs, Record->RunAs));
    ASSERT(0 == Record->Security);
    ASSERT(RecordBuf.JobControl == Record->JobControl);
    ASSERT(RecordBuf.Credentials == Record->Credentials);
    FspLaunchRegFreeRecord(Record);

    Result = FspLaunchRegSetRecord(ClassName, 0);
    ASSERT(NT_SUCCESS(Result));

    Result = FspLaunchRegGetRecord(ClassName, 0, &Record);
    ASSERT(STATUS_OBJECT_NAME_NOT_FOUND == Result);
    ASSERT(0 == Record);

    Result = FspLaunchRegSetRecord(ClassName, 0);
    ASSERT(NT_SUCCESS(Result));

    Result = FspLaunchRegGetRecord(ClassName, 0, &Record);
    ASSERT(STATUS_OBJECT_NAME_NOT_FOUND == Result);
    ASSERT(0 == Record);

    RecordBuf.Agent = L"Agent1,Agent2";
    Result = FspLaunchRegSetRecord(ClassName, &RecordBuf);
    ASSERT(NT_SUCCESS(Result));

    Result = FspLaunchRegGetRecord(ClassName, L"Agent", &Record);
    ASSERT(STATUS_OBJECT_NAME_NOT_FOUND == Result);
    ASSERT(0 == Record);

    Result = FspLaunchRegGetRecord(ClassName, L"Agent1", &Record);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 != Record);
    ASSERT(0 == wcscmp(RecordBuf.Agent, Record->Agent));
    ASSERT(0 == wcscmp(RecordBuf.Executable, Record->Executable));
    ASSERT(0 == wcscmp(RecordBuf.CommandLine, Record->CommandLine));
    ASSERT(0 == wcscmp(RecordBuf.WorkDirectory, Record->WorkDirectory));
    ASSERT(0 == wcscmp(RecordBuf.RunAs, Record->RunAs));
    ASSERT(0 == Record->Security);
    ASSERT(RecordBuf.JobControl == Record->JobControl);
    ASSERT(RecordBuf.Credentials == Record->Credentials);
    FspLaunchRegFreeRecord(Record);

    Result = FspLaunchRegGetRecord(ClassName, 0, &Record);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 != Record);
    ASSERT(0 == wcscmp(RecordBuf.Agent, Record->Agent));
    ASSERT(0 == wcscmp(RecordBuf.Executable, Record->Executable));
    ASSERT(0 == wcscmp(RecordBuf.CommandLine, Record->CommandLine));
    ASSERT(0 == wcscmp(RecordBuf.WorkDirectory, Record->WorkDirectory));
    ASSERT(0 == wcscmp(RecordBuf.RunAs, Record->RunAs));
    ASSERT(0 == Record->Security);
    ASSERT(RecordBuf.JobControl == Record->JobControl);
    ASSERT(RecordBuf.Credentials == Record->Credentials);
    FspLaunchRegFreeRecord(Record);

    Result = FspLaunchRegSetRecord(ClassName, 0);
    ASSERT(NT_SUCCESS(Result));

    Result = FspLaunchRegGetRecord(ClassName, 0, &Record);
    ASSERT(STATUS_OBJECT_NAME_NOT_FOUND == Result);
    ASSERT(0 == Record);
}

static void launch_test(void)
{
    /* this test assumes that memfs32 is registered */

    NTSTATUS Result;
    ULONG LauncherError;
    PWSTR Argv[2];
    FSP_LAUNCH_REG_RECORD *Record;
    WCHAR Buffer[1024];
    ULONG Size;
    ULONG FoundClass, FoundInst1, FoundInst2;

    Result = FspLaunchRegGetRecord(L"memfs32", 0, &Record);
    if (STATUS_OBJECT_NAME_NOT_FOUND == Result)
    {
        FspDebugLog(__FUNCTION__ ": need memfs32 registration\n");
        return;
    }
    ASSERT(NT_SUCCESS(Result));

    Argv[0] = L"";
    Argv[1] = L"*";

    Result = FspLaunchStart(L"memfs32", L"winfsp-tests-share1", 2, Argv, FALSE, &LauncherError);
    if (STATUS_OBJECT_NAME_NOT_FOUND == Result)
    {
        FspDebugLog(__FUNCTION__ ": need WinFsp.Launcher\n");
        return;
    }
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 == LauncherError);

    Result = FspLaunchStart(L"memfs32", L"winfsp-tests-share2", 2, Argv, FALSE, &LauncherError);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 == LauncherError);

    Result = FspLaunchStart(L"memfs32", L"winfsp-tests-share1", 2, Argv, FALSE, &LauncherError);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(ERROR_ALREADY_EXISTS == LauncherError);

    Result = FspLaunchStart(L"memfs32", L"winfsp-tests-share2", 2, Argv, FALSE, &LauncherError);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(ERROR_ALREADY_EXISTS == LauncherError);

    Size = sizeof Buffer;
    Result = FspLaunchGetInfo(L"memfs32", L"winfsp-tests-share1", Buffer, &Size, &LauncherError);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 == LauncherError);
    ASSERT((wcslen(L"memfs32") + 1) * sizeof(WCHAR) < Size);
    ASSERT((wcslen(L"winfsp-tests-share1") + 1) * sizeof(WCHAR) < Size);
    ASSERT(0 == wcscmp(L"memfs32", Buffer));
    ASSERT(0 == wcscmp(L"winfsp-tests-share1", Buffer + 8));

    Size = sizeof Buffer;
    Result = FspLaunchGetInfo(L"memfs32", L"winfsp-tests-share2", Buffer, &Size, &LauncherError);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 == LauncherError);
    ASSERT((wcslen(L"memfs32") + 1) * sizeof(WCHAR) < Size);
    ASSERT((wcslen(L"winfsp-tests-share2") + 1) * sizeof(WCHAR) < Size);
    ASSERT(0 == wcscmp(L"memfs32", Buffer));
    ASSERT(0 == wcscmp(L"winfsp-tests-share2", Buffer + 8));

    Size = sizeof Buffer;
    Result = FspLaunchGetNameList(Buffer, &Size, &LauncherError);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 == LauncherError);
    FoundClass = FoundInst1 = FoundInst2 = 0;
    for (PWSTR P = Buffer, EndP = (PVOID)((PUINT8)P + Size), Part = P; EndP > P; P++)
        if (L'\0' == *P)
        {
            if (0 == wcscmp(L"memfs32", Part))
                FoundClass++;
            else if (0 == wcscmp(L"winfsp-tests-share1", Part))
                FoundInst1++;
            else if (0 == wcscmp(L"winfsp-tests-share2", Part))
                FoundInst2++;
            Part = P + 1;
        }
    ASSERT(2 == FoundClass);
    ASSERT(1 == FoundInst1);
    ASSERT(1 == FoundInst2);

    Result = FspLaunchStop(L"memfs32", L"winfsp-tests-share1", &LauncherError);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 == LauncherError);

    Result = FspLaunchStop(L"memfs32", L"winfsp-tests-share2", &LauncherError);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 == LauncherError);

    /* give the launcher a chance to stop the file systems! */
    Sleep(3000);

    Size = sizeof Buffer;
    Result = FspLaunchGetInfo(L"memfs32", L"winfsp-tests-share1", Buffer, &Size, &LauncherError);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(ERROR_FILE_NOT_FOUND == LauncherError);

    Size = sizeof Buffer;
    Result = FspLaunchGetInfo(L"memfs32", L"winfsp-tests-share2", Buffer, &Size, &LauncherError);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(ERROR_FILE_NOT_FOUND == LauncherError);

    Result = FspLaunchStop(L"memfs32", L"winfsp-tests-share1", &LauncherError);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(ERROR_FILE_NOT_FOUND == LauncherError);

    Result = FspLaunchStop(L"memfs32", L"winfsp-tests-share2", &LauncherError);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(ERROR_FILE_NOT_FOUND == LauncherError);

    Size = sizeof Buffer;
    Result = FspLaunchGetNameList(Buffer, &Size, &LauncherError);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 == LauncherError);
    FoundClass = FoundInst1 = FoundInst2 = 0;
    for (PWSTR P = Buffer, EndP = (PVOID)((PUINT8)P + Size), Part = P; EndP > P; P++)
        if (L'\0' == *P)
        {
            if (0 == wcscmp(L"memfs32", Part))
                FoundClass++;
            else if (0 == wcscmp(L"winfsp-tests-share1", Part))
                FoundInst1++;
            else if (0 == wcscmp(L"winfsp-tests-share2", Part))
                FoundInst2++;
            Part = P + 1;
        }
    ASSERT(0 == FoundClass);
    ASSERT(0 == FoundInst1);
    ASSERT(0 == FoundInst2);
}

void launch_tests(void)
{
    if (OptExternal)
        return;

    TEST(launch_reg_test);
    //TEST(launch_test);
}
