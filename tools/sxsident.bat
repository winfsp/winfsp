@echo off

setlocal
setlocal EnableDelayedExpansion

set SxsDir=
set RegKey="HKLM\SOFTWARE\WinFsp"
set RegVal="SxsDir"
reg query !RegKey! /v !RegVal! /reg:32 >nul 2>&1
if !ERRORLEVEL! equ 0 (
    for /f "tokens=2,*" %%i in ('reg query !RegKey! /v !RegVal! /reg:32 ^| findstr !RegVal!') do (
        set SxsDir=%%j
    )
)
if defined SxsDir (
    set SxsDir=!SxsDir:*SxS\sxs.=!
    if !SxsDir:~-1!==\ set SxsDir=!SxsDir:~0,-1!
    echo !SxsDir!
)

exit /b 0

:fail
exit /b 1
