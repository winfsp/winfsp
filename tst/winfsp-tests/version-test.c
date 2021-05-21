/**
 * @file version-test.c
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

#include "winfsp-tests.h"

static void version_test(void)
{
    UINT32 Version1, Version2;
    NTSTATUS Result;

    Result = FspVersion(&Version1);
    ASSERT(NT_SUCCESS(Result));

    Result = FspVersion(&Version2);
    ASSERT(NT_SUCCESS(Result));

    ASSERT(Version1 == Version2);

    FspDebugLog(__FUNCTION__ ": FspVersion=%d.%d\n", HIWORD(Version1), LOWORD(Version1));
}

void version_tests(void)
{
    if (OptExternal)
        return;

    TEST(version_test);
}
