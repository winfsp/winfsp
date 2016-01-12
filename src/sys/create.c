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
static VOID FspFsvolCreatePostClose(
    FSP_FILE_CONTEXT *FsContext, UINT64 UserContext2);
static FSP_IOP_REQUEST_FINI FspFsvolCreateRequestFini;
static FSP_IOP_REQUEST_FINI FspFsvolCreateOverwriteRequestFini;
FSP_DRIVER_DISPATCH FspCreate;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCreate)
#pragma alloc_text(PAGE, FspFsvrtCreate)
#pragma alloc_text(PAGE, FspFsvolCreate)
#pragma alloc_text(PAGE, FspFsvolCreatePrepare)
#pragma alloc_text(PAGE, FspFsvolCreateComplete)
#pragma alloc_text(PAGE, FspFsvolCreatePostClose)
#pragma alloc_text(PAGE, FspFsvolCreateRequestFini)
#pragma alloc_text(PAGE, FspFsvolCreateOverwriteRequestFini)
#pragma alloc_text(PAGE, FspCreate)
#endif

#define PREFIXW                         L"" FSP_FSCTL_VOLUME_PARAMS_PREFIX
#define PREFIXW_SIZE                    (sizeof PREFIXW - sizeof(WCHAR))

enum
{
    /* CreateRequest */
    RequestFsContext                    = 0,
    RequestAccessToken                  = 1,
    RequestProcess                      = 2,
    /* OverwriteRequest */
    //RequestFsContext                  = 0,
    RequestFileObject                   = 1,
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
#pragma prefast(disable:28175, "We are a filesystem: ok to access Vpb")
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
        FspFileContextRelease(FsContext);
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
    BOOLEAN Success;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PACCESS_STATE AccessState;
    HANDLE UserModeAccessToken;
    PEPROCESS Process;
    FSP_FILE_CONTEXT *FsContext;
    PFILE_OBJECT FileObject;

    if (FspFsctlTransactCreateKind == Request->Kind)
    {
        AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;

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
    else if (FspFsctlTransactOverwriteKind == Request->Kind)
    {
        FsContext = FspIopRequestContext(Request, RequestFsContext);
        FileObject = FspIopRequestContext(Request, RequestFileObject);

        /* lock the FsContext for Paging IO */
        Success = FspFileContextPgioLockExclusive(FsContext, FALSE);
        if (!Success)
        {
            /* repost the IRP to retry later */
            FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
                FspFsvolDeviceExtension(IrpSp->DeviceObject);
            if (FspIoqPostIrpBestEffort(FsvolDeviceExtension->Ioq, Irp, &Result))
                Result = STATUS_PENDING;
            return Result;
        }

        /* no longer post a Close if the IRP gets cancelled */
        FspIopRequestContext(Request, RequestFileObject) = 0;

        /* see what the MM thinks about all this */
        LARGE_INTEGER Zero = { 0 };
        Success = MmCanFileBeTruncated(&FsContext->NonPaged->SectionObjectPointers, &Zero);
        if (!Success)
        {
            FspFileContextPgioUnlock(FsContext);

            FspFsvolCreatePostClose(FsContext, (UINT_PTR)FileObject->FsContext2);
            FspFileContextClose(FsContext, FileObject, 0);

            return STATUS_USER_MAPPED_FILE;
        }

        /* purge any caches on this file */
        CcPurgeCacheSection(&FsContext->NonPaged->SectionObjectPointers, 0, 0, FALSE);

        return STATUS_SUCCESS;
    }
    else
    {
        ASSERT(0);

        return STATUS_INVALID_PARAMETER;
    }
}

VOID FspFsvolCreateComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->DeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    FSP_FILE_CONTEXT *FsContext = FspIopRequestContext(Request, RequestFsContext);
    FSP_FILE_CONTEXT *OpenedFsContext;
    UNICODE_STRING ReparseFileName;
    BOOLEAN DeleteOnClose;

