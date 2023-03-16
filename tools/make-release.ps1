param (
    [string]$StateFile = "release.state"
)

function Write-Stdout ($Message) {
    [System.Console]::WriteLine($Message)
}

function Write-Stderr ($Message) {
    [System.Console]::Error.WriteLine($Message)
}

function Git-Describe {
    $GitDesc = git describe --long --dirty
    if ($GitDesc -match "^(.+)-([0-9]+)-g([0-9A-Fa-f]+)-?(dirty)?$") {
        $matches[1]
        $matches[2]
        $matches[3]
        $matches[4]
    }
}

function Git-Dirty {
    $Dirty = $False
    git status --porcelain=v2 2>$null | ForEach-Object {
        $Dirty = $True
    }
    return $Dirty
}

function Git-LogGrep ($Tag) {
    git log -n 1 --grep $Tag
}

function Get-ReleaseInfo ($Tag) {
    $Found = 0
    $ReleaseInfo = [PSCustomObject]@{
        Tag = ""
        ProductVersion = ""
        Prerelease = $False
        PreviousTag = ""
        PreviousProductVersion = ""
        Text = @()
    }
    foreach ($Line in Get-Content "$ProjectRoot\Changelog.md") {
        if ($Line -match "^## (v[^ ]+) *\(([^)]+)\)") {
            if ($Found) {
                $Found = 2
                $PreviousTag = $matches[1]
                $PreviousProductVersion = $matches[2]
                if ($PreviousTag -match "^v[0-9.]+$") {
                    $ReleaseInfo.PreviousTag = $PreviousTag
                    $ReleaseInfo.PreviousProductVersion = $PreviousProductVersion
                    break
                }
            } elseif ($Tag -eq $matches[1]) {
                $Found = 1
                $ReleaseInfo.Tag = $matches[1]
                $ReleaseInfo.ProductVersion = $matches[2]
                $ReleaseInfo.Prerelease = !($ReleaseInfo.Tag -match "^v[0-9.]+$")
            }
        } elseif (1 -eq $Found) {
            $ReleaseInfo.Text += $Line
        }
    }
    if ($Found) {
        return $ReleaseInfo
    }
}

function Get-HwapiCredentials {
    $Credentials = (& "$ProjectRoot\tools\wincred.ps1" get "Hardware Dashboard API")
    if ($Credentials) {
        try { $Credentials = ConvertFrom-Json $Credentials[1] } catch {;}
    }
    if ($Credentials -and $Credentials.TenantId -and $Credentials.ClientId -and $Credentials.ClientId) {
        return $Credentials
    }
}

function Start-Sdcm {
    Start-Job -ArgumentList $args -ScriptBlock {
        $env:SDCM_CREDS_TENANTID = $using:HwapiCredentials.TenantId
        $env:SDCM_CREDS_CLIENTID = $using:HwapiCredentials.ClientId
        $env:SDCM_CREDS_KEY = $using:HwapiCredentials.Key
        $env:SDCM_CREDS_URL = "https://manage.devcenter.microsoft.com"
        $env:SDCM_CREDS_URLPREFIX = "v2.0/my"
        & "$using:ProjectRoot\..\winfsp.sdcm\SurfaceDevCenterManager\bin\Debug\sdcm.exe" -creds envonly @args
    } | Receive-Job -Wait -AutoRemoveJob
}

function Get-FileVersion ($FileName) {
    return [System.Diagnostics.FileVersionInfo]::GetVersionInfo($FileName).FileVersion
}

function Task ($ScriptBlock) {
    $Name = (Get-PSCallStack)[1].FunctionName
    if ($State -contains $Name) {
        return
    }

    Write-Stdout $Name
    Invoke-Command -ScriptBlock $ScriptBlock
    Write-Stdout "$Name COMPLETE"

    Add-Content $StateFile -Value $Name

    exit 0
}

