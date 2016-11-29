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

set fsbench="%ProjRoot%\build\VStudio\build\%Configuration%\fsbench-x64.exe"
mkdir fsbench
pushd fsbench

for %%a in (1000 2000 3000 4000 5000) do (
    call :csv %%a "%fsbench% --files=%%a file_*"
)

for %%a in (100 200 300 400 500) do (
    call :csv %%a "%fsbench% --rdwr-cc=%%a rdwr_cc_*"
)

for %%a in (100 200 300 400 500) do (
    call :csv %%a "%fsbench% --rdwr-nc=%%a rdwr_nc_*"
)

for %%a in (100 200 300 400 500) do (
    call :csv %%a "%fsbench% --mmap=%%a mmap_*"
)

popd
rmdir fsbench

exit /b 0

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
