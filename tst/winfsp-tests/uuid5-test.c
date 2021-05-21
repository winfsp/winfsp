/**
 * @file uuid5-test.c
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

#pragma comment(lib, "bcrypt.lib")
#include <shared/ku/uuid5.c>

static void uuid5_test(void)
{
    // 6ba7b810-9dad-11d1-80b4-00c04fd430c8
    static const GUID GuidNs =
        { 0x6ba7b810, 0x9dad, 0x11d1, { 0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8 } };
    // 74738ff5-5367-5958-9aee-98fffdcd1876
    static const GUID Guid0 =
        { 0x74738ff5, 0x5367, 0x5958, { 0x9a, 0xee, 0x98, 0xff, 0xfd, 0xcd, 0x18, 0x76 } };
    // 63b5d721-0b97-5e7a-a550-2f0e589b5478
    static const GUID Guid1 =
        { 0x63b5d721, 0x0b97, 0x5e7a, { 0xa5, 0x50, 0x2f, 0x0e, 0x58, 0x9b, 0x54, 0x78 } };

    NTSTATUS Result;
    GUID Guid;

    Result = FspUuid5Make(&GuidNs, "www.example.org", 15, &Guid);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(IsEqualGUID(&Guid0, &Guid));

    Result = FspUuid5Make(&FspFsvrtDeviceClassGuid, "hello", 5, &Guid);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(IsEqualGUID(&Guid1, &Guid));
}

void uuid5_tests(void)
{
    if (OptExternal)
        return;

    TEST(uuid5_test);
}
