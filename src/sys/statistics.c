/**
 * @file sys/statistics.c
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

NTSTATUS FspStatisticsCreate(FSP_STATISTICS **PStatistics);
VOID FspStatisticsDelete(FSP_STATISTICS *Statistics);
NTSTATUS FspStatisticsCopy(FSP_STATISTICS *Statistics, PVOID Buffer, PULONG PLength);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspStatisticsCreate)
#pragma alloc_text(PAGE, FspStatisticsDelete)
#pragma alloc_text(PAGE, FspStatisticsCopy)
#endif

NTSTATUS FspStatisticsCreate(FSP_STATISTICS **PStatistics)
{
    PAGED_CODE();

    *PStatistics = FspAllocNonPaged(sizeof(FSP_STATISTICS) * FspProcessorCount);
    if (0 == *PStatistics)
        return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(*PStatistics, sizeof(FSP_STATISTICS) * FspProcessorCount);
    for (ULONG Index = 0; FspProcessorCount > Index; Index++)
    {
        FSP_STATISTICS *Statistics = *PStatistics + Index;

        /* pretend that we are FAT when it comes to stats */
        Statistics->Base.FileSystemType = FILESYSTEM_STATISTICS_TYPE_FAT;
        Statistics->Base.Version = 1;
        Statistics->Base.SizeOfCompleteStructure = sizeof(FSP_STATISTICS);
    }

    return STATUS_SUCCESS;
}

VOID FspStatisticsDelete(FSP_STATISTICS *Statistics)
{
    PAGED_CODE();

    FspFree(Statistics);
}

NTSTATUS FspStatisticsCopy(FSP_STATISTICS *Statistics, PVOID Buffer, PULONG PLength)
{
    PAGED_CODE();

    NTSTATUS Result;
    ULONG StatLength;

    if (0 == Buffer)
    {
        *PLength = 0;
        return STATUS_INVALID_PARAMETER;
    }

    if (sizeof(FILESYSTEM_STATISTICS) > *PLength)
    {
        *PLength = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    StatLength = sizeof(FSP_STATISTICS) * FspProcessorCount;
    if (*PLength >= StatLength)
    {
        *PLength = StatLength;
        Result = STATUS_SUCCESS;
    }
    else
        Result = STATUS_BUFFER_OVERFLOW;

    RtlCopyMemory(Buffer, Statistics, *PLength);

    return Result;
}
