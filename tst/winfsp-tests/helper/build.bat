@echo off

setlocal

cd %~dp0

call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x64
cl /Fewinfsp-tests-helper-x64.exe /MT /W2 winfsp-tests-helper.c kernel32.lib shell32.lib /link /subsystem:console /nodefaultlib
del *.obj

call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x86
cl /Fewinfsp-tests-helper-x86.exe /MT /W2 winfsp-tests-helper.c kernel32.lib shell32.lib /link /subsystem:console
del *.obj
