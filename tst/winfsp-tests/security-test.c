#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <sddl.h>
#include <strsafe.h>
#include "memfs.h"

void *memfs_start_ex(ULONG Flags, ULONG FileInfoTimeout);
void *memfs_start(ULONG Flags);
void memfs_stop(void *data);
PWSTR memfs_volumename(void *data);

extern int NtfsTests;
extern int WinFspDiskTests;
extern int WinFspNetTests;

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
FspDebugLogSD("%s\n", FileSecurityDescriptor);
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
FspDebugLogSD("%s\n", FileSecurityDescriptor);
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
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
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

    memfs_stop(memfs);
}

void setsecurity_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
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
