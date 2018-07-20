`Passthrough-fuse3` is a simple FUSE3 file system that passes all file system operations to an underlying file system.

It can be built with the following tools:

- Using Visual Studio (`winfsp.sln`).
- Using Cygwin GCC and linking directly with the WinFsp DLL (`make winfsp-fuse3`).
- Using Cygwin GCC and linking to CYGFUSE3 (`make cygfuse3`).
