# Changelog


## v2.2B4 (2026 Beta4)

- [FIX] Fixes vulnerability CVE-2026-3006 reported by Tay Kiat Loong (GitHub: @Owl4444).

- [FIX] Fixes vulnerability CVE-2026-7162 reported by Tay Kiat Loong (GitHub: @Owl4444) and uhg (GitHub: @UltimateHG).

- [FIX] Fixes vulnerability reported by Wencheng Xue (GitHub @panwnvda).

- [FIX] Fixes vulnerability reported by Abhinav Agarwal (GitHub @abhinavagarwal07).

- [FIX] Fixes deadlock during cached writes under memory pressure. See PR #677 for discussion.

- [FIX] Fixes deadlock in the file system notification mechanism with concurrent renames. See fix PR #669 by @yeonsh and test PR #672 by @Maksim-Isakau.

- [FIX] Fixes deadlock when closing named streams while the file system is being forcibly terminated. See issue #680.

- [FIX] Fixes another complicated deadlock when the file system is being forcibly terminated. See issue #682.

- [FIX] The WinFsp Network Provider provides improved shell support for network file systems not managed by the Launcher. See commit f021496.

- [FIX] Fixes a problem with mounting on a directory with a trailing backslash. See issue #674.

- [FUSE] Add FUSE option `FlushOnCleanup`. See PR #614 by @chenjie4255.

- [FUSE] FUSE now respects the `direct_io` flag. See PR #646 by @chenjie4255.

- [FUSE] Add FUSE option `AddWriteEaAccess`. See PR #648 by @chenjie4255.


## v2.2B3 (2026 Beta3)

- [FIX] Fixes vulnerability CVE-2026-3006 discovered by Tay Kiat Loong (GitHub: @Owl4444).

- [FIX] Fixes vulnerability CVE-2026-7162 discovered by Tay Kiat Loong (GitHub: @Owl4444) and uhg (GitHub: @UltimateHG).

- [FIX] Fixes deadlock during cached writes under memory pressure. See PR #677 and GitHub issue #620 for discussion.

- [FIX] Fixes deadlock in the file system notification mechanism with concurrent renames. See fix PR #669 by @yeonsh and test PR #672 by @Maksim-Isakau.

- [FIX] The WinFsp Network Provider provides improved shell support for network file systems not managed by the Launcher. See commit f021496.

- [FUSE] Add FUSE option `FlushOnCleanup`. See PR #614 by @chenjie4255.

- [FUSE] FUSE now respects the `direct_io` flag. See PR #646 by @chenjie4255.

- [FUSE] Add FUSE option `AddWriteEaAccess`. See PR #648 by @chenjie4255.

- [FUSE] Add FUSE option `WslFeatures` for WSL drvfs support. See GitHub issue #473.

- [FIX] Report driver service start failures with the underlying service exit code. See GitHub issue #477.

- [FIX] Add target container silo support for host-created file systems and `ntptfs -o TargetSiloId=GUID`. See GitHub issue #498.

- [FIX] Improve installer diagnostics when a third-party or older `WinFsp` service blocks setup. See GitHub issue #499.

- [FUSE] Treat ownership changes during file creation as best-effort when the server denies `chown`, avoiding Explorer duplicate-copy retries with SSHFS. See GitHub issue #503.

- [MEMFS] Match NTFS path errors when creating files below an existing file. See GitHub issue #504.

- [FIX] Add regression coverage for `FILE_OPEN_REPARSE_POINT` file opens and include stripped trailing slash state in create debug logs. See GitHub issue #505.

- [.NET] Make native API binding explicit and idempotent for NativeAOT compatibility. See GitHub issue #506.

- [FUSE] Preserve absolute POSIX symlink targets as volume-root relative when using `rellinks`. See GitHub issue #509.

- [FIX] Add a Win32 fallback for directory mount point creation on virtual or pooled volumes that reject the direct native create path. See GitHub issue #512.

- [FIX] Teach the passthrough sample to proxy backing reparse points so junction/symlink chains can be resolved by Windows. See GitHub issue #518.

- [FIX] Sign the installer custom action DLL before embedding it in the MSI to satisfy Windows Smart App Control code integrity policy. See GitHub issue #527.

- [DOC] Clarify Node.js/JavaScript binding status and guidance for third-party WinFsp packages. See GitHub issue #544.

- [FIX] Keep auto-created directory mount points visible and writable until unmount, and remove them explicitly during unmount. See GitHub issue #551.

- [FIX] Report physical UNC names for WinFsp network volumes to avoid Explorer srvsvc/IPC$ fallback delays at copy completion. See GitHub issue #554.

- [FIX] Cover Explorer UNC resource resolution for WinFsp network volumes. See GitHub issue #569.

- [FIX] Prioritize directory queries in the user-mode dispatch queue so listings stay responsive during heavy write workloads. See GitHub issue #588.

