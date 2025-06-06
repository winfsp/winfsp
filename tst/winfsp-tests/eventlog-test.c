/**
 * @file eventlog-test.c
 *
 * @copyright 2015-2025 Bill Zissimopoulos
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

void eventlog_test(void)
{
    /* this is not a real test! */

    FspEventLog(EVENTLOG_INFORMATION_TYPE, L"EventLog %s message", L"informational");
    FspEventLog(EVENTLOG_WARNING_TYPE, L"EventLog %s message", L"warning");
    FspEventLog(EVENTLOG_ERROR_TYPE, L"EventLog %s message", L"error");
}

void eventlog_tests(void)
{
    if (OptExternal)
        return;

    TEST_OPT(eventlog_test);
}
