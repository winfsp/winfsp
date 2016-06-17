# WinFsp - Windows File System Proxy

WinFsp is a set of software components for Windows computers that allows the creation of user mode file systems. In this sense it is similar to FUSE (Filesystem in Userspace), which provides the same functionality on UNIX-like computers.

Some of the benefits and features of using WinFsp are listed below:

* Allows for easy development of file systems in user mode. There are no restrictions on what a process can do in order to implement a file system (other than respond in a timely manner to file system requests).
* Support for disk and network based file systems.
* Support for NTFS level security and access control.
* Support for memory mapped files, cached files and the NT cache manager.
* Support for file change notifications.
* Support for file locking.
* Correct NT semantics with respect to file sharing, file deletion and renaming.

To learn more about WinFsp, please visit its website: http://www.secfs.net/winfsp/

## Project Organization

WinFsp consists of a kernel mode FSD (File System Driver) and a user mode DLL (Dynamic Link Library). The FSD interfaces with NTOS (the Windows kernel) and handles all interactions necessary to present itself as a file system driver to NTOS. The DLL interfaces with the FSD and presents an easy to use API for creating user mode file systems.

The project source code is organized as follows:

* build/VStudio: contains the WinFsp solution and project files.
* doc: contains the WinFsp license, contributor agreement and additional documentation. The WinFsp design document can be found here.
* ext/tlib: contains a small test library originally from the secfs (Secure Cloud File System) project.
* ext/test: submodule pointing to the secfs.test project, which contains a number of tools for testing Windows and POSIX file systems.
* inc/winfsp: contains public headers for the WinFsp API.
* inc/fuse: contains public headers for the FUSE compatibility layer.
* src/dll: contains the source code to the WinFsp DLL.
* src/launcher: contains the source code to the launcher service and the launchctl utility.
* src/dll/fuse: contains the source code to the FUSE compatibility layer.
* src/sys: contains the source code to the WinFsp FSD.
* tst/memfs: contains the source code to an example file system written in C++ (memfs).
* tst/winfsp-tests: contains the WinFsp test suite.

## Building

In order to build WinFsp you will need Windows 10 and Visual Studio 2015. You will also need the Windows Driver Kit (WDK) 10.

## License

WinFsp is available under the [AGPLv3](http://www.gnu.org/licenses/agpl-3.0.html) license. If you find the constraints of the AGPLv3 too onerous, a commercial license is also available. Please contact Bill Zissimopoulos <billziss at navimatics.com> for more details.