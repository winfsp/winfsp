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
static FSP_IOP_REQUEST_FINI FspFsvolCreateRequestFini;
FSP_DRIVER_DISPATCH FspCreate;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCreate)
#pragma alloc_text(PAGE, FspFsvrtCreate)
#pragma alloc_text(PAGE, FspFsvolCreate)
#pragma alloc_text(PAGE, FspFsvolCreatePrepare)
#pragma alloc_text(PAGE, FspFsvolCreateComplete)
#pragma alloc_text(PAGE, FspFsvolCreateRequestFini)
#pragma alloc_text(PAGE, FspCreate)
#endif

#define PREFIXW                         L"" FSP_FSCTL_VOLUME_PARAMS_PREFIX
#define PREFIXW_SIZE                    (sizeof PREFIXW - sizeof(WCHAR))

enum
{
    RequestFsContext = 0,
    RequestAccessToken,
    RequestProcess,
};

static NTSTATUS FspFsctlCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;

    if (0 == FileObject->RelatedFileObject &&
        PREFIXW_SIZE <= FileObject->FileName.Length &&
        RtlEqualMemory(PREFIXW, FileObject->FileName.Buffer, PREFIXW_SIZE))
        Result = FspVolumeCreate(DeviceObject, Irp, IrpSp);
    else
    {
        Result = STATUS_SUCCESS;
        Irp->IoStatus.Information = FILE_OPENED;
    }

    return Result;
}

