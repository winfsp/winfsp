param (
    [Parameter(Mandatory)][string]$Name,
    [string]$CheckpointName,
    [Parameter(Mandatory)][string[]]$Files,
    [Parameter(Mandatory)][string]$Destination
)

function Restore-VM ($Name, $CheckpointName) {
    $VM = Get-VM -Name $Name
    if ($VM.State -eq "Running") {
        Stop-VM -Name $Name -TurnOff
    }

    if (-not $CheckpointName) {
        $Checkpoint = Get-VMCheckpoint -VMName $Name |
            Sort-Object -Property CreationTime -Descending |
            select -First 1
    } else {
        $Checkpoint = Get-VMCheckpoint -VMName $Name -Name $CheckpointName
    }
    Restore-VMCheckpoint -VMCheckpoint $Checkpoint -Confirm:$false

    Start-VM -Name $Name
}

function Deploy-VMFiles ($Name, $Files, $Destination) {
    foreach ($File in $Files) {
        $Leaf = Split-Path -Path $File -Leaf
        $Dest = Join-Path $Destination $Leaf
        Copy-VMFile -Name $Name -SourcePath $File -DestinationPath $Dest -FileSource Host -CreateFullPath -Force
    }
}

Restore-VM -Name $Name -CheckpointName $CheckpointName
Deploy-VMFiles -Name $Name -Files $Files -Destination $Destination
