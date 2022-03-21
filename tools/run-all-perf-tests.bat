@echo off

setlocal
setlocal EnableDelayedExpansion

REM see https://stackoverflow.com/a/11995662
net session >nul 2>&1
if !ERRORLEVEL! neq 0 echo must be run as Administrator >&2 & goto fail

set Count=3
if not X%1==X set Count=%1

set outdir=%cd%
pushd %~dp0..
set ProjRoot=%cd%
popd

set perftests="%ProjRoot%\tools\run-perf-tests.bat"
set memfs="%ProjRoot%\build\VStudio\build\Release\memfs-x64.exe"
set ntptfs="%ProjRoot%\tst\ntptfs\build\Release\ntptfs-x64.exe"
if not exist %memfs% echo cannot find memfs >&2 & goto fail
if not exist %ntptfs% echo cannot find ntptfs >&2 & goto fail

mkdir C:\t
pushd C:\t
for /l %%i in (1,1,%Count%) do (
    echo ntfs-%%i
    call %perftests% Release > %outdir%\ntfs-%%i.csv
    if !ERRORLEVEL! neq 0 goto fail
)
popd
rmdir C:\t

start "" /b %memfs% -t -1 -n 1000000 -i -m X:
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
pushd X:\
for /l %%i in (1,1,%Count%) do (
    echo memfs-%%i
    call %perftests% Release > %outdir%\memfs-%%i.csv
    if !ERRORLEVEL! neq 0 goto fail
)
popd
taskkill /f /im memfs-x64.exe

powershell -NoProfile -ExecutionPolicy Bypass -Command "Add-MpPreference -ExclusionProcess '%ntptfs%'"
mkdir C:\t
start "" /b %ntptfs% -t -1 -p C:\t -m X:
waitfor 7BF47D72F6664550B03248ECFE77C7DD /t 3 2>nul
pushd X:\
for /l %%i in (1,1,%Count%) do (
    echo ntptfs-%%i
    call %perftests% Release > %outdir%\ntptfs-%%i.csv
    if !ERRORLEVEL! neq 0 goto fail
)
popd
taskkill /f /im ntptfs-x64.exe
rmdir C:\t
powershell -NoProfile -ExecutionPolicy Bypass -Command "Remove-MpPreference -ExclusionProcess '%ntptfs%'"

exit /b 0

:fail
exit /b 1
