@echo off

set vcvarsall="%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat"
set vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist %vswhere% (
    for /f "usebackq tokens=*" %%i in (`%vswhere% -latest -find VC\**\vcvarsall.bat`) do (
        if exist "%%i" (
            set vcvarsall="%%i"
        ) else (
            for /f "usebackq tokens=*" %%i in (`%vswhere% -latest -property installationPath`) do (
                set vcvarsall="%%i\VC\Auxiliary\Build\vcvarsall.bat"
            )
        )
    )
)
call %vcvarsall% %*
