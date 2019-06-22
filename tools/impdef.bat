@echo off

setlocal
setlocal EnableDelayedExpansion

if X%1==X goto usage
if X%2==X goto usage
set infile=%1
set infile=%infile:/=\%
set outfile=%2
set outfile=%outfile:/=\%
set workdir=!infile!.work
set workbase=!workdir!\%~n2
set outarch=%~n2
set outarch=%outarch:~-3%

set arch=x64
set cdef=/D_AMD64_
if /i X%outarch%==Xx86 set arch=x86
if /i X%outarch%==Xx86 set cdef=/D_X86_

call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" !arch!
set INCLUDE=%~dp0..\opt\fsext\inc;%~dp0..\inc;!WindowsSdkDir!Include\!WindowsSDKVersion!km\crt;!WindowsSdkDir!Include\!WindowsSDKVersion!km;!WindowsSdkDir!Include\!WindowsSDKVersion!km\shared;!INCLUDE!

if exist !workdir! rmdir /s/q !workdir!
mkdir !workdir!

type !infile! >>!workbase!.c
cl /LD /Fe!workbase!.sys /Fo!workbase!.obj !cdef! /D_KERNEL_MODE /wd4716 !workbase!.c
if errorlevel 1 goto fail

copy !workbase!.lib !outfile!
if errorlevel 1 goto fail

rmdir /s/q !workdir!

exit /b 0

:fail
exit /b 1

:usage
echo usage: impdef.bat infile.impdef outfile.lib 1>&2
exit /b 2
