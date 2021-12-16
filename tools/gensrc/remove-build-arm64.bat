@echo off

setlocal
setlocal EnableDelayedExpansion

if "%1"=="" (
    cd %~dp0..\..
) else (
    cd "%1"
)

for /r %%f in (*.vcxproj) do (
    echo %%f
    powershell -NoProfile -ExecutionPolicy Bypass -Command "& '%~dp0remove-vcxproj-config.ps1' -Path '%%f' -ProjectConfiguration '|ARM64'
)

for /r %%f in (*.sln) do (
    echo %%f
    findstr /V /C:"|ARM64" "%%f" > "%%f.new"
    move /Y "%%f.new" "%%f" >nul
)
