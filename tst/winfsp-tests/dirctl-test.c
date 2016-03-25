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

void querydir_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    WIN32_FIND_DATAW FindData;
    ULONG FileCount, FileTotal;

    for (int i = 1; 10 >= i; i++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), i);
        Success = CreateDirectoryW(FilePath, 0);
        ASSERT(Success);
    }

    for (int j = 1; 100 >= j; j++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\fileABCDEFGHIJKLMNOPQRSTUVXWYZfile%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), j);
        Handle = CreateFileW(FilePath, GENERIC_ALL, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    for (int j = 1; 100 >= j; j++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir5\\file%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), j);
        Handle = CreateFileW(FilePath, GENERIC_ALL, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir5\\*",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstFileW(FilePath, &FindData);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FileCount = FileTotal = 0;
    do
    {
        unsigned long ul;
        wchar_t *endp;

        if (2 > FileCount)
        {
            FileCount++;
            ASSERT(0 != (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
            ASSERT(FileCount == wcslen(FindData.cFileName));
            ASSERT(0 == wcsncmp(FindData.cFileName, L"..", FileCount));
            continue;
        }

        ASSERT(0 == (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
        ASSERT(0 == wcsncmp(FindData.cFileName, L"file", 4));
        ul = wcstoul(FindData.cFileName + 4, &endp, 10);
        ASSERT(0 != ul);
        ASSERT(L'\0' == *endp);

        FileCount++;
        FileTotal += ul;
    } while (FindNextFileW(Handle, &FindData));
    ASSERT(ERROR_NO_MORE_FILES == GetLastError());

    ASSERT(102 == FileCount);
    ASSERT(101 * 100 / 2 == FileTotal);

    Success = FindClose(Handle);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir*",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstFileW(FilePath, &FindData);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FileCount = FileTotal = 0;
    do
    {
        unsigned long ul;
        wchar_t *endp;

        ASSERT(0 != (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
        ASSERT(0 == wcsncmp(FindData.cFileName, L"dir", 3));
        ul = wcstoul(FindData.cFileName + 3, &endp, 10);
        ASSERT(0 != ul);
        ASSERT(L'\0' == *endp);

        FileCount++;
        FileTotal += ul;
    } while (FindNextFileW(Handle, &FindData));
    ASSERT(ERROR_NO_MORE_FILES == GetLastError());

    ASSERT(10 == FileCount);
    ASSERT(11 * 10 / 2 == FileTotal);

    Success = FindClose(Handle);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file*",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstFileW(FilePath, &FindData);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FileCount = FileTotal = 0;
    do
    {
        unsigned long ul;
        wchar_t *endp;
        size_t wcscnt = sizeof "fileABCDEFGHIJKLMNOPQRSTUVXWYZfile" - 1/* count of wchar_t*/;

        ASSERT(0 == (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
        ASSERT(0 == wcsncmp(FindData.cFileName, L"fileABCDEFGHIJKLMNOPQRSTUVXWYZfile", wcscnt));
        ul = wcstoul(FindData.cFileName + wcscnt, &endp, 10);
        ASSERT(0 != ul);
        ASSERT(L'\0' == *endp);

        FileCount++;
        FileTotal += ul;
    } while (FindNextFileW(Handle, &FindData));
    ASSERT(ERROR_NO_MORE_FILES == GetLastError());

    ASSERT(100 == FileCount);
    ASSERT(101 * 100 / 2 == FileTotal);

    Success = FindClose(Handle);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\DOES-NOT-EXIST",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstFileW(FilePath, &FindData);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    for (int j = 1; 100 >= j; j++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir5\\file%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), j);
        Success = DeleteFileW(FilePath);
        ASSERT(Success);
    }

    for (int j = 1; 100 >= j; j++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\fileABCDEFGHIJKLMNOPQRSTUVXWYZfile%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), j);
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

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file*",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstFileW(FilePath, &FindData);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    memfs_stop(memfs);
}

void querydir_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        querydir_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        querydir_dotest(MemfsDisk, 0, 0);
        querydir_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        querydir_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        querydir_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

void dirctl_tests(void)
{
    TEST(querydir_test);
}
