# Changelog


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
