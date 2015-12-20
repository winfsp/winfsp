/**
 * @file sys/debug.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

#if DBG
#define SYM(x)                          case x: return #x;
#define SYMBRC(x)                       case x: return "[" #x "]";

int fsp_bp_global = 1;
static ANSI_STRING DbgBreakPointInc = RTL_CONSTANT_STRING("Fsp*");
BOOLEAN HasDbgBreakPoint(const char *Function)
{
    /* poor man's breakpoints; work around 32 breakpoints kernel limit */
    if (KeGetCurrentIrql() > APC_LEVEL) /* FsRtlIsDbcsInExpression restriction */
        return TRUE;
    ANSI_STRING Name;
    RtlInitAnsiString(&Name, Function);
    return FsRtlIsDbcsInExpression(&DbgBreakPointInc, &Name);
}

const char *NtStatusSym(NTSTATUS Status)
{
    switch (Status)
    {
    /* cygwin: sed -n '/_WAIT_0/!s/^#define[ \t]*\(STATUS_[^ \t]*\).*NTSTATUS.*$/SYM(\1)/p' */
    #include "ntstatus.i"
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
    SYM(FSP_FSCTL_CREATE)
    SYM(FSP_FSCTL_TRANSACT)
    SYM(FSP_FSCTL_WORK)
    default:
        return "IOCTL:Unknown";
    }
}
#endif
