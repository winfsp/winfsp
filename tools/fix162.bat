@echo off

REM This script is used to fix GitHub issue #162:
REM     https://github.com/billziss-gh/winfsp/issues/162
REM
REM It works as follows:
REM
REM     - Creates a junction called `amd64` inside the `WinFsp\bin` directory
REM that points back to the directory.
REM
REM - Updates the registry `ProviderPath` of the WinFsp Network Provider to
REM point to `...\bin\%PROCESSOR_ARCHITECTURE%\winfsp-x64.dll`.
REM
REM It requires Administrator privileges in order to run.

setlocal
setlocal EnableDelayedExpansion

set RegKey="HKLM\SOFTWARE\WinFsp"
set RegVal="InstallDir"
reg query !RegKey! /v !RegVal! /reg:32 >nul 2>&1
if !ERRORLEVEL! equ 0 (
	for /f "tokens=2,*" %%i in ('reg query !RegKey! /v !RegVal! /reg:32 ^| findstr !RegVal!') do (
		set InstallDir=%%j
	)
)
if not exist "!InstallDir!" (echo cannot find WinFsp installation >&2 & goto fail)

set RegKey="HKLM\SYSTEM\CurrentControlSet\Services\WinFsp.Np\NetworkProvider"
set RegVal="ProviderPath"

if not X%1==X-u (
	echo fix #162

	if not exist "!InstallDir!bin\amd64" mklink /j "!InstallDir!bin\amd64" "!InstallDir!bin"

	reg add !RegKey! /v !RegVal! /t REG_EXPAND_SZ /d "!InstallDir!bin\%%PROCESSOR_ARCHITECTURE%%\winfsp-x64.dll" /f

) else (
	echo undo fix #162

	if exist "!InstallDir!bin\amd64" rmdir "!InstallDir!bin\amd64"

	reg add !RegKey! /v !RegVal! /t REG_SZ /d "!InstallDir!bin\winfsp-x64.dll" /f

)

exit /b 0

:fail
exit /b 1
