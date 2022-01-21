@echo off

for /F "tokens=2,3,4 delims=<>" %%a in (%~dp0..\build\VStudio\build.version.props) do (
    if "/%%a"=="%%c" (
        if "%%a"=="MyProductName" (
            set MyProductName=%%b
        ) else if "%%a"=="MyProductFileName" (
            set MyProductFileName=%%b
        ) else if "%%a"=="MyDescription" (
            set MyDescription=%%b
        ) else if "%%a"=="MyCompanyName" (
            set MyCompanyName=%%b
        ) else if "%%a"=="MyProductVersion" (
            set MyProductVersion=%%b
        ) else if "%%a"=="MyProductStage" (
            set MyProductStage=%%b
        )
    )
)

echo MyProductName=%MyProductName%
echo MyProductFileName=%MyProductFileName%
echo MyDescription=%MyDescription%
echo MyCompanyName=%MyCompanyName%
echo MyProductVersion=%MyProductVersion%
echo MyProductStage=%MyProductStage%
