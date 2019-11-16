@echo off

setlocal
setlocal EnableDelayedExpansion

if not X%1==X set Config=%1
if not X%2==X set Arch=%2
if not X%3==X set Sample=%3
if not X%4==X set ProjDir=%~4

if X!ProjDir!==X (echo usage: build-sample Config Arch Sample ProjDir >&2 & goto fail)

call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x64

if X!FSP_SAMPLE_DIR!==X (
	set RegKey="HKLM\SOFTWARE\WinFsp"
	set RegVal="InstallDir"
	reg query !RegKey! /v !RegVal! /reg:32 >nul 2>&1
	if !ERRORLEVEL! equ 0 (
	    for /f "tokens=2,*" %%i in ('reg query !RegKey! /v !RegVal! /reg:32 ^| findstr !RegVal!') do (
	        set InstallDir=%%j
	    )
	)
	if not exist "!InstallDir!" (echo cannot find WinFsp installation >&2 & goto fail)
	if not exist "!InstallDir!samples\!Sample!" (echo cannot find WinFsp sample !Sample! >&2 & goto fail)

	set SampleDir=!InstallDir!samples
) else (
	set SampleDir=!FSP_SAMPLE_DIR!
)

if exist "!ProjDir!" rmdir /s/q "!ProjDir!"
mkdir "!ProjDir!"
xcopy /s/e/q/y "!SampleDir!\!Sample!" "!ProjDir!"

devenv "!ProjDir!\!Sample!.sln" /build "!Config!|!Arch!"
if !ERRORLEVEL! neq 0 goto :fail

exit /b 0

:fail
exit /b 1
