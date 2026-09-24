`Passthrough-fuse` is a simple FUSE file system that passes all file system operations to an underlying file system.

It can be built with the following tools:

- Using Visual Studio (`winfsp.sln`).
- Using Cygwin GCC and linking directly with the WinFsp DLL (`make winfsp-fuse`).
- Using MinGW GCC and linking directly with the WinFsp DLL (`make winfsp-fuse-mingw`).
- Using Cygwin GCC and linking to CYGFUSE (`make cygfuse`).

WinFsp FUSE accepts cache-manager tuning options such as
`-o ReadAheadGranularity=N` and `-o DirtyPageThreshold=N`, where `N` is
specified in pages and `0` keeps the operating system default.
