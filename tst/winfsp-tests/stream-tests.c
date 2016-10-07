#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <sddl.h>
#include <strsafe.h>
#include "memfs.h"

#include "winfsp-tests.h"

static void stream_create_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    HANDLE Handle;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];

    /* single stream */

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    /* multiple streams */

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:bar",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:baz",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW,
        FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:bar",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:baz",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:bar",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:bar",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:baz",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    /* directory streams */

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectory(FilePath, 0);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1:bar",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1:bar",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1:bar",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    /* invalid names */

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_INVALID_NAME == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo:",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_INVALID_NAME == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo:$NONEXISTANTATTR",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_INVALID_NAME == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo:$DATA:",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_INVALID_NAME == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectory(FilePath, 0);
    ASSERT(!Success);
    ASSERT(ERROR_DIRECTORY == GetLastError());

    /* main file */

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0::$DATA",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1::$DATA",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectory(FilePath, 0);
    ASSERT(!Success);
    ASSERT(ERROR_DIRECTORY == GetLastError());

    memfs_stop(memfs);
}

static void stream_create_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        stream_create_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        stream_create_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        stream_create_dotest(MemfsNet, L"\\\\memfs\\share");
}

static void stream_create_sd_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    static PWSTR Sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;WD)";
    UINT8 AbsoluteSecurityDescriptorBuf[SECURITY_DESCRIPTOR_MIN_LENGTH];
    PSECURITY_DESCRIPTOR SecurityDescriptor, AbsoluteSecurityDescriptor = AbsoluteSecurityDescriptorBuf;
    PSID Owner;
    PACL Dacl;
    BOOL OwnerDefaulted, DaclDefaulted, DaclPresent;
    SECURITY_ATTRIBUTES SecurityAttributes = { 0 };
    HANDLE Handle;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];

    Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(Sddl, SDDL_REVISION_1, &SecurityDescriptor, 0);
    ASSERT(Success);

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &SecurityAttributes,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Success = InitializeSecurityDescriptor(AbsoluteSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    ASSERT(Success);
    Success = GetSecurityDescriptorOwner(SecurityDescriptor, &Owner, &OwnerDefaulted);
    ASSERT(Success);
    Success = SetSecurityDescriptorOwner(AbsoluteSecurityDescriptor, Owner, OwnerDefaulted);
    ASSERT(Success);
    Success = GetSecurityDescriptorGroup(SecurityDescriptor, &Owner, &OwnerDefaulted);
    ASSERT(Success);
    Success = SetSecurityDescriptorGroup(AbsoluteSecurityDescriptor, Owner, OwnerDefaulted);
    ASSERT(Success);
    Success = GetSecurityDescriptorDacl(SecurityDescriptor, &DaclPresent, &Dacl, &DaclDefaulted);
    ASSERT(Success);
    Success = SetSecurityDescriptorDacl(AbsoluteSecurityDescriptor, DaclPresent, Dacl, DaclDefaulted);
    ASSERT(Success);

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = AbsoluteSecurityDescriptor;

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &SecurityAttributes,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    LocalFree(SecurityDescriptor);

    memfs_stop(memfs);
}

static void stream_create_sd_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        stream_create_sd_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        stream_create_sd_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        stream_create_sd_dotest(MemfsNet, L"\\\\memfs\\share");
}

