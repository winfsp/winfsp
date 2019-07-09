/*
 * Compile:
 *     - cl -I"%ProgramFiles(x86)%\WinFsp\inc" "%ProgramFiles(x86)%\WinFsp\lib\winfsp-x64.lib" echo.c
 *
 * Register:
 *     - echo.reg (fix Executable path first)
 *
 * Run:
 *     - launchctl-x64 start echo 1 \foo\bar\baz
 *
 * Expect:
 *     - "\foo\bar\baz" "bar:baz" "DOMAIN\\USERNAME"
 */

#include <winfsp/winfsp.h>

int wmain(int argc, wchar_t *argv[])
{
    WCHAR buf[512], *bufp;
    int len;

    bufp = buf;
    for (int i = 0; argc > i; i++)
    {
        len = lstrlenW(argv[i]);
        memcpy(bufp, argv[i], len * sizeof(WCHAR));
        bufp += len;
        *bufp++ = '\n';
    }
    *bufp = '\0';

    FspServiceLog(EVENTLOG_INFORMATION_TYPE, L"%s", buf);

    return 0;
}