function Check-Prerequisites {
    $Name = (Get-PSCallStack)[0].FunctionName

    # check git.exe
    if (!(Get-Command "git.exe" -ErrorAction SilentlyContinue)) {
        Write-Stderr "error: cannot find git.exe"
        exit 1
    }

    # check scdm.exe
    if (!(Get-Command "$ProjectRoot\..\winfsp.sdcm\SurfaceDevCenterManager\bin\Debug\sdcm.exe" -ErrorAction SilentlyContinue)) {
        Write-Stderr "error: cannot find sdcm.exe"
        exit 1
    }

    # check gh.exe
    if (!(Get-Command "gh.exe" -ErrorAction SilentlyContinue)) {
        Write-Stderr "error: cannot find gh.exe"
        exit 1
    }

    # check choco.exe
    if (!(Get-Command "choco.exe" -ErrorAction SilentlyContinue)) {
        Write-Stderr "error: cannot find choco.exe"
        exit 1
    }

    # check git tag
    $Tag, $CommitCount, $Commit, $_ = Git-Describe
    if ("0" -ne $CommitCount) {
        Write-Stderr "error: cannot find clean git tag"
        exit 1
    }

    # check release info
    $script:ReleaseInfo = Get-ReleaseInfo $Tag
    if (!$script:ReleaseInfo) {
        Write-Stderr "error: cannot get release info for tag $Tag"
        exit 1
    }

    # check winfsp.sym
    if (!(Test-Path "$ProjectRoot\..\winfsp.sym" -ErrorAction SilentlyContinue)) {
        Write-Stderr "error: cannot find winfsp.sym repository"
        exit 1
    }

    # check winfsp.sym git status
    Push-Location "$ProjectRoot\..\winfsp.sym"
    $SymDirty = Git-Dirty
    Pop-Location
    if ($SymDirty) {
        Write-Stderr "error: winfsp.sym repository is dirty"
        exit 1
    }

    # check hardware dashboard api credentials
    $script:HwapiCredentials = Get-HwapiCredentials
    if (!$script:HwapiCredentials) {
        Write-Stderr "error: cannot get Hardware Dashboard API credentials"
        Write-Stderr '    The expected format of the credentials is as follows:'
        Write-Stderr '    TargetName: Hardware Dashboard API'
        Write-Stderr '    UserName: Credentials'
        Write-Stderr '    Password: {"TenantId":"TENANTID","ClientId":"CLIENTID","Key":"KEY"}'
        exit 1
    }

    if ($State -contains $Name) {
        if (!($State -contains "$Tag-$CommitCount-g$Commit")) {
            Write-Stderr "error: invalid state for tag $Tag"
            exit 1
        }
    } else {
        Add-Content $StateFile -Value $Name
        Add-Content $StateFile -Value "$Tag-$CommitCount-g$Commit"
    }
}

function Check-Assets {
    # check driver.cab
    if (!(Test-Path "$ProjectRoot\build\VStudio\build\Release\driver.cab" -ErrorAction SilentlyContinue)) {
        Write-Stderr "error: cannot find driver.cab"
        exit 1
    }

    # check winfsp.msi
    if (!(Test-Path "$ProjectRoot\build\VStudio\build\Release\winfsp*.msi" -ErrorAction SilentlyContinue)) {
        Write-Stderr "error: cannot find winfsp*.msi"
        exit 1
    }

    # check winfsp-tests.zip
    if (!(Test-Path "$ProjectRoot\build\VStudio\build\Release\winfsp-tests*.zip" -ErrorAction SilentlyContinue)) {
        Write-Stderr "error: cannot find winfsp-tests*.zip"
        exit 1
    }

    # check winfsp.net.nupkg
    if (!(Test-Path "$ProjectRoot\build\VStudio\build\Release\winfsp.net.*.nupkg" -ErrorAction SilentlyContinue)) {
        Write-Stderr "error: cannot find winfsp.net.*.nupkg"
        exit 1
    }

    # check winfsp.nupkg
    if (!(Test-Path "$ProjectRoot\build\VStudio\build\Release\winfsp.*.nupkg" -ErrorAction SilentlyContinue)) {
        Write-Stderr "error: cannot find winfsp.*.nupkg"
        exit 1
    }

    # check winfsp signature
    if ("Valid" -ne (Get-AuthenticodeSignature "$ProjectRoot\build\VStudio\build\Release\winfsp-x64.sys" -ErrorAction SilentlyContinue).Status) {
        Write-Stderr "error: invalid winfsp signature"
        exit 1
    }
}