static void stream_create_share_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    HANDLE Handle1, Handle2;
    WCHAR FilePath[MAX_PATH];

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle1 = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle1);
    CloseHandle(Handle1);

    {
        /* share test */

        Handle1 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        Handle2 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);

        CloseHandle(Handle1);
        CloseHandle(Handle2);

        Handle1 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        Handle2 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());

        CloseHandle(Handle1);

        Handle1 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        Handle2 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());

        CloseHandle(Handle1);
    }

    {
        /* main file deny delete test #1 */

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle1 = CreateFileW(FilePath,
            DELETE, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle2 = CreateFileW(FilePath,
            FILE_READ_DATA, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);
        CloseHandle(Handle2);

        Handle2 = CreateFileW(FilePath,
            FILE_WRITE_DATA, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);
        CloseHandle(Handle2);

        Handle2 = CreateFileW(FilePath,
            DELETE, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);

        CloseHandle(Handle1);
        CloseHandle(Handle2);

        /* main file deny delete test #2 */

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle1 = CreateFileW(FilePath,
            DELETE, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle2 = CreateFileW(FilePath,
            FILE_READ_DATA, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);
        CloseHandle(Handle2);

        Handle2 = CreateFileW(FilePath,
            FILE_WRITE_DATA, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);
        CloseHandle(Handle2);

        Handle2 = CreateFileW(FilePath,
            DELETE, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);

        CloseHandle(Handle1);
        CloseHandle(Handle2);

        /* main file deny delete test #3 */

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle1 = CreateFileW(FilePath,
            DELETE, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle2 = CreateFileW(FilePath,
            FILE_READ_DATA, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());

        Handle2 = CreateFileW(FilePath,
            FILE_WRITE_DATA, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());

        Handle2 = CreateFileW(FilePath,
            DELETE, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());

        CloseHandle(Handle1);

        /* main file deny delete test #4 */

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle1 = CreateFileW(FilePath,
            DELETE, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle2 = CreateFileW(FilePath,
            FILE_READ_DATA, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());

        Handle2 = CreateFileW(FilePath,
            FILE_WRITE_DATA, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());

        Handle2 = CreateFileW(FilePath,
            DELETE, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());

        CloseHandle(Handle1);
    }

    {
        /* stream deny delete test #1 */

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle1 = CreateFileW(FilePath,
            DELETE, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle2 = CreateFileW(FilePath,
            FILE_READ_DATA, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);
        CloseHandle(Handle2);

        Handle2 = CreateFileW(FilePath,
            FILE_WRITE_DATA, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);
        CloseHandle(Handle2);

        Handle2 = CreateFileW(FilePath,
            DELETE, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);

        CloseHandle(Handle1);
        CloseHandle(Handle2);

        /* stream deny delete test #2 */

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle1 = CreateFileW(FilePath,
            DELETE, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle2 = CreateFileW(FilePath,
            FILE_READ_DATA, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);
        CloseHandle(Handle2);

        Handle2 = CreateFileW(FilePath,
            FILE_WRITE_DATA, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);
        CloseHandle(Handle2);

        Handle2 = CreateFileW(FilePath,
            DELETE, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);

        CloseHandle(Handle1);
        CloseHandle(Handle2);

        /* stream deny delete test #3 */

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle1 = CreateFileW(FilePath,
            DELETE, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle2 = CreateFileW(FilePath,
            FILE_READ_DATA, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);
        CloseHandle(Handle2);

        Handle2 = CreateFileW(FilePath,
            FILE_WRITE_DATA, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);
        CloseHandle(Handle2);

        Handle2 = CreateFileW(FilePath,
            DELETE, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());

        CloseHandle(Handle1);

        /* stream deny delete test #4 */

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle1 = CreateFileW(FilePath,
            DELETE, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle2 = CreateFileW(FilePath,
            FILE_READ_DATA, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);
        CloseHandle(Handle2);

        Handle2 = CreateFileW(FilePath,
            FILE_WRITE_DATA, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);
        CloseHandle(Handle2);

        Handle2 = CreateFileW(FilePath,
            DELETE, 0, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());

        CloseHandle(Handle1);
    }

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle1 = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle1);
    CloseHandle(Handle1);

    memfs_stop(memfs);
}

static void stream_create_share_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        stream_create_share_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        stream_create_share_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        stream_create_share_dotest(MemfsNet, L"\\\\memfs\\share");
}

static void stream_getfileinfo_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    FILE_ATTRIBUTE_TAG_INFO AttributeTagInfo;
    FILE_BASIC_INFO BasicInfo;
    FILE_STANDARD_INFO StandardInfo;
    PUINT8 NameInfoBuf[sizeof(FILE_NAME_INFO) + MAX_PATH];
    PFILE_NAME_INFO PNameInfo = (PVOID)NameInfoBuf;
    BY_HANDLE_FILE_INFORMATION FileInfo;
    FILETIME FileTime;
    LONGLONG TimeLo, TimeHi;
    DWORD nFileIndexHigh, nFileIndexLow;

    GetSystemTimeAsFileTime(&FileTime);
    TimeLo = ((PLARGE_INTEGER)&FileTime)->QuadPart;
    TimeHi = TimeLo + 10000 * 10000/* 10 seconds */;

    /* test stream */

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Success = GetFileInformationByHandleEx(Handle, FileAttributeTagInfo, &AttributeTagInfo, sizeof AttributeTagInfo);
    ASSERT(Success);
    //ASSERT(FILE_ATTRIBUTE_ARCHIVE == AttributeTagInfo.FileAttributes);
    ASSERT(0 == AttributeTagInfo.ReparseTag);

    Success = GetFileInformationByHandleEx(Handle, FileBasicInfo, &BasicInfo, sizeof BasicInfo);
    ASSERT(Success);
    //ASSERT(FILE_ATTRIBUTE_ARCHIVE == BasicInfo.FileAttributes);
    if (-1 != Flags)
        ASSERT(
            TimeLo <= BasicInfo.CreationTime.QuadPart &&
            TimeHi >  BasicInfo.CreationTime.QuadPart);
    ASSERT(
        TimeLo <= BasicInfo.LastAccessTime.QuadPart &&
        TimeHi >  BasicInfo.LastAccessTime.QuadPart);
    ASSERT(
        TimeLo <= BasicInfo.LastWriteTime.QuadPart &&
        TimeHi >  BasicInfo.LastWriteTime.QuadPart);
    ASSERT(
        TimeLo <= BasicInfo.ChangeTime.QuadPart &&
        TimeHi >  BasicInfo.ChangeTime.QuadPart);

    Success = GetFileInformationByHandleEx(Handle, FileStandardInfo, &StandardInfo, sizeof StandardInfo);
    ASSERT(Success);
    ASSERT(0 == StandardInfo.AllocationSize.QuadPart);
    ASSERT(0 == StandardInfo.EndOfFile.QuadPart);
    ASSERT(1 == StandardInfo.NumberOfLinks);
    ASSERT(!StandardInfo.DeletePending);
    ASSERT(!StandardInfo.Directory);

    Success = GetFileInformationByHandleEx(Handle, FileNameInfo, PNameInfo, sizeof *PNameInfo);
    ASSERT(!Success);
    ASSERT(ERROR_MORE_DATA == GetLastError());
    if (-1 == Flags)
        ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 6) * sizeof(WCHAR));
    else if (0 == Prefix)
        ASSERT(PNameInfo->FileNameLength == wcslen(L"\\file0:foo") * sizeof(WCHAR));
    else
        ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 1) * sizeof(WCHAR));
    ASSERT(L'\\' == PNameInfo->FileName[0]);

    Success = GetFileInformationByHandleEx(Handle, FileNameInfo, PNameInfo, sizeof NameInfoBuf);
    ASSERT(Success);
    if (-1 == Flags)
        ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 6) * sizeof(WCHAR));
    else if (0 == Prefix)
        ASSERT(PNameInfo->FileNameLength == wcslen(L"\\file0:foo") * sizeof(WCHAR));
    else
        ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 1) * sizeof(WCHAR));
    if (-1 == Flags)
        ASSERT(0 == memcmp(FilePath + 6, PNameInfo->FileName, PNameInfo->FileNameLength));
    else if (0 == Prefix)
        ASSERT(0 == memcmp(L"\\file0:foo", PNameInfo->FileName, PNameInfo->FileNameLength));
    else
        ASSERT(0 == memcmp(FilePath + 1, PNameInfo->FileName, PNameInfo->FileNameLength));

    Success = GetFileInformationByHandle(Handle, &FileInfo);
    ASSERT(Success);
    //ASSERT(FILE_ATTRIBUTE_ARCHIVE == FileInfo.dwFileAttributes);
    if (-1 != Flags)
        ASSERT(
            TimeLo <= ((PLARGE_INTEGER)&FileInfo.ftCreationTime)->QuadPart &&
            TimeHi >  ((PLARGE_INTEGER)&FileInfo.ftCreationTime)->QuadPart);
    ASSERT(
        TimeLo <= ((PLARGE_INTEGER)&FileInfo.ftLastAccessTime)->QuadPart &&
        TimeHi >  ((PLARGE_INTEGER)&FileInfo.ftLastAccessTime)->QuadPart);
    ASSERT(
        TimeLo <= ((PLARGE_INTEGER)&FileInfo.ftLastWriteTime)->QuadPart &&
        TimeHi >  ((PLARGE_INTEGER)&FileInfo.ftLastWriteTime)->QuadPart);
    ASSERT(0 == FileInfo.nFileSizeLow && 0 == FileInfo.nFileSizeHigh);
    ASSERT(1 == FileInfo.nNumberOfLinks);

    nFileIndexHigh = FileInfo.nFileIndexHigh;
    nFileIndexLow = FileInfo.nFileIndexLow;

    CloseHandle(Handle);

    /* test main file */

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Success = GetFileInformationByHandleEx(Handle, FileAttributeTagInfo, &AttributeTagInfo, sizeof AttributeTagInfo);
    ASSERT(Success);
    //ASSERT(FILE_ATTRIBUTE_ARCHIVE == AttributeTagInfo.FileAttributes);
    ASSERT(0 == AttributeTagInfo.ReparseTag);

    Success = GetFileInformationByHandleEx(Handle, FileBasicInfo, &BasicInfo, sizeof BasicInfo);
    ASSERT(Success);
    //ASSERT(FILE_ATTRIBUTE_ARCHIVE == BasicInfo.FileAttributes);
    if (-1 != Flags)
        ASSERT(
            TimeLo <= BasicInfo.CreationTime.QuadPart &&
            TimeHi >  BasicInfo.CreationTime.QuadPart);
    ASSERT(
        TimeLo <= BasicInfo.LastAccessTime.QuadPart &&
        TimeHi >  BasicInfo.LastAccessTime.QuadPart);
    ASSERT(
        TimeLo <= BasicInfo.LastWriteTime.QuadPart &&
        TimeHi >  BasicInfo.LastWriteTime.QuadPart);
    ASSERT(
        TimeLo <= BasicInfo.ChangeTime.QuadPart &&
        TimeHi >  BasicInfo.ChangeTime.QuadPart);

    Success = GetFileInformationByHandleEx(Handle, FileStandardInfo, &StandardInfo, sizeof StandardInfo);
    ASSERT(Success);
    ASSERT(0 == StandardInfo.AllocationSize.QuadPart);
    ASSERT(0 == StandardInfo.EndOfFile.QuadPart);
    ASSERT(1 == StandardInfo.NumberOfLinks);
    ASSERT(!StandardInfo.DeletePending);
    ASSERT(!StandardInfo.Directory);

    Success = GetFileInformationByHandleEx(Handle, FileNameInfo, PNameInfo, sizeof *PNameInfo);
    ASSERT(!Success);
    ASSERT(ERROR_MORE_DATA == GetLastError());
    if (-1 == Flags)
        ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 6) * sizeof(WCHAR));
    else if (0 == Prefix)
        ASSERT(PNameInfo->FileNameLength == wcslen(L"\\file0") * sizeof(WCHAR));
    else
        ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 1) * sizeof(WCHAR));
    ASSERT(L'\\' == PNameInfo->FileName[0]);

    Success = GetFileInformationByHandleEx(Handle, FileNameInfo, PNameInfo, sizeof NameInfoBuf);
    ASSERT(Success);
    if (-1 == Flags)
        ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 6) * sizeof(WCHAR));
    else if (0 == Prefix)
        ASSERT(PNameInfo->FileNameLength == wcslen(L"\\file0") * sizeof(WCHAR));
    else
        ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 1) * sizeof(WCHAR));
    if (-1 == Flags)
        ASSERT(0 == memcmp(FilePath + 6, PNameInfo->FileName, PNameInfo->FileNameLength));
    else if (0 == Prefix)
        ASSERT(0 == memcmp(L"\\file0", PNameInfo->FileName, PNameInfo->FileNameLength));
    else
        ASSERT(0 == memcmp(FilePath + 1, PNameInfo->FileName, PNameInfo->FileNameLength));

    Success = GetFileInformationByHandle(Handle, &FileInfo);
    ASSERT(Success);
    //ASSERT(FILE_ATTRIBUTE_ARCHIVE == FileInfo.dwFileAttributes);
    if (-1 != Flags)
        ASSERT(
            TimeLo <= ((PLARGE_INTEGER)&FileInfo.ftCreationTime)->QuadPart &&
            TimeHi >  ((PLARGE_INTEGER)&FileInfo.ftCreationTime)->QuadPart);
    ASSERT(
        TimeLo <= ((PLARGE_INTEGER)&FileInfo.ftLastAccessTime)->QuadPart &&
        TimeHi >  ((PLARGE_INTEGER)&FileInfo.ftLastAccessTime)->QuadPart);
    ASSERT(
        TimeLo <= ((PLARGE_INTEGER)&FileInfo.ftLastWriteTime)->QuadPart &&
        TimeHi >  ((PLARGE_INTEGER)&FileInfo.ftLastWriteTime)->QuadPart);
    ASSERT(0 == FileInfo.nFileSizeLow && 0 == FileInfo.nFileSizeHigh);
    ASSERT(1 == FileInfo.nNumberOfLinks);

    ASSERT(nFileIndexHigh == FileInfo.nFileIndexHigh);
    ASSERT(nFileIndexLow == FileInfo.nFileIndexLow);

    CloseHandle(Handle);

    memfs_stop(memfs);
}

