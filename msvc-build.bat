setlocal
pushd "%~dp0"
set "Configuration=%1"
if [%Configuration%]==[] set Configuration=Debug
rmdir /s /q build\VStudio\build
del /f build\VStudio\UpgradeLog*.htm
del /f build\VStudio\driver.ddf

:: set WINFSP_DEVENV_OPTS="--m:1"
tools\build.bat %Configuration%
popd

exit /b %ERRORLEVEL%
