WinFsp CYGFUSE packages for MSYS2
=================================

The `fuse` and `fuse3` directories contain PKGBUILD files for the MSYS2
POSIX environment. Build them from `opt/cygfuse` with:

    make msys2

Distribution packages can be copied into `dist/msys2/<arch>` with:

    make dist-msys2

The package names are `winfsp-fuse` and `winfsp-fuse3` to avoid claiming
generic MSYS2 package names while still installing the usual FUSE headers,
import libraries, and pkg-config files.
