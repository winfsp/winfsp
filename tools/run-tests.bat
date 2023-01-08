@echo off

setlocal
setlocal EnableDelayedExpansion

set Configuration=Release
if not X%1==X set Configuration=%1

cd %~dp0..
set ProjRoot=%cd%

cd build\VStudio
if not exist build\%Configuration% echo === No tests found >&2 & goto fail
cd build\%Configuration%

launchctl-x64 start memfs64 testdsk ""            M: >nul
launchctl-x64 start memfs64 testnet \memfs64\test N: >nul
launchctl-x64 start memfs32 testdsk ""            O: >nul
launchctl-x64 start memfs32 testnet \memfs32\test P: >nul
launchctl-x64 start memfs-dotnet testdsk ""                 Q: >nul
launchctl-x64 start memfs-dotnet testnet \memfs-dotnet\test R: >nul
rem Cannot use timeout under cygwin/mintty: "Input redirection is not supported"
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 5 2>nul
cd M: >nul 2>nul || (echo === Unable to find drive M: >&2 & goto fail)
cd N: >nul 2>nul || (echo === Unable to find drive N: >&2 & goto fail)
cd O: >nul 2>nul || (echo === Unable to find drive O: >&2 & goto fail)
cd P: >nul 2>nul || (echo === Unable to find drive P: >&2 & goto fail)
cd Q: >nul 2>nul || (echo === Unable to find drive Q: >&2 & goto fail)
cd R: >nul 2>nul || (echo === Unable to find drive R: >&2 & goto fail)

set dfl_tests=^
    winfsp-tests-x64 ^
    winfsp-tests-x64-case-randomize ^
    winfsp-tests-x64-flushpurge ^
    winfsp-tests-x64-legacy-unlink-rename ^
    winfsp-tests-x64-mountpoint-drive ^
    winfsp-tests-x64-mountpoint-dir ^
    winfsp-tests-x64-mountpoint-dir-case-sensitive ^
    winfsp-tests-x64-mountmgr-drive ^
    winfsp-tests-x64-mountmgr-dir ^
    winfsp-tests-x64-mountmgrfsd-drive ^
    winfsp-tests-x64-mountmgrfsd-dir ^
    winfsp-tests-x64-no-traverse ^
    winfsp-tests-x64-oplock ^
    winfsp-tests-x64-notify ^
    winfsp-tests-x64-external ^
    winfsp-tests-x64-external-share ^
    memfs-x64-disk-fsx ^
    memfs-x64-net-fsx ^
    RETIRED-memfs-x64-disk-standby ^
    RETIRED-memfs-x64-net-standby ^
    memfs-x64-net-use ^
    memfs-x64-disk-winfstest ^
    memfs-x64-net-winfstest ^
    fscrash-x64 ^
    winfsp-tests-x86 ^
    RETIRED-winfsp-tests-x86-case-randomize ^
    RETIRED-winfsp-tests-x86-flushpurge ^
    RETIRED-winfsp-tests-x86-legacy-unlink-rename ^
    RETIRED-winfsp-tests-x86-mountpoint-drive ^
    RETIRED-winfsp-tests-x86-mountpoint-dir ^
    RETIRED-winfsp-tests-x86-mountpoint-dir-case-sensitive ^
    RETIRED-winfsp-tests-x86-mountmgr-drive ^
    RETIRED-winfsp-tests-x86-mountmgr-dir ^
    RETIRED-winfsp-tests-x86-mountmgrfsd-drive ^
    RETIRED-winfsp-tests-x86-mountmgrfsd-dir ^
    RETIRED-winfsp-tests-x86-no-traverse ^
    RETIRED-winfsp-tests-x86-oplock ^
    RETIRED-winfsp-tests-x86-notify ^
    winfsp-tests-x86-external ^
    winfsp-tests-x86-external-share ^
    memfs-x86-disk-fsx ^
    memfs-x86-net-fsx ^
    RETIRED-memfs-x86-disk-standby ^
    RETIRED-memfs-x86-net-standby ^
    memfs-x86-net-use ^
    memfs-x86-disk-winfstest ^
    memfs-x86-net-winfstest ^
    fscrash-x86 ^
    winfsp-tests-dotnet-external ^
    winfsp-tests-dotnet-external-share ^
    memfs-dotnet-disk-fsx ^
    memfs-dotnet-net-fsx ^
    memfs-dotnet-disk-winfstest ^
    memfs-dotnet-net-winfstest
set opt_tests=^
    ifstest-memfs-x64-disk ^
    ifstest-memfs-x86-disk ^
    ifstest-memfs-dotnet-disk ^
    sample-ntptfs-x64 ^
    sample-ntptfs-x64-fsx ^
    sample-ntptfs-x64-ifstest ^
    sample-ntptfs-x86 ^
    sample-ntptfs-x86-fsx ^
    sample-ntptfs-x86-ifstest ^
    sample-memfs-fuse-x64 ^
    sample-memfs-fuse-x64-fsx ^
    sample-memfs-fuse-x86 ^
    sample-memfs-fuse-x86-fsx ^
    sample-memfs-fuse3-x64 ^
    sample-memfs-fuse3-x64-fsx ^
    sample-memfs-fuse3-x86 ^
    sample-memfs-fuse3-x86-fsx ^
    sample-airfs-x64 ^
    sample-airfs-x86 ^
    sample-passthrough-x64 ^
    sample-passthrough-x86 ^
    sample-passthrough-fuse-x64 ^
    sample-passthrough-fuse-x86 ^
    sample-passthrough-fuse3-x64 ^
    sample-passthrough-fuse3-x86 ^
    sample-passthrough-dotnet ^
    slowio-memfs-x64-fsx ^
    slowio-memfs-x86-fsx ^
    slowio-memfs-dotnet-fsx ^
    compat-v1.2-memfs-x64 ^
    compat-v1.2-memfs-x86 ^
    compat-v1.1-passthrough-fuse-x64 ^
    compat-v1.1-passthrough-fuse-x86 ^
    avast-tests-x64 ^
    avast-tests-x86 ^
    avast-tests-dotnet

set tests=
for %%f in (%dfl_tests%) do (
    set test=%%f
    if NOT "XRETIRED-!test:RETIRED-=!"=="X!test!" (
        if X%2==X (
            set tests=!tests! %%f
        ) else (
            if "X%2!test:%2=!"=="X!test!" set tests=!tests! %%f
        )
    )
)
for %%f in (%opt_tests%) do (
    set test=%%f
    if NOT "XRETIRED-!test:RETIRED-=!"=="X!test!" (
        if X%2==X (
            rem
        ) else (
            set test=%%f
            if "X%2!test:%2=!"=="X!test!" set tests=!tests! %%f
        )
    )
)

