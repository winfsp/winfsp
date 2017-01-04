@echo off

setlocal
setlocal EnableDelayedExpansion

set Configuration=Release
if not X%1==X set Configuration=%1

pushd %~dp0..
set ProjRoot=%cd%
if not exist "%ProjRoot%\build\VStudio\build\%Configuration%" echo === No tests found >&2 & goto fail
popd

verifier /query | findstr winfsp >nul 2>nul
if !ERRORLEVEL! equ 0 echo warning: verifier for winfsp is ON >&2

set launchctl="%ProjRoot%\build\VStudio\build\%Configuration%\launchctl-x64.exe"
set fsbench="%ProjRoot%\build\VStudio\build\%Configuration%\fsbench-x64.exe"

if X%2==Xself (
    %launchctl% start memfs64 testdsk "" M: >nul
    rem Cannot use timeout under cygwin/mintty: "Input redirection is not supported"
    waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
    cd M: >nul 2>nul || (echo === Unable to find drive M: >&2 & goto fail)
)

mkdir fsbench
pushd fsbench

set OptFiles=1000 2000 3000 4000 5000
if X%2==Xself set OptFiles=10000
for %%a in (%OptFiles%) do (
    call :csv %%a "%fsbench% --files=%%a file_*"
)

set OptRdwrCc=100 200 300 400 500
if X%2==Xself set OptRdwrCc=1000
for %%a in (%OptRdwrCc%) do (
    call :csv %%a "%fsbench% --rdwr-cc=%%a rdwr_cc_*"
)

set OptRdwrNc=100 200 300 400 500
if X%2==Xself set OptRdwrNc=100
for %%a in (%OptRdwrNc%) do (
    call :csv %%a "%fsbench% --rdwr-nc=%%a rdwr_nc_*"
)

set OptMmap=100 200 300 400 500
if X%2==Xself set OptMmap=1000
for %%a in (%OptMmap%) do (
    call :csv %%a "%fsbench% --mmap=%%a mmap_*"
)

popd
rmdir fsbench

if X%2==Xself (
    %launchctl% stop memfs64 testdsk >nul
    rem Cannot use timeout under cygwin/mintty: "Input redirection is not supported"
    waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
)

exit /b 0

:fail
exit /b 1

:csv
set Iter=%1
for /F "tokens=1,2,3" %%i in ('%2') do (
    if %%j==OK (
        set Name=%%i
        set Name=!Name:.=!
        set Time=%%k
        set Time=!Time:s=!

        echo !Name!,!Iter!,!Time!
    )
)
exit /b 0
