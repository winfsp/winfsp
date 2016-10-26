#include <windows.h>

void *memfs_start_ex(ULONG Flags, ULONG FileInfoTimeout);
void *memfs_start(ULONG Flags);
void memfs_stop(void *data);
PWSTR memfs_volumename(void *data);

int mywcscmp(PWSTR a, int alen, PWSTR b, int blen);

#define CreateFileW HookCreateFileW
#define CloseHandle HookCloseHandle
#define DeleteFileW HookDeleteFileW
HANDLE HookCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);
BOOL HookCloseHandle(
    HANDLE hObject);
BOOL HookDeleteFileW(
    LPCWSTR lpFileName);

HANDLE ResilientCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);
BOOL ResilientCloseHandle(
    HANDLE hObject);
BOOL ResilientDeleteFileW(
    LPCWSTR lpFileName);

typedef struct
{
    BOOLEAN Disposition;
} MY_FILE_DISPOSITION_INFO;

extern int NtfsTests;
extern int WinFspDiskTests;
extern int WinFspNetTests;

extern BOOLEAN OptResilient;
extern BOOLEAN OptCaseInsensitive;
extern BOOLEAN OptCaseRandomize;
extern WCHAR OptMountPointBuf[], *OptMountPoint;
extern WCHAR OptShareNameBuf[], *OptShareName, *OptShareTarget;
    extern WCHAR OptShareComputer[];
    extern ULONG OptSharePrefixLength;
extern HANDLE OptNoTraverseToken;
    extern LUID OptNoTraverseLuid;

extern int memfs_running;