set testpass=0
set testfail=0
for %%f in (%tests%) do (
    echo === Running %%f

    if defined APPVEYOR (
        appveyor AddTest "%%f" -FileName None -Framework None -Outcome Running
    )

    pushd %cd%
    set "begtime=!time: =0!"
    call :%%f
    set ERRORLEVEL_save=!ERRORLEVEL!
    set "endtime=!time: =0!"
    popd

    REM see https://stackoverflow.com/a/9935540
    set "endtimecalc=!endtime:%time:~8,1%=%%100)*100+1!" & set "begtimecalc=!begtime:%time:~8,1%=%%100)*100+1!"
    set /A "duration=((((10!endtimecalc:%time:~2,1%=%%100)*60+1!%%100)-((((10!begtimecalc:%time:~2,1%=%%100)*60+1!%%100), duration-=(duration>>31)*24*60*60*100"
    set /A "duration=10*duration"
    set /A "durationSec=duration/1000"

    if !ERRORLEVEL_save! neq 0 (
        set /a testfail=testfail+1

        echo === Failed %%f ^(!durationSec!s^)

        if defined APPVEYOR (
            appveyor UpdateTest "%%f" -FileName None -Framework None -Outcome Failed -Duration !duration!
        )
    ) else (
        set /a testpass=testpass+1

        echo === Passed %%f ^(!durationSec!s^)

        if defined APPVEYOR (
            appveyor UpdateTest "%%f" -FileName None -Framework None -Outcome Passed -Duration !duration!
        )
    )
    echo:
)

launchctl-x64 stop memfs64 testdsk >nul
launchctl-x64 stop memfs64 testnet >nul
launchctl-x64 stop memfs32 testdsk >nul
launchctl-x64 stop memfs32 testnet >nul
launchctl-x64 stop memfs-dotnet testdsk >nul
launchctl-x64 stop memfs-dotnet testnet >nul
rem Cannot use timeout under cygwin/mintty: "Input redirection is not supported"
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 5 2>nul

set /a total=testpass+testfail
echo === Total: %testpass%/%total%
call :leak-test
if !ERRORLEVEL! neq 0 (
    rem Retry leak-test with delay. Some test systems (AppVeyor VS2019) take time before memfree.
    waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 20 2>nul
    call :leak-test
    if !ERRORLEVEL! neq 0 goto fail
)
if not %testfail%==0 goto fail

exit /b 0

:fail
exit /b 1

:winfsp-tests-x64
winfsp-tests-x64 +*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-case-randomize
winfsp-tests-x64 --case-randomize * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-flushpurge
winfsp-tests-x64 --flush-and-purge-on-cleanup * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-legacy-unlink-rename
winfsp-tests-x64 --legacy-unlink-rename * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-mountpoint-drive
winfsp-tests-x64 --mountpoint=X: --resilient * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-mountpoint-dir
winfsp-tests-x64 --mountpoint=mymnt --case-insensitive * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-mountpoint-dir-case-sensitive
winfsp-tests-x64 --mountpoint=mymnt * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-mountmgr-drive
winfsp-tests-x64 --mountpoint=\\.\X: --resilient * +ea* -exec*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-mountmgr-dir
winfsp-tests-x64 --mountpoint=\\.\%cd%\mnt --resilient * +ea* -exec*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-mountmgrfsd-drive
reg add HKLM\Software\WinFsp /v MountUseMountmgrFromFSD /t REG_DWORD /d 1 /f /reg:32
winfsp-tests-x64 --mountpoint=\\.\X: --resilient * +ea* -exec*
if !ERRORLEVEL! neq 0 goto fail
reg delete HKLM\Software\WinFsp /v MountUseMountmgrFromFSD /f /reg:32
exit /b 0

:winfsp-tests-x64-mountmgrfsd-dir
reg add HKLM\Software\WinFsp /v MountUseMountmgrFromFSD /t REG_DWORD /d 1 /f /reg:32
winfsp-tests-x64 --mountpoint=\\.\%cd%\mnt --resilient * +ea* -exec*
if !ERRORLEVEL! neq 0 goto fail
reg delete HKLM\Software\WinFsp /v MountUseMountmgrFromFSD /f /reg:32
exit /b 0

:winfsp-tests-x64-no-traverse
winfsp-tests-x64 --no-traverse * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-oplock
winfsp-tests-x64 --oplock=filter --resilient * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-notify
winfsp-tests-x64 --notify --resilient * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86
winfsp-tests-x86 +*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-case-randomize
winfsp-tests-x86 --case-randomize * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-flushpurge
winfsp-tests-x86 --flush-and-purge-on-cleanup * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-legacy-unlink-rename
winfsp-tests-x86 --legacy-unlink-rename * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-mountpoint-drive
winfsp-tests-x86 --mountpoint=X: --resilient * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-mountpoint-dir
winfsp-tests-x86 --mountpoint=mymnt --case-insensitive * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-mountpoint-dir-case-sensitive
winfsp-tests-x86 --mountpoint=mymnt * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-mountmgr-drive
winfsp-tests-x86 --mountpoint=\\.\X: --resilient * +ea* -exec*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-mountmgr-dir
winfsp-tests-x86 --mountpoint=\\.\%cd%\mnt --resilient * +ea* -exec*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-mountmgrfsd-drive
reg add HKLM\Software\WinFsp /v MountUseMountmgrFromFSD /t REG_DWORD /d 1 /f /reg:32
winfsp-tests-x86 --mountpoint=\\.\X: --resilient * +ea* -exec*
if !ERRORLEVEL! neq 0 goto fail
reg delete HKLM\Software\WinFsp /v MountUseMountmgrFromFSD /f /reg:32
exit /b 0

:winfsp-tests-x86-mountmgrfsd-dir
reg add HKLM\Software\WinFsp /v MountUseMountmgrFromFSD /t REG_DWORD /d 1 /f /reg:32
winfsp-tests-x86 --mountpoint=\\.\%cd%\mnt --resilient * +ea* -exec*
if !ERRORLEVEL! neq 0 goto fail
reg delete HKLM\Software\WinFsp /v MountUseMountmgrFromFSD /f /reg:32
exit /b 0

