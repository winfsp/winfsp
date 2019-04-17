@echo off

setlocal

set CONFIG=Debug
set SUFFIX=x64
set TARGET_MACHINE=WIN8DBG
if not X%1==X set TARGET_MACHINE=%1
set TARGET_ACCOUNT=\Users\%USERNAME%\Downloads\winfsp\
set TARGET=\\%TARGET_MACHINE%%TARGET_ACCOUNT%

cd %~dp0..
mkdir %TARGET% 2>nul
for %%f in (winfsp-%SUFFIX%.sys winfsp-%SUFFIX%.dll winfsp-tests-%SUFFIX%.exe fsbench-%SUFFIX%.exe fscrash-%SUFFIX%.exe memfs-%SUFFIX%.exe) do (
	copy build\VStudio\build\%CONFIG%\%%f %TARGET% >nul
)
copy tools\ifstest.bat %TARGET% >nul
echo sc create WinFsp type=filesys binPath=%%~dp0winfsp-%SUFFIX%.sys >%TARGET%sc-create.bat
echo sc start WinFsp >%TARGET%sc-start.bat
echo sc stop WinFsp >%TARGET%sc-stop.bat
echo sc delete WinFsp >%TARGET%sc-delete.bat
