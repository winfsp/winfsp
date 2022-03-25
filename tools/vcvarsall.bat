@echo off

set vcvarsall=""

set vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist %vswhere% (
    for /f "usebackq tokens=*" %%i in (`%vswhere% -version [15.0^,17.0^) -find VC\**\vcvarsall.bat`) do (
        set vcvarsall="%%i"
    )
)

if not exist %vcvarsall% (
    if exist %vswhere% (
        for /f "usebackq tokens=*" %%i in (`%vswhere% -version [15.0^,17.0^) -property installationPath`) do (
            set vcvarsall="%%i\VC\Auxiliary\Build\vcvarsall.bat"
        )
    )
)

if not exist %vcvarsall% (
    set vcvarsall="%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat"
)

call %vcvarsall% %*
