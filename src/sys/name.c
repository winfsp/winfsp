/**
 * @file sys/name.c
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

#include <sys/driver.h>

BOOLEAN FspFileNameIsValid(PUNICODE_STRING Path, ULONG MaxComponentLength,
    PUNICODE_STRING StreamPart, PULONG StreamType);
BOOLEAN FspFileNameIsValidPattern(PUNICODE_STRING Pattern, ULONG MaxComponentLength);
BOOLEAN FspEaNameIsValid(PSTRING Name);
VOID FspFileNameSuffix(PUNICODE_STRING Path, PUNICODE_STRING Remain, PUNICODE_STRING Suffix);
NTSTATUS FspFileNameInExpression(
    PUNICODE_STRING Expression,
    PUNICODE_STRING Name,
    BOOLEAN IgnoreCase,
    PWCH UpcaseTable,
    PBOOLEAN PResult);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFileNameIsValid)
#pragma alloc_text(PAGE, FspFileNameIsValidPattern)
#pragma alloc_text(PAGE, FspEaNameIsValid)
#pragma alloc_text(PAGE, FspFileNameSuffix)
#pragma alloc_text(PAGE, FspFileNameInExpression)
#endif

BOOLEAN FspFileNameIsValid(PUNICODE_STRING Path, ULONG MaxComponentLength,
    PUNICODE_STRING StreamPart, PULONG StreamType)
{
    PAGED_CODE();

    /* if StreamPart is not NULL, StreamType must also be not NULL */
    ASSERT(0 == StreamPart || 0 != StreamType);

    if (0 == Path->Length || 0 != Path->Length % sizeof(WCHAR))
        return FALSE;

    PWSTR PathBgn, PathEnd, PathPtr, ComponentPtr, StreamTypeStr = 0;
    UCHAR Flags = FSRTL_NTFS_LEGAL;
    ULONG Colons = 0;
    WCHAR Char;

    PathBgn = Path->Buffer;
    PathEnd = (PWSTR)((PUINT8)PathBgn + Path->Length);
    PathPtr = PathBgn;
    ComponentPtr = PathPtr;

    while (PathEnd > PathPtr)
    {
        Char = *PathPtr;

        if (L'\\' == Char)
        {
            /* stream names can only appear as the last path component */
            if (0 < Colons)
                return FALSE;

            /* path component cannot be longer than MaxComponentLength */
            if ((ULONG)(PathPtr - ComponentPtr) > MaxComponentLength)
                return FALSE;

            PathPtr++;
            ComponentPtr = PathPtr;

            /* don't like multiple backslashes */
            if (PathEnd > PathPtr && L'\\' == *PathPtr)
                return FALSE;
        }
        else if (L':' == Char)
        {
            if (0 == StreamPart)
                return FALSE;

            /*
             * Where are the docs on legal stream names?
             */

            /* stream characters now allowed */
            Flags = FSRTL_NTFS_STREAM_LEGAL;

            PathPtr++;
            Colons++;

            if (1 == Colons)
            {
                /* first time through: set up StreamPart */
                StreamPart->Length = StreamPart->MaximumLength = (USHORT)
                    ((PUINT8)PathEnd - (PUINT8)PathPtr);
                StreamPart->Buffer = PathPtr;
            }
            else if (2 == Colons)
            {
                /* second time through: fix StreamPart length to not include 2nd colon */
                StreamPart->Length = (USHORT)
                    ((PUINT8)PathPtr - (PUINT8)StreamPart->Buffer - sizeof(WCHAR));

                StreamTypeStr = PathPtr;
            }
        }
        else if (0x80 > Char && !FsRtlTestAnsiCharacter(Char, TRUE, FALSE, Flags))
            return FALSE;
        else
            PathPtr++;
    }

    /* path component cannot be longer than MaxComponentLength */
    if ((ULONG)(PathPtr - ComponentPtr) > MaxComponentLength)
        return FALSE;

    /* if we had no colons the path is valid */
    if (0 == Colons)
        return TRUE;

    ASSERT(0 != StreamPart && 0 != StreamType);

    *StreamType = FspFileNameStreamTypeNone;

    /* if we had no stream type the path is valid if there was an actual stream name */
    if (0 == StreamTypeStr)
        return 0 != StreamPart->Length;

    /* if we had a stream type the path is valid if the stream type was "$DATA" only */
    if (StreamTypeStr + 5 == PathEnd &&
        L'$' == StreamTypeStr[0] &&
        L'd' == (StreamTypeStr[1] | 0x20) &&
        L'a' == (StreamTypeStr[2] | 0x20) &&
        L't' == (StreamTypeStr[3] | 0x20) &&
        L'a' == (StreamTypeStr[4] | 0x20))
    {
        *StreamType = FspFileNameStreamTypeData;
        return TRUE;
    }

    return FALSE;
}

