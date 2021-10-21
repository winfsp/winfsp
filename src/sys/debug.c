/**
 * @file sys/debug.c
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

#if DBG

#undef STATUS_INSUFFICIENT_RESOURCES
#define STATUS_INSUFFICIENT_RESOURCES   ((NTSTATUS)0xC000009AL)

#define SYM(x)                          case x: return #x;
#define SYMBRC(x)                       case x: return "[" #x "]";

const char *NtStatusSym(NTSTATUS Status)
{
    switch (Status)
    {
    // cygwin: sed -n '/_WAIT_0/!s/^#define[ \t]*\(STATUS_[^ \t]*\).*NTSTATUS.*$/SYM(\1)/p'
    #include "ntstatus.i"
    case FSP_STATUS_IOQ_POST:
        return "FSP_STATUS_IOQ_POST";
    case FSP_STATUS_IOQ_POST_BEST_EFFORT:
        return "FSP_STATUS_IOQ_POST_BEST_EFFORT";
    default:
        return "NTSTATUS:Unknown";
    }
}

const char *IrpMajorFunctionSym(UCHAR MajorFunction)    
{
    switch (MajorFunction)
    {
    SYM(IRP_MJ_CREATE)
    SYM(IRP_MJ_CREATE_NAMED_PIPE)
    SYM(IRP_MJ_CLOSE)
    SYM(IRP_MJ_READ)
    SYM(IRP_MJ_WRITE)
    SYM(IRP_MJ_QUERY_INFORMATION)
    SYM(IRP_MJ_SET_INFORMATION)
    SYM(IRP_MJ_QUERY_EA)
    SYM(IRP_MJ_SET_EA)
    SYM(IRP_MJ_FLUSH_BUFFERS)
    SYM(IRP_MJ_QUERY_VOLUME_INFORMATION)
    SYM(IRP_MJ_SET_VOLUME_INFORMATION)
    SYM(IRP_MJ_DIRECTORY_CONTROL)
    SYM(IRP_MJ_FILE_SYSTEM_CONTROL)
    SYM(IRP_MJ_DEVICE_CONTROL)
    SYM(IRP_MJ_INTERNAL_DEVICE_CONTROL)
    SYM(IRP_MJ_SHUTDOWN)
    SYM(IRP_MJ_LOCK_CONTROL)
    SYM(IRP_MJ_CLEANUP)
    SYM(IRP_MJ_CREATE_MAILSLOT)
    SYM(IRP_MJ_QUERY_SECURITY)
    SYM(IRP_MJ_SET_SECURITY)
    SYM(IRP_MJ_POWER)
    SYM(IRP_MJ_SYSTEM_CONTROL)
    SYM(IRP_MJ_DEVICE_CHANGE)
    SYM(IRP_MJ_QUERY_QUOTA)
    SYM(IRP_MJ_SET_QUOTA)
    SYM(IRP_MJ_PNP)
    default:
        return "IRP_MJ:Unknown";
    }
}

const char *IrpMinorFunctionSym(UCHAR MajorFunction, UCHAR MinorFunction)
{
    switch (MajorFunction)
    {
    case IRP_MJ_READ:
    case IRP_MJ_WRITE:
        switch (MinorFunction)
        {
        SYMBRC(IRP_MN_NORMAL)
        SYMBRC(IRP_MN_DPC)
        SYMBRC(IRP_MN_MDL)
        SYMBRC(IRP_MN_COMPLETE)
        SYMBRC(IRP_MN_COMPRESSED)
        SYMBRC(IRP_MN_MDL_DPC)
        SYMBRC(IRP_MN_COMPLETE_MDL)
        SYMBRC(IRP_MN_COMPLETE_MDL_DPC)
        default:
            return "[IRP_MN:Unknown]";
        }
    case IRP_MJ_DIRECTORY_CONTROL:
        switch (MinorFunction)
        {
        SYMBRC(IRP_MN_QUERY_DIRECTORY)
        SYMBRC(IRP_MN_NOTIFY_CHANGE_DIRECTORY)
        default:
            return "[IRP_MN:Unknown]";
        }
    case IRP_MJ_FILE_SYSTEM_CONTROL:
        switch (MinorFunction)
        {
        SYMBRC(IRP_MN_USER_FS_REQUEST)
        SYMBRC(IRP_MN_MOUNT_VOLUME)
        SYMBRC(IRP_MN_VERIFY_VOLUME)
        SYMBRC(IRP_MN_LOAD_FILE_SYSTEM)
        SYMBRC(IRP_MN_KERNEL_CALL)
        default:
            return "[IRP_MN:Unknown]";
        }
    case IRP_MJ_LOCK_CONTROL:
        switch (MinorFunction)
        {
        SYMBRC(IRP_MN_LOCK)
        SYMBRC(IRP_MN_UNLOCK_SINGLE)
        SYMBRC(IRP_MN_UNLOCK_ALL)
        SYMBRC(IRP_MN_UNLOCK_ALL_BY_KEY)
        default:
            return "[IRP_MN:Unknown]";
        }
    case IRP_MJ_POWER:
        switch (MinorFunction)
        {
        SYMBRC(IRP_MN_WAIT_WAKE)
        SYMBRC(IRP_MN_POWER_SEQUENCE)
        SYMBRC(IRP_MN_SET_POWER)
        SYMBRC(IRP_MN_QUERY_POWER)
        default:
            return "[IRP_MN:Unknown]";
        }
    case IRP_MJ_SYSTEM_CONTROL:
        switch (MinorFunction)
        {
        SYMBRC(IRP_MN_QUERY_ALL_DATA)
        SYMBRC(IRP_MN_QUERY_SINGLE_INSTANCE)
        SYMBRC(IRP_MN_CHANGE_SINGLE_INSTANCE)
        SYMBRC(IRP_MN_CHANGE_SINGLE_ITEM)
        SYMBRC(IRP_MN_ENABLE_EVENTS)
        SYMBRC(IRP_MN_DISABLE_EVENTS)
        SYMBRC(IRP_MN_ENABLE_COLLECTION)
        SYMBRC(IRP_MN_DISABLE_COLLECTION)
        SYMBRC(IRP_MN_REGINFO)
        SYMBRC(IRP_MN_EXECUTE_METHOD)
        SYMBRC(IRP_MN_REGINFO_EX)
        default:
            return "[IRP_MN:Unknown]";
        }
    case IRP_MJ_PNP:
        switch (MinorFunction)
        {
        SYMBRC(IRP_MN_START_DEVICE)
        SYMBRC(IRP_MN_QUERY_REMOVE_DEVICE)
        SYMBRC(IRP_MN_REMOVE_DEVICE)
        SYMBRC(IRP_MN_CANCEL_REMOVE_DEVICE)
        SYMBRC(IRP_MN_SURPRISE_REMOVAL)
        default:
            return "[IRP_MN:Unknown]";
        }
    default:
        return "";
    }
}

const char *IoctlCodeSym(ULONG ControlCode)
{
    switch (ControlCode)
    {
    SYM(FSP_FSCTL_VOLUME_NAME)
    SYM(FSP_FSCTL_TRANSACT)
    SYM(FSP_FSCTL_TRANSACT_BATCH)
    SYM(FSP_FSCTL_STOP)
    SYM(FSP_FSCTL_WORK)
    SYM(FSP_FSCTL_WORK_BEST_EFFORT)
    // cygwin: sed -n '/[IF][OS]CTL.*CTL_CODE/s/^#define[ \t]*\([^ \t]*\).*/SYM(\1)/p'
    #include "ioctl.i"
    default:
        return "IOCTL:Unknown";
    }
}

