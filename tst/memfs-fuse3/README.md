`Memfs-fuse3` is an in-memory FUSE3 file system.

It can be built with the following tools:

- Using Visual Studio (`memfs-fuse3.sln`).
- Using Cygwin GCC and linking directly with the WinFsp DLL (`make winfsp-fuse3`).
- Using MinGW GCC and linking directly with the WinFsp DLL (`make winfsp-fuse3-mingw`).
- Using Cygwin GCC and linking to CYGFUSE3 (`make cygfuse3`).

WinFsp FUSE3 accepts cache-manager tuning options such as
`-o ReadAheadGranularity=N` and `-o DirtyPageThreshold=N`, where `N` is
specified in pages and `0` keeps the operating system default.
