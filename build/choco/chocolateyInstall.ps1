$ErrorActionPreference = 'Stop';

$toolsDir = "$(Split-Path -parent $MyInvocation.MyCommand.Definition)"
. "$toolsdir\chocolateyHelper.ps1"

$packageArgs = @{
    packageName     = 'winfsp'
    fileType        = 'msi'
    file            = @(Get-ChildItem $toolsDir -filter winfsp-*.msi)[0].FullName
    silentArgs      = "/qn /norestart INSTALLLEVEL=1000"
    validExitCodes  = @(0, 3010, 1641)
}
Install-ChocolateyInstallPackage @packageArgs
Remove-Item -Force $packageArgs.file
