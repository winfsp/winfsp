@echo off

setlocal

set Configuration=Release
if not X%1==X set Configuration=%1

cd %~dp0..
set ProjRoot=%cd%

cd build\VStudio
if not exist build\%Configuration% echo === No tests found >&2 & goto fail
cd build\%Configuration%

launchctl-x64 start memfs64 test \memfs64\test M: >nul
launchctl-x64 start memfs32 test \memfs32\test N: >nul
rem Cannot use timeout under cygwin/mintty: "Input redirection is not supported"
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
cd M: >nul 2>nul || (echo === Unable to find drive M: >&2 & goto fail)
cd N: >nul 2>nul || (echo === Unable to find drive N: >&2 & goto fail)

set testpass=0
set testfail=0
for %%f in (winfsp-tests-x64 winfsp-tests-x86 :fsx-memfs-x64 :fsx-memfs-x86 :winfstest-memfs-x64 :winfstest-memfs-x86) do (
    echo === Running %%f

    if defined APPVEYOR (
        appveyor AddTest "%%f" -FileName None -Framework None -Outcome Running
    )

    pushd %cd%
    call %%f
    popd

    if errorlevel 1 (
        set /a testfail=testfail+1

        echo === Failed %%f

        if defined APPVEYOR (
            appveyor UpdateTest "%%f" -FileName None -Framework None -Outcome Failed -Duration 0
        )
    ) else (
        set /a testpass=testpass+1

        echo === Passed %%f

        if defined APPVEYOR (
            appveyor UpdateTest "%%f" -FileName None -Framework None -Outcome Passed -Duration 0
        )
    )
    echo:
)

launchctl-x64 stop memfs64 test >nul
launchctl-x64 stop memfs32 test >nul

set /a total=testpass+testfail
echo === Total: %testpass%/%total%
if not %testfail%==0 goto fail

exit /b 0

:fail
exit /b 1

:fsx-memfs-x64
M:
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -N 1000 test xxxxxx
if errorlevel 1 goto fail
exit /b 0

:fsx-memfs-x86
N:
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -N 1000 test xxxxxx
if errorlevel 1 goto fail
exit /b 0

:winfstest-memfs-x64
M:
call "%ProjRoot%\ext\test\winfstest\run-winfstest.bat" base
if errorlevel 1 goto fail
exit /b 0

:winfstest-memfs-x86
N:
call "%ProjRoot%\ext\test\winfstest\run-winfstest.bat" base
if errorlevel 1 goto fail
exit /b 0
