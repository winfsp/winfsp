/**
 * @file sys/sxs.c
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

#include <sys/driver.h>

VOID FspSxsIdentInitialize(PUNICODE_STRING DriverName);
PUNICODE_STRING FspSxsIdent(VOID);
PUNICODE_STRING FspSxsSuffix(VOID);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, FspSxsIdentInitialize)
#endif

static WCHAR FspSxsIdentBuf[32 + 1] = L"";
static UNICODE_STRING FspSxsIdentStr = { 0, sizeof FspSxsIdentBuf - 1, FspSxsIdentBuf + 1 };
static UNICODE_STRING FspSxsSuffixStr = { 0, sizeof FspSxsIdentBuf, FspSxsIdentBuf };

VOID FspSxsIdentInitialize(PUNICODE_STRING DriverName)
{
    PWCHAR Ident = 0;
    USHORT Length;

    for (PWCHAR P = DriverName->Buffer + DriverName->Length / sizeof(WCHAR) - 1; DriverName->Buffer <= P; P--)
    {
        if (L'\\' == *P)
            break;
        if (FSP_SXS_SEPARATOR_CHAR == *P)
        {
            Ident = P;
            break;
        }
    }
    if (0 == Ident)
        return;

    Length = (USHORT)(((PUINT8)DriverName->Buffer + DriverName->Length) - (PUINT8)Ident);
    if (Length > sizeof FspSxsIdentBuf)
        Length = sizeof FspSxsIdentBuf;

    RtlCopyMemory(FspSxsIdentBuf, Ident, Length);
    FspSxsIdentStr.Length = Length - sizeof(WCHAR);
    FspSxsSuffixStr.Length = Length;
}

PUNICODE_STRING FspSxsIdent(VOID)
{
    return &FspSxsIdentStr;
}

PUNICODE_STRING FspSxsSuffix(VOID)
{
    return &FspSxsSuffixStr;
}
