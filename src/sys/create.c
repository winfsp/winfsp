/**
 * @file sys/create.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsctlCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvrtCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOPREP_DISPATCH FspFsvolCreatePrepare;
FSP_IOCMPL_DISPATCH FspFsvolCreateComplete;
FSP_DRIVER_DISPATCH FspCreate;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCreate)
#pragma alloc_text(PAGE, FspFsvrtCreate)
#pragma alloc_text(PAGE, FspFsvolCreate)
#pragma alloc_text(PAGE, FspFsvolCreatePrepare)
#pragma alloc_text(PAGE, FspFsvolCreateComplete)
#pragma alloc_text(PAGE, FspCreate)
#endif

static NTSTATUS FspFsctlCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = FILE_OPENED;
    return Result;
}

static NTSTATUS FspFsvrtCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = FILE_OPENED;
    return Result;
}

static NTSTATUS FspFsvolCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    PDEVICE_OBJECT FsvrtDeviceObject = FsvolDeviceExtension->FsvrtDeviceObject;

    if (!FspDeviceRetain(FsvrtDeviceObject))
        return STATUS_CANCELLED;
    try
    {
        FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension =
            FspFsvrtDeviceExtension(FsvrtDeviceObject);
        PFILE_OBJECT FileObject = IrpSp->FileObject;
        PFILE_OBJECT RelatedFileObject = FileObject->RelatedFileObject;
        UNICODE_STRING FileName = FileObject->FileName;
        PACCESS_STATE AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;
        ULONG CreateDisposition = (IrpSp->Parameters.Create.Options >> 24) & 0xff;
        ULONG CreateOptions = IrpSp->Parameters.Create.Options & 0xffffff;
        USHORT FileAttributes = IrpSp->Parameters.Create.FileAttributes;
        PSECURITY_DESCRIPTOR SecurityDescriptor = AccessState->SecurityDescriptor;
        ULONG SecurityDescriptorSize = 0;
        LARGE_INTEGER AllocationSize = Irp->Overlay.AllocationSize;
        ACCESS_MASK DesiredAccess = IrpSp->Parameters.Create.SecurityContext->DesiredAccess;
        USHORT ShareAccess = IrpSp->Parameters.Create.ShareAccess;
        PFILE_FULL_EA_INFORMATION EaBuffer = Irp->AssociatedIrp.SystemBuffer;
        //ULONG EaLength = IrpSp->Parameters.Create.EaLength;
        ULONG Flags = IrpSp->Flags;
        KPROCESSOR_MODE RequestorMode =
            FlagOn(Flags, SL_FORCE_ACCESS_CHECK) ? UserMode : Irp->RequestorMode;
        BOOLEAN HasTraversePrivilege =
            BooleanFlagOn(AccessState->Flags, TOKEN_HAS_TRAVERSE_PRIVILEGE);
        BOOLEAN IsAbsoluteSecurityDescriptor = FALSE;
        BOOLEAN IsSelfRelativeSecurityDescriptor = FALSE;
        BOOLEAN HasTrailingBackslash = FALSE;
        FSP_FILE_CONTEXT *FsContext = 0;
        FSP_FSCTL_TRANSACT_REQ *Request;

        /* cannot open the volume object */
        if (0 == RelatedFileObject && 0 == FileName.Length)
        {
            Result = STATUS_ACCESS_DENIED; /* need error code like POSIX EPERM (STATUS_NOT_SUPPORTED?) */
            goto exit;
        }

        /* cannot open a paging file */
        if (FlagOn(Flags, SL_OPEN_PAGING_FILE))
        {
            Result = STATUS_ACCESS_DENIED;
            goto exit;
        }

        /* cannot open files by fileid */
        if (FlagOn(CreateOptions, FILE_OPEN_BY_FILE_ID))
        {
            Result = STATUS_NOT_IMPLEMENTED;
            goto exit;
        }

        /* do we support EA? */
        if (0 != EaBuffer && !FsvrtDeviceExtension->VolumeParams.EaSupported)
        {
            Result = STATUS_EAS_NOT_SUPPORTED;
            goto exit;
        }

        /* check security descriptor validity */
        if (0 != SecurityDescriptor)
        {
            IsAbsoluteSecurityDescriptor = RtlValidSecurityDescriptor(SecurityDescriptor);
            if (IsAbsoluteSecurityDescriptor)
            {
                Result = RtlAbsoluteToSelfRelativeSD(SecurityDescriptor, 0, &SecurityDescriptorSize);
                if (STATUS_BUFFER_TOO_SMALL != Result)
                {
                    Result = STATUS_INVALID_PARAMETER;
                    goto exit;
                }
            }
            else
            {
                SecurityDescriptorSize = RtlLengthSecurityDescriptor(SecurityDescriptor);
                IsSelfRelativeSecurityDescriptor = RtlValidRelativeSecurityDescriptor(
                    SecurityDescriptor, SecurityDescriptorSize, 0);
                if (!IsSelfRelativeSecurityDescriptor)
                {
                    Result = STATUS_INVALID_PARAMETER;
                    goto exit;
                }
            }
        }

        /* according to fastfat, filenames that begin with two backslashes are ok */
        if (sizeof(WCHAR) * 2 <= FileName.Length &&
            L'\\' == FileName.Buffer[1] && L'\\' == FileName.Buffer[0])
        {
            FileName.Length -= sizeof(WCHAR);
            FileName.MaximumLength -= sizeof(WCHAR);
            FileName.Buffer++;

            if (sizeof(WCHAR) * 2 <= FileName.Length &&
                L'\\' == FileName.Buffer[1] && L'\\' == FileName.Buffer[0])
                {
                    Result = STATUS_OBJECT_NAME_INVALID;
                    goto exit;
                }
        }

        /* check for trailing backslash */
        if (sizeof(WCHAR) * 2/* not empty or root */ <= FileName.Length &&
            L'\\' == FileName.Buffer[FileName.Length / 2 - 1])
        {
            FileName.Length -= sizeof(WCHAR);
            HasTrailingBackslash = TRUE;

            if (sizeof(WCHAR) * 2 <= FileName.Length && L'\\' == FileName.Buffer[FileName.Length / 2 - 1])
            {
                Result = STATUS_OBJECT_NAME_INVALID;
                goto exit;
            }
        }

        /* is this a relative or absolute open? */
        if (0 != RelatedFileObject)
        {
            /* must be a relative path */
            if (sizeof(WCHAR) <= FileName.Length && L'\\' == FileName.Buffer[0])
            {
                Result = STATUS_OBJECT_NAME_INVALID;
                goto exit;
            }

            FSP_FILE_CONTEXT *RelatedFsContext = RelatedFileObject->FsContext;
            ASSERT(0 != RelatedFsContext);

            /*
             * There is no need to lock our accesses of RelatedFileObject->FsContext->FileName,
             * because RelatedFileObject->FsContext->Filename is read-only (after creation) and
             * because RelatedFileObject->FsContext is guaranteed to exist while RelatedFileObject
             * exists.
             */
            BOOLEAN AppendBackslash =
                sizeof(WCHAR) * 2/* not empty or root */ <= RelatedFsContext->FileName.Length &&
                sizeof(WCHAR) <= FileName.Length && L':' != FileName.Buffer[0];
            Result = FspFileContextCreate(
                RelatedFsContext->FileName.Length + AppendBackslash * sizeof(WCHAR) + FileName.Length,
                &FsContext);
            if (!NT_SUCCESS(Result))
                goto exit;

            Result = RtlAppendUnicodeStringToString(&FsContext->FileName, &RelatedFsContext->FileName);
            ASSERT(NT_SUCCESS(Result));
            if (AppendBackslash)
            {
                Result = RtlAppendUnicodeToString(&FsContext->FileName, L"\\");
                ASSERT(NT_SUCCESS(Result));
            }
        }
        else
        {
            /* must be an absolute path */
            if (sizeof(WCHAR) <= FileName.Length && L'\\' != FileName.Buffer[0])
            {
                Result = STATUS_OBJECT_NAME_INVALID;
                goto exit;
            }

            Result = FspFileContextCreate(
                FileName.Length,
                &FsContext);
            if (!NT_SUCCESS(Result))
                goto exit;
        }

        Result = RtlAppendUnicodeStringToString(&FsContext->FileName, &FileName);
        ASSERT(NT_SUCCESS(Result));

        /*
         * From this point forward we MUST remember to delete the FsContext on error.
         */

        /* create the user-mode file system request */
        Result = FspIopCreateRequest(Irp, &FsContext->FileName, SecurityDescriptorSize, &Request);
        if (!NT_SUCCESS(Result))
        {
            FspFileContextDelete(FsContext);
            goto exit;
        }

        /* populate the Create request */
        Request->Kind = FspFsctlTransactCreateKind;
        Request->Req.Create.CreateDisposition = CreateDisposition;
        Request->Req.Create.CreateOptions = CreateOptions;
        Request->Req.Create.FileAttributes = FileAttributes;
        Request->Req.Create.SecurityDescriptor = 0 == SecurityDescriptor ? 0 :
            FSP_FSCTL_DEFAULT_ALIGN_UP(FsContext->FileName.Length + sizeof(WCHAR));
        Request->Req.Create.SecurityDescriptorSize = (UINT16)SecurityDescriptorSize;
        Request->Req.Create.AllocationSize = AllocationSize.QuadPart;
        Request->Req.Create.AccessToken = 0;
        Request->Req.Create.DesiredAccess = DesiredAccess;
        Request->Req.Create.ShareAccess = ShareAccess;
        Request->Req.Create.Ea = 0;
        Request->Req.Create.EaSize = 0;
        Request->Req.Create.UserMode = UserMode == RequestorMode;
        Request->Req.Create.HasTraversePrivilege = HasTraversePrivilege;
        Request->Req.Create.OpenTargetDirectory = BooleanFlagOn(Flags, SL_OPEN_TARGET_DIRECTORY);
        Request->Req.Create.CaseSensitive = BooleanFlagOn(Flags, SL_CASE_SENSITIVE);

        /* copy the security descriptor into the request */
        if (IsAbsoluteSecurityDescriptor)
        {
            Result = RtlAbsoluteToSelfRelativeSD(SecurityDescriptor,
                Request->Buffer + Request->Req.Create.SecurityDescriptor, &SecurityDescriptorSize);
            if (!NT_SUCCESS(Result))
            {
                FspFileContextDelete(FsContext);
                if (STATUS_BAD_DESCRIPTOR_FORMAT == Result || STATUS_BUFFER_TOO_SMALL == Result)
                    return STATUS_INVALID_PARAMETER; /* should not happen */
                goto exit;
            }
        }
        else if (IsSelfRelativeSecurityDescriptor)
            RtlCopyMemory(Request->Buffer + Request->Req.Create.SecurityDescriptor,
                SecurityDescriptor, SecurityDescriptorSize);

        /*
         * Post the IRP to our Ioq; we do this here instead of at FSP_LEAVE_MJ time,
         * so that we can FspFileContextDelete() on failure.
         */
        if (!FspIoqPostIrp(&FsvrtDeviceExtension->Ioq, Irp))
        {
            /* this can only happen if the Ioq was stopped */
            ASSERT(FspIoqStopped(&FsvrtDeviceExtension->Ioq));
            FspFileContextDelete(FsContext);
            Result = STATUS_CANCELLED;
            goto exit;
        }

        Result = STATUS_PENDING;

    exit:;
    }
    finally
    {
        FspDeviceRelease(FsvrtDeviceObject);
    }

    return Result;
}