:winfsp-tests-x86-no-traverse
winfsp-tests-x86 --no-traverse * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-oplock
winfsp-tests-x86 --oplock=filter --resilient * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-notify
winfsp-tests-x86 --notify --resilient * +ea*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-external
M:
fltmc instances -v M: | findstr aswSnx >nul
if !ERRORLEVEL! neq 0 (
    "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x64.exe" --external --resilient +*
) else (
    REM Avast present
    "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x64.exe" --external --resilient ^
        -querydir_buffer_overflow_test
)
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-external-share
M:
fltmc instances -v M: | findstr aswSnx >nul
if !ERRORLEVEL! neq 0 (
    "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x64.exe" --external --share=winfsp-tests-share=M:\ --resilient ^
        -reparse_symlink*
) else (
    REM Avast present
    "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x64.exe" --external --share=winfsp-tests-share=M:\ --resilient ^
        -reparse_symlink* -querydir_buffer_overflow_test
)
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-x64-disk-fsx
M:
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -f foo -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-x64-net-fsx
N:
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -f foo -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-x64-disk-standby
M:
copy "%ProjRoot%\build\VStudio\build\%Configuration%\*"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-x64-net-standby
N:
copy "%ProjRoot%\build\VStudio\build\%Configuration%\*"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-x64-net-use
echo net use L: \\memfs64\share
net use L: \\memfs64\share
if !ERRORLEVEL! neq 0 goto fail
echo net use ^| findstr L: ^| findstr WinFsp.Np
net use | findstr L: | findstr WinFsp.Np
if !ERRORLEVEL! neq 0 goto fail
echo net use L: /delete
net use L: /delete
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-external
O:
fltmc instances -v O: | findstr aswSnx >nul
if !ERRORLEVEL! neq 0 (
    "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x86.exe" --external --resilient +*
) else (
    REM Avast present
    "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x86.exe" --external --resilient ^
        -querydir_buffer_overflow_test
)
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-external-share
O:
fltmc instances -v O: | findstr aswSnx >nul
if !ERRORLEVEL! neq 0 (
    "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x86.exe" --external --share=winfsp-tests-share=O:\ --resilient ^
        -reparse_symlink*
) else (
    REM Avast present
    "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x86.exe" --external --share=winfsp-tests-share=O:\ --resilient ^
        -reparse_symlink* -querydir_buffer_overflow_test
)
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-x86-disk-fsx
O:
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -f foo -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-x86-net-fsx
P:
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -f foo -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-x86-disk-standby
O:
copy "%ProjRoot%\build\VStudio\build\%Configuration%\*"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-x86-net-standby
P:
copy "%ProjRoot%\build\VStudio\build\%Configuration%\*"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-x86-net-use
echo net use L: \\memfs32\share
net use L: \\memfs32\share
if !ERRORLEVEL! neq 0 goto fail
echo net use ^| findstr L: ^| findstr WinFsp.Np
net use | findstr L: | findstr WinFsp.Np
if !ERRORLEVEL! neq 0 goto fail
echo net use L: /delete
net use L: /delete
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-x64-disk-winfstest
M:
call "%ProjRoot%\ext\test\winfstest\run-winfstest.bat"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-x64-net-winfstest
N:
call "%ProjRoot%\ext\test\winfstest\run-winfstest.bat"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-x86-disk-winfstest
O:
call "%ProjRoot%\ext\test\winfstest\run-winfstest.bat"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-x86-net-winfstest
P:
call "%ProjRoot%\ext\test\winfstest\run-winfstest.bat"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:fscrash-x64
for %%m in (^
    000002 000004 000008 000010 ^
    000020 000040 000080 000100 ^
    000800 001000 002000 004000 ^
    008000 080000 100000 200000 ^
    ) do (
    echo fscrash-x64 --terminate --mask=0x%%m --enter
    fscrash-x64 --terminate --mask=0x%%m --enter >nul 2>&1
    if !ERRORLEVEL! neq -1073741823 goto fail
    echo fscrash-x64 --terminate --mask=0x%%m --leave
    fscrash-x64 --terminate --mask=0x%%m --leave >nul 2>&1
    if !ERRORLEVEL! neq -1073741823 goto fail
)
echo fscrash-x64 --huge-alloc-size --cached
fscrash-x64 --huge-alloc-size --cached >nul 2>&1
if !ERRORLEVEL! neq 1 goto fail
exit /b 0

:fscrash-x86
for %%m in (^
    000002 000004 000008 000010 ^
    000020 000040 000080 000100 ^
    000800 001000 002000 004000 ^
    008000 080000 100000 200000 ^
    ) do (
    echo fscrash-x86 --terminate --mask=0x%%m --enter
    fscrash-x86 --terminate --mask=0x%%m --enter >nul 2>&1
    if !ERRORLEVEL! neq -1073741823 goto fail
    echo fscrash-x86 --terminate --mask=0x%%m --leave
    fscrash-x86 --terminate --mask=0x%%m --leave >nul 2>&1
    if !ERRORLEVEL! neq -1073741823 goto fail
)
echo fscrash-x86 --huge-alloc-size --cached
fscrash-x86 --huge-alloc-size --cached >nul 2>&1
if !ERRORLEVEL! neq 1 goto fail
exit /b 0

:winfsp-tests-dotnet-external
Q:
fltmc instances -v Q: | findstr aswSnx >nul
if !ERRORLEVEL! neq 0 (
    "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x64.exe" --external --resilient +*
) else (
    REM Avast present
    "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x64.exe" --external --resilient ^
        -querydir_buffer_overflow_test
)
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-dotnet-external-share
Q:
fltmc instances -v Q: | findstr aswSnx >nul
if !ERRORLEVEL! neq 0 (
    "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x64.exe" --external --share=winfsp-tests-share=Q:\ --resilient ^
        -reparse_symlink*
) else (
    REM Avast present
    "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x64.exe" --external --share=winfsp-tests-share=Q:\ --resilient ^
        -reparse_symlink* -querydir_buffer_overflow_test
)
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-dotnet-disk-fsx
Q:
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -f foo -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-dotnet-net-fsx
R:
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -f foo -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:slowio-memfs-x64-fsx
call :__run_fsx_memfs_slowio_test memfs64-slowio memfs-x64
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:slowio-memfs-x86-fsx
call :__run_fsx_memfs_slowio_test memfs32-slowio memfs-x86
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:slowio-memfs-dotnet-fsx
call :__run_fsx_memfs_slowio_test memfs.net-slowio memfs-dotnet-msil
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:__run_fsx_memfs_slowio_test
set RunSampleTestExit=0
call "%ProjRoot%\tools\fsreg" %1 "%ProjRoot%\build\VStudio\build\%Configuration%\%2.exe" "-u %%%%1 -m %%%%2 -M 50 -P 10 -R 5" "D:P(A;;RPWPLC;;;WD)"
echo net use L: "\\%1\share"
net use L: "\\%1\share"
if !ERRORLEVEL! neq 0 goto fail
echo net use ^| findstr L:
net use | findstr L:
pushd >nul
cd L: >nul 2>nul || (echo Unable to find drive L: >&2 & goto fail)
L:
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 set RunSampleTestExit=1
popd
echo net use L: /delete
net use L: /delete
call "%ProjRoot%\tools\fsreg" -u %1
exit /b !RunSampleTestExit!

:memfs-dotnet-disk-winfstest
Q:
call "%ProjRoot%\ext\test\winfstest\run-winfstest.bat"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:memfs-dotnet-net-winfstest
R:
call "%ProjRoot%\ext\test\winfstest\run-winfstest.bat"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:ifstest-memfs-x64-disk
call :__ifstest-memfs M: \Device\WinFsp.Disk C:
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:ifstest-memfs-x86-disk
call :__ifstest-memfs O: \Device\WinFsp.Disk C:
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:ifstest-memfs-dotnet-disk
call :__ifstest-memfs Q: \Device\WinFsp.Disk C:
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:__ifstest-memfs
%1
set IfsTestDirectories=^
    securit^
    opcreatg^
    opcreatp^
    closedel^
    volinfo^
    fileinfo^
    dirinfo^
    filelock^
    oplocks^
    chgnotif^
    readwr^
    seccache^
    reparspt^
    estream