BOOLEAN FspFileNameIsValidPattern(PUNICODE_STRING Path, ULONG MaxComponentLength)
{
    PAGED_CODE();

    if (0 != Path->Length % sizeof(WCHAR))
        return FALSE;

    PWSTR PathBgn, PathEnd, PathPtr, ComponentPtr;
    WCHAR Char;

    PathBgn = Path->Buffer;
    PathEnd = (PWSTR)((PUINT8)PathBgn + Path->Length);
    PathPtr = PathBgn;
    ComponentPtr = PathPtr;

    while (PathEnd > PathPtr)
    {
        Char = *PathPtr;

        /*
         * A pattern is allowed to have wildcards. It cannot consist of multiple
         * path components (have a backslash) and it cannot reference a stream (have
         * a colon).
         */

        if (L'\\' == Char)
            return FALSE;
        else if (L':' == Char)
            return FALSE;
        else if (0x80 > Char && !FsRtlTestAnsiCharacter(Char, TRUE, TRUE, FSRTL_NTFS_LEGAL))
            return FALSE;
        else
            PathPtr++;
    }

    /* path component cannot be longer than MaxComponentLength */
    if ((ULONG)(PathPtr - ComponentPtr) > MaxComponentLength)
        return FALSE;

    return TRUE;
}

BOOLEAN FspEaNameIsValid(PSTRING Name)
{
    PAGED_CODE();

    /* see FastFat's FatIsEaNameValid */

    if (0 == Name->Length || Name->Length > 254)
        return FALSE;

    PSTR NameEnd, NamePtr;
    CHAR Char;

    NamePtr = Name->Buffer;
    NameEnd = NamePtr + Name->Length;

    while (NameEnd > NamePtr)
    {
        Char = *NamePtr;

        if (FsRtlIsLeadDbcsCharacter(Char))
            NamePtr++;
        else
        if (!FsRtlIsAnsiCharacterLegalFat(Char, FALSE))
            return FALSE;

        NamePtr++;
    }

    return TRUE;
}

VOID FspFileNameSuffix(PUNICODE_STRING Path, PUNICODE_STRING Remain, PUNICODE_STRING Suffix)
{
    PAGED_CODE();

    PWSTR PathBgn, PathEnd, PathPtr, RemainEnd, SuffixBgn;

    PathBgn = Path->Buffer;
    PathEnd = (PWSTR)((PUINT8)PathBgn + Path->Length);
    PathPtr = PathBgn;

    RemainEnd = PathEnd;
    SuffixBgn = PathEnd;

    while (PathEnd > PathPtr)
        if (L'\\' == *PathPtr)
        {
            RemainEnd = PathPtr++;
            for (; PathEnd > PathPtr && L'\\' == *PathPtr; PathPtr++)
                ;
            SuffixBgn = PathPtr;
        }
        else
            PathPtr++;

    Remain->Length = Remain->MaximumLength = (USHORT)((PUINT8)RemainEnd - (PUINT8)PathBgn);
    Remain->Buffer = PathBgn;
    if (0 == Remain->Length && PathBgn < PathEnd && L'\\' == *PathBgn)
        Remain->Length = Remain->MaximumLength = sizeof(WCHAR);
    Suffix->Length = Suffix->MaximumLength = (USHORT)((PUINT8)PathEnd - (PUINT8)SuffixBgn);
    Suffix->Buffer = SuffixBgn;
}

NTSTATUS FspFileNameInExpression(
    PUNICODE_STRING Expression,
    PUNICODE_STRING Name,
    BOOLEAN IgnoreCase,
    PWCH UpcaseTable,
    PBOOLEAN PResult)
{
    PAGED_CODE();

    /* we do not support non-NULL UpcaseTable yet */
    ASSERT(0 == UpcaseTable);

    try
    {
        *PResult = FsRtlIsNameInExpression(Expression, Name, IgnoreCase, UpcaseTable);
        return STATUS_SUCCESS;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
}
