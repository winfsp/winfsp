@echo off

setlocal
setlocal EnableDelayedExpansion

if not X%1==X set File=%1
if X%File%==X (echo usage: wpr-test file.etl fsbench-args... >&2 & goto fail)

REM see https://stackoverflow.com/a/45969239/429091
set Args=%*
call set Args=%%args:*%1=%%

REM see https://stackoverflow.com/a/11995662
net session >nul 2>&1
if %ERRORLEVEL% neq 0 echo must be run as Administrator >&2 & goto fail

set outdir=%cd%
pushd %~dp0..
set ProjRoot=%cd%
popd

set fsbench="%ProjRoot%\build\VStudio\build\Release\fsbench-x64.exe"
if not exist %fsbench% echo cannot find fsbench >&2 & goto fail

wpr -start CPU -start FileIO

mkdir fsbench
pushd fsbench
%fsbench% %Args%
popd
rmdir fsbench

wpr -stop %File% %File% -skipPdbGen

exit /b 0

:fail
exit /b 1