set IfsTestMemfsExit=0
call :__ifstest %1 /g Security
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
rem OpenCreateGeneral.FileOpenByIDTest: FILE_OPEN_BY_FILE_ID not implemented
rem OpenCreateGeneral.OpenVolumeTest: volume handles can be opened/closed but no other support
call :__ifstest %1 /d %2 /g OpenCreateGeneral -t FileOpenByIDTest -t OpenVolumeTest
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
call :__ifstest %1 /g OpenCreateParameters
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
rem CloseCleanupDelete.UpdateOnCloseTest: WinFsp updates size information in directories immediately
rem CloseCleanupDelete.TunnelingTest: short names and tunneling not supported
call :__ifstest %1 /g CloseCleanupDelete -t UpdateOnCloseTest -t TunnelingTest
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
call :__ifstest %1 /g VolumeInformation
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
rem FileInformation.LinkInformationTest: WinFsp does not support hard links
rem FileInformation.StreamStandardInformationTest: test requires FileLinkInformation support (no hard links)
call :__ifstest %1 /g FileInformation -t LinkInformationTest -t StreamStandardInformationTest /r %3
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
call :__ifstest %1 /g EaInformation
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
call :__ifstest %1 /g DirectoryInformation
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
call :__ifstest %1 /g FileLocking
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
call :__ifstest %1 /g OpLocks
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
call :__ifstest %1 /g ChangeNotification
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
call :__ifstest %1 /g ReadWrite
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
call :__ifstest %1 /g SectionsCaching
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
rem ReparsePoints.SetPointEASNotSupportedTest: EA's not supported
rem ReparsePoints.EnumReparsePointsTest: enumeration of reparse points not supported
rem ReparsePoints.ChangeNotificationReparseTest: change notifications of reparse points not supported
rem ReparsePoints.SetPointIoReparseDataInvalidTest:
rem     This test succeeds on Server 2012 and fails on Server 2016/2019.
rem     Investigation on Server 2019 showed that the FSCTL_SET_REPARSE_POINT
rem     input buffer length was 23 instead of less than
rem     REPARSE_DATA_BUFFER_HEADER_SIZE(==8) like ifstest claims. This
rem     suggests that WinFsp is not the problem here, but perhaps some OS
rem     changes between Server 2012 and Server 2016. NOTE that we are still
rem     using the ifstest from Server 2012 HCK, which may account for the
rem     difference.
if "%APPVEYOR_BUILD_WORKER_IMAGE%"=="Visual Studio 2015" (
    call :__ifstest %1 /g ReparsePoints -t SetPointEASNotSupportedTest -t EnumReparsePointsTest -t ChangeNotificationReparseTest /c
    if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
) else (
    call :__ifstest %1 /g ReparsePoints -t SetPointEASNotSupportedTest -t EnumReparsePointsTest -t ChangeNotificationReparseTest -t SetPointIoReparseDataInvalidTest /c
    if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
)
rem IfsTest ReparsePoints seems to have a bug in that it cannot handle STATUS_PENDING for FSCTL_GET_REPARSE_POINT
rmdir /s/q reparspt
rem StreamEnhancements.StreamRenameTest: WinFsp does not support stream renaming
rem StreamEnhancements.StreamNotifyNameTest: WinFsp does not notify when streams are deleted because main file is deleted
call :__ifstest %1 /g StreamEnhancements -t StreamRenameTest -t StreamNotifyNameTest
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
for %%d in (!IfsTestDirectories!) do  (
    if exist %%d (echo :ifstest directory %%d still exists & set IfsTestMemfsExit=1)
)
exit /b !IfsTestMemfsExit!

:__ifstest-ntptfs
%1
set IfsTestDirectories=^
    securit^
    opcreatg^
    opcreatp^
    closedel^
    volinfo^
    fileinfo^
    dirinfo^
    filelock^
    oplocks^
    chgnotif^
    readwr^
    seccache^
    reparspt^
    estream
set IfsTestNtptfsExit=0
call :__ifstest %1 /g Security
if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
rem OpenCreateGeneral.FileOpenByIDTest: FILE_OPEN_BY_FILE_ID not implemented
rem OpenCreateGeneral.OpenVolumeTest: volume handles can be opened/closed but no other support
call :__ifstest %1 /d %2 /g OpenCreateGeneral -t FileOpenByIDTest -t OpenVolumeTest
if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
call :__ifstest %1 /g OpenCreateParameters
if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
rem CloseCleanupDelete.UpdateOnCloseTest: WinFsp updates size information in directories immediately
rem CloseCleanupDelete.TunnelingTest: short names and tunneling not supported
call :__ifstest %1 /g CloseCleanupDelete -t UpdateOnCloseTest -t TunnelingTest
if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
call :__ifstest %1 /g VolumeInformation
if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
rem FileInformation.LinkInformationTest: WinFsp does not support hard links
rem FileInformation.StreamStandardInformationTest: test requires FileLinkInformation support (no hard links)
rem On platforms other than Server 2012 disable EA testing because of EaSize mismatch.
if "%APPVEYOR_BUILD_WORKER_IMAGE%"=="Visual Studio 2015" (
    call :__ifstest %1 /g FileInformation -t LinkInformationTest -t StreamStandardInformationTest /r %3
    if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
) else (
    call :__ifstest %1 /g FileInformation -t LinkInformationTest -t StreamStandardInformationTest /z /r %3
    if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
)
rem EaInformation.SetEaInformationTest:
rem     This test fails on Server 2016/2019, because of EaSize mismatch. The reported EaSize
rem     depends on the OS version and is not a WinFsp problem .(The same test fails on NTFS.)
if "%APPVEYOR_BUILD_WORKER_IMAGE%"=="Visual Studio 2015" (
    call :__ifstest %1 /g EaInformation
    if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
) else (
    call :__ifstest %1 /g EaInformation -t SetEaInformationTest
    if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
)
rem On platforms other than Server 2012 disable EA testing because of EaSize mismatch.
if "%APPVEYOR_BUILD_WORKER_IMAGE%"=="Visual Studio 2015" (
    call :__ifstest %1 /g DirectoryInformation
    if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
) else (
    call :__ifstest %1 /g DirectoryInformation /z
    if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
)
call :__ifstest %1 /g FileLocking
if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
call :__ifstest %1 /g OpLocks
if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
call :__ifstest %1 /g ChangeNotification
if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
call :__ifstest %1 /g ReadWrite
if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
call :__ifstest %1 /g SectionsCaching
if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
rem ReparsePoints.SetPointEASNotSupportedTest: EA's not supported
rem ReparsePoints.EnumReparsePointsTest: enumeration of reparse points not supported
rem ReparsePoints.ChangeNotificationReparseTest: change notifications of reparse points not supported
rem ReparsePoints.SetPointIoReparseDataInvalidTest:
rem     This test succeeds on Server 2012 and fails on Server 2016/2019.
rem     Investigation on Server 2019 showed that the FSCTL_SET_REPARSE_POINT
rem     input buffer length was 23 instead of less than
rem     REPARSE_DATA_BUFFER_HEADER_SIZE(==8) like ifstest claims. This
rem     suggests that WinFsp is not the problem here, but perhaps some OS
rem     changes between Server 2012 and Server 2016. NOTE that we are still
rem     using the ifstest from Server 2012 HCK, which may account for the
rem     difference.
if "%APPVEYOR_BUILD_WORKER_IMAGE%"=="Visual Studio 2015" (
    call :__ifstest %1 /g ReparsePoints -t SetPointEASNotSupportedTest -t EnumReparsePointsTest -t ChangeNotificationReparseTest /c
    if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
) else if "%APPVEYOR_BUILD_WORKER_IMAGE%"=="Visual Studio 2017" (
    call :__ifstest %1 /g ReparsePoints -t SetPointEASNotSupportedTest -t EnumReparsePointsTest -t ChangeNotificationReparseTest -t SetPointIoReparseDataInvalidTest -t SetPointIoReparseTagMismatchTest -t SetPointAttributeConflictTest /c
    if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
) else (
    call :__ifstest %1 /g ReparsePoints -t SetPointEASNotSupportedTest -t EnumReparsePointsTest -t ChangeNotificationReparseTest -t SetPointIoReparseDataInvalidTest /c
    if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
)
rem IfsTest ReparsePoints seems to have a bug in that it cannot handle STATUS_PENDING for FSCTL_GET_REPARSE_POINT
rmdir /s/q reparspt
rem StreamEnhancements.StreamRenameTest: WinFsp does not support stream renaming
rem StreamEnhancements.StreamNotifyNameTest: WinFsp does not notify when streams are deleted because main file is deleted
call :__ifstest %1 /g StreamEnhancements -t StreamRenameTest -t StreamNotifyNameTest
if !ERRORLEVEL! neq 0 set IfsTestNtptfsExit=1
rem Ifstest likes to remove test directories using DELETE_ON_CLOSE.
rem On older versions of Windows NTPTFS cannot do POSIX delete and
rem if any component in the system (e.g. AntiVirus) keeps a file
rem in the directory momentarily open, then the directory cannot
rem be removed (and it fails silently). For this reason do not check
rem if directory/file removal succeeded on OS'es without POSIX delete.
if "%APPVEYOR_BUILD_WORKER_IMAGE%"=="Visual Studio 2015" exit /b !IfsTestNtptfsExit!
if "%APPVEYOR_BUILD_WORKER_IMAGE%"=="Visual Studio 2017" exit /b !IfsTestNtptfsExit!
for %%d in (!IfsTestDirectories!) do  (
    if exist %%d (echo :ifstest directory %%d still exists & set IfsTestNtptfsExit=1)
)
exit /b !IfsTestNtptfsExit!

