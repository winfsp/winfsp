/**
 * @file dll/path.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the
 * GNU Affero General Public License version 3 as published by the
 * Free Software Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <dll/library.h>

FSP_API VOID FspPathPrefix(PWSTR Path, PWSTR *PPrefix, PWSTR *PRemain, PWSTR Root)
{
    PWSTR Pointer;

    for (Pointer = Path; *Pointer; Pointer++)
        if (L'\\' == *Pointer)
        {
            if (0 != Root && Path == Pointer)
                Path = Root;
            *Pointer++ = L'\0';
            for (; L'\\' == *Pointer; Pointer++)
                ;
            break;
        }

    *PPrefix = Path;
    *PRemain = Pointer;
}

FSP_API VOID FspPathSuffix(PWSTR Path, PWSTR *PRemain, PWSTR *PSuffix, PWSTR Root)
{
    PWSTR Pointer, RemainEnd, Suffix = 0;

    for (Pointer = Path; *Pointer;)
        if (L'\\' == *Pointer)
        {
            RemainEnd = Pointer++;
            for (; L'\\' == *Pointer; Pointer++)
                ;
            Suffix = Pointer;
        }
        else
            Pointer++;

    *PRemain = Path;
    if (Path < Suffix)
    {
        if (0 != Root && Path == RemainEnd && L'\\' == *Path)
            *PRemain = Root;
        *RemainEnd = L'\0';
        *PSuffix = Suffix;
    }
    else
        *PSuffix = Pointer;
}

FSP_API VOID FspPathCombine(PWSTR Prefix, PWSTR Suffix)
{
    for (; Prefix < Suffix; Prefix++)
        if (L'\0' == *Prefix)
            *Prefix = L'\\';
}
