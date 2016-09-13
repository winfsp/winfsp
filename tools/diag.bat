@echo off

setlocal

echo WINFSP INSTALLATION DIRECTORY
reg query HKLM\SOFTWARE\WinFsp /reg:32
echo.

echo WINFSP DLL REGISTRATIONS
reg query HKLM\SYSTEM\CurrentControlSet\Control\NetworkProvider\Order
reg query HKLM\SYSTEM\CurrentControlSet\Services\WinFsp.Np\NetworkProvider
reg query HKLM\SYSTEM\CurrentControlSet\Services\EventLog\Application\WinFsp
echo.

echo WINFSP LAUNCHER REGISTRATIONS
reg query HKLM\SYSTEM\CurrentControlSet\Services\WinFsp.Launcher\Services /s
echo.

echo WINFSP FSD CONFIGURATION AND STATUS
sc query WinFsp
sc qc WinFsp
sc sdshow WinFsp
echo.

echo WINFSP LAUNCHER SERVICE CONFIGURATION AND STATUS
sc query WinFsp.Launcher
sc qc WinFsp.Launcher
sc sdshow WinFsp.Launcher
echo.

echo OS INFORMATION
systeminfo
echo.
