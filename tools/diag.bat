@echo off

setlocal

echo WINFSP AND SSHFS-WIN INSTALLATION DIRECTORIES
reg query HKLM\SOFTWARE\WinFsp /reg:32
reg query HKLM\SOFTWARE\SSHFS-Win /reg:32
echo.

echo WINFSP NETWORK PROVIDER DLL AND SSHFS REGISTRATIONS
reg query HKLM\SYSTEM\CurrentControlSet\Control\NetworkProvider\Order
reg query HKLM\SYSTEM\CurrentControlSet\Services\WinFsp.Np\NetworkProvider
reg query HKLM\SYSTEM\CurrentControlSet\Services\EventLog\Application\WinFsp
reg query HKLM\SYSTEM\CurrentControlSet\Services\WinFsp.Launcher\Services\sshfs
echo.

echo WINFSP FSD CONFIGURATION AND STATUS
sc query WinFsp
sc qc WinFsp
sc sdshow WinFsp
echo.

echo WINFSP.LAUNCHER CONFIGURATION AND STATUS
sc query WinFsp.Launcher
sc qc WinFsp.Launcher
sc sdshow WinFsp.Launcher
echo.

echo OS INFORMATION
systeminfo
echo.
