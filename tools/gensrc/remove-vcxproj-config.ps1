param (
    [Parameter(Mandatory)][string]$Path,
    [Parameter(Mandatory)][string[]]$ProjectConfiguration
)

$file = Get-Item $Path

$xmlob = New-Object xml
$xmlob.PreserveWhitespace = $true
$xmlob.Load($file.FullName)

$xmlns = @{"msbuild" = "http://schemas.microsoft.com/developer/msbuild/2003"}
$configs = Select-Xml -Xml $xmlob -Namespace $xmlns `
     -XPath "//msbuild:ProjectConfiguration[contains(@Include,'$ProjectConfiguration')]"
foreach ($config in $configs) {
    $child = $config.Node
    [void]$child.ParentNode.RemoveChild($child)
}

$xmlob.Save($file.FullName)
