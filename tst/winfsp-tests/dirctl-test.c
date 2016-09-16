#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <process.h>
#include <sddl.h>
#include <strsafe.h>
#include "memfs.h"

#include "winfsp-tests.h"

static void querydir_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout, ULONG SleepTimeout)
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

    DWORD times[2];
    times[0] = GetTickCount();

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

        if (0 < SleepTimeout && 5 == FileCount)
            Sleep(SleepTimeout);
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

        if (0 < SleepTimeout && 5 == FileCount)
            Sleep(SleepTimeout);
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

        if (0 < SleepTimeout && 5 == FileCount)
            Sleep(SleepTimeout);
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

    times[1] = GetTickCount();
    FspDebugLog(__FUNCTION__ "(Flags=%lx, Prefix=\"%S\", FileInfoTimeout=%ld, SleepTimeout=%ld): %ldms\n",
        Flags, Prefix, FileInfoTimeout, SleepTimeout, times[1] - times[0]);

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
        querydir_dotest(-1, DirBuf, 0, 0);
    }
    if (WinFspDiskTests)
    {
        querydir_dotest(MemfsDisk, 0, 0, 0);
        querydir_dotest(MemfsDisk, 0, 1000, 0);
    }
    if (WinFspNetTests)
    {
        querydir_dotest(MemfsNet, L"\\\\memfs\\share", 0, 0);
        querydir_dotest(MemfsNet, L"\\\\memfs\\share", 1000, 0);
    }
}

void querydir_expire_cache_test(void)
{
    if (WinFspDiskTests)
    {
        querydir_dotest(MemfsDisk, 0, 500, 750);
    }
    if (WinFspNetTests)
    {
        querydir_dotest(MemfsNet, L"\\\\memfs\\share", 500, 750);
    }
}

static unsigned __stdcall dirnotify_dotest_thread(void *FilePath)
{
    FspDebugLog(__FUNCTION__ ": \"%S\"\n", FilePath);

    HANDLE Handle;
    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return GetLastError();
    CloseHandle(Handle);
    return 0;
}

static void dirnotify_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout, ULONG SleepTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    WCHAR FilePath[MAX_PATH];
    HANDLE Handle;
    BOOL Success;
    HANDLE Thread;
    DWORD ExitCode;
    DWORD BytesTransferred;
    PFILE_NOTIFY_INFORMATION NotifyInfo;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    NotifyInfo = malloc(4096);
    ASSERT(0 != NotifyInfo);

    Handle = CreateFileW(FilePath,
        FILE_LIST_DIRECTORY, FILE_SHARE_READ, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Thread = (HANDLE)_beginthreadex(0, 0, dirnotify_dotest_thread, FilePath, 0, 0);
    ASSERT(0 != Thread);

    Success = ReadDirectoryChangesW(Handle,
        NotifyInfo, 4096, TRUE, FILE_NOTIFY_CHANGE_FILE_NAME, &BytesTransferred, 0, 0);
    ASSERT(Success);
    ASSERT(0 < BytesTransferred);

    ASSERT(FILE_ACTION_ADDED == NotifyInfo->Action);
    ASSERT(wcslen(L"file0") * sizeof(WCHAR) == NotifyInfo->FileNameLength);
    ASSERT(0 == memcmp(L"file0", NotifyInfo->FileName, NotifyInfo->FileNameLength));

    if (0 == NotifyInfo->NextEntryOffset)
    {
        Success = ReadDirectoryChangesW(Handle,
            NotifyInfo, 4096, TRUE, FILE_NOTIFY_CHANGE_FILE_NAME, &BytesTransferred, 0, 0);
        ASSERT(Success);
        ASSERT(0 < BytesTransferred);
    }
    else
        NotifyInfo = (PVOID)((PUINT8)NotifyInfo + NotifyInfo->NextEntryOffset);

    ASSERT(FILE_ACTION_REMOVED == NotifyInfo->Action);
    ASSERT(wcslen(L"file0") * sizeof(WCHAR) == NotifyInfo->FileNameLength);
    ASSERT(0 == memcmp(L"file0", NotifyInfo->FileName, NotifyInfo->FileNameLength));

    ASSERT(0 == NotifyInfo->NextEntryOffset);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);
    ASSERT(0 == ExitCode);

    Success = CloseHandle(Handle);
    ASSERT(Success);

    free(NotifyInfo);

    memfs_stop(memfs);
}

void dirnotify_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        dirnotify_dotest(-1, DirBuf, 0, 0);
    }
    if (WinFspDiskTests)
    {
        dirnotify_dotest(MemfsDisk, 0, 0, 0);
        dirnotify_dotest(MemfsDisk, 0, 1000, 0);
    }
    if (WinFspNetTests)
    {
        dirnotify_dotest(MemfsNet, L"\\\\memfs\\share", 0, 0);
        dirnotify_dotest(MemfsNet, L"\\\\memfs\\share", 1000, 0);
    }
}

void dirctl_tests(void)
{
    TEST(querydir_test);
    TEST(querydir_expire_cache_test);
    TEST(dirnotify_test);
}