function Build-AssetsPhase1 {
    Task -ScriptBlock {
        Push-Location "$ProjectRoot"
        tools\build.bat Release
        if ($LastExitCode -ne 0) {
            Write-Stderr "error: cannot build assets"
            exit 1
        }
        Pop-Location

        Write-Stdout @"

Assets have been built but are not properly signed.
Signable assets are ready for submission to the hardware dashboard.

"@
    }
}

function Submit-AssetsToHwapi {
    Task -ScriptBlock {
        Check-Assets

        $MsiFile = Resolve-Path "$ProjectRoot\build\VStudio\build\Release\winfsp*.msi"
        $MsiName = Split-Path -Leaf $MsiFile
        if ($MsiName -match "winfsp-(.+)\.msi") {
            $Version = $matches[1]
        }
        if (!$Version) {
            Write-Stderr "error: cannot determine version for winfsp.msi"
            exit 1
        }

        $DocRequestedSignatures = @()
        $Documentation = Invoke-WebRequest -Uri "https://raw.githubusercontent.com/MicrosoftDocs/windows-driver-docs/staging/windows-driver-docs-pr/dashboard/get-product-data.md"
        $Documentation = $Documentation.Content
        $List = $false
        foreach ($Line in $Documentation -Split "`n") {
            if ($Line -match "^### List of Operating System Codes") {
                $List = $true
            } elseif ($Line -match "^###") {
                $List = $false
            } elseif ($List -and $Line -cmatch "\| *(WINDOWS_v100_[^| ]+) *\|") {
                $DocRequestedSignatures += $matches[1]
            }
        }
        if ($DocRequestedSignatures.length -lt 32) {
            Write-Stderr "error: cannot determine signatures to request"
            Write-Stderr '    Does the document at the URL below still contain a "List of Operating System Codes":'
            Write-Stderr "    https://raw.githubusercontent.com/MicrosoftDocs/windows-driver-docs/staging/windows-driver-docs-pr/dashboard/get-product-data.md"
            exit 1
        }

        # start with the base signatures and add any new ones found in the docs
        $RequestedSignatures = @(
            "WINDOWS_v100_TH2_FULL"
            "WINDOWS_v100_X64_TH2_FULL"
            "WINDOWS_v100_RS1_FULL"
            "WINDOWS_v100_X64_RS1_FULL"
            "WINDOWS_v100_RS2_FULL"
            "WINDOWS_v100_X64_RS2_FULL"
            "WINDOWS_v100_RS3_FULL"
            "WINDOWS_v100_X64_RS3_FULL"
            "WINDOWS_v100_ARM64_RS3_FULL"
            "WINDOWS_v100_RS4_FULL"
            "WINDOWS_v100_X64_RS4_FULL"
            "WINDOWS_v100_ARM64_RS4_FULL"
            "WINDOWS_v100_RS5_FULL"
            "WINDOWS_v100_X64_RS5_FULL"
            "WINDOWS_v100_ARM64_RS5_FULL"
            "WINDOWS_v100_19H1_FULL"
            "WINDOWS_v100_X64_19H1_FULL"
            "WINDOWS_v100_ARM64_19H1_FULL"
            "WINDOWS_v100_VB_FULL"
            "WINDOWS_v100_X64_VB_FULL"
            "WINDOWS_v100_ARM64_VB_FULL"
            "WINDOWS_v100_X64_CO_FULL"
            "WINDOWS_v100_ARM64_CO_FULL"
            "WINDOWS_v100_X64_NI_FULL"
            "WINDOWS_v100_ARM64_NI_FULL"
        )
        foreach ($Signature in $DocRequestedSignatures) {
            if ($RequestedSignatures -contains $Signature) {
                continue
            }
            if ($Signature.Contains("_SERVER_")) {
                continue
            }
            if ($Signature -eq "WINDOWS_v100_TH1_FULL") {
                continue
            }
            if ($Signature -eq "WINDOWS_v100_X64_TH1_FULL") {
                continue
            }
            $RequestedSignatures += $Signature
            Write-Stdout "New doc signature: $Signature"
        }

        $CreateProduct = @{
            createType = "product"
            createProduct = @{
                productName = "winfsp-$Version"
                testHarness = "attestation"
                deviceType = "internalExternal"
                requestedSignatures = $RequestedSignatures
                # deviceMetaDataIds = $null
                # firmwareVersion = "0"
                # isTestSign = $false
                # isFlightSign = $false
                # markettingNames = $null
                # selectedProductTypes = $null
                # additionalAttributes = $null
            }
        }
        $CreateSubmission = @{
            createType = "submission"
            createSubmission = @{
                name = "winfsp-$Version"
                type = "initial"
            }
        }
        New-Item "$ProjectRoot\build\VStudio\build\Release\hwapi" -Type Directory -ErrorAction SilentlyContinue >$null
        ConvertTo-Json $CreateProduct | Out-File -Encoding Ascii "$ProjectRoot\build\VStudio\build\Release\hwapi\CreateProduct.json"
        ConvertTo-Json $CreateSubmission | Out-File -Encoding Ascii "$ProjectRoot\build\VStudio\build\Release\hwapi\CreateSubmission.json"

        Start-Sdcm -create "$ProjectRoot\build\VStudio\build\Release\hwapi\CreateProduct.json" | Tee-Object -Variable Output
        if ($LastExitCode -ne 0) {
            Write-Stderr "error: cannot create product on hardware dashboard"
            exit 1
        }
        if (-not ([string]$Output -match "--- Product: (\d+)")) {
            Write-Stderr "error: cannot get product id from hardware dashboard"
            exit 1
        }
        $ProductId = $matches[1]

        Start-Sdcm -create "$ProjectRoot\build\VStudio\build\Release\hwapi\CreateSubmission.json" -productid $ProductId  | Tee-Object -Variable Output
        if ($LastExitCode -ne 0) {
            Write-Stderr "error: cannot create submission on hardware dashboard"
            exit 1
        }
        if (-not ([string]$Output -match "---- Submission: (\d+)")) {
            Write-Stderr "error: cannot get submission id from hardware dashboard"
            exit 1
        }
        $SubmissionId = $matches[1]

        Set-Content "$ProjectRoot\build\VStudio\build\Release\hwapi\ProductId" -Value $ProductId
        Set-Content "$ProjectRoot\build\VStudio\build\Release\hwapi\SubmissionId" -Value $SubmissionId

        Write-Stdout @"

Product submission has been prepared on hardware dashboard.

"@
    }
}

