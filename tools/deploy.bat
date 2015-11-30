@echo off

set CONFIG=Debug
set SUFFIX=x64
set TARGET_MACHINE=WIN8DBG
set TARGET_ACCOUNT=\Users\%USERNAME%\Downloads\winfsp\
set TARGET=\\%TARGET_MACHINE%%TARGET_ACCOUNT%

echo on
cd %~dp0..
mkdir %TARGET% 2>nul
for %%f in (winfsp-%SUFFIX%.sys winfsp-%SUFFIX%.dll winfsp-tests-%SUFFIX%.exe mirror-%SUFFIX%.exe) do (
	copy build\VStudio\build\%CONFIG%\%%f %TARGET%
)
echo sc create WinFsp type=filesys binPath=%%~dp0%DRIVER% >%TARGET%sc-create.bat
echo sc start WinFsp >%TARGET%sc-start.bat
echo sc stop WinFsp >%TARGET%sc-stop.bat
echo sc delete WinFsp >%TARGET%sc-delete.bat