static void stream_getfileinfo_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        stream_getfileinfo_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        stream_getfileinfo_dotest(MemfsDisk, 0, 0);
        stream_getfileinfo_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        stream_getfileinfo_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        stream_getfileinfo_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void stream_setfileinfo_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle, StreamHandle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    BY_HANDLE_FILE_INFORMATION FileInfo0, FileInfo;
    FILETIME FileTime;
    DWORD Offset;
    DWORD nFileIndexHigh, nFileIndexLow;

    /* test stream */

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StreamHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != StreamHandle);

    Success = GetFileInformationByHandle(StreamHandle, &FileInfo0);
    ASSERT(Success);
    //ASSERT(FILE_ATTRIBUTE_ARCHIVE == FileInfo0.dwFileAttributes);

    Success = SetFileAttributesW(FilePath, FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN);
    ASSERT(Success);

    Success = GetFileInformationByHandle(StreamHandle, &FileInfo);
    ASSERT(Success);
    ASSERT(FILE_ATTRIBUTE_HIDDEN == FileInfo.dwFileAttributes);

    *(PUINT64)&FileTime = 0x4200000042ULL;
    Success = SetFileTime(StreamHandle, 0, &FileTime, &FileTime);
    ASSERT(Success);

    Success = GetFileInformationByHandle(StreamHandle, &FileInfo);
    ASSERT(Success);
    ASSERT(*(PUINT64)&FileInfo0.ftCreationTime == *(PUINT64)&FileInfo.ftCreationTime);
    ASSERT(0x4200000042ULL == *(PUINT64)&FileInfo.ftLastAccessTime);
    ASSERT(0x4200000042ULL == *(PUINT64)&FileInfo.ftLastWriteTime);

    Success = SetFileTime(StreamHandle, &FileTime, 0, 0);
    ASSERT(Success);

    Success = GetFileInformationByHandle(StreamHandle, &FileInfo);
    ASSERT(Success);
    ASSERT(0x4200000042ULL == *(PUINT64)&FileInfo.ftCreationTime);

    Offset = SetFilePointer(StreamHandle, 42, 0, 0);
    ASSERT(42 == Offset);

    Success = SetEndOfFile(StreamHandle);
    ASSERT(Success);

    Success = GetFileInformationByHandle(StreamHandle, &FileInfo);
    ASSERT(Success);
    ASSERT(42 == FileInfo.nFileSizeLow);
    ASSERT(0 == FileInfo.nFileSizeHigh);

    nFileIndexHigh = FileInfo.nFileIndexHigh;
    nFileIndexLow = FileInfo.nFileIndexLow;

    /* test main file */

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
        OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Success = GetFileInformationByHandle(Handle, &FileInfo);
    ASSERT(Success);
    ASSERT(0 != (FileInfo.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN));
    ASSERT(0x4200000042ULL == *(PUINT64)&FileInfo.ftLastAccessTime);
    ASSERT(0x4200000042ULL == *(PUINT64)&FileInfo.ftLastWriteTime);
    ASSERT(0x4200000042ULL == *(PUINT64)&FileInfo.ftCreationTime);
    ASSERT(0 == FileInfo.nFileSizeLow);
    ASSERT(0 == FileInfo.nFileSizeHigh);

    ASSERT(nFileIndexHigh == FileInfo.nFileIndexHigh);
    ASSERT(nFileIndexLow == FileInfo.nFileIndexLow);

    /* test mixed */

    *(PUINT64)&FileTime = 0x4100000041ULL;
    Success = SetFileTime(StreamHandle, &FileTime, &FileTime, &FileTime);
    ASSERT(Success);

    Success = GetFileInformationByHandle(Handle, &FileInfo);
    ASSERT(Success);
    ASSERT(0x4100000041ULL == *(PUINT64)&FileInfo.ftLastAccessTime);
    ASSERT(0x4100000041ULL == *(PUINT64)&FileInfo.ftLastWriteTime);
    ASSERT(0x4100000041ULL == *(PUINT64)&FileInfo.ftCreationTime);

    *(PUINT64)&FileTime = 0x4300000043ULL;
    Success = SetFileTime(Handle, &FileTime, &FileTime, &FileTime);
    ASSERT(Success);

    Success = GetFileInformationByHandle(StreamHandle, &FileInfo);
    ASSERT(Success);
    ASSERT(0x4300000043ULL == *(PUINT64)&FileInfo.ftLastAccessTime);
    ASSERT(0x4300000043ULL == *(PUINT64)&FileInfo.ftLastWriteTime);
    ASSERT(0x4300000043ULL == *(PUINT64)&FileInfo.ftCreationTime);

    CloseHandle(StreamHandle);
    CloseHandle(Handle);

    memfs_stop(memfs);
}

