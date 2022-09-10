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

static BOOLEAN FspSxsIdentInitializeFromFile(VOID)
{
    extern HINSTANCE DllInstance;
    WCHAR Path[MAX_PATH];
    DWORD Size;
    HANDLE Handle = INVALID_HANDLE_VALUE;
    CHAR Buffer[ARRAYSIZE(FspSxsIdentBuf) - 2];
    WCHAR WBuffer[ARRAYSIZE(FspSxsIdentBuf) - 2];
    BOOLEAN Result = FALSE;

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

    FspSxsIdentBuf[0] = FSP_SXS_SEPARATOR_CHAR;
    memcpy(FspSxsIdentBuf + 1, WBuffer, Size * sizeof(WCHAR));
    FspSxsIdentBuf[1 + Size] = L'\0';

    Result = TRUE;

exit:
    if (INVALID_HANDLE_VALUE != Handle)
        CloseHandle(Handle);

    return Result;
}

static BOOLEAN FspSxsIdentInitializeFromDirectory(VOID)
{
    extern HINSTANCE DllInstance;
    WCHAR Path[MAX_PATH];
    HANDLE Handle = INVALID_HANDLE_VALUE;
    WCHAR FinalPath[MAX_PATH];
    PWCHAR P, EndP, Q, EndQ;
    PWCHAR Ident = 0;
    BOOLEAN Result = FALSE;

    if (0 == GetModuleFileNameW(DllInstance, Path, MAX_PATH))
        goto exit;

    Handle = CreateFileW(
        Path,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING,
        0,
        0);
    if (INVALID_HANDLE_VALUE == Handle)
        goto exit;

    if (!GetFinalPathNameByHandleW(Handle, FinalPath, MAX_PATH, VOLUME_NAME_NONE))
        goto exit;

    EndP = FinalPath + lstrlenW(FinalPath);
    for (P = EndP - 1; FinalPath <= P; P--)
    {
        if (L'\\' == *P &&
            P + 9 < EndP &&
            (L'S' == P[1] || L's' == P[1]) &&
            (L'X' == P[2] || L'x' == P[2]) &&
            (L'S' == P[3] || L's' == P[3]) &&
            L'\\' == P[4] &&
            (L'S' == P[5] || L's' == P[5]) &&
            (L'X' == P[6] || L'x' == P[6]) &&
            (L'S' == P[7] || L's' == P[7]) &&
            L'.'  == P[8] &&
            L'\\' != P[9])
        {
            Ident = P + 9;
            break;
        }
    }
    if (0 == Ident)
        goto exit;

    FspSxsIdentBuf[0] = FSP_SXS_SEPARATOR_CHAR;
    EndQ = FspSxsIdentBuf + (ARRAYSIZE(FspSxsIdentBuf) - 1);
    for (P = Ident, Q = FspSxsIdentBuf + 1; EndP > P && EndQ > Q && L'\\' != *P; P++, Q++)
        *Q = *P;
    *Q = L'\0';

    Result = TRUE;

exit:
    if (INVALID_HANDLE_VALUE != Handle)
        CloseHandle(Handle);

    return Result;
}

static BOOL WINAPI FspSxsIdentInitialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    if (FspSxsIdentInitializeFromFile())
        goto exit;

    if (FspSxsIdentInitializeFromDirectory())
        goto exit;

exit:
    return TRUE;
}

FSP_API PWSTR FspSxsIdent(VOID)
{
    InitOnceExecuteOnce(&FspSxsIdentInitOnce, FspSxsIdentInitialize, 0, 0);
    return FspSxsIdentBuf + 1;
}

PWSTR FspSxsSuffix(VOID)
{
    InitOnceExecuteOnce(&FspSxsIdentInitOnce, FspSxsIdentInitialize, 0, 0);
    return FspSxsIdentBuf;
}

PWSTR FspSxsAppendSuffix(PWCHAR Buffer, SIZE_T Size, PWSTR Ident)
{
    PWSTR Suffix;
    SIZE_T IdentSize, SuffixSize;

    Suffix = FspSxsSuffix();
    IdentSize = lstrlenW(Ident) * sizeof(WCHAR);
    SuffixSize = lstrlenW(Suffix) * sizeof(WCHAR);
    if (Size < IdentSize + SuffixSize + sizeof(WCHAR))
        return L"<INVALID>";

    memcpy(Buffer, Ident, IdentSize);
    memcpy((PUINT8)Buffer + IdentSize, Suffix, SuffixSize);
    *(PWCHAR)((PUINT8)Buffer + IdentSize + SuffixSize) = L'\0';

    return Buffer;
}