static NTSTATUS FspFsvrtCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    Irp->IoStatus.Information = FILE_OPENED;
    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolCreate(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PFILE_OBJECT RelatedFileObject = FileObject->RelatedFileObject;
    UNICODE_STRING FileName = FileObject->FileName;

    /* open the volume object? */
    if ((0 == RelatedFileObject || RelatedFileObject->FsContext) && 0 == FileName.Length)
    {
        if (0 != FsvolDeviceExtension->FsvrtDeviceObject)
            FileObject->Vpb = FsvolDeviceExtension->FsvrtDeviceObject->Vpb;

        Irp->IoStatus.Information = FILE_OPENED;
        return STATUS_SUCCESS;
    }

    PACCESS_STATE AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;
    ULONG CreateOptions = IrpSp->Parameters.Create.Options;
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
    FSP_FILE_CONTEXT *FsContext, *RelatedFsContext;
    FSP_FSCTL_TRANSACT_REQ *Request;

    /* cannot open files by fileid */
    if (FlagOn(CreateOptions, FILE_OPEN_BY_FILE_ID))
        return STATUS_NOT_IMPLEMENTED;

    /* do we not support EA */
    if (0 != EaBuffer)
        return STATUS_EAS_NOT_SUPPORTED;

    /* cannot open a paging file */
    if (FlagOn(Flags, SL_OPEN_PAGING_FILE))
        return STATUS_ACCESS_DENIED;

    /* check create options */
    if (FlagOn(CreateOptions, FILE_NON_DIRECTORY_FILE) && FlagOn(CreateOptions, FILE_DIRECTORY_FILE))
        return STATUS_INVALID_PARAMETER;

    /* check security descriptor validity */
    if (0 != SecurityDescriptor)
    {
        IsAbsoluteSecurityDescriptor = RtlValidSecurityDescriptor(SecurityDescriptor);
        if (IsAbsoluteSecurityDescriptor)
        {
            Result = RtlAbsoluteToSelfRelativeSD(SecurityDescriptor, 0, &SecurityDescriptorSize);
            if (STATUS_BUFFER_TOO_SMALL != Result)
                return STATUS_INVALID_PARAMETER;
        }
        else
        {
            SecurityDescriptorSize = RtlLengthSecurityDescriptor(SecurityDescriptor);
            IsSelfRelativeSecurityDescriptor = RtlValidRelativeSecurityDescriptor(
                SecurityDescriptor, SecurityDescriptorSize, 0);
            if (!IsSelfRelativeSecurityDescriptor)
                return STATUS_INVALID_PARAMETER;
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
                return STATUS_OBJECT_NAME_INVALID;
    }

    /* check for trailing backslash */
    if (sizeof(WCHAR) * 2/* not empty or root */ <= FileName.Length &&
        L'\\' == FileName.Buffer[FileName.Length / 2 - 1])
    {
        FileName.Length -= sizeof(WCHAR);
        HasTrailingBackslash = TRUE;

        if (sizeof(WCHAR) * 2 <= FileName.Length && L'\\' == FileName.Buffer[FileName.Length / 2 - 1])
            return STATUS_OBJECT_NAME_INVALID;
    }
    if (HasTrailingBackslash && !FlagOn(CreateOptions, FILE_DIRECTORY_FILE))
        return STATUS_OBJECT_NAME_INVALID;

    /* is this a relative or absolute open? */
    if (0 != RelatedFileObject)
    {
        RelatedFsContext = RelatedFileObject->FsContext;

        /* is this a valid RelatedFileObject? */
        if (!FspFileContextIsValid(RelatedFsContext))
            return STATUS_OBJECT_PATH_NOT_FOUND;

        /* must be a relative path */
        if (sizeof(WCHAR) <= FileName.Length && L'\\' == FileName.Buffer[0])
            return STATUS_OBJECT_NAME_INVALID;

        /* cannot FILE_DELETE_ON_CLOSE on the root directory */
        if (FlagOn(CreateOptions, FILE_DELETE_ON_CLOSE) &&
            sizeof(WCHAR) == RelatedFsContext->FileName.Length && 0 == FileName.Length)
            return STATUS_CANNOT_DELETE;

        /*
         * There is no need to lock our accesses of RelatedFileObject->FsContext->FileName,
         * because RelatedFileObject->FsContext->Filename is read-only (after creation) and
         * because RelatedFileObject->FsContext is guaranteed to exist while RelatedFileObject
         * exists.
         */
        BOOLEAN AppendBackslash =
            sizeof(WCHAR) * 2/* not empty or root */ <= RelatedFsContext->FileName.Length &&
            sizeof(WCHAR) <= FileName.Length && L':' != FileName.Buffer[0];
        Result = FspFileContextCreate(FsvolDeviceObject,
            RelatedFsContext->FileName.Length + AppendBackslash * sizeof(WCHAR) + FileName.Length,
            &FsContext);
        if (!NT_SUCCESS(Result))
            return Result;

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
            return STATUS_OBJECT_NAME_INVALID;

        /* cannot FILE_DELETE_ON_CLOSE on the root directory */
        if (FlagOn(CreateOptions, FILE_DELETE_ON_CLOSE) &&
            sizeof(WCHAR) == FileName.Length)
            return STATUS_CANNOT_DELETE;

        Result = FspFileContextCreate(FsvolDeviceObject,
            FileName.Length,
            &FsContext);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    Result = RtlAppendUnicodeStringToString(&FsContext->FileName, &FileName);
    ASSERT(NT_SUCCESS(Result));

    /* create the user-mode file system request */
    Result = FspIopCreateRequestEx(Irp, &FsContext->FileName, SecurityDescriptorSize,
        FspFsvolCreateRequestFini, &Request);
    if (!NT_SUCCESS(Result))
    {
        FspFileContextDelete(FsContext);
        return Result;
    }

    /*
     * The new request is associated with our IRP. Go ahead and associate our FsContext
     * with the Request as well. After this is done completing our IRP will automatically
     * delete the Request and any associated resources.
     */
    FspIopRequestContext(Request, RequestFsContext) = FsContext;

    /* populate the Create request */
    Request->Kind = FspFsctlTransactCreateKind;
    Request->Req.Create.CreateOptions = CreateOptions;
    Request->Req.Create.FileAttributes = FileAttributes;
    Request->Req.Create.SecurityDescriptor.Offset = 0 == SecurityDescriptorSize ? 0 :
        FSP_FSCTL_DEFAULT_ALIGN_UP(Request->FileName.Size);
    Request->Req.Create.SecurityDescriptor.Size = (UINT16)SecurityDescriptorSize;
    Request->Req.Create.AllocationSize = AllocationSize.QuadPart;
    Request->Req.Create.AccessToken = 0;
    Request->Req.Create.DesiredAccess = DesiredAccess;
    Request->Req.Create.ShareAccess = ShareAccess;
    Request->Req.Create.Ea.Offset = 0;
    Request->Req.Create.Ea.Size = 0;
    Request->Req.Create.UserMode = UserMode == RequestorMode;
    Request->Req.Create.HasTraversePrivilege = HasTraversePrivilege;
    Request->Req.Create.OpenTargetDirectory = BooleanFlagOn(Flags, SL_OPEN_TARGET_DIRECTORY);
    Request->Req.Create.CaseSensitive = BooleanFlagOn(Flags, SL_CASE_SENSITIVE);

    /* copy the security descriptor into the request */
    if (IsAbsoluteSecurityDescriptor)
    {
        Result = RtlAbsoluteToSelfRelativeSD(SecurityDescriptor,
            Request->Buffer + Request->Req.Create.SecurityDescriptor.Offset, &SecurityDescriptorSize);
        if (!NT_SUCCESS(Result))
        {
            if (STATUS_BAD_DESCRIPTOR_FORMAT == Result || STATUS_BUFFER_TOO_SMALL == Result)
                Result = STATUS_INVALID_PARAMETER; /* should not happen */
            return Result;
        }
    }
    else if (IsSelfRelativeSecurityDescriptor)
        RtlCopyMemory(Request->Buffer + Request->Req.Create.SecurityDescriptor.Offset,
            SecurityDescriptor, SecurityDescriptorSize);

    return STATUS_PENDING;
}

NTSTATUS FspFsvolCreatePrepare(
    PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request)
{
    PAGED_CODE();

    NTSTATUS Result;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PACCESS_STATE AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;
    HANDLE UserModeAccessToken;
    PEPROCESS Process;

    /* get a user-mode handle to the access token */
    Result = ObOpenObjectByPointer(SeQuerySubjectContextToken(&AccessState->SubjectSecurityContext),
        0, 0, TOKEN_QUERY, *SeTokenObjectType, UserMode, &UserModeAccessToken);
    if (!NT_SUCCESS(Result))
        return Result;

    /* get a pointer to the current process so that we can close the access token later */
    Process = PsGetCurrentProcess();
    ObReferenceObject(Process);

    /* send the user-mode handle to the user-mode file system */
    FspIopRequestContext(Request, RequestAccessToken) = UserModeAccessToken;
    FspIopRequestContext(Request, RequestProcess) = Process;
    Request->Req.Create.AccessToken = (UINT_PTR)UserModeAccessToken;

    return STATUS_SUCCESS;
}

VOID FspFsvolCreateComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->DeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    SHARE_ACCESS TemporaryShareAccess;
    UNICODE_STRING ReparseFileName;
    FSP_FSCTL_TRANSACT_REQ *Request;
    FSP_FILE_CONTEXT *FsContext;
    BOOLEAN Inserted;

    /* did the user-mode file system sent us a failure code? */
    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = (ULONG_PTR)Response->IoStatus.Information;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    /* special case STATUS_REPARSE */
    if (STATUS_REPARSE == Result)
    {
        ReparseFileName.Buffer =
            (PVOID)(Response->Buffer + Response->Rsp.Create.Reparse.FileName.Offset);
        ReparseFileName.Length = ReparseFileName.MaximumLength =
            Response->Rsp.Create.Reparse.FileName.Size;

        Result = STATUS_ACCESS_DENIED;
        if (IO_REPARSE == Response->IoStatus.Information)
        {
            if (0 == ReparseFileName.Length ||
                (PUINT8)ReparseFileName.Buffer + ReparseFileName.Length >
                (PUINT8)Response + Response->Size)
                FSP_RETURN();

            if (ReparseFileName.Length > FileObject->FileName.MaximumLength)
            {
                PVOID Buffer = FspAllocExternal(ReparseFileName.Length);
                if (0 == Buffer)
                    FSP_RETURN(Result = STATUS_INSUFFICIENT_RESOURCES);
                FspFreeExternal(FileObject->FileName.Buffer);
                FileObject->FileName.MaximumLength = ReparseFileName.Length;
                FileObject->FileName.Buffer = Buffer;
            }
            FileObject->FileName.Length = 0;
            RtlCopyUnicodeString(&FileObject->FileName, &ReparseFileName);

            /*
             * The RelatedFileObject does not need to be changed according to:
             * https://support.microsoft.com/en-us/kb/319447
             *
             * Quote:
             *     The fact that the first create-file operation is performed
             *     relative to another file object does not matter. Do not modify
             *     the RelatedFileObject field of the FILE_OBJECT. To perform the
             *     reparse operation, the IO Manager considers only the FileName
             *     field and not the RelatedFileObject. Additionally, the IO Manager
             *     frees the RelatedFileObject, as appropriate, when it handles the
             *     STATUS_REPARSE status returned by the filter. Therefore, it is not
             *     the responsibility of the filter to free that file object.
             */
        }
        else
        if (IO_REMOUNT == Response->IoStatus.Information)
        {
            if (0 != ReparseFileName.Length)
                FSP_RETURN();
        }
        else
            FSP_RETURN();

        Irp->IoStatus.Information = (ULONG_PTR)Response->IoStatus.Information;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    /* get the FsContext from our Request and associate it with the Response UserContext */
    Request = FspIrpRequest(Irp);
    FsContext = FspIopRequestContext(Request, RequestFsContext);
    FsContext->Header.AllocationSize.QuadPart = Response->Rsp.Create.Opened.AllocationSize;
    FsContext->Header.FileSize.QuadPart = Response->Rsp.Create.Opened.AllocationSize;
    FsContext->UserContext = Response->Rsp.Create.Opened.UserContext;

    /*
     * Attempt to insert our FsContext into the volume device's generic table.
     * If an FsContext with the same UserContext already exists, then use that
     * FsContext instead.
     */
    FspFsvolDeviceLockContextTable(FsvolDeviceObject);
    FsContext = FspFsvolDeviceInsertContext(FsvolDeviceObject,
        FsContext->UserContext, FsContext, &FsContext->ElementStorage, &Inserted);
    ASSERT(0 != FsContext);
    if (Inserted)
        /* Our FsContext was inserted into the volume device's generic table.
         * Disassociate it from the Request.
         */
        FspIopRequestContext(Request, RequestFsContext) = 0;
    else
        /*
         * We are using a previously inserted FsContext. We must retain it.
         * Our own FsContext is still associated with the Request and will be
         * deleted during IRP completion.
         */
        FspFileContextRetain(FsContext);
    FspFileContextOpen(FsContext);
    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    /* set up share access on FileObject; user-mode file system assumed to have done share check */
    IoSetShareAccess(Response->Rsp.Create.Opened.GrantedAccess, IrpSp->Parameters.Create.ShareAccess,
        FileObject, &TemporaryShareAccess);

    /* finish seting up the FileObject */
    if (0 != FsvolDeviceExtension->FsvrtDeviceObject)
        FileObject->Vpb = FsvolDeviceExtension->FsvrtDeviceObject->Vpb;
    FileObject->SectionObjectPointer = &FsContext->NonPaged->SectionObjectPointers;
    FileObject->PrivateCacheMap = 0;
    FileObject->FsContext = FsContext;
    FileObject->FsContext2 = (PVOID)(UINT_PTR)Response->Rsp.Create.Opened.UserContext2;

    /* SUCCESS! */
    Irp->IoStatus.Information = (ULONG_PTR)Response->IoStatus.Information;
    Result = Response->IoStatus.Status;

    FSP_LEAVE_IOC(
        "FileObject=%p[%p:\"%wZ\"]",
        IrpSp->FileObject, IrpSp->FileObject->RelatedFileObject, IrpSp->FileObject->FileName);
}

static VOID FspFsvolCreateRequestFini(PVOID Context[3])
{
    PAGED_CODE();

    if (0 != Context[RequestFsContext])
    {
        FspFileContextRelease(Context[RequestFsContext]);
        Context[RequestFsContext] = 0;
    }

    if (0 != Context[RequestAccessToken])
    {
        PEPROCESS Process = Context[RequestProcess];
        KAPC_STATE ApcState;
        BOOLEAN Attach;

        ASSERT(0 != Process);
        Attach = Process != PsGetCurrentProcess();

        if (Attach)
            KeStackAttachProcess(Process, &ApcState);
#if DBG
        NTSTATUS Result0;
        Result0 = ObCloseHandle(Context[RequestAccessToken], UserMode);
        if (!NT_SUCCESS(Result0))
            DEBUGLOG("ObCloseHandle() = %s", NtStatusSym(Result0));
#else
        ObCloseHandle(Context[RequestAccessToken], UserMode);
#endif
        if (Attach)
            KeUnstackDetachProcess(&ApcState);

        ObDereferenceObject(Process);

        Context[RequestAccessToken] = 0;
        Context[RequestProcess] = 0;
    }
}

NTSTATUS FspCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

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