static void stream_setfileinfo_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        stream_setfileinfo_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        stream_setfileinfo_dotest(MemfsDisk, 0, 0);
        stream_setfileinfo_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        stream_setfileinfo_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        stream_setfileinfo_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void stream_delete_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR Dir1Path[MAX_PATH], Dir1StreamPath[MAX_PATH];
    WCHAR FilePath[MAX_PATH], FileStreamPath[MAX_PATH];

    StringCbPrintfW(Dir1Path, sizeof Dir1Path, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(Dir1StreamPath, sizeof Dir1StreamPath, L"%s%s\\dir1:foo1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(FileStreamPath, sizeof FileStreamPath, L"%s%s\\dir1\\file0:foo0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(Dir1Path, 0);
    ASSERT(Success);

    Handle = CreateFileW(Dir1StreamPath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FileStreamPath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(!Success);
    ASSERT(ERROR_DIR_NOT_EMPTY == GetLastError());

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    Success = DeleteFileW(FilePath);
    ASSERT(!Success);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(Success);

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(!Success);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    memfs_stop(memfs);
}

static void stream_delete_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        stream_delete_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        stream_delete_dotest(MemfsDisk, 0, 0);
        stream_delete_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        stream_delete_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        stream_delete_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void stream_delete_pending_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle, StreamHandle;
    BOOL Success;
    WCHAR Dir1Path[MAX_PATH], Dir1StreamPath[MAX_PATH];
    WCHAR FilePath[MAX_PATH], FileStreamPath[MAX_PATH];
    FILE_DISPOSITION_INFO DispositionInfo;

    StringCbPrintfW(Dir1Path, sizeof Dir1Path, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(Dir1StreamPath, sizeof Dir1StreamPath, L"%s%s\\dir1:foo1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(FileStreamPath, sizeof FileStreamPath, L"%s%s\\dir1\\file0:foo0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(Dir1Path, 0);
    ASSERT(Success);

    Handle = CreateFileW(Dir1StreamPath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FileStreamPath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(!Success);
    ASSERT(ERROR_DIR_NOT_EMPTY == GetLastError());

    {
        Handle = CreateFileW(FilePath,
            DELETE, FILE_SHARE_DELETE, 0,
            OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        DispositionInfo.DeleteFile = TRUE;
        Success = SetFileInformationByHandle(Handle,
            FileDispositionInfo, &DispositionInfo, sizeof DispositionInfo);
        ASSERT(Success);

        StreamHandle = CreateFileW(FileStreamPath,
            FILE_READ_ATTRIBUTES, 0, 0,
            OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == StreamHandle);
        ASSERT(ERROR_ACCESS_DENIED == GetLastError());

        CloseHandle(Handle);
    }

    Success = DeleteFileW(FilePath);
    ASSERT(!Success);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    {
        Handle = CreateFileW(Dir1Path,
            DELETE, FILE_SHARE_DELETE, 0,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        DispositionInfo.DeleteFile = TRUE;
        Success = SetFileInformationByHandle(Handle,
            FileDispositionInfo, &DispositionInfo, sizeof DispositionInfo);
        ASSERT(Success);

        StreamHandle = CreateFileW(Dir1StreamPath,
            FILE_READ_ATTRIBUTES, 0, 0,
            OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == StreamHandle);
        ASSERT(ERROR_ACCESS_DENIED == GetLastError());

        CloseHandle(Handle);
    }

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(!Success);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    memfs_stop(memfs);
}

static void stream_delete_pending_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        stream_delete_pending_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        stream_delete_pending_dotest(MemfsDisk, 0, 0);
        stream_delete_pending_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        stream_delete_pending_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        stream_delete_pending_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void stream_getsecurity_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
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

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &SecurityAttributes,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
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

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
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

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    LocalFree(SecurityDescriptor);

    memfs_stop(memfs);
}

static void stream_getsecurity_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        stream_getsecurity_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        stream_getsecurity_dotest(MemfsDisk, 0, 0);
        stream_getsecurity_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        stream_getsecurity_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        stream_getsecurity_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void stream_setsecurity_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
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
    HANDLE Handle, StreamHandle;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH], StreamPath[MAX_PATH];

    Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(Sddl, SDDL_REVISION_1, &SecurityDescriptor, 0);
    ASSERT(Success);

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;

    StringCbPrintfW(StreamPath, sizeof StreamPath, L"%s%s\\file0:foo",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StreamHandle = CreateFileW(StreamPath,
        GENERIC_READ | GENERIC_WRITE | WRITE_DAC, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &SecurityAttributes,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != StreamHandle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
        OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
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

    Success = SetKernelObjectSecurity(StreamHandle, DACL_SECURITY_INFORMATION, SecurityDescriptor);
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

    CloseHandle(StreamHandle);
    CloseHandle(Handle);

    LocalFree(SecurityDescriptor);

    memfs_stop(memfs);
}

static void stream_setsecurity_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        stream_setsecurity_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        stream_setsecurity_dotest(MemfsDisk, 0, 0);
        stream_setsecurity_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        stream_setsecurity_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        stream_setsecurity_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void stream_getstreaminfo_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout, ULONG SleepTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    WIN32_FIND_STREAM_DATA FindData;
    ULONG FileCount, FileTotal;

    for (int i = 1; 10 >= i; i++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), i);
        Handle = CreateFileW(FilePath, GENERIC_ALL, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    for (int i = 1; 10 >= i; i++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), i);
        Success = CreateDirectoryW(FilePath, 0);
        ASSERT(Success);
    }

    for (int j = 1; 100 >= j; j++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file5:s%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), j);
        Handle = CreateFileW(FilePath, GENERIC_ALL, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    for (int j = 1; 100 >= j; j++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir5:s%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), j);
        Handle = CreateFileW(FilePath, GENERIC_ALL, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    DWORD times[2];
    times[0] = GetTickCount();

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstStreamW(FilePath, FindStreamInfoStandard, &FindData, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    ASSERT(0 == wcscmp(FindData.cStreamName, L"::$DATA"));
    Success = FindNextStreamW(Handle, &FindData);
    ASSERT(!Success);
    ASSERT(ERROR_HANDLE_EOF == GetLastError());
    Success = FindClose(Handle);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file5",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstStreamW(FilePath, FindStreamInfoStandard, &FindData, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FileCount = FileTotal = 0;
    do
    {
        unsigned long ul;
        wchar_t *endp;

        if (1 > FileCount)
        {
            FileCount++;
            ASSERT(0 == wcscmp(FindData.cStreamName, L"::$DATA"));
            continue;
        }

        ASSERT(0 == wcsncmp(FindData.cStreamName, L":s", 2));
        ul = wcstoul(FindData.cStreamName + 2, &endp, 10);
        ASSERT(0 != ul);
        ASSERT(L':' == *endp);

        FileCount++;
        FileTotal += ul;

        if (0 < SleepTimeout && 5 == FileCount)
            Sleep(SleepTimeout);
    } while (FindNextStreamW(Handle, &FindData));
    ASSERT(ERROR_HANDLE_EOF == GetLastError());

    ASSERT(101 == FileCount);
    ASSERT(101 * 100 / 2 == FileTotal);

    Success = FindClose(Handle);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file5:s50",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstStreamW(FilePath, FindStreamInfoStandard, &FindData, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FileCount = FileTotal = 0;
    do
    {
        unsigned long ul;
        wchar_t *endp;

        if (1 > FileCount)
        {
            FileCount++;
            ASSERT(0 == wcscmp(FindData.cStreamName, L"::$DATA"));
            continue;
        }

        ASSERT(0 == wcsncmp(FindData.cStreamName, L":s", 2));
        ul = wcstoul(FindData.cStreamName + 2, &endp, 10);
        ASSERT(0 != ul);
        ASSERT(L':' == *endp);

        FileCount++;
        FileTotal += ul;

        if (0 < SleepTimeout && 5 == FileCount)
            Sleep(SleepTimeout);
    } while (FindNextStreamW(Handle, &FindData));
    ASSERT(ERROR_HANDLE_EOF == GetLastError());

    ASSERT(101 == FileCount);
    ASSERT(101 * 100 / 2 == FileTotal);

    Success = FindClose(Handle);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstStreamW(FilePath, FindStreamInfoStandard, &FindData, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_HANDLE_EOF == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir5",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstStreamW(FilePath, FindStreamInfoStandard, &FindData, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FileCount = FileTotal = 0;
    do
    {
        unsigned long ul;
        wchar_t *endp;

        ASSERT(0 == wcsncmp(FindData.cStreamName, L":s", 2));
        ul = wcstoul(FindData.cStreamName + 2, &endp, 10);
        ASSERT(0 != ul);
        ASSERT(L':' == *endp);

        FileCount++;
        FileTotal += ul;

        if (0 < SleepTimeout && 5 == FileCount)
            Sleep(SleepTimeout);
    } while (FindNextStreamW(Handle, &FindData));
    ASSERT(ERROR_HANDLE_EOF == GetLastError());

    ASSERT(100 == FileCount);
    ASSERT(101 * 100 / 2 == FileTotal);

    Success = FindClose(Handle);
    ASSERT(Success);

    times[1] = GetTickCount();
    FspDebugLog(__FUNCTION__ "(Flags=%lx, Prefix=\"%S\", FileInfoTimeout=%ld, SleepTimeout=%ld): %ldms\n",
        Flags, Prefix, FileInfoTimeout, SleepTimeout, times[1] - times[0]);

    for (int i = 1; 10 >= i; i++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), i);
        Success = DeleteFileW(FilePath);
        ASSERT(Success);
    }

    for (int i = 1; 10 >= i; i++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), i);
        Success = RemoveDirectoryW(FilePath);
        ASSERT(Success);
    }

    memfs_stop(memfs);
}

