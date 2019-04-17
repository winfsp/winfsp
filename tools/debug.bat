@echo off

setlocal

set DebugWorkspace=winfsp
set DebugPort=50000
set DebugKey=1.1.1.1

set RegKey="HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots"
set RegVal="KitsRoot10"
reg query %RegKey% /v %RegVal% >nul 2>&1 || (echo Cannot find Windows Kit >&2 & exit /b 1)
for /f "tokens=2,*" %%i in ('reg query %RegKey% /v %RegVal% ^| findstr %RegVal%') do (
    set KitRoot="%%j"
)
start "winfsp" %KitRoot%\Debuggers\x64\windbg -W %DebugWorkspace% -k net:port=%DebugPort%,key=%DebugKey%
