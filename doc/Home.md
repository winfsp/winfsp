# WinFsp - Windows File System Proxy

[[WinFsp-Icon.png]]

Developing file systems is a challenging proposition. Developing file systems for Windows is an order of magnitude more difficult. WinFsp eases the task of writing a new file system for Windows. WinFsp file systems are user mode programs and they can be written in a variety of languages and frameworks.

The documentation available here discusses various aspects of WinFsp.

## Programming

- The [[Tutorial|WinFsp-Tutorial]] describes how to create a simple, but complete file system in C/C++.
- The [[API Reference|WinFsp-API-winfsp.h]] describes the native WinFsp API.
- There is also a FUSE compatibility layer for native Windows and Cygwin. See fuse.h in the source repository.
- This [[document|Native-API-vs-FUSE]] discusses the need for both a native API and FUSE and gives some pointers on which one to choose.
- Since release 2019.3 WinFsp supports development of file systems in kernel mode. This [[document|WinFsp-Kernel-Mode-File-Systems]] discusses how to write such file systems.

## Design

- The [[Design|WinFsp-Design]] document describes the high-level design of WinFsp.
- The [[IPC|WinFsp-as-an-IPC-Mechanism]] document offers insights into the WinFsp Inter-Process Communication mechanism.
- The [[Queued Events|Queued-Events]] document discusses a low-level synchronization primitive that is largely responsible for the excellent performance of the WinFsp IPC mechanism.
- The [[Service Architecture|WinFsp-Service-Architecture]] document discusses how to intergrate a file system into Windows as a service and the reasons to do so.
- The [[SSHFS Port Case Study|SSHFS-Port-Case-Study]] document chronicles the creation of the WinFsp-FUSE compatibility layer and the decisions that led to its design.

## Testing

- The [[Testing|WinFsp-Testing]] document discusses the WinFsp testing strategy and how WinFsp achieves correctness and stability.
- The [[Performance|WinFsp-Performance-Testing]] document compares WinFsp performance against other file systems.

## Compatibility

- The [[Compatibility|NTFS-Compatibility]] document discusses current WinFsp compatibility with NTFS.
