/**
 * @file dll/sxs.c
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

#include <dll/library.h>

static INIT_ONCE FspSxsIdentInitOnce = INIT_ONCE_STATIC_INIT;
static WCHAR FspSxsIdentBuf[32 + 2] = L"";

static BOOL WINAPI FspSxsIdentInitialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    extern HINSTANCE DllInstance;
    WCHAR Path[MAX_PATH];
    DWORD Size;
    HANDLE Handle = INVALID_HANDLE_VALUE;
    CHAR Buffer[ARRAYSIZE(FspSxsIdentBuf) - 2];
    WCHAR WBuffer[ARRAYSIZE(FspSxsIdentBuf) - 2];

    if (0 == GetModuleFileNameW(DllInstance, Path, MAX_PATH))
        goto exit;

    Size = lstrlenW(Path);
    if (4 < Size &&
        (L'.' == Path[Size - 4]) &&
        (L'D' == Path[Size - 3] || L'd' == Path[Size - 3]) &&
        (L'L' == Path[Size - 2] || L'l' == Path[Size - 2]) &&
        (L'L' == Path[Size - 1] || L'l' == Path[Size - 1]) &&
        (L'\0' == Path[Size]))
        ;
    else
        goto exit;

    Size -= 4;
    for (PWCHAR P = Path + Size - 1; Path <= P; P--)
    {
        if (L'\\' == *P)
            break;
        if (L'-' == *P)
        {
            /* arch */
            Size = (DWORD)(P - Path);
            break;
        }
    }

    Path[Size + 0] = L'.';
    Path[Size + 1] = L's';
    Path[Size + 2] = L'x';
    Path[Size + 3] = L's';
    Path[Size + 4] = L'\0';

    Handle = CreateFileW(
        Path,
        FILE_READ_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING,
        0,
        0);
    if (INVALID_HANDLE_VALUE == Handle)
        goto exit;

    if (!ReadFile(Handle, Buffer, sizeof Buffer, &Size, 0))
        goto exit;

    for (PCHAR P = Buffer, EndP = P + Size; EndP > P; P++)
        if ('\r' == *P || '\n' == *P)
        {
            Size = (DWORD)(P - Buffer);
            break;
        }

    Size = MultiByteToWideChar(CP_UTF8, 0,
        Buffer, Size, WBuffer, ARRAYSIZE(WBuffer));
    if (0 == Size)
        goto exit;

    FspSxsIdentBuf[0] = L'+';
    memcpy(FspSxsIdentBuf + 1, WBuffer, Size * sizeof(WCHAR));
    FspSxsIdentBuf[1 + Size] = L'\0';

exit:
    if (INVALID_HANDLE_VALUE != Handle)
        CloseHandle(Handle);

    return TRUE;
}

PWSTR FspSxsIdent(VOID)
{
    InitOnceExecuteOnce(&FspSxsIdentInitOnce, FspSxsIdentInitialize, 0, 0);
    return FspSxsIdentBuf + 1;
}

PWSTR FspSxsSuffix(VOID)
{
    InitOnceExecuteOnce(&FspSxsIdentInitOnce, FspSxsIdentInitialize, 0, 0);
    return FspSxsIdentBuf;
}