const char *FileInformationClassSym(FILE_INFORMATION_CLASS FileInformationClass)
{
    switch (FileInformationClass)
    {
    SYM(FileDirectoryInformation)
    SYM(FileFullDirectoryInformation)
    SYM(FileBothDirectoryInformation)
    SYM(FileBasicInformation)
    SYM(FileStandardInformation)
    SYM(FileInternalInformation)
    SYM(FileEaInformation)
    SYM(FileAccessInformation)
    SYM(FileNameInformation)
    SYM(FileRenameInformation)
    SYM(FileLinkInformation)
    SYM(FileNamesInformation)
    SYM(FileDispositionInformation)
    SYM(FilePositionInformation)
    SYM(FileFullEaInformation)
    SYM(FileModeInformation)
    SYM(FileAlignmentInformation)
    SYM(FileAllInformation)
    SYM(FileAllocationInformation)
    SYM(FileEndOfFileInformation)
    SYM(FileAlternateNameInformation)
    SYM(FileStreamInformation)
    SYM(FilePipeInformation)
    SYM(FilePipeLocalInformation)
    SYM(FilePipeRemoteInformation)
    SYM(FileMailslotQueryInformation)
    SYM(FileMailslotSetInformation)
    SYM(FileCompressionInformation)
    SYM(FileObjectIdInformation)
    SYM(FileCompletionInformation)
    SYM(FileMoveClusterInformation)
    SYM(FileQuotaInformation)
    SYM(FileReparsePointInformation)
    SYM(FileNetworkOpenInformation)
    SYM(FileAttributeTagInformation)
    SYM(FileTrackingInformation)
    SYM(FileIdBothDirectoryInformation)
    SYM(FileIdFullDirectoryInformation)
    SYM(FileValidDataLengthInformation)
    SYM(FileShortNameInformation)
    SYM(FileIoCompletionNotificationInformation)
    SYM(FileIoStatusBlockRangeInformation)
    SYM(FileIoPriorityHintInformation)
    SYM(FileSfioReserveInformation)
    SYM(FileSfioVolumeInformation)
    SYM(FileHardLinkInformation)
    SYM(FileProcessIdsUsingFileInformation)
    SYM(FileNormalizedNameInformation)
    SYM(FileNetworkPhysicalNameInformation)
    SYM(FileIdGlobalTxDirectoryInformation)
    SYM(FileIsRemoteDeviceInformation)
    SYM(FileUnusedInformation)
    SYM(FileNumaNodeInformation)
    SYM(FileStandardLinkInformation)
    SYM(FileRemoteProtocolInformation)
    SYM(FileRenameInformationBypassAccessCheck)
    SYM(FileLinkInformationBypassAccessCheck)
    SYM(FileVolumeNameInformation)
    SYM(FileIdInformation)
    SYM(FileIdExtdDirectoryInformation)
    SYM(FileReplaceCompletionInformation)
    SYM(FileHardLinkFullIdInformation)
    SYM(FileIdExtdBothDirectoryInformation)
    SYM(FileDispositionInformationEx)
    SYM(FileRenameInformationEx)
    SYM(FileRenameInformationExBypassAccessCheck)
    case 68: return "FileStatInformation";
    case 70: return "FileStatLxInformation";
    default:
        return "FILE_INFORMATION_CLASS:Unknown";
    }
}

