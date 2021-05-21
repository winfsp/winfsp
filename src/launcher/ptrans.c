/**
 * @file launcher/ptrans.c
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

/**
 * Path Transformation Language
 *
 * Syntax:
 *     PERCENT BACKLASH replace-sep *(*sep component) [*sep UNDERSCORE] arg
 *
 * Arg:
 *     The command line argument that a rule is applied on. These are denoted
 *     by digits [0-9] or a capital letter (A-Z).
 *
 * Component:
 *     A path component denoted by a small letter (a-z). The letter a denotes
 *     the first path component, the letter b the second path component, etc.
 *
 * UNDERSCORE:
 *     The UNDERSCORE (_) denotes the "rest of the path". This is any path
 *     left after any path components explicitly mentioned by using small
 *     letters (a-z).
 *
 * Sep:
 *     A separator symbol that is used to separated components.
 *     E.g. slash (/), colon (:), etc.
 *
 * Replace-sep:
 *     A separator symbol that replaces the backslash (\) separator in the
 *     "rest of the path" (UNDERSCORE). E.g. slash (/), colon (:), etc.
 *
 *
 * Examples:
 *     - %\/b:_1
 *         - Transforms \rclone\REMOTE\PATH\TO\FILES to REMOTE:PATH/TO/FILES
 *     - %\/b:/_1
 *         - Transforms \rclone\REMOTE\PATH\TO\FILES to REMOTE:/PATH/TO/FILES
 *     - %\/_1
 *         - Transforms \P1\P2\P3 to /P1/P2/P3
 *     - %\+_1
 *         - Transforms \P1\P2\P3 to +P1+P2+P3
 *     - %\\_1
 *         - Transforms \P1\P2\P3 to \\P1\\P2\\P3
 *         (Backslash is doubled up when used as a replacement separator!)
 */

#include <winfsp/launch.h>
#include <shared/um/minimal.h>

static PWSTR PathCopy(PWSTR Dest, PWSTR Arg, PWSTR ArgEnd, BOOLEAN WriteDest, WCHAR Replacement)
{
    if (0 != Replacement)
    {
        for (PWSTR P = Arg, EndP = (0 != ArgEnd ? ArgEnd : (PWSTR)(UINT_PTR)~0); EndP > P && *P; P++)
            if (L'\\' == *P)
            {
                if (L'\\' == Replacement)
                {
                    if (WriteDest)
                        *Dest = Replacement;
                    Dest++;
                }

                if (WriteDest)
                    *Dest = Replacement;
                Dest++;
            }
            else if (L'"' != *P)
            {
                if (WriteDest)
                    *Dest = *P;
                Dest++;
            }
    }
    else
    {
        for (PWSTR P = Arg, EndP = (0 != ArgEnd ? ArgEnd : (PWSTR)(UINT_PTR)~0); EndP > P && *P; P++)
            if (L'"' != *P)
            {
                if (WriteDest)
                    *Dest = *P;
                Dest++;
            }
    }

    return Dest;
}

static inline BOOLEAN PatternEnd(WCHAR C)
{
    return L'\0' == C ||
        (L'0' <= C && C <= '9') ||
        (L'A' <= C && C <= 'Z');
}

PWSTR PathTransform(PWSTR Dest, PWSTR Arg, PWSTR Pattern)
{
    BOOLEAN WriteDest = 0 != Dest;
    WCHAR Replacement;
    PWSTR Components[26][2];
    PWSTR Remainder = Arg;
    ULONG RemainderIndex = 0;
    PWSTR P;

    if (0 == Pattern)
        return PathCopy(Dest, Arg, 0, WriteDest, 0);

    for (ULONG I = 0; 26 > I; I++)
        Components[I][0] = 0;

    Replacement = *Pattern++;
    if (PatternEnd(Replacement))
        return Dest;

    while (!PatternEnd(*Pattern))
    {
        if (L'a' <= *Pattern && *Pattern <= 'z')
        {
            ULONG I = *Pattern - 'a', J;
            if (0 == Components[I][0])
            {
                P = Remainder;
                J = RemainderIndex;

                while (L'\\' == *P)
                    P++;

                for (;;)
                {
                    Components[J][0] = P;
                    while (*P && L'\\' != *P)
                        P++;
                    Components[J][1] = P;

                    while (L'\\' == *P)
                        P++;

                    if (I == J)
                    {
                        Remainder = P;
                        RemainderIndex = I + 1;
                        break;
                    }

                    J++;
                }
            }

            Dest = PathCopy(Dest, Components[I][0], Components[I][1], WriteDest, Replacement);
        }
        else
        if (L'_' == *Pattern)
            Dest = PathCopy(Dest, Remainder, 0, WriteDest, Replacement);
        else
        {
            if (WriteDest)
                *Dest = *Pattern;
            Dest++;
        }

        Pattern++;
    }

    return Dest;
}
