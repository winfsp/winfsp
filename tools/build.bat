@echo off

setlocal

set Configuration=Release
set MsiName="WinFsp - Windows File System Proxy"
set CrossCert="%~dp0DigiCert High Assurance EV Root CA.crt"
set Issuer="DigiCert"
set Subject="Navimatics Corporation"

if not X%1==X set Configuration=%1

call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x64

cd %~dp0..\build\VStudio
if exist build\ del /s/q build >nul

devenv winfsp.sln /build "%Configuration%|x64"
if errorlevel 1 goto fail
devenv winfsp.sln /build "%Configuration%|x86"
if errorlevel 1 goto fail

set signfail=0
for %%f in (build\%Configuration%\winfsp-x64.sys build\%Configuration%\winfsp-x86.sys) do (
	signtool sign /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha1 /t http://timestamp.digicert.com %%f
	if errorlevel 1 set /a signfail=signfail+1
	signtool sign /as /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha256 /tr http://timestamp.digicert.com /td sha256 %%f
	if errorlevel 1 set /a signfail=signfail+1
)

pushd build\%Configuration%
for %%a in (x64 x86) do (
	echo .OPTION EXPLICIT >driver-%%a.ddf
	echo .Set CabinetFileCountThreshold=0 >>driver-%%a.ddf
	echo .Set FolderFileCountThreshold=0 >>driver-%%a.ddf
	echo .Set FolderSizeThreshold=0 >>driver-%%a.ddf
	echo .Set MaxCabinetSize=0 >>driver-%%a.ddf
	echo .Set MaxDiskFileCount=0 >>driver-%%a.ddf
	echo .Set MaxDiskSize=0 >>driver-%%a.ddf
	echo .Set CompressionType=MSZIP >>driver-%%a.ddf
	echo .Set Cabinet=on >>driver-%%a.ddf
	echo .Set Compress=on >>driver-%%a.ddf
	echo .Set CabinetNameTemplate=driver-%%a.cab >>driver-%%a.ddf
	echo .Set DiskDirectory1=. >>driver-%%a.ddf
	echo .Set DestinationDir=winfsp >>driver-%%a.ddf
	echo driver-%%a.inf >>driver-%%a.ddf
	echo winfsp-%%a.sys >>driver-%%a.ddf
	makecab /F driver-%%a.ddf
)
popd

devenv winfsp.sln /build "Installer.%Configuration%|x86"
if errorlevel 1 goto fail

for %%f in (build\%Configuration%\winfsp-*.msi) do (
	signtool sign /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha1 /t http://timestamp.digicert.com /d %MsiName% %%f
	if errorlevel 1 set /a signfail=signfail+1
	REM signtool sign /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha256 /tr http://timestamp.digicert.com /td sha256 /d %MsiName% %%f
	REM if errorlevel 1 set /a signfail=signfail+1
)

if not %signfail%==0 echo SIGNING FAILED! The product has been successfully built, but not signed.

exit /b 0

:fail
exit /b 1
