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

static void rdwr_dotest(ULONG Flags, PWSTR VolPrefix, PWSTR Prefix, ULONG FileInfoTimeout, DWORD CreateFlags)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    SYSTEM_INFO SystemInfo;
    DWORD SectorsPerCluster;
    DWORD BytesPerSector;
    DWORD FreeClusters;
    DWORD TotalClusters;
    PVOID AllocBuffer[2], Buffer[2];
    ULONG AllocBufferSize;
    DWORD BytesTransferred;
    DWORD FilePointer;

    GetSystemInfo(&SystemInfo);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        VolPrefix ? L"" : L"\\\\?\\GLOBALROOT", VolPrefix ? VolPrefix : memfs_volumename(memfs));

    Success = GetDiskFreeSpaceW(FilePath, &SectorsPerCluster, &BytesPerSector, &FreeClusters, &TotalClusters);
    ASSERT(Success);
    AllocBufferSize = 16 * SystemInfo.dwPageSize;

    AllocBuffer[0] = _aligned_malloc(AllocBufferSize, SystemInfo.dwPageSize);
    AllocBuffer[1] = _aligned_malloc(AllocBufferSize, SystemInfo.dwPageSize);
    ASSERT(0 != AllocBuffer[0] && 0 != AllocBuffer[1]);

    srand((unsigned)time(0));
    for (PUINT8 Bgn = AllocBuffer[0], End = Bgn + AllocBufferSize; End > Bgn; Bgn++)
        *Bgn = rand();

    Buffer[0] = (PVOID)((PUINT8)AllocBuffer[0] + BytesPerSector);
    Buffer[1] = (PVOID)((PUINT8)AllocBuffer[1] + BytesPerSector);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | CreateFlags | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    Success = WriteFile(Handle, Buffer[0], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));

    FilePointer = SetFilePointer(Handle, 2 * BytesPerSector, 0, FILE_BEGIN);
    ASSERT(2 * BytesPerSector == FilePointer);
    Success = WriteFile(Handle, Buffer[0], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    FilePointer = SetFilePointer(Handle, 2 * BytesPerSector, 0, FILE_BEGIN);
    ASSERT(2 * BytesPerSector == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    FilePointer = SetFilePointer(Handle, 2 * BytesPerSector, 0, FILE_BEGIN);
    ASSERT(2 * BytesPerSector == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    FilePointer = SetFilePointer(Handle, 3 * BytesPerSector, 0, FILE_BEGIN);
    ASSERT(3 * BytesPerSector == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(0 == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    Success = WriteFile(Handle, Buffer[0], 2 * SystemInfo.dwPageSize, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * SystemInfo.dwPageSize, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Buffer[0] = AllocBuffer[0];
    Buffer[1] = AllocBuffer[0];

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    Success = WriteFile(Handle, Buffer[0], 2 * SystemInfo.dwPageSize, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * SystemInfo.dwPageSize, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    Success = WriteFile(Handle, Buffer[0], 2 * SystemInfo.dwPageSize + BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize + BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * SystemInfo.dwPageSize + BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize + BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Success = CloseHandle(Handle);
    ASSERT(Success);

    _aligned_free(AllocBuffer[0]);
    _aligned_free(AllocBuffer[1]);

    memfs_stop(memfs);
}

static void rdwr_overlapped_dotest(ULONG Flags, PWSTR VolPrefix, PWSTR Prefix, ULONG FileInfoTimeout, DWORD CreateFlags)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    SYSTEM_INFO SystemInfo;
    DWORD SectorsPerCluster;
    DWORD BytesPerSector;
    DWORD FreeClusters;
    DWORD TotalClusters;
    PVOID AllocBuffer[2], Buffer[2];
    ULONG AllocBufferSize;
    DWORD BytesTransferred;
    OVERLAPPED Overlapped;

    GetSystemInfo(&SystemInfo);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        VolPrefix ? L"" : L"\\\\?\\GLOBALROOT", VolPrefix ? VolPrefix : memfs_volumename(memfs));

    Success = GetDiskFreeSpaceW(FilePath, &SectorsPerCluster, &BytesPerSector, &FreeClusters, &TotalClusters);
    ASSERT(Success);
    AllocBufferSize = 16 * SystemInfo.dwPageSize;

    AllocBuffer[0] = _aligned_malloc(AllocBufferSize, SystemInfo.dwPageSize);
    AllocBuffer[1] = _aligned_malloc(AllocBufferSize, SystemInfo.dwPageSize);
    ASSERT(0 != AllocBuffer[0] && 0 != AllocBuffer[1]);

    srand((unsigned)time(0));
    for (PUINT8 Bgn = AllocBuffer[0], End = Bgn + AllocBufferSize; End > Bgn; Bgn++)
        *Bgn = rand();

    Buffer[0] = (PVOID)((PUINT8)AllocBuffer[0] + BytesPerSector);
    Buffer[1] = (PVOID)((PUINT8)AllocBuffer[1] + BytesPerSector);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    memset(&Overlapped, 0, sizeof Overlapped);
    Overlapped.hEvent = CreateEvent(0, TRUE, FALSE, 0);
    ASSERT(0 != Overlapped.hEvent);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | CreateFlags | FILE_FLAG_OVERLAPPED | FILE_FLAG_DELETE_ON_CLOSE,
        0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Overlapped.Offset = 0;
    Success = WriteFile(Handle, Buffer[0], BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);

    Overlapped.Offset = 2 * BytesPerSector;
    Success = WriteFile(Handle, Buffer[0], BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);

    Overlapped.Offset = 0;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Overlapped.Offset = 2 * BytesPerSector;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Overlapped.Offset = 2 * BytesPerSector;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Overlapped.Offset = 3 * BytesPerSector;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError() || ERROR_HANDLE_EOF == GetLastError());
    if (ERROR_HANDLE_EOF != GetLastError())
    {
        Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
        ASSERT(!Success && ERROR_HANDLE_EOF == GetLastError());
    }
    ASSERT(0 == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Overlapped.Offset = 0;
    Success = WriteFile(Handle, Buffer[0], 2 * SystemInfo.dwPageSize, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);

    Overlapped.Offset = 0;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * SystemInfo.dwPageSize, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Buffer[0] = AllocBuffer[0];
    Buffer[1] = AllocBuffer[0];

    Overlapped.Offset = 0;
    Success = WriteFile(Handle, Buffer[0], 2 * SystemInfo.dwPageSize, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);

    Overlapped.Offset = 0;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * SystemInfo.dwPageSize, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Overlapped.Offset = 0;
    Success = WriteFile(Handle, Buffer[0], 2 * SystemInfo.dwPageSize + BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize + BytesPerSector == BytesTransferred);

    Overlapped.Offset = 0;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * SystemInfo.dwPageSize + BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize + BytesPerSector == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Success = CloseHandle(Handle);
    ASSERT(Success);

    Success = CloseHandle(Overlapped.hEvent);
    ASSERT(Success);

    _aligned_free(AllocBuffer[0]);
    _aligned_free(AllocBuffer[1]);

    memfs_stop(memfs);
}

void rdwr_noncached_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        rdwr_dotest(-1, L"C:", DirBuf, 0, FILE_FLAG_NO_BUFFERING);
    }
    if (WinFspDiskTests)
    {
        rdwr_dotest(MemfsDisk, 0, 0, 1000, FILE_FLAG_NO_BUFFERING);
        rdwr_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_NO_BUFFERING);
    }
    if (WinFspNetTests)
    {
        rdwr_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, FILE_FLAG_NO_BUFFERING);
        rdwr_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_NO_BUFFERING);
    }
}

