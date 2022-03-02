# Changelog


## v1.11B1 (2022+ARM64 Beta1)

- [NEW] ARM64 support! For details see [WinFsp on ARM64](https://github.com/winfsp/winfsp/wiki/WinFsp-on-ARM64).

- [NEW] New `ntptfs` sample file system. This is a production quality pass through file system and should be used instead of the original `passthrough` file system that was developed for education purposes only.

- [NEW] The default value for the registry setting `DistinctPermsForSameOwnerGroup` has been changed from 0 to 1.

- [BUILD] Product configuration (`MyProductName`, etc.) is done by the file `build.version.props` located in `build\VStudio`. This file was previously named `version.properties`.


## v1.10 (2022)

Prior changes are recorded in `doc/archive/Changelog-upto-v1.10.asciidoc`.
