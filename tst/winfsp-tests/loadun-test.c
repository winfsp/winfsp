/**
 * @file loadun-test.c
 *
 * @copyright 2015-2022 Bill Zissimopoulos
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

static void load_unload_test(void)
{
    /* this is not a real test! */

    FspFsctlStartService();
    FspFsctlStartService();

    FspFsctlStopService();
    FspFsctlStopService();
}

void load_unload_tests(void)
{
    if (OptExternal)
        return;

    /*
     * An attempt to unload the driver while other tests are executing can make all tests fail.
     * For this reason we do not enable this test, except when doing specialized testing.
     */
    //TEST_OPT(load_unload_test);
}