:__ifstest
set IfsTestFound=
set IfsTestName=
set IfsTestGroup=
set IfsTestStatus=
set IfsTestLines=
set IfsTestExit=0
(SET LF=^
%=this line is empty=%
)
for /F "delims=" %%l in ('call "%ProjRoot%\tools\ifstest.bat" %* /v ^| findstr /n "^"') do (
    set IfsTestLine=%%l
    set IfsTestLine=!IfsTestLine:*:=!

    for /F "tokens=1,2,3 delims=:" %%h in ("%%l") do (
        set FieldName=%%i
        set FieldName=!FieldName: =!
        set FieldValue=%%j

        if not X!IfsTestLine!==X (
            set IfsTestLines=!IfsTestLines!!LF!    !IfsTestLine!

            if X!FieldName!==XTest (
                set IfsTestName=!FieldValue!
                set IfsTestFound=YES
            ) else if X!FieldName!==XGroup (
                set IfsTestGroup=!FieldValue!
            ) else if X!FieldName!==XStatus (
                set IfsTestStatus=!FieldValue!
            )
        ) else (
            set IfsTestPrefix=!IfsTestGroup!.!IfsTestName!..............................................................
            set IfsTestPrefix=!IfsTestPrefix:~0,63!
            if X!IfsTestName!==X (
                rem
            ) else if X!IfsTestStatus!==X (
                rem
            ) else if not "X!IfsTestStatus:(IFSTEST_SUCCESS)=!"=="X!IfsTestStatus!" (
                echo !IfsTestPrefix! OK
            ) else if not "X!IfsTestStatus:(IFSTEST_TEST_NOT_SUPPORTED)=!"=="X!IfsTestStatus!" (
                echo !IfsTestPrefix! SKIP
            ) else if not "X!IfsTestStatus:(IFSTEST_SUCCESS_NOT_SUPPORTED)=!"=="X!IfsTestStatus!" (
                echo !IfsTestPrefix! SKIP
            ) else if not "X!IfsTestStatus:(IFSTEST_INFO_END_OF_GROUP)=!"=="X!IfsTestStatus!" (
                rem
            ) else (
                echo !IfsTestPrefix! KO!IfsTestLines!
                set IfsTestExit=1
            )
            set IfsTestName=
            set IfsTestGroup=
            set IfsTestStatus=
            set IfsTestLines=
        )
    )
)
if not X!IfsTestFound!==XYES set IfsTestExit=1
exit /b !IfsTestExit!

:sample-ntptfs-x64
if "%APPVEYOR_BUILD_WORKER_IMAGE%"=="Visual Studio 2015" (
    rem need POSIX delete for rename_mmap_test
    call :__run_sample_ntptfs_test ntptfs x64 ntptfs-x64 ^
        "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x64.exe" ^
        "--external --resilient +* -rename_flipflop_test -rename_mmap_test -exec_rename_dir_test -stream_rename_flipflop_test"
    if !ERRORLEVEL! neq 0 goto fail
) else if "%APPVEYOR_BUILD_WORKER_IMAGE%"=="Visual Studio 2017" (
    rem need POSIX delete for rename_mmap_test
    call :__run_sample_ntptfs_test ntptfs x64 ntptfs-x64 ^
        "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x64.exe" ^
        "--external --resilient +* -rename_flipflop_test -rename_mmap_test -exec_rename_dir_test -stream_rename_flipflop_test"
    if !ERRORLEVEL! neq 0 goto fail
) else (
    call :__run_sample_ntptfs_test ntptfs x64 ntptfs-x64 ^
        "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x64.exe" ^
        "--external --resilient +* -rename_flipflop_test -exec_rename_dir_test -stream_rename_flipflop_test"
    if !ERRORLEVEL! neq 0 goto fail
)
exit /b 0

