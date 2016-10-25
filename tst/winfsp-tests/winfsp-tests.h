#include <windows.h>

void *memfs_start_ex(ULONG Flags, ULONG FileInfoTimeout);
void *memfs_start(ULONG Flags);
void memfs_stop(void *data);
PWSTR memfs_volumename(void *data);

int mywcscmp(PWSTR a, int alen, PWSTR b, int blen);

#define CreateFileW HookCreateFileW
HANDLE HookCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);

extern int NtfsTests;
extern int WinFspDiskTests;
extern int WinFspNetTests;

extern BOOLEAN OptCaseInsensitive;
extern BOOLEAN OptCaseRandomize;
extern WCHAR OptMountPointBuf[], *OptMountPoint;
extern WCHAR OptShareNameBuf[], *OptShareName, *OptShareTarget;
    extern WCHAR OptShareComputer[];
    extern ULONG OptSharePrefixLength;
extern HANDLE OptNoTraverseToken;
    extern LUID OptNoTraverseLuid;

extern int memfs_running;
