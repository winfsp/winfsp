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

    # check winfsp.nupkg
    if (!(Test-Path "$ProjectRoot\build\VStudio\build\Release\winfsp*.nupkg" -ErrorAction SilentlyContinue)) {
        Write-Stderr "error: cannot find winfsp*.nupkg"
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

Upload file driver.cab to Microsoft Partner Center for attestation signing.
When the file has been signed, download and extract to ~\Downloads\drivers

"@
    }
}

function Build-AssetsPhase2 {
    Task -ScriptBlock {
        Check-Assets

        # check signed drivers folder
        if (!(Test-Path ~\Downloads\drivers -ErrorAction SilentlyContinue)) {
            Write-Stderr "error: cannot find ~\Downloads\drivers"
            exit 1
        }

        $SignedPackage = Resolve-Path ~\Downloads\drivers

        $VerX64 = Get-FileVersion "$ProjectRoot\build\VStudio\build\Release\winfsp-x64.sys"
        if ($VerX64 -ne (Get-FileVersion "$SignedPackage\x64\winfsp-x64.sys")) {
            Write-Stderr "error: incompatible versions in ~\Downloads\drivers"
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

        if ((Resolve-Path "$ProjectRoot\build\VStudio\build\Release\winfsp*.msi") -match "\\winfsp-(.+)\.msi") {
            $Version = $matches[1]
        }

        $DownloadColor = "blue"
        $PrereleaseOpt = ""
        if ($ReleaseInfo.Prerelease) {
            $DownloadColor = "e52e4b"
            $PrereleaseOpt = "-p"
        }

        $ReleaseNotes = @"
[![Download WinFsp](https://img.shields.io/badge/-Download%20WinFsp-$DownloadColor.svg?style=for-the-badge&labelColor=grey&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA0ODAgNDgwIj48cGF0aCBkPSJNMzg3LjAwMiAyMDEuMDAxQzM3Mi45OTggMTMyLjAwMiAzMTIuOTk4IDgwIDI0MCA4MGMtNTcuOTk4IDAtMTA3Ljk5OCAzMi45OTgtMTMyLjk5OCA4MS4wMDFDNDcuMDAyIDE2Ny4wMDIgMCAyMTcuOTk4IDAgMjgwYzAgNjUuOTk2IDUzLjk5OSAxMjAgMTIwIDEyMGgyNjBjNTUgMCAxMDAtNDUgMTAwLTEwMCAwLTUyLjk5OC00MC45OTYtOTYuMDAxLTkyLjk5OC05OC45OTl6TTIwOCAyNTJ2LTc2aDY0djc2aDY4TDI0MCAzNTIgMTQwIDI1Mmg2OHoiIGZpbGw9IiNmZmYiLz48L3N2Zz4=)](https://github.com/billziss-gh/winfsp/releases/download/$($ReleaseInfo.Tag)/winfsp-$Version.msi)

[VirusTotal Scan Results]()

## CHANGES SINCE WINFSP $($ReleaseInfo.PreviousProductVersion)
$($ReleaseInfo.Text -join "`n")
"@

        gh release create $ReleaseInfo.Tag --draft --title "WinFsp $($ReleaseInfo.ProductVersion)" --notes "$ReleaseNotes" $PrereleaseOpt (Resolve-Path "$ProjectRoot\build\VStudio\build\Release\winfsp*.msi") (Resolve-Path "$ProjectRoot\build\VStudio\build\Release\winfsp-tests*.zip")
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
        $SymHasTag = Git-LogGrep $ReleaseInfo.Tag
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

function Make-ChocoRelease {
    Task -ScriptBlock {
        Check-Assets

        Push-Location "$ProjectRoot\build\VStudio\build\Release"
        choco push
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
Build-AssetsPhase2
Make-GitHubRelease
Upload-Symbols
Make-ChocoRelease
