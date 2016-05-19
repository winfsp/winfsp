@echo off

set Configuration=Release
set MsiName="WinFsp - Windows File System Proxy"
set CrossCert="%~dp0DigiCert High Assurance EV Root CA.crt"
set Issuer="DigiCert"
set Subject="Navimatics Corporation"

call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat"

if not X%1==X set Configuration=%1

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
