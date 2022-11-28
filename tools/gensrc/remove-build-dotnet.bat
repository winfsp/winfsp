@echo off

setlocal
setlocal EnableDelayedExpansion

if "%1"=="" (
    cd %~dp0..\..
) else (
    cd "%1"
)


if exist winfsp.sln (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "& '%~dp0remove-sln-project.ps1' -Path '%cd%\winfsp.sln' -Match '*dotnet*'
) else (
    echo winfsp.sln not found in %cd%
)
