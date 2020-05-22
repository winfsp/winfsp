@echo off

setlocal
setlocal EnableDelayedExpansion

set Config=Debug
set Suffix=x64
set Deploy=C:\Deploy\winfsp
set Target=Win10DBG
if not X%1==X set Target=%1

set Files=
for %%f in (winfsp-%Suffix%.sys winfsp-%Suffix%.dll winfsp-tests-%Suffix%.exe memfs-%Suffix%.exe) do (
    if [!Files!] == [] (
        set Files='%~dp0..\build\VStudio\build\%Config%\%%f'
    ) else (
        set Files=!Files!,'%~dp0..\build\VStudio\build\%Config%\%%f'
    )
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "& '%~dp0deploy.ps1' -Name '%Target%' -Files !Files! -Destination '%Deploy%'"