NTSTATUS FspFsvolCreatePrepare(
    PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request)
{
    FSP_ENTER_IOP(PAGED_CODE());

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(IrpSp->DeviceObject);
    ASSERT(FspFsvolDeviceExtensionKind == FsvolDeviceExtension->Base.Kind);

    /* it is not necessary to retain the FsvrtDeviceObject; our callers have already done so */
    PDEVICE_OBJECT FsvrtDeviceObject = FsvolDeviceExtension->FsvrtDeviceObject;
    FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension = FspFsvrtDeviceExtension(FsvrtDeviceObject);

    /* if the user-mode file system is not doing access checks, there is nothing else to do */
    if (!FsvrtDeviceExtension->VolumeParams.NoSystemAccessCheck)
    {
        ASSERT(0 == Request->Req.Create.AccessToken);
        FSP_RETURN(Result = STATUS_SUCCESS);
    }

    PACCESS_STATE AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;
    HANDLE UserModeAccessToken;

    /* get a user-mode handle to the access token */
    Result = ObOpenObjectByPointer(SeQuerySubjectContextToken(&AccessState->SubjectSecurityContext),
        0, 0, TOKEN_QUERY, *SeTokenObjectType, UserMode, &UserModeAccessToken);
    if (!NT_SUCCESS(Result))
        FSP_RETURN();

    /* send the user-mode handle to the user-mode file system */
    Irp->Tail.Overlay.DriverContext[1] = UserModeAccessToken;
    Request->Req.Create.AccessToken = (UINT_PTR)UserModeAccessToken;

    FSP_LEAVE_IOP();
}