:sample-ntptfs-x86
if "%APPVEYOR_BUILD_WORKER_IMAGE%"=="Visual Studio 2015" (
    rem need POSIX delete for rename_mmap_test
    call :__run_sample_ntptfs_test ntptfs x86 ntptfs-x86 ^
        "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x86.exe" ^
        "--external --resilient +* -rename_flipflop_test -rename_mmap_test -exec_rename_dir_test -stream_rename_flipflop_test"
    if !ERRORLEVEL! neq 0 goto fail
) else if "%APPVEYOR_BUILD_WORKER_IMAGE%"=="Visual Studio 2017" (
    rem need POSIX delete for rename_mmap_test
    call :__run_sample_ntptfs_test ntptfs x86 ntptfs-x86 ^
        "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x86.exe" ^
        "--external --resilient +* -rename_flipflop_test -rename_mmap_test -exec_rename_dir_test -stream_rename_flipflop_test"
    if !ERRORLEVEL! neq 0 goto fail
) else (
    call :__run_sample_ntptfs_test ntptfs x86 ntptfs-x86 ^
        "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x86.exe" ^
        "--external --resilient +* -rename_flipflop_test -exec_rename_dir_test -stream_rename_flipflop_test"
    if !ERRORLEVEL! neq 0 goto fail
)
exit /b 0

:sample-ntptfs-x64-fsx
call :__run_sample_ntptfs_test ntptfs x64 ntptfs-x64 ^
    "%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" ^
    "-N 5000 test xxxxxx"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-ntptfs-x86-fsx
call :__run_sample_ntptfs_test ntptfs x86 ntptfs-x86 ^
    "%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" ^
    "-N 5000 test xxxxxx"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-ntptfs-x64-ifstest
call :__run_sample_ntptfs_test ntptfs x64 ntptfs-x64 ^
    call ^
    ":__ifstest-ntptfs L: \Device\WinFsp.Disk C:"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-ntptfs-x86-ifstest
call :__run_sample_ntptfs_test ntptfs x64 ntptfs-x64 ^
    call ^
    ":__ifstest-ntptfs L: \Device\WinFsp.Disk C:"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-memfs-fuse-x64
call :__run_sample_fuse_test memfs-fuse x64 memfs-fuse-x64 winfsp-tests-x64 "+*"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-memfs-fuse-x86
call :__run_sample_fuse_test memfs-fuse x86 memfs-fuse-x86 winfsp-tests-x86 "+*"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-memfs-fuse-x64-fsx
call :__run_sample_fsx_fuse_test memfs-fuse x64 memfs-fuse-x64 fsx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-memfs-fuse-x86-fsx
call :__run_sample_fsx_fuse_test memfs-fuse x86 memfs-fuse-x86 fsx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-memfs-fuse3-x64
call :__run_sample_fuse_test memfs-fuse3 x64 memfs-fuse3-x64 winfsp-tests-x64 ^
    "+* -create_fileattr_test -create_readonlydir_test -setfileinfo_test -delete_access_test -delete_ex_test"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-memfs-fuse3-x86
call :__run_sample_fuse_test memfs-fuse3 x86 memfs-fuse3-x86 winfsp-tests-x86 ^
    "+* -create_fileattr_test -create_readonlydir_test -setfileinfo_test -delete_access_test -delete_ex_test"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-memfs-fuse3-x64-fsx
call :__run_sample_fsx_fuse_test memfs-fuse3 x64 memfs-fuse3-x64 fsx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-memfs-fuse3-x86-fsx
call :__run_sample_fsx_fuse_test memfs-fuse3 x86 memfs-fuse3-x86 fsx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-airfs-x64
call :__run_sample_disk_test airfs x64 airfs-x64 winfsp-tests-x64 NOEXCL
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-airfs-x86
call :__run_sample_disk_test airfs x86 airfs-x86 winfsp-tests-x86 NOEXCL
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-passthrough-x64
call :__run_sample_test passthrough x64 passthrough-x64 winfsp-tests-x64
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-passthrough-x86
call :__run_sample_test passthrough x86 passthrough-x86 winfsp-tests-x86
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-passthrough-cpp-x64
call :__run_sample_test passthrough-cpp x64 passthrough-cpp-x64 winfsp-tests-x64
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-passthrough-cpp-x86
call :__run_sample_test passthrough-cpp x86 passthrough-cpp-x86 winfsp-tests-x86
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-passthrough-dotnet
call :__run_sample_test passthrough-dotnet anycpu passthrough-dotnet winfsp-tests-x64 ^
    "-create_backup_test -create_restore_test -create_namelen_test -getfileattr_test -delete_access_test -querydir_namelen_test"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-passthrough-fuse-x64
call :__run_sample_fuse_oldtest passthrough-fuse x64 passthrough-fuse-x64 winfsp-tests-x64
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-passthrough-fuse-x86
call :__run_sample_fuse_oldtest passthrough-fuse x86 passthrough-fuse-x86 winfsp-tests-x86
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-passthrough-fuse-x64-fsx
call :__run_sample_fsx_fuse_test passthrough-fuse x64 passthrough-fuse-x64 fsx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-passthrough-fuse-x86-fsx
call :__run_sample_fsx_fuse_test passthrough-fuse x86 passthrough-fuse-x86 fsx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-passthrough-fuse3-x64
call :__run_sample_fuse_oldtest passthrough-fuse3 x64 passthrough-fuse3-x64 winfsp-tests-x64 ^
    "-create_fileattr_test -create_readonlydir_test -setfileinfo_test"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-passthrough-fuse3-x86
call :__run_sample_fuse_oldtest passthrough-fuse3 x86 passthrough-fuse3-x86 winfsp-tests-x86 ^
    "-create_fileattr_test -create_readonlydir_test -setfileinfo_test"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-passthrough-fuse3-x64-fsx
call :__run_sample_fsx_fuse_test passthrough-fuse3 x64 passthrough-fuse3-x64 fsx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:sample-passthrough-fuse3-x86-fsx
call :__run_sample_fsx_fuse_test passthrough-fuse3 x86 passthrough-fuse3-x86 fsx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:__run_sample_ntptfs_test
set RunSampleTestExit=0
call %ProjRoot%\tools\build-sample %Configuration% %2 %1 "%TMP%\%1"
if !ERRORLEVEL! neq 0 goto fail
mkdir "%TMP%\%1\test"
call "%ProjRoot%\tools\fsreg" %1 "%TMP%\%1\build\%Configuration%\%3.exe" "-p %%%%1 -m %%%%2 -o ExtraFeatures -o SetAllocationSizeOnCleanup" "D:P(A;;RPWPLC;;;WD)"
echo launchctl-x64 start %1 testdsk "%TMP%\%1\test" L:
launchctl-x64 start %1 testdsk "%TMP%\%1\test" L: >nul
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
pushd >nul
cd L: >nul 2>nul || (echo Unable to find drive L: >&2 & goto fail)
L:
%4 %~5
if !ERRORLEVEL! neq 0 set RunSampleTestExit=1
popd
echo launchctl-x64 stop %1 testdsk
launchctl-x64 stop %1 testdsk >nul
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
call "%ProjRoot%\tools\fsreg" -u %1
rmdir /s/q "%TMP%\%1"
exit /b !RunSampleTestExit!

