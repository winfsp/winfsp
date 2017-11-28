# WinFsp - Windows File System Proxy

![WinFsp Demo](http://www.secfs.net/winfsp/files/cap.gif)


<a href="https://github.com/billziss-gh/winfsp/releases/latest"><img src="http://www.secfs.net/winfsp/resources/Download-WinFsp.png" alt="Download WinFsp Installer" width="244" height="34"></a>
&emsp;
<a href="https://chocolatey.org/packages/winfsp"><img src="http://www.secfs.net/winfsp/resources/Choco-WinFsp.png" alt="choco install winfsp" width="244" height="34"></a>



WinFsp is a set of software components for Windows computers that allows the creation of user mode file systems. In this sense it is similar to FUSE (Filesystem in Userspace), which provides the same functionality on UNIX-like computers.

Some of the benefits of using WinFsp are listed below:

* Very well-tested and stable. Read about its [Testing Strategy](doc/WinFsp-Testing.asciidoc).
* Very fast. Read about its [Performance](doc/WinFsp-Performance-Testing.asciidoc).
* Strives for compatibility with NTFS. Read about its [Compatibility](doc/NTFS-Compatibility.asciidoc ).
* Easy to understand but comprehensive API. Consult the [API Reference](http://www.secfs.net/winfsp/apiref/). There is also a simple [Tutorial](doc/WinFsp-Tutorial.asciidoc).
* FUSE compatibility layer for native Windows and Cygwin. See [fuse.h](inc/fuse/fuse.h).
* .NET layer for managed development. See [src/dotnet](src/dotnet).
* Signed drivers provided on every release.
* Available under the [GPLv3](License.txt) license with a special exception for Free/Libre and Open Source Software.

To learn more about WinFsp, please visit its website: http://www.secfs.net/winfsp/

## Project Organization

WinFsp consists of a kernel mode FSD (File System Driver) and a user mode DLL (Dynamic Link Library). The FSD interfaces with NTOS (the Windows kernel) and handles all interactions necessary to present itself as a file system driver to NTOS. The DLL interfaces with the FSD and presents an easy to use API for creating user mode file systems.

The project source code is organized as follows:

* `build/VStudio`: WinFsp solution and project files.
* `doc`: The WinFsp design documents and additional documentation can be found here.
* `ext/tlib`: A small test library originally from the secfs (Secure Cloud File System) project.
* `ext/test`: Submodule pointing to the secfs.test project, which contains a number of tools for testing Windows and POSIX file systems.
* `inc/fuse`: Public headers for the FUSE compatibility layer.
* `inc/winfsp`: Public headers for the WinFsp API.
* `src/dll`: Source code to the WinFsp DLL.
* `src/dll/fuse`: Source code to the FUSE compatibility layer.
* `src/dotnet`: Source code to the .NET layer.
* `src/fsptool`: Source code to fsptool command line utility.
* `src/launcher`: Source code to the launcher service and the launchctl utility.
* `src/sys`: Source code to the WinFsp FSD.
* `opt/cygfuse`: Source code for the Cygwin FUSE package.
* `tst/memfs*`: Source code to an example file system written in C/C++ (memfs) or C# (memfs-dotnet).
* `tst/passthrough*`: Source code to additional example file systems.
* `tst/winfsp-tests`: WinFsp test suite.
* `tools`: Various tools for building and testing WinFsp.

## Building and Running

In order to build WinFsp you will need the following:

* Visual Studio 2015
* Windows Driver Kit (WDK) 10
* [Wix toolset](http://wixtoolset.org)

To fully build WinFsp (including the installer) you must use `tools\build.bat`. By default it builds a Release build, but you can choose either the Debug or Release configuration by using the syntax:

    tools\build.bat CONFIGURATION

If you build the driver yourself it will not be signed and Windows will refuse to load it unless you enable "testsigning". You can enable "testsigning" using the command `bcdedit.exe -set testsigning on`. For more information see this [document](http://www.secfs.net/winfsp/develop/debug/).

WinFsp is designed to run on Windows 7 and above. It has been tested on the following platforms:

* Windows 7 Enterprise
* Windows 8 Pro
* Windows Server 2012
* Windows 10 Pro
* Windows Server 2016

## How to Help

I am looking for help in the following areas:

* If you have a file system that runs on FUSE please consider porting it to WinFsp. WinFsp has a native API, but it also has a FUSE (high-level) API.
* If you are working with a language other than C/C++ (e.g. Delphi, Java, etc.) and you are interested in porting/wrapping WinFsp I would love to hear from you.
* There are a number of outstanding issues listed in the [GitHub repository](https://github.com/billziss-gh/winfsp/issues). Many of these require knowledge of Windows kernel-mode and an understanding of the internals of WinFsp so they are not for the faint of heart.

In all cases I can provide ideas and/or support.

## Where to Discuss

If you wish to discuss WinFsp there are now two options:

- [WinFsp Google Group](https://groups.google.com/forum/#!forum/winfsp)
- [Author's Twitter](https://twitter.com/BZissimopoulos)

## License

WinFsp is available under the [GPLv3](License.txt) license with a special exception for Free/Libre and Open Source Software. A commercial license is also available. Please contact Bill Zissimopoulos \<billziss at navimatics.com> for more details.
