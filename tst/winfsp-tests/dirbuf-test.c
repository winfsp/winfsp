/**
 * @file dirbuf-test.c
 *
 * @copyright 2015-2018 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <time.h>

#include "winfsp-tests.h"

static void dirbuf_empty_test(void)
{
    PVOID DirBuffer = 0;
    NTSTATUS Result;
    BOOLEAN Success;
    UINT8 Buffer[64];
    ULONG BytesTransferred;

    BytesTransferred = 0;
    FspFileSystemReadDirectoryBuffer(&DirBuffer, 0, Buffer, sizeof Buffer, &BytesTransferred);
    ASSERT(sizeof(UINT16) == BytesTransferred);

    Result = STATUS_UNSUCCESSFUL;
    Success = FspFileSystemAcquireDirectoryBuffer(&DirBuffer, FALSE, &Result);
    ASSERT(Success);
    ASSERT(STATUS_SUCCESS == Result);

    FspFileSystemReleaseDirectoryBuffer(&DirBuffer);

    BytesTransferred = 0;
    FspFileSystemReadDirectoryBuffer(&DirBuffer, 0, Buffer, sizeof Buffer, &BytesTransferred);
    ASSERT(sizeof(UINT16) == BytesTransferred);

    Result = STATUS_UNSUCCESSFUL;
    Success = FspFileSystemAcquireDirectoryBuffer(&DirBuffer, FALSE, &Result);
    ASSERT(!Success);
    ASSERT(STATUS_SUCCESS == Result);

    BytesTransferred = 0;
    FspFileSystemReadDirectoryBuffer(&DirBuffer, 0, Buffer, sizeof Buffer, &BytesTransferred);
    ASSERT(sizeof(UINT16) == BytesTransferred);

    Result = STATUS_UNSUCCESSFUL;
    Success = FspFileSystemAcquireDirectoryBuffer(&DirBuffer, TRUE, &Result);
    ASSERT(Success);
    ASSERT(STATUS_SUCCESS == Result);

    FspFileSystemReleaseDirectoryBuffer(&DirBuffer);

    BytesTransferred = 0;
    FspFileSystemReadDirectoryBuffer(&DirBuffer, 0, Buffer, sizeof Buffer, &BytesTransferred);
    ASSERT(sizeof(UINT16) == BytesTransferred);

    FspFileSystemDeleteDirectoryBuffer(&DirBuffer);
    FspFileSystemDeleteDirectoryBuffer(&DirBuffer);
}

static void dirbuf_dots_test(void)
{
    PVOID DirBuffer = 0;
    NTSTATUS Result;
    BOOLEAN Success;
    union
    {
        UINT8 B[sizeof(FSP_FSCTL_DIR_INFO) + MAX_PATH * sizeof(WCHAR)];
        FSP_FSCTL_DIR_INFO D;
    } DirInfoBuf;
    FSP_FSCTL_DIR_INFO *DirInfo = &DirInfoBuf.D, *DirInfoEnd;
    UINT8 Buffer[1024];
    ULONG Length, BytesTransferred;
    WCHAR CurrFileName[MAX_PATH];
    ULONG N;

    Length = sizeof Buffer;

    Result = STATUS_UNSUCCESSFUL;
    Success = FspFileSystemAcquireDirectoryBuffer(&DirBuffer, FALSE, &Result);
    ASSERT(Success);
    ASSERT(STATUS_SUCCESS == Result);

    memset(&DirInfoBuf, 0, sizeof DirInfoBuf);
    DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + 1 * sizeof(WCHAR));
    DirInfo->FileNameBuf[0] = L'.';
    Success = FspFileSystemFillDirectoryBuffer(&DirBuffer, DirInfo, &Result);
    ASSERT(Success);
    ASSERT(STATUS_SUCCESS == Result);

    memset(&DirInfoBuf, 0, sizeof DirInfoBuf);
    DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + 1 * sizeof(WCHAR));
    DirInfo->FileNameBuf[0] = L' ';
    Success = FspFileSystemFillDirectoryBuffer(&DirBuffer, DirInfo, &Result);
    ASSERT(Success);
    ASSERT(STATUS_SUCCESS == Result);

    memset(&DirInfoBuf, 0, sizeof DirInfoBuf);
    DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + 2 * sizeof(WCHAR));
    DirInfo->FileNameBuf[0] = L'.';
    DirInfo->FileNameBuf[1] = L'.';
    Success = FspFileSystemFillDirectoryBuffer(&DirBuffer, DirInfo, &Result);
    ASSERT(Success);
    ASSERT(STATUS_SUCCESS == Result);

    FspFileSystemReleaseDirectoryBuffer(&DirBuffer);

    BytesTransferred = 0;
    FspFileSystemReadDirectoryBuffer(&DirBuffer, 0, Buffer, Length, &BytesTransferred);

    N = 0;
    for (
        DirInfo = (PVOID)Buffer, DirInfoEnd = (PVOID)(Buffer + BytesTransferred);
        DirInfoEnd > DirInfo && 0 != DirInfo->Size;
        DirInfo = (PVOID)((PUINT8)DirInfo + FSP_FSCTL_DEFAULT_ALIGN_UP(DirInfo->Size)), N++)
    {
        memcpy(CurrFileName, DirInfo->FileNameBuf, DirInfo->Size - sizeof *DirInfo);
        CurrFileName[(DirInfo->Size - sizeof *DirInfo) / sizeof(WCHAR)] = L'\0';

        if (0 == N)
            ASSERT(0 == wcscmp(CurrFileName, L"."));
        else if (1 == N)
            ASSERT(0 == wcscmp(CurrFileName, L".."));
        else if (2 == N)
            ASSERT(0 == wcscmp(CurrFileName, L" "));
    }
    ASSERT(DirInfoEnd > DirInfo);
    ASSERT(0 == DirInfo->Size);
    ASSERT(N == 3);

    FspFileSystemDeleteDirectoryBuffer(&DirBuffer);
}

static void dirbuf_fill_dotest(unsigned seed, ULONG Count)
{
    PVOID DirBuffer = 0;
    NTSTATUS Result;
    BOOLEAN Success;
    union
    {
        UINT8 B[sizeof(FSP_FSCTL_DIR_INFO) + MAX_PATH * sizeof(WCHAR)];
        FSP_FSCTL_DIR_INFO D;
    } DirInfoBuf;
    FSP_FSCTL_DIR_INFO *DirInfo = &DirInfoBuf.D, *DirInfoEnd;
    PUINT8 Buffer;
    ULONG Length, BytesTransferred;
    WCHAR CurrFileName[MAX_PATH], PrevFileName[MAX_PATH];
    ULONG DotIndex = Count / 3, DotDotIndex = Count / 3 * 2;
    WCHAR Marker[MAX_PATH];
    ULONG N, MarkerIndex, MarkerLength;

    srand(seed);

    Length = Count * FSP_FSCTL_DEFAULT_ALIGN_UP(sizeof DirInfoBuf) + sizeof(UINT16);
    Buffer = malloc(Length);
    ASSERT(0 != Buffer);

    Result = STATUS_UNSUCCESSFUL;
    Success = FspFileSystemAcquireDirectoryBuffer(&DirBuffer, FALSE, &Result);
    ASSERT(Success);
    ASSERT(STATUS_SUCCESS == Result);

    for (ULONG I = 0; Count > I; I++)
    {
        memset(&DirInfoBuf, 0, sizeof DirInfoBuf);

        if (I == DotIndex)
        {
            N = 1;
            DirInfo->FileNameBuf[0] = L'.';
        }
        else if (I == DotDotIndex)
        {
            N = 2;
            DirInfo->FileNameBuf[0] = L'.';
            DirInfo->FileNameBuf[1] = L'.';
        }
        else
        {
            N = 24 + rand() % (32 - 24);
            for (ULONG J = 0; N > J; J++)
                DirInfo->FileNameBuf[J] = 'A' + rand() % 26;
        }
        DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + N * sizeof(WCHAR));

        Success = FspFileSystemFillDirectoryBuffer(&DirBuffer, DirInfo, &Result);
        ASSERT(Success);
        ASSERT(STATUS_SUCCESS == Result);
    }

    FspFileSystemReleaseDirectoryBuffer(&DirBuffer);

    MarkerIndex = rand() % Count;

    BytesTransferred = 0;
    FspFileSystemReadDirectoryBuffer(&DirBuffer, 0, Buffer, Length, &BytesTransferred);

    N = 0;
    PrevFileName[0] = L'\0';
    for (
        DirInfo = (PVOID)Buffer, DirInfoEnd = (PVOID)(Buffer + BytesTransferred);
        DirInfoEnd > DirInfo && 0 != DirInfo->Size;
        DirInfo = (PVOID)((PUINT8)DirInfo + FSP_FSCTL_DEFAULT_ALIGN_UP(DirInfo->Size)), N++)
    {
        memcpy(CurrFileName, DirInfo->FileNameBuf, DirInfo->Size - sizeof *DirInfo);
        CurrFileName[(DirInfo->Size - sizeof *DirInfo) / sizeof(WCHAR)] = L'\0';

        if (0 == N)
            ASSERT(0 == wcscmp(CurrFileName, L"."));
        else if (1 == N)
            ASSERT(0 == wcscmp(CurrFileName, L".."));
        else
            ASSERT(0 != wcscmp(CurrFileName, L".") && 0 != wcscmp(CurrFileName, L".."));

        //ASSERT(wcscmp(PrevFileName, CurrFileName) <= 0);
            /* filenames are random enought that should not happen in practice */
        ASSERT(wcscmp(PrevFileName, CurrFileName) < 0);

        memcpy(PrevFileName, CurrFileName, sizeof CurrFileName);

        if (N == MarkerIndex)
        {
            memcpy(Marker, CurrFileName, sizeof CurrFileName);
            MarkerLength = (ULONG)((PUINT8)DirInfo + FSP_FSCTL_DEFAULT_ALIGN_UP(DirInfo->Size) - Buffer);
        }
    }
    ASSERT(DirInfoEnd > DirInfo);
    ASSERT(0 == DirInfo->Size);
    ASSERT(N == Count);

    BytesTransferred = 0;
    FspFileSystemReadDirectoryBuffer(&DirBuffer, Marker, Buffer, Length, &BytesTransferred);

    N = 0;
    PrevFileName[0] = L'\0';
    for (
        DirInfo = (PVOID)Buffer, DirInfoEnd = (PVOID)(Buffer + BytesTransferred);
        DirInfoEnd > DirInfo && 0 != DirInfo->Size;
        DirInfo = (PVOID)((PUINT8)DirInfo + FSP_FSCTL_DEFAULT_ALIGN_UP(DirInfo->Size)), N++)
    {
        memcpy(CurrFileName, DirInfo->FileNameBuf, DirInfo->Size - sizeof *DirInfo);
        CurrFileName[(DirInfo->Size - sizeof *DirInfo) / sizeof(WCHAR)] = L'\0';

        ASSERT(0 != wcscmp(CurrFileName, Marker));

        //ASSERT(wcscmp(PrevFileName, CurrFileName) <= 0);
            /* filenames are random enought that should not happen in practice */
        ASSERT(wcscmp(PrevFileName, CurrFileName) < 0);

        memcpy(PrevFileName, CurrFileName, sizeof CurrFileName);
    }
    ASSERT(DirInfoEnd > DirInfo);
    ASSERT(0 == DirInfo->Size);
    ASSERT(N == Count - MarkerIndex - 1);

    BytesTransferred = 0;
    FspFileSystemReadDirectoryBuffer(&DirBuffer, 0, Buffer, MarkerLength, &BytesTransferred);

    N = 0;
    PrevFileName[0] = L'\0';
    for (
        DirInfo = (PVOID)Buffer, DirInfoEnd = (PVOID)(Buffer + BytesTransferred);
        DirInfoEnd > DirInfo && 0 != DirInfo->Size;
        DirInfo = (PVOID)((PUINT8)DirInfo + FSP_FSCTL_DEFAULT_ALIGN_UP(DirInfo->Size)), N++)
    {
        memcpy(CurrFileName, DirInfo->FileNameBuf, DirInfo->Size - sizeof *DirInfo);
        CurrFileName[(DirInfo->Size - sizeof *DirInfo) / sizeof(WCHAR)] = L'\0';

        if (N == MarkerIndex)
            ASSERT(0 == wcscmp(CurrFileName, Marker));
        else
            ASSERT(0 != wcscmp(CurrFileName, Marker));

        //ASSERT(wcscmp(PrevFileName, CurrFileName) <= 0);
            /* filenames are random enought that should not happen in practice */
        ASSERT(wcscmp(PrevFileName, CurrFileName) < 0);

        memcpy(PrevFileName, CurrFileName, sizeof CurrFileName);
    }
    ASSERT(DirInfoEnd <= DirInfo);
    ASSERT(N == MarkerIndex + 1);

    FspFileSystemDeleteDirectoryBuffer(&DirBuffer);

    free(Buffer);
}

static void dirbuf_fill_test(void)
{
    unsigned seed = (unsigned)time(0);

    dirbuf_fill_dotest(1485473509, 10);

    for (ULONG I = 0; 10000 > I; I++)
        dirbuf_fill_dotest(seed + I, 10);

    for (ULONG I = 0; 1000 > I; I++)
        dirbuf_fill_dotest(seed + I, 100);

    for (ULONG I = 0; 100 > I; I++)
        dirbuf_fill_dotest(seed + I, 1000);

    for (ULONG I = 0; 10 > I; I++)
        dirbuf_fill_dotest(seed + I, 10000);
}

void dirbuf_tests(void)
{
    if (OptExternal)
        return;

    TEST(dirbuf_empty_test);
    TEST(dirbuf_dots_test);
    TEST(dirbuf_fill_test);
}
