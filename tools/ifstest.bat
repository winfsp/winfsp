@echo off

setlocal
setlocal EnableDelayedExpansion

set Arch=amd64

set RegKey="HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots"
set RegVal="KitsRoot10"
reg query %RegKey% /v %RegVal% >nul 2>&1
if !ERRORLEVEL! equ 0 (
	for /f "tokens=2,*" %%i in ('reg query %RegKey% /v %RegVal% ^| findstr %RegVal%') do (
	    set KitRoot=%%jHardware Lab Kit\
	)
) else (
	set "KitRoot=C:\Program Files (x86)\Windows Kits\8.1\Hardware Certification Kit\"
)

set BaseDir=%KitRoot%Tests\%Arch%
set PATH=%BaseDir%\nttest\commontest\ntlog;%BaseDir%\nttest\basetest\core_file_services\shared_libs\fbslog;%PATH%
"%BaseDir%\nttest\basetest\core_file_services\ifs_test_kit\ifstest.exe" %*
