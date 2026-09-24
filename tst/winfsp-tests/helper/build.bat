@echo off

setlocal

cd %~dp0

call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x64
cl /Fewinfsp-tests-helper-x64.exe /MT /W2 winfsp-tests-helper.c kernel32.lib shell32.lib /link /subsystem:console /nodefaultlib
cl /Fewinfsp-tests-helper-dll-x64.dll /LD /MT /W2 winfsp-tests-helper-dll.c kernel32.lib /link /subsystem:windows
del *.obj *.lib *.exp

call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x86
cl /Fewinfsp-tests-helper-x86.exe /MT /W2 winfsp-tests-helper.c kernel32.lib shell32.lib /link /subsystem:console
cl /Fewinfsp-tests-helper-dll-x86.dll /LD /MT /W2 winfsp-tests-helper-dll.c kernel32.lib /link /subsystem:windows
del *.obj *.lib *.exp
