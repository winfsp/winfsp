/**
 * @file launcher-ptrans-test.c
 *
 * @copyright 2015-2026 Bill Zissimopoulos
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

#include <launcher/ptrans.c>

static void launcher_ptrans_test(void)
{
    PWSTR ipaths[] =
    {
        L"", 0,
        L"\\foo\\bar", 0,
        L"", L"",
        L"\\foo\\bar", L"",
        L"\\foo\\bar", L"/",
        L"\\foo\\bar", L"/_",
        L"\\foo\\bar", L"/_1",
        L"\\foo\\bar", L"\\_",
        L"\\foo\\bar", L"\\_A",
        L"\\foo\\bar", L"/a",
        L"\\foo\\bar", L"/b",
        L"\\foo\\bar", L"/c",
        L"\\foo\\bar", L"/d",
        L"\\foo\\bar", L"/a:b",
        L"\\foo\\bar", L"/a:b&c!d",
        L"\\foo\\bar", L"/b:a",
        L"\\foo\\bar", L"/d!c&b:a",
        L"\\foo\\bar", L"/a:_",
        L"\\foo\\bar", L"/b:_",
        L"\\foo\\bar", L"/c:_",
        L"\\foo\\bar\\baz", L"/b:_",
        L"\\foo\\bar\\baz\\bag", L"/b:_",
        L"\\foo\\bar\\baz", L"/b:/_",
        L"\\foo\\bar\\baz\\bag", L"/b:/_",
        L"\\foo\\bar\\baz\\bag", L"/a:_:b:_",
        L"\\foo\\bar\\baz\\bag", L"/_:_",
    };
    PWSTR opaths[] =
    {
        L"",
        L"\\foo\\bar",
        L"",
        L"",
        L"",
        L"/foo/bar",
        L"/foo/bar",
        L"\\\\foo\\\\bar",
        L"\\\\foo\\\\bar",
        L"foo",
        L"bar",
        L"",
        L"",
        L"foo:bar",
        L"foo:bar&!",
        L"bar:foo",
        L"!&bar:foo",
        L"foo:bar",
        L"bar:",
        L":",
        L"bar:baz",
        L"bar:baz/bag",
        L"bar:/baz",
        L"bar:/baz/bag",
        L"foo:bar/baz/bag:bar:baz/bag",
        L"/foo/bar/baz/bag:/foo/bar/baz/bag",
    };

    for (size_t i = 0; sizeof ipaths / (sizeof ipaths[0] * 2) > i; i++)
    {
        WCHAR Buf[1024];
        ULONG Length;
        PWSTR Dest;

        Length = (ULONG)(UINT_PTR)PathTransform(0, ipaths[2 * i + 0], ipaths[2 * i + 1], FALSE);
        ASSERT(Length == wcslen(opaths[i]) * sizeof(WCHAR));

        Dest = PathTransform(Buf, ipaths[2 * i + 0], ipaths[2 * i + 1], FALSE);
        *Dest = L'\0';
        ASSERT(Dest == Buf + wcslen(opaths[i]));
        ASSERT(0 == wcscmp(Buf, opaths[i]));
    }
}

static void launcher_ptrans_quote_test(void)
{
    PWSTR ipaths[] =
    {
        L"", 0,
        L"foo", 0,
        L"foo\\", 0,
        L"foo\\\\", 0,
        L"foo\\\\\\", 0,
        L"\\foo\\bar", 0,
        L"foo bar\\", 0,
        L"foo\tbar\\", 0,
        L"foo\\\"", 0,
        L"foo\\\"\\", 0,
        L"foo\\\"bar", 0,
        L"\\foo\\bar\\", L"/b",
        L"\\foo\\bar\\", L"/_",
        L"\\foo\\bar\" baz\\qux\\", L"/b:_",
        L"\\foo", L"/a\\b",
        L"\\foo\\\"", L"/a\\b",
        L"\\foo\\bar", L"/a\\b",
        L"\\foo", L"/a\\\\b",
        L"", L"/\\",
        L"\\foo\\bar\\", L"\\_",
        L"\\foo\\bar\\\\", L"\\_",
        L"\\", L"\\_\\b",
        L"foo\\\\\\\\", 0,
        L"\\", 0,
        L"\"\"", 0,
        L"\\foo", L"",
        L"\\foo", L"/",
        L"\\foo", L"/a\\b\\c",
        L"\\foo\\bar", L"/a\\b\\c",
        L"\\foo", L"/a\\\\\\b",
        L"\\foo", L"/a\\:b",
        L"\\foo", L"/a\\_",
        L"\\foo\\\"", L"/a\\_",
        L"\\foo\\bar\\", L"/a\\_",
        L"\\", L"/\\_",
        L"\"\"", L"/\\_",
        L"\\", L"\\\\_",
        L"\\", L"\\_\\_",
        L"\\foo", L"/a\\z",
        L"\\foo\\\"\\bar", L"/a\\b",
        L"\\foo", L"/a\\1",
        L"\\foo", L"/a\\U",
    };
    /* Unquoted output, quoted output, parsed argument. */
    PWSTR opaths[][3] =
    {
        { L"", L"", L"" },
        { L"foo", L"foo", L"foo" },
        { L"foo\\", L"foo\\\\", L"foo\\" },
        { L"foo\\\\", L"foo\\\\", L"foo\\" },
        { L"foo\\\\\\", L"foo\\\\\\\\", L"foo\\\\" },
        { L"\\foo\\bar", L"\\foo\\bar", L"\\foo\\bar" },
        { L"foo bar\\", L"foo bar\\\\", L"foo bar\\" },
        { L"foo\tbar\\", L"foo\tbar\\\\", L"foo\tbar\\" },
        { L"foo\\", L"foo\\\\", L"foo\\" },
        { L"foo\\\\", L"foo\\\\", L"foo\\" },
        { L"foo\\bar", L"foo\\bar", L"foo\\bar" },
        { L"bar", L"bar", L"bar" },
        { L"/foo/bar/", L"/foo/bar/", L"/foo/bar/" },
        { L"bar baz:qux/", L"bar baz:qux/", L"bar baz:qux/" },
        { L"foo\\", L"foo\\\\", L"foo\\" },
        { L"foo\\", L"foo\\\\", L"foo\\" },
        { L"foo\\bar", L"foo\\bar", L"foo\\bar" },
        { L"foo\\\\", L"foo\\\\", L"foo\\" },
        { L"\\", L"\\\\", L"\\" },
        { L"\\\\foo\\\\bar\\\\", L"\\\\foo\\\\bar\\\\", L"\\\\foo\\\\bar\\" },
        { L"\\\\foo\\\\bar\\\\\\\\", L"\\\\foo\\\\bar\\\\\\\\", L"\\\\foo\\\\bar\\\\" },
        { L"\\\\\\", L"\\\\\\\\", L"\\\\" },
        { L"foo\\\\\\\\", L"foo\\\\\\\\", L"foo\\\\" },
        { L"\\", L"\\\\", L"\\" },
        { L"", L"", L"" },
        { L"", L"", L"" },
        { L"", L"", L"" },
        { L"foo\\\\", L"foo\\\\", L"foo\\" },
        { L"foo\\bar\\", L"foo\\bar\\\\", L"foo\\bar\\" },
        { L"foo\\\\\\", L"foo\\\\\\\\", L"foo\\\\" },
        { L"foo\\:", L"foo\\:", L"foo\\:" },
        { L"foo\\", L"foo\\\\", L"foo\\" },
        { L"foo\\", L"foo\\\\", L"foo\\" },
        { L"foo\\bar/", L"foo\\bar/", L"foo\\bar/" },
        { L"\\/", L"\\/", L"\\/" },
        { L"\\", L"\\\\", L"\\" },
        { L"\\\\\\", L"\\\\\\\\", L"\\\\" },
        { L"\\\\\\\\\\", L"\\\\\\\\\\\\", L"\\\\\\" },
        { L"foo\\", L"foo\\\\", L"foo\\" },
        { L"foo\\", L"foo\\\\", L"foo\\" },
        { L"foo\\", L"foo\\\\", L"foo\\" },
        { L"foo\\", L"foo\\\\", L"foo\\" },
    };

    for (size_t i = 0; sizeof ipaths / (sizeof ipaths[0] * 2) > i; i++)
        for (BOOLEAN Quote = FALSE; TRUE >= Quote; Quote++)
        {
            WCHAR Buf[1024];
            ULONG Length;
            PWSTR Dest;

            Length = (ULONG)(UINT_PTR)PathTransform(0, ipaths[2 * i + 0], ipaths[2 * i + 1], Quote);
            ASSERT(Length == wcslen(opaths[i][Quote]) * sizeof(WCHAR));

            ASSERT(Length < sizeof Buf);
            Buf[Length / sizeof(WCHAR)] = L'!';
            Dest = PathTransform(Buf, ipaths[2 * i + 0], ipaths[2 * i + 1], Quote);
            ASSERT(L'!' == *Dest);
            *Dest = L'\0';
            ASSERT(Dest == Buf + wcslen(opaths[i][Quote]));
            ASSERT(0 == wcscmp(Buf, opaths[i][Quote]));

            if (Quote)
            {
                WCHAR CommandLine[2048];
                PWSTR *Argv;
                int Argc;

                lstrcpyW(CommandLine, L"test.exe \"");
                lstrcatW(CommandLine, Buf);
                lstrcatW(CommandLine, L"\" sentinel");
                Argv = CommandLineToArgvW(CommandLine, &Argc);
                ASSERT(0 != Argv);
                ASSERT(3 == Argc);
                ASSERT(0 == wcscmp(Argv[0], L"test.exe"));
                ASSERT(0 == wcscmp(Argv[1], opaths[i][2]));
                ASSERT(0 == wcscmp(Argv[2], L"sentinel"));
                LocalFree(Argv);
            }
        }
}

void launcher_ptrans_tests(void)
{
    if (OptExternal)
        return;

    TEST_OPT(launcher_ptrans_test);
    TEST_OPT(launcher_ptrans_quote_test);
}
