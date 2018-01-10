/**
 * @file launch-test.c
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
}

void launch_tests(void)
{
    if (OptExternal)
        return;

    TEST(launch_reg_test);
    TEST(launch_test);
}