const char *FsInformationClassSym(FS_INFORMATION_CLASS FsInformationClass)
{
    switch (FsInformationClass)
    {
    SYM(FileFsVolumeInformation)
    SYM(FileFsLabelInformation)
    SYM(FileFsSizeInformation)
    SYM(FileFsDeviceInformation)
    SYM(FileFsAttributeInformation)
    SYM(FileFsControlInformation)
    SYM(FileFsFullSizeInformation)
    SYM(FileFsObjectIdInformation)
    SYM(FileFsDriverPathInformation)
    SYM(FileFsVolumeFlagsInformation)
    SYM(FileFsSectorSizeInformation)
    SYM(FileFsDataCopyInformation)
    SYM(FileFsMetadataSizeInformation)
    default:
        return "FS_INFORMATION_CLASS:Unknown";
    }
}

const char *DeviceExtensionKindSym(UINT32 Kind)
{
    switch (Kind)
    {
    case FspFsctlDeviceExtensionKind:
        return "Ctl";
    case FspFsmupDeviceExtensionKind:
        return "Mup";
    case FspFsvrtDeviceExtensionKind:
        return "Vrt";
    case FspFsvolDeviceExtensionKind:
        return "Vol";
    default:
        return "***";
    }
}

ULONG DebugRandom(VOID)
{
    static KSPIN_LOCK SpinLock = 0;
    static ULONG Seed = 1;
    KIRQL Irql;
    ULONG Result;

    KeAcquireSpinLock(&SpinLock, &Irql);

    /* see ucrt sources */
    Seed = Seed * 214013 + 2531011;
    Result = (Seed >> 16) & 0x7fff;

    KeReleaseSpinLock(&SpinLock, Irql);

    return Result;
}

VOID FspDebugLogIrp(const char *func, PIRP Irp, NTSTATUS Result)
{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    DbgPrint("[%d] " DRIVER_NAME "!%s: IRP=%p, %s%c, %s%s, IoStatus=%s[%lld]\n",
        KeGetCurrentIrql(),
        func,
        Irp,
        DeviceExtensionKindSym(FspDeviceExtension(IrpSp->DeviceObject)->Kind),
        Irp->RequestorMode == KernelMode ? 'K' : 'U',
        IrpMajorFunctionSym(IrpSp->MajorFunction),
        IrpMinorFunctionSym(IrpSp->MajorFunction, IrpSp->MinorFunction),
        NtStatusSym(Result),
        (LONGLONG)Irp->IoStatus.Information);
}

#endif