function Upload-AssetsToHwapi {
    Task -ScriptBlock {
        Check-Assets

        $ProductId = Get-Content "$ProjectRoot\build\VStudio\build\Release\hwapi\ProductId"
        $SubmissionId = Get-Content "$ProjectRoot\build\VStudio\build\Release\hwapi\SubmissionId"

        Start-Sdcm -upload "$ProjectRoot\build\VStudio\build\Release\driver.cab" -productid $ProductId -submissionid $SubmissionId
        if ($LastExitCode -ne 0) {
            Write-Stderr "error: cannot upload driver.cab to hardware dashboard"
            exit 1
        }

        Start-Sdcm -commit -productid $ProductId -submissionid $SubmissionId
        if ($LastExitCode -ne 0) {
            Write-Stderr "error: cannot commit submission to hardware dashboard"
            exit 1
        }

        Write-Stdout @"

Signable assets have been uploaded to the hardware dashboard.

"@
    }
}

function Download-AssetsFromHwapi {
    Task -ScriptBlock {
        Check-Assets

        $ProductId = Get-Content "$ProjectRoot\build\VStudio\build\Release\hwapi\ProductId"
        $SubmissionId = Get-Content "$ProjectRoot\build\VStudio\build\Release\hwapi\SubmissionId"

        Remove-Item -Force "$ProjectRoot\build\VStudio\build\Release\hwapi\Signed-$SubmissionId.zip" -ErrorAction SilentlyContinue
        Remove-Item -Recurse -Force "$ProjectRoot\build\VStudio\build\Release\hwapi\drivers" -ErrorAction SilentlyContinue

        Start-Sdcm -download "$ProjectRoot\build\VStudio\build\Release\hwapi\Signed-$SubmissionId.zip" -productid $ProductId -submissionid $SubmissionId
        if ($LastExitCode -ne 0) {
            Write-Stderr "error: cannot download signed drivers from hardware dashboard"
            exit 1
        }

        if (!(Test-Path "$ProjectRoot\build\VStudio\build\Release\hwapi\Signed-$SubmissionId.zip" -ErrorAction SilentlyContinue)) {
            Write-Stderr "error: cannot download signed drivers from hardware dashboard"
            exit 1
        }

        $ExpandError = ""
        Expand-Archive "$ProjectRoot\build\VStudio\build\Release\hwapi\Signed-$SubmissionId.zip" -DestinationPath "$ProjectRoot\build\VStudio\build\Release\hwapi" -ErrorVariable ExpandError
        if ($ExpandError) {
            Write-Stderr "error: cannot expand signed drivers archive"
            exit 1
        }

        if (!(Test-Path "$ProjectRoot\build\VStudio\build\Release\hwapi\drivers" -ErrorAction SilentlyContinue)) {
            Write-Stderr "error: cannot expand signed drivers archive"
            exit 1
        }

        Write-Stdout @"

Signable assets have been downloaded and can be used to complete the build.

"@
    }
}

