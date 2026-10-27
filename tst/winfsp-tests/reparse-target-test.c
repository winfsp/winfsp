/* Real-driver regression for directory-mounted absolute target classification.
 * Distributed under the same license as reparse-test.c. */
#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <strsafe.h>
#include <stdio.h>
#include "memfs.h"
#define WINFSP_TESTS_NO_HOOKS
#include "winfsp-tests.h"

static volatile LONG ObservedTarget;

static NTSTATUS ObserveReparseTarget(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PWSTR FileName, PVOID Buffer, SIZE_T Size)
{
    UNREFERENCED_PARAMETER(FileSystem);
    UNREFERENCED_PARAMETER(FileContext);
    UNREFERENCED_PARAMETER(FileName);
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Size);
    InterlockedExchange(&ObservedTarget,
        FspFileSystemGetOperationContext()->Request->Req.FileSystemControl.TargetOnFileSystem);
    /* Observe the real kernel decision without installing a reparse point or
     * changing any target data. FUSE uses this same request field. */
    return STATUS_ACCESS_DENIED;
}

static void CheckReparseTarget(HANDLE Handle, PWSTR Target, LONG Expected)
{
    union { REPARSE_DATA_BUFFER D; UINT8 B[MAXIMUM_REPARSE_DATA_BUFFER_SIZE]; } Data = { 0 };
    USHORT Length = (USHORT)(wcslen(Target) * sizeof(WCHAR));
    DWORD Bytes;
    Data.D.ReparseTag = IO_REPARSE_TAG_SYMLINK;
    Data.D.ReparseDataLength = 12 + 2 * (Length + sizeof(WCHAR));
    Data.D.SymbolicLinkReparseBuffer.SubstituteNameLength = Length;
    Data.D.SymbolicLinkReparseBuffer.PrintNameOffset = Length + sizeof(WCHAR);
    Data.D.SymbolicLinkReparseBuffer.PrintNameLength = Length;
    memcpy(Data.D.SymbolicLinkReparseBuffer.PathBuffer, Target, Length);
    memcpy((PUINT8)Data.D.SymbolicLinkReparseBuffer.PathBuffer + Length + sizeof(WCHAR), Target, Length);
    InterlockedExchange(&ObservedTarget, -1);
    ASSERT(!DeviceIoControl(Handle, FSCTL_SET_REPARSE_POINT, &Data,
        REPARSE_DATA_BUFFER_HEADER_SIZE + Data.D.ReparseDataLength, 0, 0, &Bytes, 0));
    ASSERT(ERROR_ACCESS_DENIED == GetLastError());
    LONG Observed = InterlockedCompareExchange(&ObservedTarget, 0, 0);
    if (Expected != Observed)
        fprintf(stderr, "REPARSE_TARGET target=%ls expected=%ld observed=%ld\n",
            Target, Expected, Observed);
    ASSERT(Expected == Observed);
}

void reparse_mount_target_test(void)
{
    MEMFS *Memfs;
    FSP_FILE_SYSTEM_INTERFACE Interface;
    FSP_FILE_SYSTEM *FileSystem;
    WCHAR Temp[MAX_PATH], Mount[MAX_PATH], Link[MAX_PATH], Target[2 * MAX_PATH];
    WCHAR Alias[MAX_PATH], Child[MAX_PATH], Collision[MAX_PATH];
    HANDLE Handle;
    LONG Boundary;
    DWORD TempLength = GetTempPathW(MAX_PATH, Temp);
    ASSERT(0 < TempLength && MAX_PATH > TempLength);
    ASSERT(GetTempFileNameW(Temp, L"fsp", 0, Mount));
    ASSERT(DeleteFileW(Mount));
    ASSERT(NT_SUCCESS(MemfsCreate(MemfsDisk | MemfsCaseInsensitive, 0, 128,
        1024 * 1024, 0, 0, &Memfs)));
    FileSystem = MemfsFileSystem(Memfs);
    Interface = *FileSystem->Interface;
    Interface.SetReparsePoint = ObserveReparseTarget;
    FileSystem->Interface = &Interface;
    ASSERT(NT_SUCCESS(FspFileSystemSetMountPoint(FileSystem, Mount)));
    ASSERT(NT_SUCCESS(MemfsStart(Memfs)));
    StringCbPrintfW(Link, sizeof Link, L"%s\\probe", Mount);
    Handle = CreateFileW(Link, FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, CREATE_NEW, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Boundary = (LONG)((wcslen(L"\\??\\") + wcslen(Mount)) * sizeof(WCHAR));

    /* Every descendant is dangling. The classifier must stop at the proven
     * root, not require a leaf or strip away a nonexistent parent suffix. */
    StringCbPrintfW(Target, sizeof Target, L"\\??\\%s\\parent\\child\\missing", Mount);
    CheckReparseTarget(Handle, Target, Boundary);
    CharLowerBuffW(Target, (DWORD)wcslen(Target));
    CheckReparseTarget(Handle, Target, Boundary);
    StringCbPrintfW(Target, sizeof Target, L"%s\\missing", FileSystem->VolumeName);
    CheckReparseTarget(Handle, Target, (LONG)(wcslen(FileSystem->VolumeName) * sizeof(WCHAR)));

    /* Raw FSCTL input bypasses Win32 normalization: reject traversal even
     * when its spelling starts with the genuine mounted root. */
    StringCbPrintfW(Target, sizeof Target, L"\\??\\%s\\..\\outside", Mount);
    CheckReparseTarget(Handle, Target, 0);
    StringCbPrintfW(Target, sizeof Target, L"\\??\\%s\\.\\missing", Mount);
    CheckReparseTarget(Handle, Target, 0);
    StringCbPrintfW(Target, sizeof Target, L"\\??\\%s", Mount);
    CheckReparseTarget(Handle, Target, 0); /* exact-root limitation is explicit */
    StringCbPrintfW(Collision, sizeof Collision, L"%s-other", Mount);
    ASSERT(CreateDirectoryW(Collision, 0));
    StringCbPrintfW(Target, sizeof Target, L"\\??\\%s\\missing", Collision);
    CheckReparseTarget(Handle, Target, 0);
    ASSERT(RemoveDirectoryW(Collision));

    StringCbPrintfW(Alias, sizeof Alias, L"%s-alias", Mount);
    ASSERT(CreateSymbolicLinkW(Alias, Mount, SYMBOLIC_LINK_FLAG_DIRECTORY));
    StringCbPrintfW(Target, sizeof Target, L"\\??\\%s\\missing", Alias);
    CheckReparseTarget(Handle, Target, (LONG)((4 + wcslen(Alias)) * sizeof(WCHAR)));
    ASSERT(RemoveDirectoryW(Alias));
    StringCbPrintfW(Child, sizeof Child, L"%s\\child", Mount);
    ASSERT(CreateDirectoryW(Child, 0));
    ASSERT(CreateSymbolicLinkW(Alias, Child, SYMBOLIC_LINK_FLAG_DIRECTORY));
    CheckReparseTarget(Handle, Target, 0); /* alias into a subdirectory is not a root */
    ASSERT(RemoveDirectoryW(Alias));
    ASSERT(RemoveDirectoryW(Child));
    ASSERT(CloseHandle(Handle));
    ASSERT(0 == (GetFileAttributesW(Link) & FILE_ATTRIBUTE_REPARSE_POINT));
    ASSERT(DeleteFileW(Link));
    MemfsStop(Memfs);
    MemfsDelete(Memfs);
}

void reparse_target_tests(void)
{
    if (WinFspDiskTests && !OptExternal)
        TEST(reparse_mount_target_test);
}
