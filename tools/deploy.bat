@echo off

set CONFIG=Debug
set DRIVER=winfsp-x64.sys
set TARGET_MACHINE=WIN8DBG
set TARGET_ACCOUNT=\Users\%USERNAME%\Downloads\winfsp\
set TARGET=\\%TARGET_MACHINE%%TARGET_ACCOUNT%

echo on
cd %~dp0..
mkdir %TARGET% 2>nul
copy build\VStudio\build\%CONFIG%\%DRIVER% %TARGET%
echo sc create WinFsp type=filesys binPath=%%~dp0%DRIVER% >%TARGET%sc-create.bat
echo sc delete WinFsp >%TARGET%sc-delete.bat
