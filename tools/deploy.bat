@echo off

setlocal
setlocal EnableDelayedExpansion

set Config=Debug
set Suffix=x64
set Deploy=C:\Deploy\winfsp
set Target=Win10DBG
set Chkpnt=docker+winfsp
set CImage=mcr.microsoft.com/windows/servercore:1909
if not X%1==X set Target=%1
if not X%2==X set Chkpnt=%2

(
    echo regsvr32 /s winfsp-x64.dll
) > %~dp0..\build\VStudio\build\%Config%\deploy-setup.bat

(
    echo docker run -it --rm --isolation=process -v%Deploy%:%Deploy%:RO %CImage% cmd.exe
) > %~dp0..\build\VStudio\build\%Config%\docker-run.bat

set Files=
for %%f in (
    %~dp0..\build\VStudio\build\%Config%\
        winfsp-%Suffix%.sys
        winfsp-%Suffix%.dll
        winfsp-tests-%Suffix%.exe
        memfs-%Suffix%.exe
        deploy-setup.bat
        docker-run.bat
    ) do (
    set File=%%~f
    if [!File:~-1!] == [\] (
        set Dir=!File!
    ) else (
        if not [!Files!] == [] set Files=!Files!,
        set Files=!Files!'!Dir!!File!'
    )
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "& '%~dp0deploy.ps1' -Name '%Target%' -CheckpointName '%Chkpnt%' -Files !Files! -Destination '%Deploy%'"