VOID FspFsvolCreateComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC(
        "FileObject=%p[%p:\"%wZ\"]",
        IrpSp->FileObject, IrpSp->FileObject->RelatedFileObject, IrpSp->FileObject->FileName);
}

NTSTATUS FspCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_CREATE == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolCreate(DeviceObject, Irp, IrpSp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtCreate(DeviceObject, Irp, IrpSp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlCreate(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ(
        "FileObject=%p[%p:\"%wZ\"], "
        "Flags=%x, "
        "DesiredAccess=%#lx, "
        "ShareAccess=%#x, "
        "Options=%#lx, "
        "FileAttributes=%#x, "
        "AllocationSize=%#lx:%#lx, "
        "Ea=%p, EaLength=%ld",
        IrpSp->FileObject, IrpSp->FileObject->RelatedFileObject, IrpSp->FileObject->FileName,
        IrpSp->Flags,
        IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
        IrpSp->Parameters.Create.ShareAccess,
        IrpSp->Parameters.Create.Options,
        IrpSp->Parameters.Create.FileAttributes,
        Irp->Overlay.AllocationSize.HighPart, Irp->Overlay.AllocationSize.LowPart,
        Irp->AssociatedIrp.SystemBuffer, IrpSp->Parameters.Create.EaLength);
}
