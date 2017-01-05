/**
 * @file sys/statistics.c
 *
 * @copyright 2015-2017 Bill Zissimopoulos
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
    FspFree(Statistics);
}

NTSTATUS FspStatisticsCopy(FSP_STATISTICS *Statistics, PVOID Buffer, PULONG PLength)
{
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
