#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <strsafe.h>
#include <time.h>
#include "memfs.h"

void *memfs_start_ex(ULONG Flags, ULONG FileInfoTimeout);
void *memfs_start(ULONG Flags);
void memfs_stop(void *data);
PWSTR memfs_volumename(void *data);

extern int NtfsTests;
extern int WinFspDiskTests;
extern int WinFspNetTests;

void rdwr_noncached_dotest(ULONG Flags, PWSTR VolPrefix, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    DWORD SectorsPerCluster;
    DWORD BytesPerSector;
    DWORD FreeClusters;
    DWORD TotalClusters;
    PVOID Buffer[2];
    DWORD BytesTransferred;
    DWORD FilePointer;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        VolPrefix ? L"" : L"\\\\?\\GLOBALROOT", VolPrefix ? VolPrefix : memfs_volumename(memfs));

    Success = GetDiskFreeSpaceW(FilePath, &SectorsPerCluster, &BytesPerSector, &FreeClusters, &TotalClusters);
    ASSERT(Success);

    Buffer[0] = _aligned_malloc(BytesPerSector, BytesPerSector);
    Buffer[1] = _aligned_malloc(BytesPerSector, BytesPerSector);
    ASSERT(0 != Buffer[0] && 0 != Buffer[1]);

    srand((unsigned)time(0));
    for (PUINT8 Bgn = Buffer[0], End = Bgn + BytesPerSector; End > Bgn; Bgn++)
        *Bgn = rand();

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    Success = WriteFile(Handle, Buffer[0], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    FilePointer = SetFilePointer(Handle, 0, 0, FILE_CURRENT);
    ASSERT(BytesTransferred == FilePointer);

    FilePointer = SetFilePointer(Handle, 2 * BytesPerSector, 0, FILE_BEGIN);
    ASSERT(2 * BytesPerSector == FilePointer);
    Success = WriteFile(Handle, Buffer[0], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    FilePointer = SetFilePointer(Handle, 0, 0, FILE_CURRENT);
    ASSERT(2 * BytesPerSector + BytesTransferred == FilePointer);

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    FilePointer = SetFilePointer(Handle, 0, 0, FILE_CURRENT);
    ASSERT(BytesTransferred == FilePointer);

    FilePointer = SetFilePointer(Handle, 2 * BytesPerSector, 0, FILE_BEGIN);
    ASSERT(2 * BytesPerSector == FilePointer);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    FilePointer = SetFilePointer(Handle, 0, 0, FILE_CURRENT);
    ASSERT(2 * BytesPerSector + BytesTransferred == FilePointer);

    FilePointer = SetFilePointer(Handle, 2 * BytesPerSector, 0, FILE_BEGIN);
    ASSERT(2 * BytesPerSector == FilePointer);
    Success = ReadFile(Handle, Buffer[1], 2 * BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    FilePointer = SetFilePointer(Handle, 0, 0, FILE_CURRENT);
    ASSERT(2 * BytesPerSector + BytesTransferred == FilePointer);

    FilePointer = SetFilePointer(Handle, 3 * BytesPerSector, 0, FILE_BEGIN);
    ASSERT(3 * BytesPerSector == FilePointer);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(0 == BytesTransferred);
    FilePointer = SetFilePointer(Handle, 0, 0, FILE_CURRENT);
    ASSERT(3 * BytesPerSector + BytesTransferred == FilePointer);

    Success = CloseHandle(Handle);
    ASSERT(Success);

    free(Buffer[0]);
    free(Buffer[1]);

    memfs_stop(memfs);
}

void rdwr_noncached_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        rdwr_noncached_dotest(-1, L"C:", DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        rdwr_noncached_dotest(MemfsDisk, 0, 0, 1000);
        rdwr_noncached_dotest(MemfsDisk, 0, 0, INFINITE);
    }
    if (WinFspNetTests)
    {
        rdwr_noncached_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000);
        rdwr_noncached_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE);
    }
}

void rdwr_tests(void)
{
    TEST(rdwr_noncached_test);
}