:__run_sample_disk_test
set RunSampleTestExit=0
call %ProjRoot%\tools\build-sample %Configuration% %2 %1 "%TMP%\%1"
if !ERRORLEVEL! neq 0 goto fail
mkdir "%TMP%\%1\test"
call "%ProjRoot%\tools\fsreg" %1 "%TMP%\%1\build\%Configuration%\%3.exe" "-i -u %%%%1 -m %%%%2" "D:P(A;;RPWPLC;;;WD)"
echo launchctl-x64 start %1 testdsk "" L:
launchctl-x64 start %1 testdsk "" L: >nul
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
pushd >nul
cd L: >nul 2>nul || (echo Unable to find drive L: >&2 & goto fail)
L:
if X%5==XNOEXCL (
    "%ProjRoot%\build\VStudio\build\%Configuration%\%4.exe" ^
        --external --resilient
) else (
    "%ProjRoot%\build\VStudio\build\%Configuration%\%4.exe" ^
        --external --resilient --case-insensitive-cmp --share-prefix="\%1\%TMP::=$%\%1\test" ^
        -create_allocation_test -getfileinfo_name_test -rename_flipflop_test -rename_mmap_test -exec_rename_dir_test ^
        -reparse* -stream* %~5
)
if !ERRORLEVEL! neq 0 set RunSampleTestExit=1
popd
echo launchctl-x64 stop %1 testdsk
launchctl-x64 stop %1 testdsk >nul
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
call "%ProjRoot%\tools\fsreg" -u %1
rmdir /s/q "%TMP%\%1"
exit /b !RunSampleTestExit!

:__run_sample_test
set RunSampleTestExit=0
call %ProjRoot%\tools\build-sample %Configuration% %2 %1 "%TMP%\%1"
if !ERRORLEVEL! neq 0 goto fail
mkdir "%TMP%\%1\test"
call "%ProjRoot%\tools\fsreg" %1 "%TMP%\%1\build\%Configuration%\%3.exe" "-u %%%%1 -m %%%%2" "D:P(A;;RPWPLC;;;WD)"
echo net use L: "\\%1\%TMP::=$%\%1\test"
net use L: "\\%1\%TMP::=$%\%1\test"
if !ERRORLEVEL! neq 0 goto fail
echo net use ^| findstr L:
net use | findstr L:
pushd >nul
cd L: >nul 2>nul || (echo Unable to find drive L: >&2 & goto fail)
L:
if X%5==XNOEXCL (
    "%ProjRoot%\build\VStudio\build\%Configuration%\%4.exe" ^
        --external --resilient
) else (
    "%ProjRoot%\build\VStudio\build\%Configuration%\%4.exe" ^
        --external --resilient --case-insensitive-cmp --share-prefix="\%1\%TMP::=$%\%1\test" ^
        -create_allocation_test -getfileinfo_name_test -rename_flipflop_test -rename_mmap_test -exec_rename_dir_test ^
        -reparse* -stream* %~5
)
if !ERRORLEVEL! neq 0 set RunSampleTestExit=1
"%ProjRoot%\build\VStudio\build\%Configuration%\%4.exe" ^
    --external --resilient --case-insensitive-cmp --share-prefix="\%1\%TMP::=$%\%1\test" ^
    +querydir_single_test
if !ERRORLEVEL! neq 0 set RunSampleTestExit=1
popd
echo net use L: /delete
net use L: /delete
call "%ProjRoot%\tools\fsreg" -u %1
rmdir /s/q "%TMP%\%1"
exit /b !RunSampleTestExit!

:__run_sample_fuse_test
set RunSampleTestExit=0
call %ProjRoot%\tools\build-sample %Configuration% %2 %1 "%TMP%\%1"
if !ERRORLEVEL! neq 0 goto fail
mkdir "%TMP%\%1\test"
call "%ProjRoot%\tools\fsreg" %1 "%TMP%\%1\build\%Configuration%\%3.exe" ^
    "-orellinks,uid=11,gid=65792 --VolumePrefix=%%%%1 %%%%2" "D:P(A;;RPWPLC;;;WD)"
echo net use L: "\\%1\%TMP::=$%\%1\test"
net use L: "\\%1\%TMP::=$%\%1\test"
if !ERRORLEVEL! neq 0 goto fail
echo net use ^| findstr L:
net use | findstr L:
pushd >nul
cd L: >nul 2>nul || (echo Unable to find drive L: >&2 & goto fail)
L:
REM Exclude reparse_symlink* tests as they do not pass on AppVeyor VS2015. They do pass locally.
if defined APPVEYOR (
    "%ProjRoot%\build\VStudio\build\%Configuration%\%4.exe" ^
        --fuse-external --resilient --case-insensitive-cmp --share-prefix="\%1\%TMP::=$%\%1\test" %~5 ^
        -reparse_symlink*
) else (
    "%ProjRoot%\build\VStudio\build\%Configuration%\%4.exe" ^
        --fuse-external --resilient --case-insensitive-cmp --share-prefix="\%1\%TMP::=$%\%1\test" %~5
)
if !ERRORLEVEL! neq 0 set RunSampleTestExit=1
popd
echo net use L: /delete
net use L: /delete
call "%ProjRoot%\tools\fsreg" -u %1
rmdir /s/q "%TMP%\%1"
exit /b !RunSampleTestExit!

:__run_sample_fuse_oldtest
set RunSampleTestExit=0
call %ProjRoot%\tools\build-sample %Configuration% %2 %1 "%TMP%\%1"
if !ERRORLEVEL! neq 0 goto fail
mkdir "%TMP%\%1\test"
call "%ProjRoot%\tools\fsreg" %1 "%TMP%\%1\build\%Configuration%\%3.exe" ^
    "-ouid=11,gid=65792 --VolumePrefix=%%%%1 %%%%2" "D:P(A;;RPWPLC;;;WD)"
echo net use L: "\\%1\%TMP::=$%\%1\test"
net use L: "\\%1\%TMP::=$%\%1\test"
if !ERRORLEVEL! neq 0 goto fail
echo net use ^| findstr L:
net use | findstr L:
pushd >nul
cd L: >nul 2>nul || (echo Unable to find drive L: >&2 & goto fail)
L:
if X%5==XNOEXCL (
    "%ProjRoot%\build\VStudio\build\%Configuration%\%4.exe" ^
        --external --resilient
) else (
    "%ProjRoot%\build\VStudio\build\%Configuration%\%4.exe" ^
        --external --resilient --case-insensitive-cmp --share-prefix="\%1\%TMP::=$%\%1\test" ^
        -create_allocation_test -create_notraverse_test -create_backup_test -create_restore_test -create_namelen_test ^
        -getfileattr_test -getfileinfo_name_test -delete_access_test -delete_mmap_test -delete_ex_test -rename_flipflop_test -rename_mmap_test -rename_ex_test -setsecurity_test -querydir_namelen_test -exec_rename_dir_test ^
        -reparse* -stream* %~5
)
if !ERRORLEVEL! neq 0 set RunSampleTestExit=1
popd
echo net use L: /delete
net use L: /delete
call "%ProjRoot%\tools\fsreg" -u %1
rmdir /s/q "%TMP%\%1"
exit /b !RunSampleTestExit!

