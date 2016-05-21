@echo off

setlocal

set Configuration=Release
if not X%1==X set Configuration=%1

cd %~dp0..\build\VStudio
if not exist build\%Configuration% (
    echo === No tests found 1>&2
    goto fail
)
cd build\%Configuration%

set testpass=0
set testfail=0
for %%f in (winfsp-tests-x64 winfsp-tests-x86 :winfstest) do (
    echo === Running %%f

    if defined APPVEYOR (
        appveyor AddTest "%%f" -FileName None -Framework None -Outcome Running
    )

    call %%f

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

set /a total=testpass+testfail
echo === Total: %testpass%/%total%
if not %testfail%==0 goto fail

exit /b 0

:fail
exit /b 1

:winfstest
exit /b 0
