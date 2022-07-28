# WinFsp Registry Settings

<details>
<summary>
📁 <code>HKLM\SYSTEM\CurrentControlSet\Services\WinFsp</code>
</summary>
<blockquote>

Stores information about the WinFsp file system driver as required by the Windows OS.

</blockquote>
</details>

<details>
<summary>
📁 <code>HKLM\SYSTEM\CurrentControlSet\Services\WinFsp.Launcher</code>
</summary>
<blockquote>

Stores information about the WinFsp Launcher service as required by the Windows OS.

</blockquote>
</details>

<details>
<summary>
📁 <code>HKLM\SYSTEM\CurrentControlSet\Services\WinFsp.Np</code>
</summary>
<blockquote>

Stores information about the WinFsp network provider as required by the Windows OS.

</blockquote>
</details>

<details>
<summary>
📁 <code>HKLM\SYSTEM\CurrentControlSet\Services\EventLog\Application\WinFsp</code>
</summary>
<blockquote>

Stores information about the WinFsp event source as required by the Windows OS.

</blockquote>
</details>

<details>
<summary>
📁 <code>HKLM\SYSTEM\CurrentControlSet\Control\NetworkProvider\Order</code>
</summary>
<blockquote>

Stores information about the WinFsp network provider as required by the Windows OS.

</blockquote>
</details>

<details>
<summary>
📁 <code>HKLM\SOFTWARE\WinFsp</code>
</summary>
<blockquote>

Primary registry key used to store WinFsp settings. On a 64-bit system (x64 or ARM64) this key is stored in the 32-bit portion of the registry and its true location is `HKLM\SOFTWARE\WOW6432Node\WinFsp`.

* `InstallDir (REG_SZ)`: Contains the WinFsp installation directory.

* `DistinctPermsForSameOwnerGroup (REG_DWORD)`: Directs how WinFsp-FUSE should consider UNIX owner and group permissions in the case when the Windows owner and group SID are the same (for example, this can happen when someone uses a Microsoft account as their primary login). When this setting is 0 and the Windows owner and group SID are the same, WinFsp-FUSE combines the UNIX owner and group permissions (for example, user permission `rw-` and group permission `---` combine to `---`), which can result in inadvertent "access denied" errors. When this setting is 1 and even if the Windows owner and group SID are the same, WinFsp-FUSE looks at the UNIX owner permissions and the UNIX group permissions separately. The default value is 1 since v1.11B1 and was 0 in earlier versions.

* `MountBroadcastDriveChange (REG_DWORD)`: A value of 1 instructs WinFsp to broadcast an additional "drive change" message to all top-level windows during mounting and unmounting. The default value is 0. Normally the Windows infrastructure broadcasts a `WM_DEVICECHANGE` message whenever a drive gets added/removed. In some rare systems it is possible for this message to get lost or stalled. The workaround for these rare systems is to enable this registry setting, in which case WinFsp will broadcast the `WM_DEVICECHANGE` using a slightly different but more reliable method than the one Windows uses.

* `MountDoNotUseLauncher (REG_DWORD)`: A value of 1 disallows the use of the Launcher for drive mounting. The default value of 0 allows use of the Launcher for drive mounting when necessary. In general the Launcher is not necessary for mounting. However when running a file system in the Windows Service context (session 0) under an account that is not LocalSystem (e.g. `NT AUTHORITY\NETWORK SERVICE`), the Launcher is used to create global drives.

* `MountUseMountmgrFromFSD (REG_DWORD)`: A value of 1 instructs WinFsp to use the Mount Manager from the FSD (File System Driver) which runs in kernel mode. The default value of 0 instructs WinFsp to use the Mount Manager from the DLL which runs in user mode. Using the Mount Manager from user mode requires Administrator access and this setting allows a file system to circumvent the Administrator access requirement. This setting is not recommended for general use.

</blockquote>
</details>


<details>
<summary>
📁 <code>HKLM\SOFTWARE\WinFsp\Services\:SERVICE</code>
</summary>
<blockquote>

Registry key used to store information about the WinFsp service with name `:SERVICE`. WinFsp services are user mode file systems controlled by the Launcher; for more information see the [Service Architecture](WinFsp-Service-Architecture.asciidoc) document. On a 64-bit system (x64 or ARM64) this key is stored in the 32-bit portion of the registry and its true location is `HKLM\SOFTWARE\WOW6432Node\WinFsp\Services\:SERVICE`.

* `Agent (REG_SZ)`: UNDOCUMENTED (see source code).

* `Executable (REG_SZ)`: Contains the path to the executable to use when launching the service.

* `CommandLine (REG_SZ)`: Contains the command line to use when launching the service.

* `WorkDirectory (REG_SZ)`: Contains the working directory to use when launching the service.

* `RunAs (REG_SZ)`: Controls the account used when launching the service. Possible values are `LocalSystem` (default), `LocalService`, `NetworkService` and `.` (dot). The `.` (dot) value means that the service should be launched as the account that is launching the file system (e.g. via `net use` or Explorer's "Map Network Drive").

* `Security (REG_SZ)`: Controls which users can launch the service.

* `AuthPackage (REG_SZ)`: UNDOCUMENTED (see source code).

* `Stderr (REG_SZ)`: Specifies a path that the Launcher will redirect service error output to.

* `JobControl (REG_DWORD)`: Controls whether the service is running in the same job as the Launcher. The default value is 1.

* `Credentials (REG_DWORD)`: Controls whether the file system requires credentials.

* `AuthPackageId (REG_DWORD)`: UNDOCUMENTED (see source code).

* `Recovery (REG_DWORD)`: A value of 1 instructs the Launcher to restart a service that has crashed. The default value is 0.

</blockquote>
</details>

<details>
<summary>
📁 <code>HKLM\SOFTWARE\WinFsp\Fsext</code>
</summary>
<blockquote>

Registry key used to store WinFsp fsext provider information. Fsext providers are kernel mode file systems that interface with WinFsp; for more information see the [Kernel Mode File Systems](WinFsp-Kernel-Mode-File-Systems.asciidoc) document. On a 64-bit system (x64 or ARM64) this key is stored in the 32-bit portion of the registry and its true location is `HKLM\SOFTWARE\WOW6432Node\WinFsp\Fsext`.

* `:CTLCODE (REG_SZ)`: The `:CTLCODE` name is the string representation of the fsext provider's transact code in `%08lx` format and the value is the provider's driver name.

</blockquote>
</details>