:__run_sample_fsx_fuse_test
set RunSampleTestExit=0
call %ProjRoot%\tools\build-sample %Configuration% %2 %1 "%TMP%\%1"
if !ERRORLEVEL! neq 0 goto fail
mkdir "%TMP%\%1\test"
call "%ProjRoot%\tools\fsreg" %1 "%TMP%\%1\build\%Configuration%\%3.exe" ^
    "-ouid=11,gid=65792 --VolumePrefix=%%%%1 %%%%2" "D:P(A;;RPWPLC;;;WD)"
echo net use L: "\\%1\%TMP::=$%\%1\test"
net use L: "\\%1\%TMP::=$%\%1\test"
if !ERRORLEVEL! neq 0 goto fail
echo net use ^| findstr L:
net use | findstr L:
pushd >nul
cd L: >nul 2>nul || (echo Unable to find drive L: >&2 & goto fail)
L:
"%ProjRoot%\ext\test\fstools\src\fsx\%4.exe" -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 set RunSampleTestExit=1
popd
echo net use L: /delete
net use L: /delete
call "%ProjRoot%\tools\fsreg" -u %1
rmdir /s/q "%TMP%\%1"
exit /b !RunSampleTestExit!

:compat-v1.2-memfs-x64
copy "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-*.dll" "%ProjRoot%\tst\compat\v1.2\memfs"
call :__run_compat_memfs_test compat-memfs v1.2\memfs\memfs-x64 winfsp-tests-x64
if !ERRORLEVEL! neq 0 goto fail
del "%ProjRoot%\tst\compat\v1.2\memfs\winfsp-*.dll"
exit /b 0

:compat-v1.2-memfs-x86
copy "%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-*.dll" "%ProjRoot%\tst\compat\v1.2\memfs"
call :__run_compat_memfs_test compat-memfs v1.2\memfs\memfs-x86 winfsp-tests-x86
if !ERRORLEVEL! neq 0 goto fail
del "%ProjRoot%\tst\compat\v1.2\memfs\winfsp-*.dll"
exit /b 0

:__run_compat_memfs_test
set RunSampleTestExit=0
call "%ProjRoot%\tools\fsreg" %1 "%ProjRoot%\tst\compat\%2.exe" ^
    "-i -F NTFS -n 65536 -s 67108864 -u %%%%1 -m %%%%2" "D:P(A;;RPWPLC;;;WD)"
echo net use L: "\\%1\share"
net use L: "\\%1\share"
if !ERRORLEVEL! neq 0 goto fail
echo net use ^| findstr L:
net use | findstr L:
pushd >nul
cd L: >nul 2>nul || (echo Unable to find drive L: >&2 & goto fail)
L:
"%ProjRoot%\build\VStudio\build\%Configuration%\%3.exe" ^
    --external --resilient --share-prefix="\%1\share"
if !ERRORLEVEL! neq 0 set RunSampleTestExit=1
popd
echo net use L: /delete
net use L: /delete
call "%ProjRoot%\tools\fsreg" -u %1
exit /b !RunSampleTestExit!

:compat-v1.1-passthrough-fuse-x64
call :__run_compat_fuse_test passthrough-fuse v1.1\passthrough-fuse\passthrough-fuse-x64 winfsp-tests-x64
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:compat-v1.1-passthrough-fuse-x86
call :__run_compat_fuse_test passthrough-fuse v1.1\passthrough-fuse\passthrough-fuse-x86 winfsp-tests-x86
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:__run_compat_fuse_test
set RunSampleTestExit=0
mkdir "%TMP%\%1\test"
call "%ProjRoot%\tools\fsreg" %1 "%ProjRoot%\tst\compat\%2.exe" ^
    "-ouid=11,gid=65792 --VolumePrefix=%%%%1 %%%%2" "D:P(A;;RPWPLC;;;WD)"
echo net use L: "\\%1\%TMP::=$%\%1\test"
net use L: "\\%1\%TMP::=$%\%1\test"
if !ERRORLEVEL! neq 0 goto fail
echo net use ^| findstr L:
net use | findstr L:
pushd >nul
cd L: >nul 2>nul || (echo Unable to find drive L: >&2 & goto fail)
L:
"%ProjRoot%\build\VStudio\build\%Configuration%\%3.exe" ^
    --external --resilient --case-insensitive-cmp --share-prefix="\%1\%TMP::=$%\%1\test" ^
    -create_fileattr_test -create_readonlydir_test -create_allocation_test -create_notraverse_test -create_backup_test -create_restore_test -create_namelen_test ^
    -getfileattr_test -getfileinfo_name_test -setfileinfo_test -delete_access_test -delete_mmap_test -delete_ex_test -rename_flipflop_test -rename_mmap_test -rename_ex_test -setsecurity_test -querydir_namelen_test -exec_rename_dir_test ^
    -reparse* -stream*
if !ERRORLEVEL! neq 0 set RunSampleTestExit=1
popd
echo net use L: /delete
net use L: /delete
call "%ProjRoot%\tools\fsreg" -u %1
rmdir /s/q "%TMP%\%1"
exit /b !RunSampleTestExit!

:avast-tests-x64
call :winfsp-tests-x64-external
if !ERRORLEVEL! neq 0 goto fail
call :winfsp-tests-x64-external-share
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:avast-tests-x86
call :winfsp-tests-x86-external
if !ERRORLEVEL! neq 0 goto fail
call :winfsp-tests-x86-external-share
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:avast-tests-dotnet
call :winfsp-tests-dotnet-external
if !ERRORLEVEL! neq 0 goto fail
call :winfsp-tests-dotnet-external-share
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:leak-test
for /F "tokens=1,2 delims=:" %%i in ('verifier /query ^| findstr ^
    /c:"Current Pool Allocations:" ^
    /c:"CurrentPagedPoolAllocations:" ^
    /c:"CurrentNonPagedPoolAllocations:"'
    ) do (

    set FieldName=%%i
    set FieldName=!FieldName: =!

    set FieldValue=%%j
    set FieldValue=!FieldValue: =!
    set FieldValue=!FieldValue:^(=!
    set FieldValue=!FieldValue:^)=!

    if X!FieldName!==XCurrentPoolAllocations (
        for /F "tokens=1,2 delims=/" %%k in ("!FieldValue!") do (
            set NonPagedAlloc=%%k
            set PagedAlloc=%%l
        )
    ) else if X!FieldName!==XCurrentPagedPoolAllocations (
        set PagedAlloc=!FieldValue!
    ) else if X!FieldName!==XCurrentNonPagedPoolAllocations (
        set NonPagedAlloc=!FieldValue!
    )
)
set /A TotalAlloc=PagedAlloc+NonPagedAlloc
if !TotalAlloc! equ 0 (
    echo === Leaks: None
) else (
    echo === Leaks: !NonPagedAlloc! NP / !PagedAlloc! P
    goto fail
)
exit /b 0
