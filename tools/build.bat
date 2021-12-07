@echo off

setlocal
setlocal EnableDelayedExpansion

call "%~dp0myinfo.bat"

set MsiName="%MyProductName% - %MyDescription%"
set CrossCert="%~dp0DigiCert High Assurance EV Root CA.crt"
set Issuer="DigiCert"
set Subject="%MyCompanyName%"

set Configuration=Release
set SignedPackage=

if not X%1==X set Configuration=%1
if not X%2==X set SignedPackage=%2

echo Configuration=%Configuration%
echo:

if X%~nx0==Xbuild-choco.bat (
    cd %~dp0..\build\VStudio
    goto :choco
)

call "%~dp0vcvarsall.bat" x64

if not X%SignedPackage%==X (
    if not exist "%~dp0..\build\VStudio\build\%Configuration%\%MyProductFileName%-*.msi" (echo previous build not found >&2 & exit /b 1)
    if not exist "%SignedPackage%" (echo signed package not found >&2 & exit /b 1)
    del "%~dp0..\build\VStudio\build\%Configuration%\%MyProductFileName%-*.msi"
    if exist "%~dp0..\build\VStudio\build\%Configuration%\winfsp.*.nupkg" del "%~dp0..\build\VStudio\build\%Configuration%\winfsp.*.nupkg"
    for /R "%SignedPackage%" %%f in (*.sys) do (
        copy "%%f" "%~dp0..\build\VStudio\build\%Configuration%" >nul
    )
)

cd %~dp0..\build\VStudio
set signfail=0

if X%SignedPackage%==X (
    if exist build\ for /R build\ %%d in (%Configuration%) do (
        if exist "%%d" rmdir /s/q "%%d"
    )

    devenv winfsp.sln /build "%Configuration%|ARM64"
    if errorlevel 1 goto fail
    devenv winfsp.sln /build "%Configuration%|x64"
    if errorlevel 1 goto fail
    devenv winfsp.sln /build "%Configuration%|x86"
    if errorlevel 1 goto fail

    for %%f in (build\%Configuration%\%MyProductFileName%-a64.sys build\%Configuration%\%MyProductFileName%-x64.sys build\%Configuration%\%MyProductFileName%-x86.sys) do (
        signtool sign /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha1 /t http://timestamp.digicert.com %%f
        if errorlevel 1 set /a signfail=signfail+1
        signtool sign /as /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha256 /tr http://timestamp.digicert.com /td sha256 %%f
        if errorlevel 1 set /a signfail=signfail+1
    )

    pushd build\%Configuration%
    echo .OPTION EXPLICIT >driver.ddf
    echo .Set CabinetFileCountThreshold=0 >>driver.ddf
    echo .Set FolderFileCountThreshold=0 >>driver.ddf
    echo .Set FolderSizeThreshold=0 >>driver.ddf
    echo .Set MaxCabinetSize=0 >>driver.ddf
    echo .Set MaxDiskFileCount=0 >>driver.ddf
    echo .Set MaxDiskSize=0 >>driver.ddf
    echo .Set CompressionType=MSZIP >>driver.ddf
    echo .Set Cabinet=on >>driver.ddf
    echo .Set Compress=on >>driver.ddf
    echo .Set CabinetNameTemplate=driver.cab >>driver.ddf
    echo .Set DiskDirectory1=. >>driver.ddf
    echo .Set DestinationDir=a64 >>driver.ddf
    echo driver-a64.inf >>driver.ddf
    echo %MyProductFileName%-a64.sys >>driver.ddf
    echo .Set DestinationDir=x64 >>driver.ddf
    echo driver-x64.inf >>driver.ddf
    echo %MyProductFileName%-x64.sys >>driver.ddf
    echo .Set DestinationDir=x86 >>driver.ddf
    echo driver-x86.inf >>driver.ddf
    echo %MyProductFileName%-x86.sys >>driver.ddf
    makecab /F driver.ddf
    signtool sign /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha256 /tr http://timestamp.digicert.com /td sha256 driver.cab
    if errorlevel 1 set /a signfail=signfail+1
    popd
)

devenv winfsp.sln /build "Installer.%Configuration%|x86"
if errorlevel 1 goto fail

for %%f in (build\%Configuration%\%MyProductFileName%-*.msi) do (
    signtool sign /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha1 /t http://timestamp.digicert.com /d %MsiName% %%f
    if errorlevel 1 set /a signfail=signfail+1
    REM signtool sign /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha256 /tr http://timestamp.digicert.com /td sha256 /d %MsiName% %%f
    REM if errorlevel 1 set /a signfail=signfail+1
)

if not %signfail%==0 echo SIGNING FAILED! The product has been successfully built, but not signed.

set Version=
for %%f in (build\%Configuration%\%MyProductFileName%-*.msi) do set Version=%%~nf
set Version=!Version:%MyProductFileName%-=!
if X%SignedPackage%==X (
    pushd build\%Configuration%
    powershell -command "Compress-Archive -Path winfsp-tests-*.exe,..\..\..\..\License.txt,..\..\..\..\tst\winfsp-tests\README.md -DestinationPath winfsp-tests-!Version!.zip"
    if errorlevel 1 goto fail
    popd
)

:choco
if not exist "build\%Configuration%\%MyProductFileName%-*.msi" (echo installer msi not found >&2 & exit /b 1)
if not X!MyProductName!==XWinFsp (echo skipping choco build for !MyProductName! >&2 & exit /b 0)
set Version=
for %%f in (build\%Configuration%\%MyProductFileName%-*.msi) do set Version=%%~nf
set Version=!Version:%MyProductFileName%-=!
set PackageVersion=!Version!
if not X!MyProductStage!==XGold (
    set PackageVersion=!Version!-pre
)
where /q choco.exe
if %ERRORLEVEL% equ 0 (
    copy ..\choco\* build\%Configuration%
    copy ..\choco\LICENSE.TXT /B + ..\..\License.txt /B build\%Configuration%\LICENSE.txt /B
    certutil -hashfile build\%Configuration%\%MyProductFileName%-!Version!.msi SHA256 >>build\%Configuration%\VERIFICATION.txt
    choco pack build\%Configuration%\winfsp.nuspec --version=!PackageVersion! --outputdirectory=build\%Configuration% MsiVersion=!Version!
    if errorlevel 1 goto fail
)

exit /b 0

:fail
exit /b 1
