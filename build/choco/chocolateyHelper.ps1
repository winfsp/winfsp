$ErrorActionPreference = 'Stop';

$packageName = 'winfsp'
$softwareName = 'WinFsp*'
$installerType = 'msi'
$silentArgs = '/qn /norestart'
$validExitCodes = @(0, 3010, 1605, 1614, 1641)

[array]$key = Get-UninstallRegistryKey -SoftwareName $softwareName

if ($key.Count -eq 1) {
    $key | % {
        # The Product Code GUID is all that should be passed for MSI, and very
        # FIRST, because it comes directly after /x, which is already set in the
        # Uninstall-ChocolateyPackage msiargs (facepalm).
        $silentArgs = "$($_.PSChildName) $silentArgs"

        # Don't pass anything for file, it is ignored for msi (facepalm number 2)
        # Alternatively if you need to pass a path to an msi, determine that and
        # use it instead of the above in silentArgs, still very first
        $file = ''

        Uninstall-ChocolateyPackage `
            -PackageName $packageName `
            -FileType $installerType `
            -SilentArgs "$silentArgs" `
            -ValidExitCodes $validExitCodes `
            -File "$file"
    }
} elseif ($key.Count -eq 0) {
    # Write-Warning "$packageName is not installed"
} elseif ($key.Count -gt 1) {
    Write-Warning "Too many matching packages found! Package may not be uninstalled."
    Write-Warning "Please alert package maintainer the following packages were matched:"
    $key | % {Write-Warning "- $_"}
}
