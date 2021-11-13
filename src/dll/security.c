/**
 * @file dll/security.c
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

#include <dll/library.h>

static GENERIC_MAPPING FspFileGenericMapping =
{
    .GenericRead = FILE_GENERIC_READ,
    .GenericWrite = FILE_GENERIC_WRITE,
    .GenericExecute = FILE_GENERIC_EXECUTE,
    .GenericAll = FILE_ALL_ACCESS,
};

FSP_API PGENERIC_MAPPING FspGetFileGenericMapping(VOID)
{
    return &FspFileGenericMapping;
}

static NTSTATUS FspGetSecurityByName(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    for (;;)
    {
        NTSTATUS Result = FileSystem->Interface->GetSecurityByName(FileSystem,
            FileName, PFileAttributes, *PSecurityDescriptor, PSecurityDescriptorSize);
        if (STATUS_BUFFER_OVERFLOW != Result)
            return Result;

        MemFree(*PSecurityDescriptor);
        *PSecurityDescriptor = MemAlloc(*PSecurityDescriptorSize);
        if (0 == *PSecurityDescriptor)
            return STATUS_INSUFFICIENT_RESOURCES;
    }
}

FSP_API NTSTATUS FspAccessCheckEx(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    BOOLEAN CheckParentOrMain, BOOLEAN AllowTraverseCheck,
    UINT32 DesiredAccess, PUINT32 PGrantedAccess,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
{
    BOOLEAN CheckParentDirectory, CheckMainFile;

    CheckParentDirectory = CheckMainFile = FALSE;
    if (CheckParentOrMain)
    {
        if (!Request->Req.Create.NamedStream)
            CheckParentDirectory = TRUE;
        else
            CheckMainFile = TRUE;
    }

    *PGrantedAccess = 0;
    if (0 != PSecurityDescriptor)
        *PSecurityDescriptor = 0;

    if (FspFsctlTransactCreateKind != Request->Kind)
        return STATUS_INVALID_PARAMETER;

    if (CheckParentDirectory &&
        L'\\' == ((PWSTR)Request->Buffer)[0] && L'\0' == ((PWSTR)Request->Buffer)[1])
        return STATUS_INVALID_PARAMETER;

    if (0 == FileSystem->Interface->GetSecurityByName ||
        (!Request->Req.Create.UserMode && 0 == PSecurityDescriptor))
    {
        *PGrantedAccess = (MAXIMUM_ALLOWED & DesiredAccess) ?
            FspFileGenericMapping.GenericAll : DesiredAccess;
        return STATUS_SUCCESS;
    }

    NTSTATUS Result;
    WCHAR Root[2] = L"\\", TraverseCheckRoot[2] = L"\\";
    PWSTR FileName, Suffix, Prefix, Remain;
    UINT32 FileAttributes = 0;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    SIZE_T SecurityDescriptorSize;
    UINT8 PrivilegeSetBuf[sizeof(PRIVILEGE_SET) + 15 * sizeof(LUID_AND_ATTRIBUTES)];
    PPRIVILEGE_SET PrivilegeSet = (PVOID)PrivilegeSetBuf;
    DWORD PrivilegeSetLength = sizeof PrivilegeSetBuf;
    UINT32 TraverseAccess, ParentAccess, DesiredAccess2;
    UINT16 NamedStreamSave;
    BOOL AccessStatus;

    if (CheckParentDirectory)
        FspPathSuffix((PWSTR)Request->Buffer, &FileName, &Suffix, Root);
    else if (CheckMainFile)
    {
        ((PWSTR)Request->Buffer)[Request->Req.Create.NamedStream / sizeof(WCHAR)] = L'\0';
        FileName = (PWSTR)Request->Buffer;
    }
    else
        FileName = (PWSTR)Request->Buffer;

    SecurityDescriptorSize = 1024;
    SecurityDescriptor = MemAlloc(SecurityDescriptorSize);
    if (0 == SecurityDescriptor)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (Request->Req.Create.UserMode &&
        AllowTraverseCheck && !Request->Req.Create.HasTraversePrivilege &&
        !(L'\\' == FileName[0] && L'\0' == FileName[1])/* no need to traverse check for root */)
    {
        Remain = FileName;
        for (;;)
        {
            while (L'\\' != *Remain)
            {
                if (L'\0' == *Remain || L':' == *Remain)
                    goto traverse_check_done;
                Remain++;
            }

            *Remain = L'\0';
            Prefix = Remain > FileName ? FileName : TraverseCheckRoot;

            FileAttributes = 0;
            Result = FspGetSecurityByName(FileSystem, Prefix, &FileAttributes,
                &SecurityDescriptor, &SecurityDescriptorSize);

            /*
             * We check to see if this is a reparse point and then compute the ReparsePointIndex
             * and place it in FileAttributes.  We do this check BEFORE the directory check,
             * because contrary to NTFS we want to allow non-directory symlinks to directories.
             */
            if (NT_SUCCESS(Result) && STATUS_REPARSE != Result &&
                (FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
            {
                FileAttributes = FspPathSuffixIndex(Prefix);
                Result = STATUS_REPARSE;
            }

            *Remain = L'\\';
            do
            {
                Remain++;
            } while (L'\\' == *Remain);

            if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
            {
                if (STATUS_OBJECT_NAME_NOT_FOUND == Result)
                    Result = STATUS_OBJECT_PATH_NOT_FOUND;
                goto exit;
            }

            /*
             * Check if this is a directory, otherwise the path is invalid.
             */
            if (0 == (FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                Result = STATUS_OBJECT_PATH_NOT_FOUND; /* use STATUS_OBJECT_PATH_INVALID? */
                goto exit;
            }

            if (0 < SecurityDescriptorSize)
            {
                if (AccessCheck(SecurityDescriptor,
                    FSP_FSCTL_TRANSACT_REQ_TOKEN_HANDLE(Request->Req.Create.AccessToken),
                    FILE_TRAVERSE,
                    &FspFileGenericMapping,
                    PrivilegeSet, &PrivilegeSetLength,
                    &TraverseAccess, &AccessStatus))
                    Result = AccessStatus ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
                else
                    Result = FspNtStatusFromWin32(GetLastError());
                if (!NT_SUCCESS(Result))
                    goto exit;
            }
        }
    traverse_check_done:
        ;
    }

    FileAttributes = 0;
    Result = FspGetSecurityByName(FileSystem, FileName, &FileAttributes,
        &SecurityDescriptor, &SecurityDescriptorSize);
    if (!NT_SUCCESS(Result) || STATUS_REPARSE == Result)
        goto exit;

    if (!CheckParentOrMain && Request->Req.Create.HasTrailingBackslash &&
        !(FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        Result = STATUS_OBJECT_NAME_INVALID;
        goto exit;
    }

    if (Request->Req.Create.UserMode && 0 < SecurityDescriptorSize)
    {
        if (0 == DesiredAccess)
            Result = STATUS_SUCCESS;
        else if (AccessCheck(SecurityDescriptor,
            FSP_FSCTL_TRANSACT_REQ_TOKEN_HANDLE(Request->Req.Create.AccessToken),
            DesiredAccess,
            &FspFileGenericMapping,
            PrivilegeSet, &PrivilegeSetLength,
            PGrantedAccess, &AccessStatus))
            Result = AccessStatus ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
        else
            Result = FspNtStatusFromWin32(GetLastError());
        if (!NT_SUCCESS(Result))
        {
            /*
             * If the desired access includes the DELETE or FILE_READ_ATTRIBUTES
             * (or MAXIMUM_ALLOWED) rights we must still check with our parent to
             * see if it gives us access (through the FILE_DELETE_CHILD and
             * FILE_LIST_DIRECTORY rights).
             *
             * Does the Windows security model suck? Ermmmm...
             */
            if (STATUS_ACCESS_DENIED != Result ||
                0 == ((MAXIMUM_ALLOWED | DELETE | FILE_READ_ATTRIBUTES) & DesiredAccess))
                goto exit;

            NamedStreamSave = Request->Req.Create.NamedStream;
            Request->Req.Create.NamedStream = 0;
            Result = FspAccessCheck(FileSystem, Request, TRUE, FALSE,
                (MAXIMUM_ALLOWED & DesiredAccess) ? (FILE_DELETE_CHILD | FILE_LIST_DIRECTORY) :
                (
                    ((DELETE & DesiredAccess) ? FILE_DELETE_CHILD : 0) |
                    ((FILE_READ_ATTRIBUTES & DesiredAccess) ? FILE_LIST_DIRECTORY : 0)
                ),
                &ParentAccess);
            Request->Req.Create.NamedStream = NamedStreamSave;
            if (!NT_SUCCESS(Result))
            {
                /* any failure just becomes ACCESS DENIED at this point */
                Result = STATUS_ACCESS_DENIED;
                goto exit;
            }

            /* redo the access check but remove the DELETE and/or FILE_READ_ATTRIBUTES rights */
            DesiredAccess2 = DesiredAccess & ~(
                ((FILE_DELETE_CHILD & ParentAccess) ? DELETE : 0) |
                ((FILE_LIST_DIRECTORY & ParentAccess) ? FILE_READ_ATTRIBUTES : 0));
            if (0 != DesiredAccess2)
            {
                if (AccessCheck(SecurityDescriptor,
                    FSP_FSCTL_TRANSACT_REQ_TOKEN_HANDLE(Request->Req.Create.AccessToken),
                    DesiredAccess2,
                    &FspFileGenericMapping,
                    PrivilegeSet, &PrivilegeSetLength,
                    PGrantedAccess, &AccessStatus))
                    Result = AccessStatus ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
                else
                    /* any failure just becomes ACCESS DENIED at this point */
                    Result = STATUS_ACCESS_DENIED;
                if (!NT_SUCCESS(Result))
                    goto exit;
            }

            if (FILE_DELETE_CHILD & ParentAccess)
                *PGrantedAccess |= DELETE;
            if (FILE_LIST_DIRECTORY & ParentAccess)
                *PGrantedAccess |= FILE_READ_ATTRIBUTES;
        }
    }

    if (CheckParentDirectory)
    {
        /*
         * We check to see if this is a reparse point and then immediately return
         * STATUS_REPARSE. We do this check BEFORE the directory check, because
         * contrary to NTFS we want to allow non-directory symlinks to directories.
         */
        if (FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        {
            FileAttributes = FspPathSuffixIndex(FileName);
            Result = STATUS_REPARSE;
            goto exit;
        }

        if (0 == (FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            Result = STATUS_NOT_A_DIRECTORY;
            goto exit;
        }
    }
    else if (CheckMainFile)
    {
        /*
         * We check to see if this is a reparse point and FILE_OPEN_REPARSE_POINT
         * was not specified, in which case we return STATUS_REPARSE.
         */
        if (0 != (FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
            0 == (Request->Req.Create.CreateOptions & FILE_OPEN_REPARSE_POINT))
        {
            FileAttributes = FspPathSuffixIndex(FileName);
            Result = STATUS_REPARSE;
            goto exit;
        }
    }
    else
    {
        /*
         * We check to see if this is a reparse point and FILE_OPEN_REPARSE_POINT
         * was not specified, in which case we return STATUS_REPARSE.
         */
        if (0 != (FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
            0 == (Request->Req.Create.CreateOptions & FILE_OPEN_REPARSE_POINT))
        {
            FileAttributes = FspPathSuffixIndex(FileName);
            Result = STATUS_REPARSE;
            goto exit;
        }

        /*
         * We allow some file systems (notably FUSE) to open reparse points
         * regardless of the FILE_DIRECTORY_FILE / FILE_NON_DIRECTORY_FILE options.
         */
        if (0 != (FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && FileSystem->UmNoReparsePointsDirCheck)
            goto skip_reparse_dir_check;

        if ((Request->Req.Create.CreateOptions & FILE_DIRECTORY_FILE) &&
            0 == (FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            Result = STATUS_NOT_A_DIRECTORY;
            goto exit;
        }
        if ((Request->Req.Create.CreateOptions & FILE_NON_DIRECTORY_FILE) &&
            0 != (FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            Result = STATUS_FILE_IS_A_DIRECTORY;
            goto exit;
        }

    skip_reparse_dir_check:
        ;
    }

    if (Request->Req.Create.UserMode)
    {
        if (FILE_ATTRIBUTE_READONLY == (FileAttributes & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_DIRECTORY)) &&
            (DesiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_ADD_SUBDIRECTORY | FILE_DELETE_CHILD)))
        {
            Result = STATUS_ACCESS_DENIED;
            goto exit;
        }

        if (FILE_ATTRIBUTE_READONLY == (FileAttributes & FILE_ATTRIBUTE_READONLY) &&
            (Request->Req.Create.CreateOptions & FILE_DELETE_ON_CLOSE))
        {
            Result = STATUS_CANNOT_DELETE;
            goto exit;
        }

        if (0 == SecurityDescriptorSize)
            *PGrantedAccess = (MAXIMUM_ALLOWED & DesiredAccess) ?
                FspFileGenericMapping.GenericAll : DesiredAccess;

        if (FILE_ATTRIBUTE_READONLY == (FileAttributes & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_DIRECTORY)) &&
            0 != (MAXIMUM_ALLOWED & DesiredAccess))
            *PGrantedAccess &= ~(FILE_WRITE_DATA | FILE_APPEND_DATA |
                FILE_ADD_SUBDIRECTORY | FILE_DELETE_CHILD);
    }
    else
        *PGrantedAccess = (MAXIMUM_ALLOWED & DesiredAccess) ?
            FspFileGenericMapping.GenericAll : DesiredAccess;

    Result = STATUS_SUCCESS;

exit:
    if (STATUS_REPARSE == Result)
        *PGrantedAccess = FileAttributes; /* FileAttributes contains ReparsePointIndex */
    else if (0 != PSecurityDescriptor && 0 < SecurityDescriptorSize && NT_SUCCESS(Result))
        *PSecurityDescriptor = SecurityDescriptor;
    else
        MemFree(SecurityDescriptor);

    if (CheckParentDirectory)
    {
        FspPathCombine((PWSTR)Request->Buffer, Suffix);

        if (STATUS_OBJECT_NAME_NOT_FOUND == Result)
            Result = STATUS_OBJECT_PATH_NOT_FOUND;
    }
    else if (CheckMainFile)
        ((PWSTR)Request->Buffer)[Request->Req.Create.NamedStream / sizeof(WCHAR)] = L':';

    return Result;
}

FSP_API NTSTATUS FspCreateSecurityDescriptor(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PSECURITY_DESCRIPTOR ParentDescriptor,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
{
    *PSecurityDescriptor = 0;

    if (FspFsctlTransactCreateKind != Request->Kind)
        return STATUS_INVALID_PARAMETER;

    /* stream support: return NULL security descriptor when creating named stream */
    if (Request->Req.Create.NamedStream)
        return STATUS_SUCCESS;

    if (!CreatePrivateObjectSecurity(
        ParentDescriptor,
        0 != Request->Req.Create.SecurityDescriptor.Offset ?
            (PSECURITY_DESCRIPTOR)(Request->Buffer + Request->Req.Create.SecurityDescriptor.Offset) : 0,
        PSecurityDescriptor,
        0 != (Request->Req.Create.CreateOptions & FILE_DIRECTORY_FILE),
        FSP_FSCTL_TRANSACT_REQ_TOKEN_HANDLE(Request->Req.Create.AccessToken),
        &FspFileGenericMapping))
        return FspNtStatusFromWin32(GetLastError());

    //DEBUGLOGSD("SDDL=%s", *PSecurityDescriptor);

    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspSetSecurityDescriptor(
    PSECURITY_DESCRIPTOR InputDescriptor,
    SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR ModificationDescriptor,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor)
{
    *PSecurityDescriptor = 0;

    if (0 == InputDescriptor)
        return STATUS_NO_SECURITY_ON_OBJECT;

    /*
     * SetPrivateObjectSecurity is a broken API. It assumes that the passed
     * descriptor resides on memory allocated by CreatePrivateObjectSecurity
     * or SetPrivateObjectSecurity and frees the descriptor on success.
     *
     * In our case the security descriptor comes from the user mode file system,
     * which may conjure it any way it sees fit. So we have to somehow make a copy
     * of the InputDescriptor and place it in memory that SetPrivateObjectSecurity
     * can then free. To complicate matters there is no API that can be used for
     * this purpose. What a PITA!
     */
    /* !!!: HACK! HACK! HACK!
     *
     * Turns out that SetPrivateObjectSecurity and friends really use RtlProcessHeap
     * internally, which is just another name for GetProcessHeap().
     *
     * I wish there was a cleaner way to do this!
     */

    HANDLE ProcessHeap = GetProcessHeap();
    DWORD InputDescriptorSize = GetSecurityDescriptorLength(InputDescriptor);
    PSECURITY_DESCRIPTOR CopiedDescriptor;

    CopiedDescriptor = HeapAlloc(ProcessHeap, 0, InputDescriptorSize);
    if (0 == CopiedDescriptor)
        return STATUS_INSUFFICIENT_RESOURCES;
    memcpy(CopiedDescriptor, InputDescriptor, InputDescriptorSize);
    InputDescriptor = CopiedDescriptor;

    if (!SetPrivateObjectSecurity(
        SecurityInformation,
        ModificationDescriptor,
        &InputDescriptor,
        &FspFileGenericMapping,
        0))
    {
        HeapFree(ProcessHeap, 0, CopiedDescriptor);
        return FspNtStatusFromWin32(GetLastError());
    }

    /* CopiedDescriptor has been freed by SetPrivateObjectSecurity! */

    *PSecurityDescriptor = InputDescriptor;

    //DEBUGLOGSD("SDDL=%s", *PSecurityDescriptor);

    return STATUS_SUCCESS;
}

FSP_API VOID FspDeleteSecurityDescriptor(PSECURITY_DESCRIPTOR SecurityDescriptor,
    NTSTATUS (*CreateFunc)())
{
    /* stream support: allow NULL security descriptors */
    if (0 == SecurityDescriptor)
        return;

    if ((NTSTATUS (*)())FspAccessCheckEx == CreateFunc ||
        (NTSTATUS (*)())FspPosixMapPermissionsToSecurityDescriptor == CreateFunc ||
        (NTSTATUS (*)())FspPosixMergePermissionsToSecurityDescriptor == CreateFunc)
        MemFree(SecurityDescriptor);
    else
    if ((NTSTATUS (*)())FspCreateSecurityDescriptor == CreateFunc ||
        (NTSTATUS (*)())FspSetSecurityDescriptor == CreateFunc)
        DestroyPrivateObjectSecurity(&SecurityDescriptor);
}
