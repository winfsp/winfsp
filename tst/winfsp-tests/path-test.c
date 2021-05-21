/**
 * @file path-test.c
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

void path_prefix_test(void)
{
    PWSTR ipaths[] =
    {
        L"",
        L"\\",
        L"\\\\",
        L"\\a",
        L"\\\\a",
        L"\\\\a\\",
        L"\\\\a\\\\",
        L"a\\",
        L"a\\\\",
        L"a\\b",
        L"a\\\\b",
        L"foo\\\\\\bar\\\\baz",
        L"foo\\\\\\bar\\\\baz\\",
        L"foo\\\\\\bar\\\\baz\\\\",
        L"foo",
    };
    PWSTR opaths[] =
    {
        L"", L"",
        L"ROOT", L"",
        L"ROOT", L"",
        L"ROOT", L"a",
        L"ROOT", L"a",
        L"ROOT", L"a\\",
        L"ROOT", L"a\\\\",
        L"a", L"",
        L"a", L"",
        L"a", L"b",
        L"a", L"b",
        L"foo", L"bar\\\\baz",
        L"foo", L"bar\\\\baz\\",
        L"foo", L"bar\\\\baz\\\\",
        L"foo", L"",
    };

    for (size_t i = 0; sizeof ipaths / sizeof ipaths[0] > i; i++)
    {
        PWSTR Prefix, Remain;
        WCHAR buf[32];
        wcscpy_s(buf, 32, ipaths[i]);
        FspPathPrefix(buf, &Prefix, &Remain, L"ROOT");
        ASSERT(0 == wcscmp(opaths[2 * i + 0], Prefix));
        ASSERT(0 == wcscmp(opaths[2 * i + 1], Remain));
        FspPathCombine(buf, Remain);
        ASSERT(0 == wcscmp(ipaths[i], buf));
    }
}

void path_suffix_test(void)
{
    PWSTR ipaths[] =
    {
        L"",
        L"\\",
        L"\\\\",
        L"\\a",
        L"\\\\a",
        L"\\\\a\\",
        L"\\\\a\\\\",
        L"a\\",
        L"a\\\\",
        L"a\\b",
        L"a\\\\b",
        L"foo\\\\\\bar\\\\baz",
        L"foo\\\\\\bar\\\\baz\\",
        L"foo\\\\\\bar\\\\baz\\\\",
        L"foo",
    };
    PWSTR opaths[] =
    {
        L"", L"",
        L"ROOT", L"",
        L"ROOT", L"",
        L"ROOT", L"a",
        L"ROOT", L"a",
        L"\\\\a", L"",
        L"\\\\a", L"",
        L"a", L"",
        L"a", L"",
        L"a", L"b",
        L"a", L"b",
        L"foo\\\\\\bar", L"baz",
        L"foo\\\\\\bar\\\\baz", L"",
        L"foo\\\\\\bar\\\\baz", L"",
        L"foo", L"",
    };

    for (size_t i = 0; sizeof ipaths / sizeof ipaths[0] > i; i++)
    {
        PWSTR Remain, Suffix;
        WCHAR buf[32];
        wcscpy_s(buf, 32, ipaths[i]);
        FspPathSuffix(buf, &Remain, &Suffix, L"ROOT");
        ASSERT(0 == wcscmp(opaths[2 * i + 0], Remain));
        ASSERT(0 == wcscmp(opaths[2 * i + 1], Suffix));
        FspPathCombine(buf, Suffix);
        ASSERT(0 == wcscmp(ipaths[i], buf));
    }
}

void path_tests(void)
{
    if (OptExternal)
        return;

    TEST(path_prefix_test);
    TEST(path_suffix_test);
}
