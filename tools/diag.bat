@echo off

setlocal
setlocal EnableDelayedExpansion

REM Determine the SxS (side-by-side) identifier.
set SxsDir=
set RegKey="HKLM\SOFTWARE\WinFsp"
set RegVal="SxsDir"
reg query !RegKey! /v !RegVal! /reg:32 >nul 2>&1
if !ERRORLEVEL! equ 0 (
    for /f "tokens=2,*" %%i in ('reg query !RegKey! /v !RegVal! /reg:32 ^| findstr !RegVal!') do (
        set SxsDir=%%j
    )
)
set SxsSuffix=
if defined SxsDir (
    set SxsSuffix=!SxsDir:*SxS\sxs.=!
    if !SxsSuffix:~-1!==\ set SxsSuffix=!SxsSuffix:~0,-1!
    set SxsSuffix=+!SxsSuffix!
)

echo WINFSP FSD
sc query WinFsp!SxsSuffix!
sc qc WinFsp!SxsSuffix!
sc sdshow WinFsp!SxsSuffix!
echo.
echo.

echo WINFSP DLL
reg query HKLM\SYSTEM\CurrentControlSet\Control\NetworkProvider\Order
reg query HKLM\SYSTEM\CurrentControlSet\Services\WinFsp.Np\NetworkProvider
reg query HKLM\SYSTEM\CurrentControlSet\Services\EventLog\Application\WinFsp
echo.

echo WINFSP LAUNCHER
sc query WinFsp.Launcher
sc qc WinFsp.Launcher
sc sdshow WinFsp.Launcher
echo.
echo.

echo WINFSP REGISTRY
reg query HKLM\SOFTWARE\WinFsp /s /reg:32
echo.

echo FILE SYSTEM FILTERS (REQUIRES ADMINISTRATOR)
fltmc filters
echo.
echo.

echo OS INFORMATION
systeminfo
echo.
