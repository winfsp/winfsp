@echo off

setlocal
setlocal EnableDelayedExpansion

set Config=Debug
set Suffix=x64
set Deploy=C:\Deploy\winfsp
set Target=Win10DBG
set Chkpnt=winfsp
if not X%1==X set Target=%1
if not X%2==X set Chkpnt=%2

set Files=
for %%f in (winfsp-%Suffix%.sys winfsp-%Suffix%.dll winfsp-tests-%Suffix%.exe memfs-%Suffix%.exe) do (
    if [!Files!] == [] (
        set Files='%~dp0..\build\VStudio\build\%Config%\%%f'
    ) else (
        set Files=!Files!,'%~dp0..\build\VStudio\build\%Config%\%%f'
    )
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "& '%~dp0deploy.ps1' -Name '%Target%' -CheckpointName '%Chkpnt%' -Files !Files! -Destination '%Deploy%'"