- [TEST] Cover loading a companion DLL from WinFsp volumes to guard executable scenarios that depend on same-directory DLL loads. See GitHub issue #599.

- [MEMFS] Report `NTFS` by default from the standalone MEMFS samples so elevated executables work on newer Windows versions. See GitHub issue #611.

- [FIX] Answer MountDev identity IOCTL's on mounted file-system volumes so Shell path lookups remain stable for directory mount points. See GitHub issue #612.

- [FIX] Invalidate cached file metadata after Cleanup updates so kernel-cache volumes report fresh write times after close. See GitHub issue #629.

- [FUSE] Support FUSE3 `lseek` through `FSCTL_QUERY_ALLOCATED_RANGES` for sparse-file range queries. See GitHub issue #632.

- [TEST] Cover final DOS path resolution for MountMgr directory mounts and document the elevated execution requirements. See GitHub issue #641.

- [DOC] Clarify that WinFsp UNC prefixes are local Network Provider names and that remote sharing must use a disk file system exported through the Windows Server service. See GitHub issue #650.

- [FIX] Initialize WinFsp's IRP request context before dispatch so foreign `DriverContext` data on VHDX paging writes is not reused as a request. See GitHub issue #651.

- [NEW] Add an opt-in user-mode access-check deferral flag and FUSE `-o defer_permissions` so file systems can enforce custom POSIX/AD permissions themselves. See GitHub issue #654.

- [FIX] Clarify independent directory info caching and expose `DirInfoTimeout` on the C++ file-system helper so directory metadata can be cached without enabling file data caching. See GitHub issue #665.

- [DOC] Clarify that service-created drive letters are controlled by Windows DOS device namespaces and must be created per user session to be hidden from other users. See GitHub issue #675.

- [NEW] Add an opt-in persistent MountDev unique ID for Mount Manager mounts so reconnect-aware folder monitors can correlate a new volume instance with the prior one. See GitHub issue #678.

- [FIX] Avoid probing optical drives through Mount Manager in the WinFsp Network Provider to prevent Explorer startup stalls when CD/DVD media is present. See GitHub issue #679.


## v2.2B2 (2026 Beta2)

- [FIX] Fixes vulnerability CVE-2026-3006 discovered by Tay Kiat Loong (GitHub: @Owl4444).

- [FIX] Fixes vulnerability CVE-2026-7162 discovered by Tay Kiat Loong (GitHub: @Owl4444) and uhg (GitHub: @UltimateHG).

- [FIX] Fixes deadlock in the file system notification mechanism with concurrent renames. See fix PR #669 by @yeonsh and test PR #672 by @Maksim-Isakau.

- [FIX] The WinFsp Network Provider provides improved shell support for network file systems not managed by the Launcher. See commit f021496.

- [FUSE] Add FUSE option `FlushOnCleanup`. See PR #614 by @chenjie4255.

- [FUSE] FUSE now respects the `direct_io` flag. See PR #646 by @chenjie4255.

- [FUSE] Add FUSE option `AddWriteEaAccess`. See PR #648 by @chenjie4255.


## v2.2B1 (2026 Beta1)

- [FIX] Fixes vulnerability CVE-2026-3006 discovered by Tay Kiat Loong.

- [FIX] The WinFsp Network Provider provides improved shell support for network file systems not managed by the Launcher. See commit f021496.

- [FUSE] Add FUSE option `FlushOnCleanup`. See PR #614 by @chenjie4255.

- [FUSE] FUSE now respects the `direct_io` flag. See PR #646 by @chenjie4255.

- [FUSE] Add FUSE option `AddWriteEaAccess`. See PR #648 by @chenjie4255.


## v2.1 (2025)

- [FIX] Fixes a compatibility problem with certain AntiVirus products (e.g. Trend Micro).

- [FIX] Fixes a couple of rare BSODs on recent versions of Windows 11. See commits a482183, a2cd697 for details.

