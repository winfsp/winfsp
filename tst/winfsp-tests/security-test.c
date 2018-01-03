/**
 * @file security-test.c
 *
 * @copyright 2015-2018 Bill Zissimopoulos
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

#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <sddl.h>
#include <strsafe.h>
#include "memfs.h"

#include "winfsp-tests.h"

void getsecurity_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    static PWSTR Sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;WD)";
    PSECURITY_DESCRIPTOR SecurityDescriptor, FileSecurityDescriptor;
    SECURITY_ATTRIBUTES SecurityAttributes = { 0 };
    PSID Owner, Group;
    PACL Dacl, Sacl;
    BOOL OwnerDefaulted, GroupDefaulted, DaclDefaulted, DaclPresent, SaclDefaulted, SaclPresent;
    DWORD Length;
    HANDLE Handle;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];

    Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(Sddl, SDDL_REVISION_1, &SecurityDescriptor, 0);
    ASSERT(Success);

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &SecurityAttributes,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Success = GetKernelObjectSecurity(Handle, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
        0, 0, &Length);
    ASSERT(!Success);
    ASSERT(ERROR_INSUFFICIENT_BUFFER == GetLastError());
    FileSecurityDescriptor = malloc(Length);
    Success = GetKernelObjectSecurity(Handle, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
        FileSecurityDescriptor, Length, &Length);
    ASSERT(Success);
    Success = GetSecurityDescriptorOwner(FileSecurityDescriptor, &Owner, &OwnerDefaulted);
    ASSERT(Success);
    ASSERT(0 != Owner);
    Success = GetSecurityDescriptorGroup(FileSecurityDescriptor, &Group, &GroupDefaulted);
    ASSERT(Success);
    ASSERT(0 != Group);
    Success = GetSecurityDescriptorDacl(FileSecurityDescriptor, &DaclPresent, &Dacl, &DaclDefaulted);
    ASSERT(Success);
    ASSERT(!DaclPresent);
    Success = GetSecurityDescriptorSacl(FileSecurityDescriptor, &SaclPresent, &Sacl, &SaclDefaulted);
    ASSERT(Success);
    ASSERT(!SaclPresent);
    free(FileSecurityDescriptor);

    Success = GetKernelObjectSecurity(Handle, DACL_SECURITY_INFORMATION,
        0, 0, &Length);
    ASSERT(!Success);
    ASSERT(ERROR_INSUFFICIENT_BUFFER == GetLastError());
    FileSecurityDescriptor = malloc(Length);
    Success = GetKernelObjectSecurity(Handle, DACL_SECURITY_INFORMATION,
        FileSecurityDescriptor, Length, &Length);
    ASSERT(Success);
    Success = GetSecurityDescriptorOwner(FileSecurityDescriptor, &Owner, &OwnerDefaulted);
    ASSERT(Success);
    ASSERT(0 == Owner);
    Success = GetSecurityDescriptorGroup(FileSecurityDescriptor, &Group, &GroupDefaulted);
    ASSERT(Success);
    ASSERT(0 == Group);
    Success = GetSecurityDescriptorDacl(FileSecurityDescriptor, &DaclPresent, &Dacl, &DaclDefaulted);
    ASSERT(Success);
    ASSERT(DaclPresent);
    ASSERT(0 != Dacl);
    Success = GetSecurityDescriptorSacl(FileSecurityDescriptor, &SaclPresent, &Sacl, &SaclDefaulted);
    ASSERT(Success);
    ASSERT(!SaclPresent);
    free(FileSecurityDescriptor);

    CloseHandle(Handle);

    LocalFree(SecurityDescriptor);

    memfs_stop(memfs);
}

void getsecurity_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        getsecurity_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        getsecurity_dotest(MemfsDisk, 0, 0);
        getsecurity_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        getsecurity_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        getsecurity_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

void setsecurity_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    static PWSTR Sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;WD)";
    static PWSTR Sddl2 = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)";
    PWSTR ConvertedSddl;
    PSECURITY_DESCRIPTOR SecurityDescriptor, FileSecurityDescriptor, FileSecurityDescriptor2;
    SECURITY_ATTRIBUTES SecurityAttributes = { 0 };
    PSID Owner, Owner2, Group, Group2;
    PACL Dacl, Dacl2, Sacl, Sacl2;
    BOOL OwnerDefaulted, GroupDefaulted, DaclDefaulted, DaclPresent, SaclDefaulted, SaclPresent;
    DWORD Length;
    HANDLE Handle;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];

    Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(Sddl, SDDL_REVISION_1, &SecurityDescriptor, 0);
    ASSERT(Success);

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &SecurityAttributes,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE | WRITE_DAC, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Success = GetKernelObjectSecurity(Handle,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        0, 0, &Length);
    ASSERT(!Success);
    ASSERT(ERROR_INSUFFICIENT_BUFFER == GetLastError());
    FileSecurityDescriptor = malloc(Length);
    Success = GetKernelObjectSecurity(Handle,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        FileSecurityDescriptor, Length, &Length);
    ASSERT(Success);
    Success = GetSecurityDescriptorOwner(FileSecurityDescriptor, &Owner, &OwnerDefaulted);
    ASSERT(Success);
    ASSERT(0 != Owner);
    Success = GetSecurityDescriptorGroup(FileSecurityDescriptor, &Group, &GroupDefaulted);
    ASSERT(Success);
    ASSERT(0 != Group);
    Success = GetSecurityDescriptorDacl(FileSecurityDescriptor, &DaclPresent, &Dacl, &DaclDefaulted);
    ASSERT(Success);
    ASSERT(DaclPresent);
    ASSERT(0 != Dacl);
    Success = GetSecurityDescriptorSacl(FileSecurityDescriptor, &SaclPresent, &Sacl, &SaclDefaulted);
    ASSERT(Success);
    ASSERT(!SaclPresent);

    LocalFree(SecurityDescriptor);
    Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(Sddl2, SDDL_REVISION_1, &SecurityDescriptor, 0);
    ASSERT(Success);

    Success = SetKernelObjectSecurity(Handle, DACL_SECURITY_INFORMATION, SecurityDescriptor);
    ASSERT(Success);

    Success = GetKernelObjectSecurity(Handle,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        0, 0, &Length);
    ASSERT(!Success);
    ASSERT(ERROR_INSUFFICIENT_BUFFER == GetLastError());
    FileSecurityDescriptor2 = malloc(Length);
    Success = GetKernelObjectSecurity(Handle,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        FileSecurityDescriptor2, Length, &Length);
    ASSERT(Success);
    Success = GetSecurityDescriptorOwner(FileSecurityDescriptor2, &Owner2, &OwnerDefaulted);
    ASSERT(Success);
    ASSERT(0 != Owner2);
    Success = GetSecurityDescriptorGroup(FileSecurityDescriptor2, &Group2, &GroupDefaulted);
    ASSERT(Success);
    ASSERT(0 != Group2);
    Success = GetSecurityDescriptorDacl(FileSecurityDescriptor2, &DaclPresent, &Dacl2, &DaclDefaulted);
    ASSERT(Success);
    ASSERT(DaclPresent);
    ASSERT(0 != Dacl2);
    Success = GetSecurityDescriptorSacl(FileSecurityDescriptor2, &SaclPresent, &Sacl2, &SaclDefaulted);
    ASSERT(Success);
    ASSERT(!SaclPresent);

    ASSERT(EqualSid(Owner, Owner2));
    ASSERT(EqualSid(Group, Group2));
    ASSERT(ConvertSecurityDescriptorToStringSecurityDescriptorW(FileSecurityDescriptor2, SDDL_REVISION_1,
        DACL_SECURITY_INFORMATION, &ConvertedSddl, 0));
    ASSERT(0 == wcscmp(L"D:P(A;;FA;;;SY)(A;;FA;;;BA)", ConvertedSddl));
    LocalFree(ConvertedSddl);

    free(FileSecurityDescriptor);
    free(FileSecurityDescriptor2);

    CloseHandle(Handle);

    LocalFree(SecurityDescriptor);

    memfs_stop(memfs);
}

void setsecurity_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        setsecurity_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        setsecurity_dotest(MemfsDisk, 0, 0);
        setsecurity_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        setsecurity_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        setsecurity_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

void security_tests(void)
{
    TEST(getsecurity_test);
    TEST(setsecurity_test);
}
