# Changelog


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
