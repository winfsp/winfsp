# Changelog


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
