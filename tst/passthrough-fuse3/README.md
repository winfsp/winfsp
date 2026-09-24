`Passthrough-fuse3` is a simple FUSE3 file system that passes all file system operations to an underlying file system.

It can be built with the following tools:

- Using Visual Studio (`winfsp.sln`).
- Using Cygwin GCC and linking directly with the WinFsp DLL (`make winfsp-fuse3`).
- Using MinGW GCC and linking directly with the WinFsp DLL (`make winfsp-fuse3-mingw`).
- Using Cygwin GCC and linking to CYGFUSE3 (`make cygfuse3`).

WinFsp FUSE3 accepts cache-manager tuning options such as
`-o ReadAheadGranularity=N` and `-o DirtyPageThreshold=N`, where `N` is
specified in pages and `0` keeps the operating system default.
