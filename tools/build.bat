@echo off

set Configuration=Release
set CrossCert="%~dp0DigiCert High Assurance EV Root CA.crt"
set Issuer="DigiCert"
set Subject="Navimatics Corporation"

call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat"

if not X%1==X set Configuration=%1

cd %~dp0..\build\VStudio
if exist build\ del /s/q build >nul

devenv winfsp.sln /build "%Configuration%|x64"
devenv winfsp.sln /build "%Configuration%|x86"

for %%f in (build\%Configuration%\winfsp-x64.sys build\%Configuration%\winfsp-x86.sys) do (
	signtool sign /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha1 /t http://timestamp.digicert.com %%f
	signtool sign /as /ac %CrossCert% /i %Issuer% /n %Subject% /fd sha256 /tr http://timestamp.digicert.com /td sha256 %%f
)

devenv winfsp.sln /build "Installer.%Configuration%|x86"
