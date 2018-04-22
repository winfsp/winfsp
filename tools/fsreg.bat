@echo off

setlocal
setlocal EnableDelayedExpansion

set RegKey=HKLM\Software\WinFsp\Services

if not X%1==X-u (
	set unreg=0

	if not X%1==X set fsname=%1
	if not X%2==X set fsexec="%~f2"
	if not X%3==X set fscmdl=%3
	if not X%4==X set fssecu=%4

	if X!fscmdl!==X goto usage
	if not exist !fsexec! goto notfound

	reg add !RegKey!\!fsname! /v Executable /t REG_SZ /d !fsexec! /f /reg:32
	reg add !RegKey!\!fsname! /v CommandLine /t REG_SZ /d !fscmdl! /f /reg:32
	reg add !RegKey!\!fsname! /v JobControl /t REG_DWORD /d 1 /f /reg:32
	if not X!fssecu!==X reg add !RegKey!\!fsname! /v Security /t REG_SZ /d !fssecu! /f /reg:32
) else (
	set unreg=1

	if not X%2==X set fsname=%2

	if X!fsname!==X goto usage

	reg delete !RegKey!\!fsname! /f /reg:32
)

exit /b 0

:notfound
echo executable !fsexec! not found >&2
exit /b 2

:usage
echo usage: fsreg NAME EXECUTABLE COMMANDLINE [SECURITY] >&2
echo usage: fsreg -u NAME >&2
exit /b 2
