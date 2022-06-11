<h1 align="center">WinFsp &middot; Windows File System Proxy</h1>

<p align="center">
    <img src="art/winfsp-glow.png" width="128"/>
    <br/>
    <br/>
    <i>WinFsp enables developers to write their own file systems (i.e. "Windows drives") as user mode programs and without any knowledge of Windows kernel programming. It is similar to FUSE (Filesystem in Userspace) for Linux and other UNIX-like computers.</i>
    <br/>
    <br/>
    <a href="https://winfsp.dev"><b>winfsp.dev</b></a>
    <br/>
    <br/>
    <a href="https://github.com/winfsp/winfsp/releases/latest">
        <img src="https://img.shields.io/github/release/winfsp/winfsp.svg?label=stable&style=for-the-badge&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA0ODAgNDgwIj48cGF0aCBkPSJNMzg3LjAwMiAyMDEuMDAxQzM3Mi45OTggMTMyLjAwMiAzMTIuOTk4IDgwIDI0MCA4MGMtNTcuOTk4IDAtMTA3Ljk5OCAzMi45OTgtMTMyLjk5OCA4MS4wMDFDNDcuMDAyIDE2Ny4wMDIgMCAyMTcuOTk4IDAgMjgwYzAgNjUuOTk2IDUzLjk5OSAxMjAgMTIwIDEyMGgyNjBjNTUgMCAxMDAtNDUgMTAwLTEwMCAwLTUyLjk5OC00MC45OTYtOTYuMDAxLTkyLjk5OC05OC45OTl6TTIwOCAyNTJ2LTc2aDY0djc2aDY4TDI0MCAzNTIgMTQwIDI1Mmg2OHoiIGZpbGw9IiNmZmYiLz48L3N2Zz4="/>
    </a>
    <a href="https://github.com/winfsp/winfsp/releases">
        <img src="https://img.shields.io/github/release/winfsp/winfsp/all.svg?label=latest&colorB=e52e4b&style=for-the-badge&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA0ODAgNDgwIj48cGF0aCBkPSJNMzg3LjAwMiAyMDEuMDAxQzM3Mi45OTggMTMyLjAwMiAzMTIuOTk4IDgwIDI0MCA4MGMtNTcuOTk4IDAtMTA3Ljk5OCAzMi45OTgtMTMyLjk5OCA4MS4wMDFDNDcuMDAyIDE2Ny4wMDIgMCAyMTcuOTk4IDAgMjgwYzAgNjUuOTk2IDUzLjk5OSAxMjAgMTIwIDEyMGgyNjBjNTUgMCAxMDAtNDUgMTAwLTEwMCAwLTUyLjk5OC00MC45OTYtOTYuMDAxLTkyLjk5OC05OC45OTl6TTIwOCAyNTJ2LTc2aDY0djc2aDY4TDI0MCAzNTIgMTQwIDI1Mmg2OHoiIGZpbGw9IiNmZmYiLz48L3N2Zz4="/>
    </a>
    <a href="https://chocolatey.org/packages/winfsp">
        <img src="https://img.shields.io/badge/choco-install%20winfsp-black.svg?style=for-the-badge"/>
    </a>
    <br/>
    <br/>
    <img src="doc/cap.gif" height="450"/>
    <br/>
    <br/>
</p>

<hr/>

## Overview

WinFsp is system software that provides runtime and development support for custom file systems on Windows computers. Typically any information or storage may be organized and presented as a file system via WinFsp, with the benefit being that the information can be accessed via the standand Windows file API’s by any Windows application.

The core WinFsp consists of a kernel mode file system driver (FSD) and a user mode DLL that provides an API for developers. This API allows developers to handle Windows file system functions easily. For example, when an application attempts to open a file, WinFsp will call an `Open` function with the necessary information.

The WinFsp package also includes development resources and additional tools to help with developing and managing user mode file systems.

## Benefits

**Stability**: Stable software without any known kernel mode crashes, resource leaks or similar problems. WinFsp owes this stability to its [Design](doc/WinFsp-Design.asciidoc) and its rigorous [Testing Regime](doc/WinFsp-Testing.asciidoc).

**Correctness**: Strives for file system correctness and compatibility with NTFS. For details see the [Compatibility](doc/NTFS-Compatibility.asciidoc) document.

**Performance**: Has excellent performance that rivals or exceeds that of NTFS in many file system scenarios. Read more about its [Performance](doc/WinFsp-Performance-Testing.asciidoc).

<p align="center">
    <img src="doc/WinFsp-Performance-Testing/file_tests.png" height="300"/>
    <img src="doc/WinFsp-Performance-Testing/rdwr_tests.png" height="300"/>
</p>

**Wide support**: Supports Windows 7 to Windows 11 and the x86, x64 and ARM64 architectures.

**Flexible API**: Flexible, comprehensive and easy to use API. This [Tutorial](doc/WinFsp-Tutorial.asciidoc) explains how to build a file system.

- Native [API Reference](doc/WinFsp-API-winfsp.h.md)

- .NET layer for managed development. See [src/dotnet](src/dotnet)

- FUSE 2.8 compatibility layer: [fuse/fuse.h](inc/fuse/fuse.h)

- FUSE 3.2 compatibility layer: [fuse3/fuse.h](inc/fuse3/fuse.h)

- Wrapper libraries for other languages and environments also exist. See the [Known File Systems and File System Libraries](doc/Known-File-Systems.asciidoc) document.

**Shell integration**: Provides facilities to integrate user mode file systems with the Windows shell. See the [Service Architecture](doc/WinFsp-Service-Architecture.asciidoc) document.

**Self-contained**: Self-contained software without external dependencies.

**Widely used**: Used in many open-source and commercial applications with millions of installations (estimated: the WinFsp project does not track its users).

**Flexible licensing**: Available under the [GPLv3](License.txt) license with a special exception for Free/Libre and Open Source Software. A commercial license is also available. Please contact Bill Zissimopoulos \<billziss at navimatics.com> for more details.

## Installation

Download and run the [WinFsp installer](https://github.com/winfsp/winfsp/releases/latest).

In the installer select the option to install the developer files. These include the MEMFS sample file system, but also header and library files that let you develop your own user-mode file system.

You should not need to reboot, unless WinFsp was already running on your system.

<img src="doc/WinFsp-Tutorial/Installer.png" height="290"/>

## Resources

**Documentation**:

- Project [wiki](https://github.com/winfsp/winfsp/wiki) (most up-to-date)

- Project [website](https://winfsp.dev/doc/)

**Discuss**:

- [WinFsp Google Group](https://groups.google.com/forum/#!forum/winfsp)

- [Author's Twitter](https://twitter.com/BZissimopoulos)