    if (FspFsctlTransactCreateKind == Request->Kind)
    {
        /* did the user-mode file system sent us a failure code? */
        if (!NT_SUCCESS(Response->IoStatus.Status))
        {
            Irp->IoStatus.Information = 0;
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

        /* populate the FsContext fields from the Response */
        FsContext->Header.AllocationSize.QuadPart = Response->Rsp.Create.Opened.AllocationSize;
        FsContext->Header.FileSize.QuadPart = Response->Rsp.Create.Opened.FileSize;
        FsContext->UserContext = Response->Rsp.Create.Opened.UserContext;

        DeleteOnClose = BooleanFlagOn(Request->Req.Create.CreateOptions, FILE_DELETE_ON_CLOSE);

        /* open the FsContext */
        OpenedFsContext = FspFileContextOpen(FsContext, FileObject,
            Response->Rsp.Create.Opened.GrantedAccess, IrpSp->Parameters.Create.ShareAccess,
            DeleteOnClose,
            &Result);
        if (0 == OpenedFsContext)
        {
            /* unable to open the FsContext; post a Close request */
            FspFsvolCreatePostClose(FsContext,
                Response->Rsp.Create.Opened.UserContext2);

            FSP_RETURN();
        }

        FsContext = OpenedFsContext;

        /* set up the FileObject */
        if (0 != FsvolDeviceExtension->FsvrtDeviceObject)
#pragma prefast(disable:28175, "We are a filesystem: ok to access Vpb")
            FileObject->Vpb = FsvolDeviceExtension->FsvrtDeviceObject->Vpb;
        FileObject->SectionObjectPointer = &FsContext->NonPaged->SectionObjectPointers;
        FileObject->PrivateCacheMap = 0;
        FileObject->FsContext = FsContext;
        FileObject->FsContext2 = (PVOID)(UINT_PTR)Response->Rsp.Create.Opened.UserContext2;

        if (FILE_OPENED == Response->IoStatus.Information)
        {
            /*
             * FastFat quote:
             *     If the user wants write access access to the file make sure there
             *     is not a process mapping this file as an image.  Any attempt to
             *     delete the file will be stopped in fileinfo.c
             *
             *     If the user wants to delete on close, we must check at this
             *     point though.
             */
            if (FlagOn(Response->Rsp.Create.Opened.GrantedAccess, FILE_WRITE_DATA) ||
                DeleteOnClose)
            {
                if (!MmFlushImageSection(&FsContext->NonPaged->SectionObjectPointers,
                    MmFlushForWrite))
                {
                    FspFsvolCreatePostClose(FsContext,
                        Response->Rsp.Create.Opened.UserContext2);
                    FspFileContextClose(FsContext, FileObject, 0);

                    Result = DeleteOnClose ? STATUS_CANNOT_DELETE : STATUS_SHARING_VIOLATION;
                    FSP_RETURN();
                }
            }

            /* SUCCESS! */
            Irp->IoStatus.Information = (ULONG_PTR)Response->IoStatus.Information;
            Result = STATUS_SUCCESS;
        }
        else
        if (FILE_SUPERSEDED == Response->IoStatus.Information ||
            FILE_OVERWRITTEN == Response->IoStatus.Information)
        {
            /*
             * Oh, noes! We have to go back to user mode to overwrite the file!
             */

            /* delete the old request */
            FspIrpRequest(Irp) = 0;
            FspIopDeleteRequest(Request);

            /* create the Overwrite request; MustSucceed because we must either overwrite or close */
            FspIopCreateRequestFunnel(Irp,
                FsvolDeviceExtension->VolumeParams.FileNameRequired ? &FsContext->FileName : 0, 0,
                FspFsvolCreateOverwriteRequestFini, TRUE,
                &Request);

            /* associate the FsContext and FileObject with the Overwrite request */
            FspIopRequestContext(Request, RequestFsContext) = FsContext;
            FspIopRequestContext(Request, RequestFileObject) = FileObject;

            /* populate the Overwrite request */
            Request->Kind = FspFsctlTransactOverwriteKind;
            Request->Req.Overwrite.UserContext = FsContext->UserContext;
            Request->Req.Overwrite.UserContext2 = (UINT_PTR)FileObject->FsContext2;
            Request->Req.Overwrite.FileAttributes = IrpSp->Parameters.Create.FileAttributes;
            Request->Req.Overwrite.Supersede = FILE_SUPERSEDED == Response->IoStatus.Information;

            /*
             * Post it as BestEffort.
             *
             * Note that it is still possible for this request to not be delivered,
             * if the volume device Ioq is stopped or if the IRP is canceled.
             */
            if (FspIoqPostIrpBestEffort(FsvolDeviceExtension->Ioq, Irp, &Result))
                Result = STATUS_PENDING;
        }
        else
        {
            /* SUCCESS! */
            Irp->IoStatus.Information = (ULONG_PTR)Response->IoStatus.Information;
            Result = STATUS_SUCCESS;
        }
    }
    else if (FspFsctlTransactOverwriteKind == Request->Kind)
    {
        /*
         * An Overwrite request will either succeed or else fail and close the corresponding file.
         * There is no need to reach out to user-mode again and ask them to close the file.
         */

        /* did the user-mode file system sent us a failure code? */
        if (!NT_SUCCESS(Response->IoStatus.Status))
        {
            FspFileContextPgioUnlock(FsContext);

            FspFileContextClose(FsContext, FileObject, 0);

            Irp->IoStatus.Information = 0;
            Result = Response->IoStatus.Status;
            FSP_RETURN();
        }

        /* file was successfully overwritten/superseded */
        FsContext->Header.AllocationSize.QuadPart = Response->Rsp.Overwrite.AllocationSize;
        FsContext->Header.FileSize.QuadPart = Response->Rsp.Overwrite.FileSize;
        CcSetFileSizes(FileObject, (PCC_FILE_SIZES)&FsContext->Header.AllocationSize);

        FspFileContextPgioUnlock(FsContext);

        /* SUCCESS! */
        Irp->IoStatus.Information = Request->Req.Overwrite.Supersede ? FILE_SUPERSEDED : FILE_OVERWRITTEN;
        Result = STATUS_SUCCESS;
    }
    else
        ASSERT(0);

    FSP_LEAVE_IOC(
        "FileObject=%p[%p:\"%wZ\"]",
        IrpSp->FileObject, IrpSp->FileObject->RelatedFileObject, IrpSp->FileObject->FileName);
}

static VOID FspFsvolCreatePostClose(
    FSP_FILE_CONTEXT *FsContext, UINT64 UserContext2)
{
    PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject = FsContext->FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    FSP_FSCTL_TRANSACT_REQ *Request;

    /* create the user-mode file system request; MustSucceed because we cannot fail */
    FspIopCreateRequestMustSucceed(0,
        FsvolDeviceExtension->VolumeParams.FileNameRequired ? &FsContext->FileName : 0,
        0, &Request);

    /* populate the Close request */
    Request->Kind = FspFsctlTransactCloseKind;
    Request->Req.Close.UserContext = FsContext->UserContext;
    Request->Req.Close.UserContext2 = UserContext2;

    /*
     * Post as a BestEffort work request. This allows us to complete our own IRP
     * and return immediately.
     */
    FspIopPostWorkRequestBestEffort(FsvolDeviceObject, Request);

    /*
     * Note that it is still possible for this request to not be delivered,
     * if the volume device Ioq is stopped. But such failures are benign
     * from our perspective, because they mean that the file system is going
     * away and should correctly tear things down.
     */
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

static VOID FspFsvolCreateOverwriteRequestFini(PVOID Context[3])
{
    PAGED_CODE();

    if (0 != Context[RequestFsContext])
    {
        FSP_FILE_CONTEXT *FsContext = Context[RequestFsContext];
        PFILE_OBJECT FileObject = Context[RequestFileObject];

        if (0 != FileObject)
        {
            FspFsvolCreatePostClose(FsContext, (UINT_PTR)FileObject->FsContext2);
            FspFileContextClose(FsContext, FileObject, 0);
        }

        FspFileContextRelease(FsContext);

        Context[RequestFileObject] = 0;
        Context[RequestFsContext] = 0;
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
