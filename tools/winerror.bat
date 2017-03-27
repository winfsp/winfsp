@echo off

setlocal

if X%1==X (echo usage: %~n0 VALUE >&2 & exit /b 1)

set RegKey="HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots"
set RegVal="KitsRoot10"
reg query %RegKey% /v %RegVal% >nul 2>&1 || (echo Cannot find Windows Kit >&2 & exit /b 1)
for /f "tokens=2,*" %%i in ('reg query %RegKey% /v %RegVal% ^| findstr %RegVal%') do (
    set KitRoot=%%j
)

findstr /R /I "\<0*[Xx]*%1[Ll]*\>" "%KitRoot%Include\10.0.10586.0\shared\%~n0.h"
