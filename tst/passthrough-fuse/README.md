`Passthrough-fuse` is a simple FUSE file system that passes all file system operations to an underlying file system.

It can be built with the following tools:

- Using Visual Studio (`winfsp.sln`).
- Using Cygwin GCC and linking directly with the WinFsp DLL (`make winfsp-fuse`).
- Using Cygwin GCC and linking to CYGFUSE (`make cygfuse`).
