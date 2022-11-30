param (
    [Parameter(Mandatory)][string]$Path,
    [Parameter(Mandatory)][string]$Match
)

echo "Removing projects that match $($Match) from $($Path)"

Get-Content $Path -Delimiter 'EndProject' |
  Where-Object {$_ -notlike $Match} |
  Set-Content "$($Path).new"
  
 Move-Item -Path "$($Path).new" -Destination $Path -Force