void rdwr_noncached_overlapped_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        rdwr_overlapped_dotest(-1, L"C:", DirBuf, 0, FILE_FLAG_NO_BUFFERING);
    }
    if (WinFspDiskTests)
    {
        rdwr_overlapped_dotest(MemfsDisk, 0, 0, 1000, FILE_FLAG_NO_BUFFERING);
        rdwr_overlapped_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_NO_BUFFERING);
    }
    if (WinFspNetTests)
    {
        rdwr_overlapped_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, FILE_FLAG_NO_BUFFERING);
        rdwr_overlapped_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_NO_BUFFERING);
    }
}

void rdwr_cached_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        rdwr_dotest(-1, L"C:", DirBuf, 0, 0);
    }
    if (WinFspDiskTests)
    {
        rdwr_dotest(MemfsDisk, 0, 0, 1000, 0);
        rdwr_dotest(MemfsDisk, 0, 0, INFINITE, 0);
    }
    if (WinFspNetTests)
    {
        rdwr_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, 0);
        rdwr_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, 0);
    }
}

void rdwr_cached_overlapped_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        rdwr_overlapped_dotest(-1, L"C:", DirBuf, 0, 0);
    }
    if (WinFspDiskTests)
    {
        rdwr_overlapped_dotest(MemfsDisk, 0, 0, 1000, 0);
        rdwr_overlapped_dotest(MemfsDisk, 0, 0, INFINITE, 0);
    }
    if (WinFspNetTests)
    {
        rdwr_overlapped_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, 0);
        rdwr_overlapped_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, 0);
    }
}

void rdwr_tests(void)
{
    TEST(rdwr_noncached_test);
    TEST(rdwr_noncached_overlapped_test);
    TEST(rdwr_cached_test);
    TEST(rdwr_cached_overlapped_test);
}