function Build-AssetsPhase2 {
    Task -ScriptBlock {
        Check-Assets

        # check signed drivers folder
        if (!(Test-Path "$ProjectRoot\build\VStudio\build\Release\hwapi\drivers" -ErrorAction SilentlyContinue)) {
            Write-Stderr "error: cannot find hwapi\drivers"
            exit 1
        }

        $SignedPackage = Resolve-Path "$ProjectRoot\build\VStudio\build\Release\hwapi\drivers"

        $VerX64 = Get-FileVersion "$ProjectRoot\build\VStudio\build\Release\winfsp-x64.sys"
        if ($VerX64 -ne (Get-FileVersion "$SignedPackage\x64\winfsp-x64.sys")) {
            Write-Stderr "error: incompatible versions in hwapi\drivers"
            exit 1
        }

        Push-Location "$ProjectRoot"
        tools\build.bat Release $SignedPackage
        if ($LastExitCode -ne 0) {
            Write-Stderr "error: cannot build assets"
            exit 1
        }
        Pop-Location

        Write-Stdout @"

Assets have been built and signed. You may want to perform some smoke testing.

"@
    }
}

function Make-GitHubRelease {
    Task -ScriptBlock {
        Check-Assets

        $DownloadColor = "blue"
        $PrereleaseOpt = ""
        if ($ReleaseInfo.Prerelease) {
            $DownloadColor = "e52e4b"
            $PrereleaseOpt = "-p"
        }

        $MsiFile = Resolve-Path "$ProjectRoot\build\VStudio\build\Release\winfsp*.msi"
        $ZipFile = Resolve-Path "$ProjectRoot\build\VStudio\build\Release\winfsp-tests*.zip"
        $MsiName = Split-Path -Leaf $MsiFile
        $ZipName = Split-Path -Leaf $ZipFile
        $MsiHash = (Get-FileHash -Algorithm SHA256 $MsiFile).Hash
        $ZipHash = (Get-FileHash -Algorithm SHA256 $ZipFile).Hash

        if ($MsiName -match "winfsp-(.+)\.msi") {
            $Version = $matches[1]
        }

        $ReleaseNotes = @"
[![Download WinFsp](https://img.shields.io/badge/-Download%20WinFsp-$DownloadColor.svg?style=for-the-badge&labelColor=grey&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA0ODAgNDgwIj48cGF0aCBkPSJNMzg3LjAwMiAyMDEuMDAxQzM3Mi45OTggMTMyLjAwMiAzMTIuOTk4IDgwIDI0MCA4MGMtNTcuOTk4IDAtMTA3Ljk5OCAzMi45OTgtMTMyLjk5OCA4MS4wMDFDNDcuMDAyIDE2Ny4wMDIgMCAyMTcuOTk4IDAgMjgwYzAgNjUuOTk2IDUzLjk5OSAxMjAgMTIwIDEyMGgyNjBjNTUgMCAxMDAtNDUgMTAwLTEwMCAwLTUyLjk5OC00MC45OTYtOTYuMDAxLTkyLjk5OC05OC45OTl6TTIwOCAyNTJ2LTc2aDY0djc2aDY4TDI0MCAzNTIgMTQwIDI1Mmg2OHoiIGZpbGw9IiNmZmYiLz48L3N2Zz4=)](https://github.com/winfsp/winfsp/releases/download/$($ReleaseInfo.Tag)/winfsp-$Version.msi)

## CHANGES SINCE WINFSP $($ReleaseInfo.PreviousProductVersion)
$($ReleaseInfo.Text -join "`n")
<details>
<summary>
<b>BUILD HASHES (SHA256)</b>
<p/>
</summary>

- **``$MsiName``**: $MsiHash
- **``$ZipName``**: $ZipHash
</details>
"@

        gh release create $ReleaseInfo.Tag --draft --title "WinFsp $($ReleaseInfo.ProductVersion)" --notes "$ReleaseNotes" $PrereleaseOpt $MsiFile $ZipFile
        if ($LastExitCode -ne 0) {
            Write-Stderr "error: cannot create GitHub release"
            exit 1
        }

        Write-Stdout @"

Draft GitHub release for tag $($ReleaseInfo.Tag) has been created.
Publish the release on GitHub and update the VirusTotal link.

"@
    }
}

function Upload-Symbols {
    Task -ScriptBlock {
        Check-Assets

        # check winfsp.sym git log
        Push-Location "$ProjectRoot\..\winfsp.sym"
        $SymHasTag = Git-LogGrep "^$($ReleaseInfo.Tag)$"
        Pop-Location
        if ($SymHasTag) {
            Write-Stderr "warning: winfsp.sym repository already has commit for tag $($ReleaseInfo.Tag)"
        } else {
            Push-Location "$ProjectRoot\..\winfsp.sym"
            .\tools\symadd.ps1 ..\winfsp\build\VStudio\build\Release -PdbKind Private
            if ($LastExitCode -ne 0) {
                Write-Stderr "error: cannot add files to winfsp.sym repository"
                exit 1
            }
            git add .
            if ($LastExitCode -ne 0) {
                Write-Stderr "error: cannot add files to winfsp.sym repository staging area"
                exit 1
            }
            git commit -m $ReleaseInfo.Tag
            if ($LastExitCode -ne 0) {
                Write-Stderr "error: cannot commit files to winfsp.sym repository"
                exit 1
            }
            Pop-Location

            Write-Stdout @"

Commit for $($ReleaseInfo.Tag) symbols has been created.
Push the winfsp.sym repository to GitHub.

"@
        }
    }
}

function Make-NugetRelease {
    Task -ScriptBlock {
        Check-Assets

        Push-Location "$ProjectRoot\build\VStudio\build\Release"
        nuget push (Resolve-Path winfsp.net.[0-9]*.nupkg) -Source https://api.nuget.org/v3/index.json
        if ($LastExitCode -ne 0) {
            Write-Stderr "error: cannot push to Nuget"
            exit 1
        }
        Pop-Location

        Write-Stdout @"

Nuget release for $($ReleaseInfo.Tag) has been pushed.

"@
    }
}

function Make-ChocoRelease {
    Task -ScriptBlock {
        Check-Assets

        Push-Location "$ProjectRoot\build\VStudio\build\Release"
        choco push (Resolve-Path winfsp.[0-9]*.nupkg)
        if ($LastExitCode -ne 0) {
            Write-Stderr "error: cannot push to Chocolatey"
            exit 1
        }
        Pop-Location

        Write-Stdout @"

Chocolatey release for $($ReleaseInfo.Tag) has been pushed.

"@
    }
}

$ProjectRoot = Split-Path $PSScriptRoot
$StateFile = Join-Path $pwd $StateFile

Write-Stdout "Using state file $StateFile"
$State = @(Get-Content $StateFile -ErrorAction Ignore)

Check-Prerequisites

# Workflow tasks
Build-AssetsPhase1
Submit-AssetsToHwapi
Upload-AssetsToHwapi
Download-AssetsFromHwapi
Build-AssetsPhase2
Make-GitHubRelease
Upload-Symbols
Make-NugetRelease
Make-ChocoRelease
Write-Stdout "ALL COMPLETE"
