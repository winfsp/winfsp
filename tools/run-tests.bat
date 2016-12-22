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
rem Cannot use timeout under cygwin/mintty: "Input redirection is not supported"
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
cd M: >nul 2>nul || (echo === Unable to find drive M: >&2 & goto fail)
cd N: >nul 2>nul || (echo === Unable to find drive N: >&2 & goto fail)
cd O: >nul 2>nul || (echo === Unable to find drive O: >&2 & goto fail)
cd P: >nul 2>nul || (echo === Unable to find drive P: >&2 & goto fail)

set dfl_tests=^
    winfsp-tests-x64 ^
    winfsp-tests-x64-case-randomize ^
    winfsp-tests-x64-mountpoint-drive ^
    winfsp-tests-x64-mountpoint-dir ^
    winfsp-tests-x64-no-traverse ^
    winfsp-tests-x64-oplock ^
    winfsp-tests-x64-external-share ^
    fsx-memfs-x64-disk ^
    fsx-memfs-x64-net ^
    standby-memfs-x64-disk ^
    standby-memfs-x64-net ^
    net-use-memfs-x64 ^
    winfstest-memfs-x64-disk ^
    winfstest-memfs-x64-net ^
    fscrash-x64 ^
    winfsp-tests-x86 ^
    winfsp-tests-x86-case-randomize ^
    winfsp-tests-x86-mountpoint-drive ^
    winfsp-tests-x86-mountpoint-dir ^
    winfsp-tests-x86-no-traverse ^
    winfsp-tests-x86-oplock ^
    winfsp-tests-x86-external-share ^
    fsx-memfs-x86-disk ^
    fsx-memfs-x86-net ^
    standby-memfs-x86-disk ^
    standby-memfs-x86-net ^
    net-use-memfs-x86 ^
    winfstest-memfs-x86-disk ^
    winfstest-memfs-x86-net ^
    fscrash-x86
set opt_tests=^
    ifstest-memfs-x64-disk ^
    ifstest-memfs-x86-disk

set tests=
for %%f in (%dfl_tests%) do (
    if X%2==X (
        set tests=!tests! %%f
    ) else (
        set test=%%f
        if not "X!test:%2=!"=="X!test!" set tests=!tests! %%f
    )
)
for %%f in (%opt_tests%) do (
    if X%2==X (
        rem
    ) else (
        set test=%%f
        if not "X!test:%2=!"=="X!test!" set tests=!tests! %%f
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
    call :%%f
    popd

    if !ERRORLEVEL! neq 0 (
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

launchctl-x64 stop memfs64 testdsk >nul
launchctl-x64 stop memfs64 testnet >nul
launchctl-x64 stop memfs32 testdsk >nul
launchctl-x64 stop memfs32 testnet >nul
rem Cannot use timeout under cygwin/mintty: "Input redirection is not supported"
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul

set /a total=testpass+testfail
echo === Total: %testpass%/%total%
call :leak-test
if !ERRORLEVEL! neq 0 goto fail
if not %testfail%==0 goto fail

exit /b 0

:fail
exit /b 1

:winfsp-tests-x64
winfsp-tests-x64 +*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-case-randomize
winfsp-tests-x64 --case-randomize
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-mountpoint-drive
winfsp-tests-x64 --mountpoint=X:
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-mountpoint-dir
winfsp-tests-x64 --mountpoint=mymnt --case-insensitive
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-no-traverse
winfsp-tests-x64 --no-traverse
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-oplock
winfsp-tests-x64 --oplock=filter --resilient
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86
winfsp-tests-x86 +*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-case-randomize
winfsp-tests-x86 --case-randomize
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-mountpoint-drive
winfsp-tests-x86 --mountpoint=X:
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-mountpoint-dir
winfsp-tests-x86 --mountpoint=mymnt --case-insensitive
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-no-traverse
winfsp-tests-x86 --no-traverse
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x86-oplock
winfsp-tests-x86 --oplock=filter --resilient
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfsp-tests-x64-external-share
M:
"%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x64.exe" --external --share=winfsp-tests-share=M:\ --resilient ^
    -reparse_symlink*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:fsx-memfs-x64-disk
M:
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -f foo -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:fsx-memfs-x64-net
N:
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -f foo -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:standby-memfs-x64-disk
M:
copy "%ProjRoot%\build\VStudio\build\%Configuration%\*"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:standby-memfs-x64-net
N:
copy "%ProjRoot%\build\VStudio\build\%Configuration%\*"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:net-use-memfs-x64
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

:winfsp-tests-x86-external-share
O:
"%ProjRoot%\build\VStudio\build\%Configuration%\winfsp-tests-x86.exe" --external --share=winfsp-tests-share=O:\ --resilient ^
    -reparse_symlink*
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:fsx-memfs-x86-disk
O:
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -f foo -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:fsx-memfs-x86-net
P:
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
"%ProjRoot%\ext\test\fstools\src\fsx\fsx.exe" -f foo -N 5000 test xxxxxx
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:standby-memfs-x86-disk
O:
copy "%ProjRoot%\build\VStudio\build\%Configuration%\*"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:standby-memfs-x86-net
P:
copy "%ProjRoot%\build\VStudio\build\%Configuration%\*"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:net-use-memfs-x86
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

:winfstest-memfs-x64-disk
M:
call "%ProjRoot%\ext\test\winfstest\run-winfstest.bat"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfstest-memfs-x64-net
N:
call "%ProjRoot%\ext\test\winfstest\run-winfstest.bat"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfstest-memfs-x86-disk
O:
call "%ProjRoot%\ext\test\winfstest\run-winfstest.bat"
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:winfstest-memfs-x86-net
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

:ifstest-memfs-x64-disk
M:
call :__ifstest-memfs M: \Device\WinFsp.Disk
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:ifstest-memfs-x86-disk
O:
call :__ifstest-memfs O: \Device\WinFsp.Disk
if !ERRORLEVEL! neq 0 goto fail
exit /b 0

:__ifstest-memfs
set IfsTestMemfsExit=0
M:
call :__ifstest %1 /d %2 /g OpenCreateGeneral -t FileOpenByIDTest -t OpenVolumeTest /z /v
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
call :__ifstest %1 /d %2 /g OpenCreateParameters /z /v
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
rem call :__ifstest %1 /d %2 /g CloseCleanupDelete /z /v
rem if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
call :__ifstest %1 /d %2 /g VolumeInformation /z /v
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
rem call :__ifstest %1 /d %2 /g FileInformation /z /v
rem if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
rem call :__ifstest %1 /d %2 /g DirectoryInformation /z /v
rem if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
call :__ifstest %1 /d %2 /g FileLocking /z /v
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
rem call :__ifstest %1 /d %2 /g OpLocks /z /v
rem if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
rem call :__ifstest %1 /d %2 /g ChangeNotification /z /v
rem if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
call :__ifstest %1 /d %2 /g ReadWrite /z /v
if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
rem call :__ifstest %1 /d %2 /g SectionsCaching /z /v
rem if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
rem call :__ifstest %1 /d %2 /g ReparsePoints /z /v
rem if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
rem call :__ifstest %1 /d %2 /g StreamEnhancements /z /v
rem if !ERRORLEVEL! neq 0 set IfsTestMemfsExit=1
exit /b !IfsTestMemfsExit!

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
for /F "delims=" %%l in ('call "%ProjRoot%\tools\ifstest.bat" %* ^| findstr /n "^"') do (
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
            if X!IfsTestStatus!==X (
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
