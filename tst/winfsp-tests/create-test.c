#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <sddl.h>
#include <strsafe.h>
#include "memfs.h"

void *memfs_start(ULONG Flags);
void memfs_stop(void *data);
PWSTR memfs_volumename(void *data);

extern int NtfsTests;
extern int WinFspDiskTests;
extern int WinFspNetTests;

void create_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    HANDLE Handle;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
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
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, 0);
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

    if (0 == Prefix)
    {
        /* double backslash at path root */

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        CloseHandle(Handle);

        Handle = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        CloseHandle(Handle);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\\\\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle);
        ASSERT(ERROR_INVALID_NAME == GetLastError());
    }

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectory(FilePath, 0);
    ASSERT(Success);

    Success = CreateDirectory(FilePath, 0);
    ASSERT(!Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    {
        /* attempt to DELETE_ON_CLOSE a non-empty directory! */

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        CloseHandle(Handle);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file2",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    }

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    if (-1 == Flags || 0 == Prefix)
    {
        /* backslash at path end */

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0\\",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle);
        ASSERT(ERROR_INVALID_NAME == GetLastError());

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\\\",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Success = CreateDirectory(FilePath, 0);
        ASSERT(!Success);
        ASSERT(ERROR_INVALID_NAME == GetLastError());

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Success = CreateDirectory(FilePath, 0);
        ASSERT(Success);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        CloseHandle(Handle);
    }

    memfs_stop(memfs);
}

void create_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        create_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        create_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_dotest(MemfsNet, L"\\\\memfs\\share");
}

void create_related_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    HANDLE DirHandle, FileHandle;
    NTSTATUS Result;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];
    WCHAR UnicodePathBuf[MAX_PATH] = L"file2";
    UNICODE_STRING UnicodePath;
    OBJECT_ATTRIBUTES Obja;
    IO_STATUS_BLOCK Iosb;
    LARGE_INTEGER LargeZero = { 0 };

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectory(FilePath, 0);
    ASSERT(Success);

    DirHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != DirHandle);

    UnicodePath.Length = (USHORT)wcslen(UnicodePathBuf) * sizeof(WCHAR);
    UnicodePath.MaximumLength = sizeof UnicodePathBuf;
    UnicodePath.Buffer = UnicodePathBuf;
    InitializeObjectAttributes(&Obja, &UnicodePath, 0, DirHandle, 0);
    Result = NtCreateFile(&FileHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, &Obja, &Iosb,
        &LargeZero, FILE_ATTRIBUTE_NORMAL, 0,
        FILE_CREATE, FILE_DELETE_ON_CLOSE, 0, 0);
    ASSERT(STATUS_SUCCESS == Result);
    CloseHandle(FileHandle);

    CloseHandle(DirHandle);

    DirHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == DirHandle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    memfs_stop(memfs);
}

void create_related_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        create_related_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        create_related_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_related_dotest(MemfsNet, L"\\\\memfs\\share");
}

void create_sd_dotest(ULONG Flags, PWSTR Prefix)
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
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectory(FilePath, &SecurityAttributes);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
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
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectory(FilePath, &SecurityAttributes);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    LocalFree(SecurityDescriptor);

    memfs_stop(memfs);
}

void create_sd_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        create_sd_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        create_sd_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_sd_dotest(MemfsNet, L"\\\\memfs\\share");
}

void create_share_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    HANDLE Handle1, Handle2;
    WCHAR FilePath[MAX_PATH];

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
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

    Handle1 = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle1);
    CloseHandle(Handle1);

    memfs_stop(memfs);
}

void create_share_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        create_share_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        create_share_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_share_dotest(MemfsNet, L"\\\\memfs\\share");
}

void create_tests(void)
{
    TEST(create_test);
    TEST(create_related_test);
    TEST(create_sd_test);
    TEST(create_share_test);
}