- [FIX] Fixes a rare problem when using `NtCreateFile` to perform "relative" opens on a network drive (see GitHub issue #561).

- [FIX] Fixes a racing issue with two processes competing to start the FSD discovered during testing.


## v2.1B2 (2024 Beta2)

- [FIX] Fixes a rare BSOD on recent versions of Windows. See commit a482183 for details.

- [FIX] Fixes a rare problem when using `NtCreateFile` to perform "relative" opens on a network drive (see GitHub issue #561).

- [FIX] Fixes a racing issue with two processes competing to start the FSD discovered during testing.


## v2.1B1 (2024 Beta1)

- [FIX] Fixes a rare BSOD on recent versions of Windows. See commit a482183 for details.

- [FIX] Fixes a racing issue with two processes competing to start the FSD discovered during testing.


## v2.0 (2023)

This release is a major version change for WinFsp (from 1.x to 2.x). There are no backwards incompatible API changes in this release, but nevertheless enough things change that warrant a version change.

The major new feature of this release is that it allows uninstallation and reinstallation of WinFsp **without reboot**. Going forward installers named `winfsp-2.x.y.msi` can be uninstalled and reinstalled without reboot. Furthermore a later version `winfsp-2.x.y.msi` installer can be used to upgrade over an earlier version `winfsp-2.x.y.msi` installer. However note that a `winfsp-2.x.y.msi` installer cannot be used to upgrade over a legacy `winfsp-1.x.y.msi` installer; you will still need to uninstall the old `winfsp-1.x.y.msi` installer, potentially reboot and then install the new `winfsp-2.x.y.msi` installer.

Changes visible to file system developers are listed below:

- WinFsp executable files are now installed by default in the directory `C:\Program Files (x86)\WinFsp\SxS\sxs.<InstanceID>\bin`. The previous directory `C:\Program Files (x86)\WinFsp\bin` is now a junction that points to the above directory.

- The WinFsp driver name is no longer `winfsp`, but rather a name such as `winfsp+<InstanceID>`. This means that managing the driver using the `sc.exe` utility is no longer as easy.

- The `fsptool` utility has been updated with new commands `lsdrv`, `load`, `unload` and `ver`. The `lsdrv`, `load` and `unload` commands can be used to manage the driver from the command line. This is rarely necessary, but may be useful for troubleshooting purposes.

- Prior to this release the WinFsp driver would never unmount a file system volume unless the user mode file system requested the unmount. From this release onward it is possible for the WinFsp driver to unmount a file system volume, without a user mode file system request. This is to allow for the driver to be unloaded.

    A new operation `DispatcherStopped` has been added to `FSP_FILE_SYSTEM_INTERFACE`, which is sent after the file system volume has been unmounted and the file system dispatcher has been stopped. This can happen because of a user mode file system request via `FspFileSystemStopDispatcher` or because of driver unload. The `DispatcherStopped` operation includes a `Normally` parameter, which is `TRUE` for normal file system shutdown via `FspFileSystemStopDispatcher` and `FALSE` otherwise.

    Native file systems that use the `FspService` infrastructure can use the `FspFileSystemStopServiceIfNecessary` API to handle the `DispatcherStopped` operation (see the MEMFS and NTPTFS samples). FUSE file systems get this functionality for free. .NET file systems that use the `Service` class infrastructure also get this functionality for free.

- WinFsp now offers a .NET library that targets .NET Framework 3.5 (as before) and one that targets .NET Standard 2.0. This is due to work by @Noire001 in PR #451.

    - There is now a winfsp.net nuget package at https://www.nuget.org/packages/winfsp.net

- FUSE now supports path components up to 255 characters long (previously it was 255 bytes). This is due to work by @zeho11 in PR #474.

    - The FUSE passthrough file systems have been updated to support long paths. This is also due to work by @zeho11.

- In some rare circumstances WinFsp file systems could report duplicate directory entries. This problem has been fixed. (GitHub issue #475.)

- The WinFsp symbols directory has been removed. If you are looking for WinFsp symbols you can find them at https://github.com/winfsp/winfsp.sym


## v2.0RC1 (2023 RC1)

This release is a major version change for WinFsp (from 1.x to 2.x). There are no backwards incompatible API changes in this release, but nevertheless enough things change that warrant a version change.

The major new feature of this release is that it allows uninstallation and reinstallation of WinFsp **without reboot**. Going forward installers named `winfsp-2.x.y.msi` can be uninstalled and reinstalled without reboot. Furthermore a later version `winfsp-2.x.y.msi` installer can be used to upgrade over an earlier version `winfsp-2.x.y.msi` installer. However note that a `winfsp-2.x.y.msi` installer cannot be used to upgrade over a legacy `winfsp-1.x.y.msi` installer; you will still need to uninstall the old `winfsp-1.x.y.msi` installer, potentially reboot and then install the new `winfsp-2.x.y.msi` installer.

Changes visible to file system developers are listed below:

- WinFsp executable files are now installed by default in the directory `C:\Program Files (x86)\WinFsp\SxS\sxs.<InstanceID>\bin`. The previous directory `C:\Program Files (x86)\WinFsp\bin` is now a junction that points to the above directory.

- The WinFsp driver name is no longer `winfsp`, but rather a name such as `winfsp+<InstanceID>`. This means that managing the driver using the `sc.exe` utility is no longer as easy.

- The `fsptool` utility has been updated with new commands `lsdrv`, `load`, `unload` and `ver`. The `lsdrv`, `load` and `unload` commands can be used to manage the driver from the command line. This is rarely necessary, but may be useful for troubleshooting purposes.

- Prior to this release the WinFsp driver would never unmount a file system volume unless the user mode file system requested the unmount. From this release onward it is possible for the WinFsp driver to unmount a file system volume, without a user mode file system request. This is to allow for the driver to be unloaded.

    A new operation `DispatcherStopped` has been added to `FSP_FILE_SYSTEM_INTERFACE`, which is sent after the file system volume has been unmounted and the file system dispatcher has been stopped. This can happen because of a user mode file system request via `FspFileSystemStopDispatcher` or because of driver unload. The `DispatcherStopped` operation includes a `Normally` parameter, which is `TRUE` for normal file system shutdown via `FspFileSystemStopDispatcher` and `FALSE` otherwise.

    Native file systems that use the `FspService` infrastructure can use the `FspFileSystemStopServiceIfNecessary` API to handle the `DispatcherStopped` operation (see the MEMFS and NTPTFS samples). FUSE file systems get this functionality for free. .NET file systems that use the `Service` class infrastructure also get this functionality for free.

- WinFsp now offers a .NET library that targets .NET Framework 3.5 (as before) and one that targets .NET Standard 2.0. This is due to work by @Noire001 in PR #451.

    - There is now a winfsp.net nuget package at https://www.nuget.org/packages/winfsp.net

- FUSE now supports path components up to 255 characters long (previously it was 255 bytes). This is due to work by @zeho11 in PR #474.

    - The FUSE passthrough file systems have been updated to support long paths. This is also due to work by @zeho11.

- In some rare circumstances WinFsp file systems could report duplicate directory entries. This problem has been fixed. (GitHub issue #475.)

- The WinFsp symbols directory has been removed. If you are looking for WinFsp symbols you can find them at https://github.com/winfsp/winfsp.sym


## v2.0B2 (2023 Beta2)

This release is a major version change for WinFsp (from 1.x to 2.x). There are no backwards incompatible API changes in this release, but nevertheless enough things change that warrant a version change.

The major new feature of this release is that it allows uninstallation and reinstallation of WinFsp **without reboot**. Going forward installers named `winfsp-2.x.y.msi` can be uninstalled and reinstalled without reboot. Furthermore a later version `winfsp-2.x.y.msi` installer can be used to upgrade over an earlier version `winfsp-2.x.y.msi` installer. However note that a `winfsp-2.x.y.msi` installer cannot be used to upgrade over a "legacy" `winfsp-1.x.y.msi` installer; you will still need to uninstall the "old" `winfsp-1.x.y.msi` installer, potentially reboot and then install the "new" `winfsp-2.x.y.msi` installer.

Changes visible to file system developers are listed below:

- WinFsp executable files are now installed by default in the directory `C:\Program Files (x86)\WinFsp\SxS\sxs.<InstanceID>\bin`. The previous directory `C:\Program Files (x86)\WinFsp\bin` is now a junction that points to the above directory.

- The WinFsp driver name is no longer `winfsp`, but rather a name such as `winfsp+<InstanceID>`. This means that managing the driver using the `sc.exe` utility is no longer as easy.

- The `fsptool` utility has been updated with new commands `lsdrv`, `load`, `unload` and `ver`. The `lsdrv`, `load` and `unload` commands can be used to manage the driver from the command line. This is rarely necessary, but may be useful for troubleshooting purposes.

- Prior to this release the WinFsp driver would never unmount a file system volume unless the user mode file system requested the unmount. From this release onward it is possible for the WinFsp driver to unmount a file system volume, without a user mode file system request. This is to allow for the driver to be unloaded.

    A new operation `DispatcherStopped` has been added to `FSP_FILE_SYSTEM_INTERFACE`, which is sent after the file system volume has been unmounted and the file system dispatcher has been stopped. This can happen because of a user mode file system request via `FspFileSystemStopDispatcher` or because of driver unload. The `DispatcherStopped` operation includes a `Normally` parameter, which is `TRUE` for normal file system shutdown via `FspFileSystemStopDispatcher` and `FALSE` otherwise.

    Native file systems that use the `FspService` infrastructure can use the `FspFileSystemStopServiceIfNecessary` API to handle the `DispatcherStopped` operation (see the MEMFS and NTPTFS samples). FUSE file systems get this functionality for free. .NET file systems that use the `Service` class infrastructure also get this functionality for free.

- WinFsp now offers a .NET library that targets .NET Framework 3.5 (as before) and one that targets .NET Standard 2.0. This is due to work by @Noire001 in PR #451.

- FUSE now supports path components up to 255 characters long (previously it was 255 bytes). This is due to work by @zeho11 in PR #474.

    - The FUSE passthrough file systems have been updated to support long paths. This is also due to work by @zeho11.

- The WinFsp symbols directory has been removed. If you are looking for WinFsp symbols you can find them at https://github.com/winfsp/winfsp.sym


## v2.0B1 (2023 Beta1)

This release is a major version change for WinFsp (from 1.x to 2.x). There are no backwards incompatible API changes in this release, but nevertheless enough things change that warrant a version change.

The major new feature of this release is that it allows uninstallation and reinstallation of WinFsp **without reboot**. Going forward installers named `winfsp-2.x.y.msi` can be uninstalled and reinstalled without reboot. Furthermore a later version `winfsp-2.x.y.msi` installer can be used to upgrade over an earlier version `winfsp-2.x.y.msi` installer. However note that a `winfsp-2.x.y.msi` installer cannot be used to upgrade over a "legacy" `winfsp-1.x.y.msi` installer; you will still need to uninstall the "old" `winfsp-1.x.y.msi` installer, potentially reboot and then install the "new" `winfsp-2.x.y.msi` installer.

Some changes that may be visible to file system developers are listed below:

- WinFsp executable files are now installed by default in the directory `C:\Program Files (x86)\WinFsp\SxS\sxs.<InstanceID>\bin`. The previous directory `C:\Program Files (x86)\WinFsp\bin` is now a junction that points to the above directory.

- The WinFsp driver name is no longer `winfsp`, but rather a name such as `winfsp+<InstanceID>`. This means that managing the driver using the `sc.exe` utility is no longer as easy.

- The `fsptool` utility has been updated with new commands `lsdrv`, `load`, `unload` and `ver`. The `lsdrv`, `load` and `unload` commands can be used to manage the driver from the command line. This is rarely necessary, but may be useful for troubleshooting purposes.

- The WinFsp symbols directory has been removed. If you are looking for WinFsp symbols you can find them at https://github.com/winfsp/winfsp.sym


## v1.12.22339 (2022.2 Update1)

*Note: This release (`v1.12.22339`) is the same as the previous release (`v1.12`) except that: (1) the kernel-mode drivers are now digitally signed only with the Microsoft Attestation signature, and that: (2) no release assets are digitally signed with SHA-1. (This change was necessary to fix a problem in older versions of Windows such as Windows 7.)*

- [NEW] WinFsp now supports mounting as directory using the Mount Manager. Use the syntax `\\.\C:\Path\To\Mount\Directory`.

- [NEW] A new registry setting `MountUseMountmgrFromFSD` has been added. See [WinFsp Registry Settings](https://github.com/winfsp/winfsp/wiki/WinFsp-Registry-Settings) for details.

- [FIX] A problem with Windows containers has been fixed. (GitHub issue #438.)

- [FIX] File systems can now be mounted as directories on ARM64. (GitHub issue #448.)

- [FIX] The passthrough file system now reports correct `IndexNumber`. (GitHub issue #325.)

- [BUILD] Product configuration for the relative paths to the File System Driver, Network Provider and EventLog is now possible via the file `build.version.props` located in `build\VStudio`.


## v1.12 (2022.2)

- [NEW] WinFsp now supports mounting as directory using the Mount Manager. Use the syntax `\\.\C:\Path\To\Mount\Directory`.

- [NEW] A new registry setting `MountUseMountmgrFromFSD` has been added. See [WinFsp Registry Settings](https://github.com/winfsp/winfsp/wiki/WinFsp-Registry-Settings) for details.

- [FIX] A problem with Windows containers has been fixed. (GitHub issue #438.)

- [FIX] File systems can now be mounted as directories on ARM64. (GitHub issue #448.)

- [FIX] The passthrough file system now reports correct `IndexNumber`. (GitHub issue #325.)

- [BUILD] Product configuration for the relative paths to the File System Driver, Network Provider and EventLog is now possible via the file `build.version.props` located in `build\VStudio`.


## v1.12B2 (2022.2 Beta2)

- [NEW] WinFsp now supports mounting as directory using the Mount Manager. Use the syntax `\\.\C:\Path\To\Mount\Directory`.

- [NEW] A new registry setting `MountUseMountmgrFromFSD` has been added. See [WinFsp Registry Settings](https://github.com/winfsp/winfsp/wiki/WinFsp-Registry-Settings) for details.

- [FIX] A problem with Windows containers has been fixed. (GitHub issue #438.)

- [FIX] File systems can now be mounted as directories on ARM64. (GitHub issue #448.)

- [FIX] The passthrough file system now reports correct `IndexNumber`. (GitHub issue #325.)

- [BUILD] Product configuration for the relative paths to the File System Driver, Network Provider and EventLog is now possible via the file `build.version.props` located in `build\VStudio`.


## v1.12B1 (2022.2 Beta1)

- [NEW] WinFsp now supports mounting as directory using the Mount Manager. Use the syntax `\\.\C:\Path\To\Mount\Directory`.

- [NEW] A new registry setting `MountUseMountmgrFromFSD` has been added. See [WinFsp Registry Settings](https://github.com/winfsp/winfsp/wiki/WinFsp-Registry-Settings) for details.

- [BUILD] Product configuration for the relative paths to the File System Driver, Network Provider and EventLog is now possible via the file `build.version.props` located in `build\VStudio`.


## v1.11 (2022+ARM64)

- [NEW] ARM64 support! For details see [WinFsp on ARM64](https://github.com/winfsp/winfsp/wiki/WinFsp-on-ARM64).

- [NEW] A new file system operation has been added to the FUSE API:

    ```C
    int (*getpath)(const char *path, char *buf, size_t size,
        struct fuse_file_info *fi);
    ```

    The `getpath` operation allows a case-insensitive file system to report the correct case of a file path. For example, `getpath` can be used to report that the actual path of a file opened as `/PATH/TO/FILE` is really `/Path/To/File`. This capability is important for some Windows file system scenarios and can sometimes result in a performance improvement.

- [NEW] New `ntptfs` sample file system. This is a production quality pass through file system and should be used instead of the original `passthrough` file system that was developed for education purposes only.

- [NEW] Many performance improvements:

    - A new `PostDispositionForDirOnly` setting has been added to `FSP_FSCTL_VOLUME_PARAMS`. This allows a file system to declare that it does not want to see `SetInformation/Disposition` requests for files (such requests will still be sent for directories, because a file system is supposed to check if a directory is empty before deletion). This makes file (not directory) deletion faster. This optimization should be safe to enable for most file systems. FUSE file systems get this optimization for free.

    - The FSD now implements "fast I/O" reads and writes. Fast I/O is a technique for doing I/O without using IRP's (I/O Request Packets) and can only work for file systems using the cache manager (`FileInfoTimeout==-1`). This results in significant improvement in read/write scenarios.

    - The FSD now implements "fast I/O" for "transact" messages. Transact messages are used in the communication protocol between the kernel-mode FSD and the user-mode file system. Fast I/O speeds this communication protocol by as much as 10% in some scenarios. (Fast I/O for transact messages is enabled only when using the new `FSP_IOCTL_TRANSACT` control code, but does not require any other special configuration to be enabled.)

    - The FSD per directory cache limit has been increased from 16K to 64K. This should allow for more directory data to be maintained in kernel and reduce round-trips to the user mode file system.

    - The user mode directory buffering mechanism (`FspFileSystemAcquireDirectoryBuffer`) has been improved. The mechanism uses the quick-sort algorithm internally which can exhibit bad performance when sorting already sorted data. The quick-sort algorithm has been improved with the use use of median of three partitioning, which alleviates this problem.

- [NEW] A new registry setting under `HKLM\SOFTWARE\WinFsp` (or `HKLM\SOFTWARE\WOW6432Node\WinFsp` on a 64-bit system) called `MountBroadcastDriveChange` has been introduced, which if set to 1 will broadcast an additional "drive change" message to all top-level windows (including Explorer) during mounting and unmounting.

    - Normally the Windows infrastructure broadcasts a `WM_DEVICECHANGE` message whenever a drive gets added/removed. In some rare systems it is possible for this message to get lost or stalled. The workaround for these rare systems is to enable this registry setting, in which case WinFsp will broadcast the `WM_DEVICECHANGE` using a slightly different but more reliable method than the one Windows uses.

    - For more details see source code comments at [`FspMountBroadcastDriveChange`](https://github.com/winfsp/winfsp/blob/v1.11B3/src/dll/mount.c#L390-L406).

- [FIX] The WinFsp Network Provider now implements `NPGetUniversalName`. This fixes problems with some apps (e.g. Photos app).

- [FIX] WinFsp-FUSE now supports Azure AD accounts when specifying the `-o uid=-1` option. In addition a new option `-o uidmap=UID:SID` allows the specification of arbitrary UID<->SID or UID<->UserName mappings.

- [FIX] All executables (`*.exe,*.dll,*.sys`) in the WinFsp installation `bin` folder are now signed.

- [FIX] The default value for the registry setting `DistinctPermsForSameOwnerGroup` has been changed from 0 to 1.

- [BUILD] Product configuration (`MyProductName`, etc.) is done by the file `build.version.props` located in `build\VStudio`. This file was previously named `version.properties`.


## v1.11RC1 (2022+ARM64 RC1)

- [NEW] ARM64 support! For details see [WinFsp on ARM64](https://github.com/winfsp/winfsp/wiki/WinFsp-on-ARM64).

- [NEW] A new file system operation has been added to the FUSE API:

    ```C
    int (*getpath)(const char *path, char *buf, size_t size,
        struct fuse_file_info *fi);
    ```

    The `getpath` operation allows a case-insensitive file system to report the correct case of a file path. For example, `getpath` can be used to report that the actual path of a file opened as `/PATH/TO/FILE` is really `/Path/To/File`. This capability is important for some Windows file system scenarios and can sometimes result in a performance improvement.

- [NEW] New `ntptfs` sample file system. This is a production quality pass through file system and should be used instead of the original `passthrough` file system that was developed for education purposes only.

- [NEW] Many performance improvements:

    - A new `PostDispositionForDirOnly` setting has been added to `FSP_FSCTL_VOLUME_PARAMS`. This allows a file system to declare that it does not want to see `SetInformation/Disposition` requests for files (such requests will still be sent for directories, because a file system is supposed to check if a directory is empty before deletion). This makes file (not directory) deletion faster. This optimization should be safe to enable for most file systems. FUSE file systems get this optimization for free.

    - The FSD now implements "fast I/O" reads and writes. Fast I/O is a technique for doing I/O without using IRP's (I/O Request Packets) and can only work for file systems using the cache manager (`FileInfoTimeout==-1`). This results in significant improvement in read/write scenarios.

    - The FSD now implements "fast I/O" for "transact" messages. Transact messages are used in the communication protocol between the kernel-mode FSD and the user-mode file system. Fast I/O speeds this communication protocol by as much as 10% in some scenarios. (Fast I/O for transact messages is enabled only when using the new `FSP_IOCTL_TRANSACT` control code, but does not require any other special configuration to be enabled.)

    - The FSD per directory cache limit has been increased from 16K to 64K. This should allow for more directory data to be maintained in kernel and reduce round-trips to the user mode file system.

    - The user mode directory buffering mechanism (`FspFileSystemAcquireDirectoryBuffer`) has been improved. The mechanism uses the quick-sort algorithm internally which can exhibit bad performance when sorting already sorted data. The quick-sort algorithm has been improved with the use use of median of three partitioning, which alleviates this problem.

- [NEW] A new registry setting under `HKLM\SOFTWARE\WinFsp` (or `HKLM\SOFTWARE\WOW6432Node\WinFsp` on a 64-bit system) called `MountBroadcastDriveChange` has been introduced, which if set to 1 will broadcast an additional "drive change" message to all top-level windows (including Explorer) during mounting and unmounting.

    - Normally the Windows infrastructure broadcasts a `WM_DEVICECHANGE` message whenever a drive gets added/removed. In some rare systems it is possible for this message to get lost or stalled. The workaround for these rare systems is to enable this registry setting, in which case WinFsp will broadcast the `WM_DEVICECHANGE` using a slightly different but more reliable method than the one Windows uses.

    - For more details see source code comments at [`FspMountBroadcastDriveChange`](https://github.com/winfsp/winfsp/blob/v1.11B3/src/dll/mount.c#L390-L406).

- [FIX] The WinFsp Network Provider now implements `NPGetUniversalName`. This fixes problems with some apps (e.g. Photos app).

- [FIX] WinFsp-FUSE now supports Azure AD accounts when specifying the `-o uid=-1` option. In addition a new option `-o uidmap=UID:SID` allows the specification of arbitrary UID<->SID or UID<->UserName mappings.

- [FIX] All executables (`*.exe,*.dll,*.sys`) in the WinFsp installation `bin` folder are now signed.

- [FIX] The default value for the registry setting `DistinctPermsForSameOwnerGroup` has been changed from 0 to 1.

- [BUILD] Product configuration (`MyProductName`, etc.) is done by the file `build.version.props` located in `build\VStudio`. This file was previously named `version.properties`.


## v1.11B3 (2022+ARM64 Beta3)

- [NEW] ARM64 support! For details see [WinFsp on ARM64](https://github.com/winfsp/winfsp/wiki/WinFsp-on-ARM64).

- [NEW] A new file system operation has been added to the FUSE API:

    ```C
    int (*getpath)(const char *path, char *buf, size_t size,
        struct fuse_file_info *fi);
    ```

    The `getpath` operation allows a case-insensitive file system to report the correct case of a file path. For example, `getpath` can be used to report that the actual path of a file opened as `/PATH/TO/FILE` is really `/Path/To/File`. This capability is important for some Windows file system scenarios and can sometimes result in a performance improvement.

- [NEW] New `ntptfs` sample file system. This is a production quality pass through file system and should be used instead of the original `passthrough` file system that was developed for education purposes only.

- [NEW] Many performance improvements:

    - A new `PostDispositionForDirOnly` setting has been added to `FSP_FSCTL_VOLUME_PARAMS`. This allows a file system to declare that it does not want to see `SetInformation/Disposition` requests for files (such requests will still be sent for directories, because a file system is supposed to check if a directory is empty before deletion). This makes file (not directory) deletion faster. This optimization should be safe to enable for most file systems. FUSE file systems get this optimization for free.

    - The FSD now implements "fast I/O" reads and writes. Fast I/O is a technique for doing I/O without using IRP's (I/O Request Packets) and can only work for file systems using the cache manager (`FileInfoTimeout==-1`). This results in significant improvement in read/write scenarios.

    - The FSD now implements "fast I/O" for "transact" messages. Transact messages are used in the communication protocol between the kernel-mode FSD and the user-mode file system. Fast I/O speeds this communication protocol by as much as 10% in some scenarios. (Fast I/O for transact messages is enabled only when using the new `FSP_IOCTL_TRANSACT` control code, but does not require any other special configuration to be enabled.)

    - The FSD per directory cache limit has been increased from 16K to 64K. This should allow for more directory data to be maintained in kernel and reduce round-trips to the user mode file system.

    - The user mode directory buffering mechanism (`FspFileSystemAcquireDirectoryBuffer`) has been improved. The mechanism uses the quick-sort algorithm internally which can exhibit bad performance when sorting already sorted data. The quick-sort algorithm has been improved with the use use of median of three partitioning, which alleviates this problem.

- [NEW] A new registry setting under `HKLM\SOFTWARE\WinFsp` (or `HKLM\SOFTWARE\WOW6432Node\WinFsp` on a 64-bit system) called `MountBroadcastDriveChange` has been introduced, which if set to 1 will broadcast an additional "drive change" message to all top-level windows (including Explorer) during mounting and unmounting.

    - Normally the Windows infrastructure broadcasts a `WM_DEVICECHANGE` message whenever a drive gets added/removed. In some rare systems it is possible for this message to get lost or stalled. The workaround for these rare systems is to enable this registry setting, in which case WinFsp will broadcast the `WM_DEVICECHANGE` using a slightly different but more reliable method than the one Windows uses.

    - For more details see source code comments at [`FspMountBroadcastDriveChange`](https://github.com/winfsp/winfsp/blob/v1.11B3/src/dll/mount.c#L390-L406).

- [FIX] The WinFsp Network Provider now implements `NPGetUniversalName`. This fixes problems with some apps (e.g. Photos app).

- [FIX] WinFsp-FUSE now supports Azure AD accounts when specifying the `-o uid=-1` option. In addition a new option `-o uidmap=UID:SID` allows the specification of arbitrary UID<->SID or UID<->UserName mappings.

- [FIX] All executables (`*.exe,*.dll,*.sys`) in the WinFsp installation `bin` folder are now signed.

- [FIX] The default value for the registry setting `DistinctPermsForSameOwnerGroup` has been changed from 0 to 1.

- [BUILD] Product configuration (`MyProductName`, etc.) is done by the file `build.version.props` located in `build\VStudio`. This file was previously named `version.properties`.


## v1.11B2 (2022+ARM64 Beta2)

- [NEW] ARM64 support! For details see [WinFsp on ARM64](https://github.com/winfsp/winfsp/wiki/WinFsp-on-ARM64).

- [NEW] A new file system operation has been added to the FUSE API:

    ```C
    int (*getpath)(const char *path, char *buf, size_t size,
        struct fuse_file_info *fi);
    ```

    The `getpath` operation allows a case-insensitive file system to report the correct case of a file path. For example, `getpath` can be used to report that the actual path of a file opened as `/PATH/TO/FILE` is really `/Path/To/File`. This capability is important for some Windows file system scenarios and can sometimes result in a performance improvement.

- [NEW] Many performance improvements:

    - A new `PostDispositionForDirOnly` setting has been added to `FSP_FSCTL_VOLUME_PARAMS`. This allows a file system to declare that it does not want to see `SetInformation/Disposition` requests for files (such requests will still be sent for directories, because a file system is supposed to check if a directory is empty before deletion). This makes file (not directory) deletion faster. This optimization should be safe to enable for most file systems. FUSE file systems get this optimization for free.

    - The FSD now implements "fast I/O" reads and writes. Fast I/O is a technique for doing I/O without using IRP's (I/O Request Packets) and can only work for file systems using the cache manager (`FileInfoTimeout==-1`). This results in significant improvement in read/write scenarios.

    - The FSD per directory cache limit has been increased from 16K to 64K. This should allow for more directory data to be maintained in kernel and reduce round-trips to a user mode file system.

    - The user mode directory buffering mechanism (`FspFileSystemAcquireDirectoryBuffer`) has been improved. The mechanism uses the quick-sort algorithm internally which can exhibit bad performance when sorting already sorted data. The quick-sort algorithm has been improved with the use use of median of three partitioning, which alleviates this problem.

- [NEW] The default value for the registry setting `DistinctPermsForSameOwnerGroup` has been changed from 0 to 1.

- [NEW] New `ntptfs` sample file system. This is a production quality pass through file system and should be used instead of the original `passthrough` file system that was developed for education purposes only.

- [FIX] The WinFsp Network Provider now implements `NPGetUniversalName`. This fixes problems with some apps (e.g. Photos app).

- [BUILD] Product configuration (`MyProductName`, etc.) is done by the file `build.version.props` located in `build\VStudio`. This file was previously named `version.properties`.


## v1.11B1 (2022+ARM64 Beta1)

- [NEW] ARM64 support! For details see [WinFsp on ARM64](https://github.com/winfsp/winfsp/wiki/WinFsp-on-ARM64).

- [NEW] New `ntptfs` sample file system. This is a production quality pass through file system and should be used instead of the original `passthrough` file system that was developed for education purposes only.

- [NEW] The default value for the registry setting `DistinctPermsForSameOwnerGroup` has been changed from 0 to 1.

- [BUILD] Product configuration (`MyProductName`, etc.) is done by the file `build.version.props` located in `build\VStudio`. This file was previously named `version.properties`.


## v1.10 (2022)

Prior changes are recorded in `doc/archive/Changelog-upto-v1.10.asciidoc`.
