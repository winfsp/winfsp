@echo off

setlocal
setlocal EnableDelayedExpansion

for %%f in (%*) do (
	set file="%%~f"
	if exist !file! (
		echo version-info: !file!
		powershell -command "[System.Diagnostics.FileVersionInfo]::GetVersionInfo(""!file!"") | fl -property ProductName, ProductVersion, ProductVersionRaw, LegalCopyright, CompanyName, FileDescription, FileVersionRaw, FileVersion"
		powershell -command "try { [System.Reflection.AssemblyName]::GetAssemblyName(""!file!"") | fl -property FullName, Version } catch {}"
	) else (
		echo version-info: file !file! not found
	)
)
