@echo off

setlocal
setlocal EnableDelayedExpansion

set Arch=amd64

set RegKey="HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots"
set RegVal="KitsRoot10"
reg query %RegKey% /v %RegVal% >nul 2>&1 || (echo Cannot find Windows Kit >&2 & exit /b 1)
for /f "tokens=2,*" %%i in ('reg query %RegKey% /v %RegVal% ^| findstr %RegVal%') do (
    set KitRoot=%%j
)

set BaseDir=%KitRoot%Hardware Lab Kit\Tests\%Arch%
set PATH=%BaseDir%\nttest\commontest\ntlog;%BaseDir%\nttest\basetest\core_file_services\shared_libs\fbslog;%PATH%
"%KitRoot%Hardware Lab Kit\Tests\%Arch%\nttest\basetest\core_file_services\ifs_test_kit\ifstest.exe" %*
