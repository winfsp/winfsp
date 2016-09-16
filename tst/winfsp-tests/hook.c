#include <winfsp/winfsp.h>

HANDLE HookCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile)
{
    HANDLE h = CreateFileW(
        lpFileName,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile);

    DWORD LastError = GetLastError();
    FspDebugLog("CreateFileW(\"%S\", %#lx, %#lx, %p, %#lx, %#lx, %p) = %p[%#lx]\n",
        lpFileName,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile,
        h, INVALID_HANDLE_VALUE != h ? 0 : LastError);
    SetLastError(LastError);

    return h;
}
