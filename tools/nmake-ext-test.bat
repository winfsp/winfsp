@echo off

setlocal

set Configuration=Release
if not X%1==X set Configuration=%1

call "%~dp0vcvarsall.bat" x64

cd %~dp0..\ext\test
nmake /f Nmakefile
