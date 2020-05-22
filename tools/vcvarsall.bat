@echo off

set vcvarsall="%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat"
set vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist %vswhere% (
    for /f "usebackq tokens=*" %%i in (`%vswhere% -find VC\**\vcvarsall.bat`) do (
        set vcvarsall="%%i"
    )
)
call %vcvarsall% %*