void stream_getstreaminfo_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        stream_getstreaminfo_dotest(-1, DirBuf, 0, 0);
    }
    if (WinFspDiskTests)
    {
        stream_getstreaminfo_dotest(MemfsDisk, 0, 0, 0);
        stream_getstreaminfo_dotest(MemfsDisk, 0, 1000, 0);
    }
    if (WinFspNetTests)
    {
        stream_getstreaminfo_dotest(MemfsNet, L"\\\\memfs\\share", 0, 0);
        stream_getstreaminfo_dotest(MemfsNet, L"\\\\memfs\\share", 1000, 0);
    }
}

void stream_getstreaminfo_expire_cache_test(void)
{
    if (WinFspDiskTests)
    {
        stream_getstreaminfo_dotest(MemfsDisk, 0, 500, 750);
    }
    if (WinFspNetTests)
    {
        stream_getstreaminfo_dotest(MemfsNet, L"\\\\memfs\\share", 500, 750);
    }
}

void stream_tests(void)
{
    TEST(stream_create_test);
    TEST(stream_create_sd_test);
    TEST(stream_create_share_test);
    TEST(stream_getfileinfo_test);
    TEST(stream_setfileinfo_test);
    TEST(stream_delete_test);
    TEST(stream_delete_pending_test);
    TEST(stream_getsecurity_test);
    TEST(stream_setsecurity_test);
    TEST(stream_getstreaminfo_test);
    TEST(stream_getstreaminfo_expire_cache_test);
}
