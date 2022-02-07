@echo off

setlocal
setlocal EnableDelayedExpansion

if NOT [%1]==[] set Arch=%1
if NOT [%2]==[] set Mount=%2
if NOT [%3]==[] set Image=%3
if [!Mount!]==[] (echo usage: winfsp-winpe Arch Mount [Image] >&2 & goto fail)

set SkipDism=0
if [!Image!]==[] set SkipDism=1

set RegKey="HKLM\SOFTWARE\WinFsp"
set RegVal="InstallDir"
reg query !RegKey! /v !RegVal! /reg:32 >nul 2>&1
if !ERRORLEVEL! equ 0 (
	for /f "tokens=2,*" %%i in ('reg query !RegKey! /v !RegVal! /reg:32 ^| findstr !RegVal!') do (
		set InstallDir=%%j
	)
)
if not exist "!InstallDir!" (echo cannot find WinFsp installation >&2 & goto fail)

if not exist !Mount! (echo cannot find mount directory !Mount! >&2 & goto fail)
if !SkipDism! equ 0 (
    if not exist !Image! (echo cannot find image file !Image! >&2 & goto fail)
)

if !SkipDism! equ 0 (
    echo Dism /Mount-Image /ImageFile:!Image! /Index:1 /MountDir:!Mount!
    Dism /Mount-Image /ImageFile:!Image! /Index:1 /MountDir:!Mount!
    if !ERRORLEVEL! neq 0 goto fail
)

echo copy "!InstallDir!"\bin\winfsp-!Arch!.dll !Mount!\Windows\System32
copy "!InstallDir!"\bin\winfsp-!Arch!.dll !Mount!\Windows\System32 >nul
if !ERRORLEVEL! neq 0 goto fail

echo copy "!InstallDir!"\bin\winfsp-!Arch!.sys !Mount!\Windows\System32
copy "!InstallDir!"\bin\winfsp-!Arch!.sys !Mount!\Windows\System32 >nul
if !ERRORLEVEL! neq 0 goto fail

rem echo copy "!InstallDir!"\bin\memfs-!Arch!.exe !Mount!\Windows\System32
rem copy "!InstallDir!"\bin\memfs-!Arch!.exe !Mount!\Windows\System32 >nul
rem if !ERRORLEVEL! neq 0 goto fail

echo Creating !Mount!\Windows\System32\startnet.cmd
echo regsvr32 /s winfsp-x64.dll > !Mount!\Windows\System32\startnet.cmd
echo wpeinit >> !Mount!\Windows\System32\startnet.cmd

if !SkipDism! equ 0 (
    echo Dism /Unmount-Image /MountDir:!Mount! /Commit
    Dism /Unmount-Image /MountDir:!Mount! /Commit
    if !ERRORLEVEL! neq 0 goto fail
)

exit /b 0

:fail
exit /b 1
