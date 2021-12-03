/**
 * @file dirbuf-test.c
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

static void dirbuf_fill_dotest(unsigned seed, ULONG Count, ULONG InvalidCount)
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

#if 1
    {
        ASSERT(Count > InvalidCount);

#if !defined(FspFileSystemDirectoryBufferEntryInvalid)
        const ULONG FspFileSystemDirectoryBufferEntryInvalid = ((ULONG)-1);
#endif
        typedef struct
        {
            SRWLOCK Lock;
            ULONG Capacity, LoMark, HiMark;
            PUINT8 Buffer;
        } FSP_FILE_SYSTEM_DIRECTORY_BUFFER;
        FSP_FILE_SYSTEM_DIRECTORY_BUFFER *PeekDirBuffer = DirBuffer;
        PUINT8 PeekBuffer = PeekDirBuffer->Buffer;
        PULONG PeekIndex = (PULONG)(PeekDirBuffer->Buffer + PeekDirBuffer->HiMark);
        ULONG PeekCount = (PeekDirBuffer->Capacity - PeekDirBuffer->HiMark) / sizeof(ULONG);

        ASSERT(Count == PeekCount);

        for (ULONG I = 0; InvalidCount > I; I++)
        {
            for (;;)
            {
                N = rand() % PeekCount;
                if (FspFileSystemDirectoryBufferEntryInvalid == PeekIndex[N])
                    continue;

                DirInfo = (PVOID)(PeekBuffer + PeekIndex[N]);
                memcpy(CurrFileName, DirInfo->FileNameBuf, DirInfo->Size - sizeof *DirInfo);
                CurrFileName[(DirInfo->Size - sizeof *DirInfo) / sizeof(WCHAR)] = L'\0';

                /* do not invalidate dot entries as they are used in testing below */
                if (0 == wcscmp(CurrFileName, L".") || 0 == wcscmp(CurrFileName, L".."))
                    continue;

                PeekIndex[N] = FspFileSystemDirectoryBufferEntryInvalid;
                break;
            }
        }

        Count -= InvalidCount;
    }
#endif

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

    dirbuf_fill_dotest(1485473509, 10, 0);
    dirbuf_fill_dotest(1485473509, 10, 3);

    for (ULONG I = 0; 10000 > I; I++)
    {
        dirbuf_fill_dotest(seed + I, 10, 0);
        dirbuf_fill_dotest(seed + I, 10, 3);
    }

    for (ULONG I = 0; 1000 > I; I++)
    {
        dirbuf_fill_dotest(seed + I, 100, 0);
        dirbuf_fill_dotest(seed + I, 100, 30);
    }

    for (ULONG I = 0; 100 > I; I++)
    {
        dirbuf_fill_dotest(seed + I, 1000, 0);
        dirbuf_fill_dotest(seed + I, 1000, 300);
    }

    for (ULONG I = 0; 10 > I; I++)
    {
        dirbuf_fill_dotest(seed + I, 10000, 0);
        dirbuf_fill_dotest(seed + I, 10000, 3000);
    }
}

static void dirbuf_boundary_dotest(PWSTR Marker, ULONG ExpectI, ULONG ExpectN, ...)
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
    PWSTR Name;
    va_list Names0, Names1;

    va_start(Names0, ExpectN);
    va_copy(Names1, Names0);

    Length = sizeof Buffer;

    Result = STATUS_UNSUCCESSFUL;
    Success = FspFileSystemAcquireDirectoryBuffer(&DirBuffer, FALSE, &Result);
    ASSERT(Success);
    ASSERT(STATUS_SUCCESS == Result);

    while (0 != (Name = va_arg(Names0, PWSTR)))
    {
        memset(&DirInfoBuf, 0, sizeof DirInfoBuf);
        DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + wcslen(Name) * sizeof(WCHAR));
        wcscpy_s(DirInfo->FileNameBuf, MAX_PATH, Name);
        Success = FspFileSystemFillDirectoryBuffer(&DirBuffer, DirInfo, &Result);
        ASSERT(Success);
        ASSERT(STATUS_SUCCESS == Result);
    }

    FspFileSystemReleaseDirectoryBuffer(&DirBuffer);

    BytesTransferred = 0;
    FspFileSystemReadDirectoryBuffer(&DirBuffer, Marker, Buffer, Length, &BytesTransferred);

    for (N = 0; ExpectI > N; N++)
        va_arg(Names1, PWSTR);

    for (N = 0,
        DirInfo = (PVOID)Buffer, DirInfoEnd = (PVOID)(Buffer + BytesTransferred);
        DirInfoEnd > DirInfo && 0 != DirInfo->Size;
        DirInfo = (PVOID)((PUINT8)DirInfo + FSP_FSCTL_DEFAULT_ALIGN_UP(DirInfo->Size)), N++)
    {
        memcpy(CurrFileName, DirInfo->FileNameBuf, DirInfo->Size - sizeof *DirInfo);
        CurrFileName[(DirInfo->Size - sizeof *DirInfo) / sizeof(WCHAR)] = L'\0';

        Name = va_arg(Names1, PWSTR);
        ASSERT(0 == wcscmp(CurrFileName, Name));
    }
    ASSERT(ExpectN == N);

    FspFileSystemDeleteDirectoryBuffer(&DirBuffer);

    va_end(Names1);
    va_end(Names0);
}

static void dirbuf_boundary_test(void)
{
    dirbuf_boundary_dotest(L"A", 0, 0, 0);

    dirbuf_boundary_dotest(L"A", 0, 1, L"B", 0);
    dirbuf_boundary_dotest(L"B", 0, 0, L"B", 0);
    dirbuf_boundary_dotest(L"C", 0, 0, L"B", 0);

    dirbuf_boundary_dotest(L"A", 0, 2, L"B", L"D", 0);
    dirbuf_boundary_dotest(L"B", 1, 1, L"B", L"D", 0);
    dirbuf_boundary_dotest(L"C", 1, 1, L"B", L"D", 0);
    dirbuf_boundary_dotest(L"D", 0, 0, L"B", L"D", 0);
    dirbuf_boundary_dotest(L"E", 0, 0, L"B", L"D", 0);

    dirbuf_boundary_dotest(L"A", 0, 3, L"B", L"D", L"F", 0);
    dirbuf_boundary_dotest(L"B", 1, 2, L"B", L"D", L"F", 0);
    dirbuf_boundary_dotest(L"C", 1, 2, L"B", L"D", L"F", 0);
    dirbuf_boundary_dotest(L"D", 2, 1, L"B", L"D", L"F", 0);
    dirbuf_boundary_dotest(L"E", 2, 1, L"B", L"D", L"F", 0);
    dirbuf_boundary_dotest(L"F", 0, 0, L"B", L"D", L"F", 0);
    dirbuf_boundary_dotest(L"G", 0, 0, L"B", L"D", L"F", 0);
}

void dirbuf_tests(void)
{
    if (OptExternal)
        return;

    TEST(dirbuf_empty_test);
    TEST(dirbuf_dots_test);
    TEST(dirbuf_fill_test);
    TEST(dirbuf_boundary_test);
}
