$ErrorActionPreference = 'Stop';

$toolsDir           = "$(Split-Path -parent $MyInvocation.MyCommand.Definition)"
$fileLocation       = @(Get-ChildItem $toolsDir -filter winfsp-*.msi)[0].FullName

$packageArgs = @{
    packageName     = 'winfsp'
    fileType        = 'msi'
    file            = $fileLocation
    silentArgs      = "/qn /norestart INSTALLLEVEL=1000"
    validExitCodes  = @(0, 3010, 1641)
}

Install-ChocolateyInstallPackage @packageArgs

Remove-Item -Force $packageArgs.file
