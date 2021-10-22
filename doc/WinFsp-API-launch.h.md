# winfsp/launch.h

WinFsp Launch API.

In order to use the WinFsp Launch API a program must include <winfsp/launch.h>
and link with the winfsp\_x64.dll (or winfsp\_x86.dll) library.

## LAUNCH CONTROL

### Functions

<details>
<summary>
<b>FspLaunchCallLauncherPipe</b> - Call launcher pipe.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspLaunchCallLauncherPipe( 
    WCHAR Command,
    ULONG Argc,
    PWSTR *Argv,
    ULONG *Argl, 
    PWSTR Buffer,
    PULONG PSize, 
    PULONG PLauncherError);  
```

**Parameters**

- _Command_ \- Launcher command to send. For example, the 'L' launcher command instructs
the launcher to list all running service instances.
- _Argc_ \- Command argument count. May be 0.
- _Argv_ \- Command argument array. May be NULL.
- _Argl_ \- Command argument length array. May be NULL. If this is NULL all command arguments
are assumed to be NULL-terminated strings. It is also possible for specific arguments
to be NULL-terminated; in this case pass -1 in the corresponding Argl position.
- _Buffer_ \- Buffer that receives the command response. May be NULL.
- _PSize_ \- Pointer to a ULONG. On input it contains the size of the Buffer. On output it
contains the number of bytes transferred. May be NULL.
- _PLauncherError_ \- Receives the launcher error if any. This is always a Win32 error code. May not be NULL.

**Return Value**

STATUS\_SUCCESS if the command is sent successfully to the launcher, even if the launcher
returns an error. Other status codes indicate a communication error. Launcher errors are
reported through PLauncherError.

**Discussion**

This function is used to send a command to the launcher and receive a response.


</blockquote>
</details>

<details>
<summary>
<b>FspLaunchCallLauncherPipeEx</b> - Call launcher pipe.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspLaunchCallLauncherPipeEx( 
    WCHAR Command,
    ULONG Argc,
    PWSTR *Argv,
    ULONG *Argl, 
    PWSTR Buffer,
    PULONG PSize, 
    BOOLEAN AllowImpersonation, 
    PULONG PLauncherError);  
```

**Parameters**

- _Command_ \- Launcher command to send. For example, the 'L' launcher command instructs
the launcher to list all running service instances.
- _Argc_ \- Command argument count. May be 0.
- _Argv_ \- Command argument array. May be NULL.
- _Argl_ \- Command argument length array. May be NULL. If this is NULL all command arguments
are assumed to be NULL-terminated strings. It is also possible for specific arguments
to be NULL-terminated; in this case pass -1 in the corresponding Argl position.
- _Buffer_ \- Buffer that receives the command response. May be NULL.
- _PSize_ \- Pointer to a ULONG. On input it contains the size of the Buffer. On output it
contains the number of bytes transferred. May be NULL.
- _AllowImpersonation_ \- Allow caller to be impersonated by launcher.
- _PLauncherError_ \- Receives the launcher error if any. This is always a Win32 error code. May not be NULL.

**Return Value**

STATUS\_SUCCESS if the command is sent successfully to the launcher, even if the launcher
returns an error. Other status codes indicate a communication error. Launcher errors are
reported through PLauncherError.

**Discussion**

This function is used to send a command to the launcher and receive a response.


</blockquote>
</details>

<details>
<summary>
<b>FspLaunchGetInfo</b> - Get information about a service instance.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspLaunchGetInfo( 
    PWSTR ClassName,
    PWSTR InstanceName, 
    PWSTR Buffer,
    PULONG PSize, 
    PULONG PLauncherError);  
```

**Parameters**

- _ClassName_ \- Class name of the service instance to stop.
- _InstanceName_ \- Instance name of the service instance to stop.
- _Buffer_ \- Buffer that receives the command response. May be NULL.
- _PSize_ \- Pointer to a ULONG. On input it contains the size of the Buffer. On output it
contains the number of bytes transferred. May be NULL.
- _PLauncherError_ \- Receives the launcher error if any. This is always a Win32 error code. May not be NULL.

**Return Value**

STATUS\_SUCCESS if the command is sent successfully to the launcher, even if the launcher
returns an error. Other status codes indicate a communication error. Launcher errors are
reported through PLauncherError.

**Discussion**

The information is a list of NULL-terminated strings: the class name of the service instance,
the instance name of the service instance and the full command line used to start the service
instance.


</blockquote>
</details>

<details>
<summary>
<b>FspLaunchGetNameList</b> - List service instances.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspLaunchGetNameList( 
    PWSTR Buffer,
    PULONG PSize, 
    PULONG PLauncherError);  
```

**Parameters**

- _Buffer_ \- Buffer that receives the command response. May be NULL.
- _PSize_ \- Pointer to a ULONG. On input it contains the size of the Buffer. On output it
contains the number of bytes transferred. May be NULL.
- _PLauncherError_ \- Receives the launcher error if any. This is always a Win32 error code. May not be NULL.

**Return Value**

STATUS\_SUCCESS if the command is sent successfully to the launcher, even if the launcher
returns an error. Other status codes indicate a communication error. Launcher errors are
reported through PLauncherError.

**Discussion**

The information is a list of pairs of NULL-terminated strings. Each pair contains the class
name and instance name of a service instance. All currently running service instances are
listed.


</blockquote>
</details>

<details>
<summary>
<b>FspLaunchStart</b> - Start a service instance.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspLaunchStart( 
    PWSTR ClassName,
    PWSTR InstanceName,
    ULONG Argc,
    PWSTR *Argv, 
    BOOLEAN HasSecret, 
    PULONG PLauncherError);  
```

**Parameters**

- _ClassName_ \- Class name of the service instance to start.
- _InstanceName_ \- Instance name of the service instance to start.
- _Argc_ \- Service instance argument count. May be 0.
- _Argv_ \- Service instance argument array. May be NULL.
- _HasSecret_ \- Whether the last argument in Argv is assumed to be a secret (e.g. password) or not.
Secrets are passed to service instances through standard input rather than the command
line.
- _PLauncherError_ \- Receives the launcher error if any. This is always a Win32 error code. May not be NULL.

**Return Value**

STATUS\_SUCCESS if the command is sent successfully to the launcher, even if the launcher
returns an error. Other status codes indicate a communication error. Launcher errors are
reported through PLauncherError.


</blockquote>
</details>

<details>
<summary>
<b>FspLaunchStartEx</b> - Start a service instance.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspLaunchStartEx( 
    PWSTR ClassName,
    PWSTR InstanceName,
    ULONG Argc,
    PWSTR *Argv, 
    BOOLEAN HasSecret, 
    BOOLEAN AllowImpersonation, 
    PULONG PLauncherError);  
```

**Parameters**

- _ClassName_ \- Class name of the service instance to start.
- _InstanceName_ \- Instance name of the service instance to start.
- _Argc_ \- Service instance argument count. May be 0.
- _Argv_ \- Service instance argument array. May be NULL.
- _HasSecret_ \- Whether the last argument in Argv is assumed to be a secret (e.g. password) or not.
Secrets are passed to service instances through standard input rather than the command
line.
- _AllowImpersonation_ \- Allow caller to be impersonated by launcher.
- _PLauncherError_ \- Receives the launcher error if any. This is always a Win32 error code. May not be NULL.

**Return Value**

STATUS\_SUCCESS if the command is sent successfully to the launcher, even if the launcher
returns an error. Other status codes indicate a communication error. Launcher errors are
reported through PLauncherError.


</blockquote>
</details>

<details>
<summary>
<b>FspLaunchStop</b> - Stop a service instance.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspLaunchStop( 
    PWSTR ClassName,
    PWSTR InstanceName, 
    PULONG PLauncherError);  
```

**Parameters**

- _ClassName_ \- Class name of the service instance to stop.
- _InstanceName_ \- Instance name of the service instance to stop.
- _PLauncherError_ \- Receives the launcher error if any. This is always a Win32 error code. May not be NULL.

**Return Value**

STATUS\_SUCCESS if the command is sent successfully to the launcher, even if the launcher
returns an error. Other status codes indicate a communication error. Launcher errors are
reported through PLauncherError.


</blockquote>
</details>

## SERVICE REGISTRY

### Functions

<details>
<summary>
<b>FspLaunchRegFreeRecord</b> - Free a service registry record.
</summary>
<blockquote>
<br/>

```c
FSP_API VOID FspLaunchRegFreeRecord( 
    FSP_LAUNCH_REG_RECORD *Record);  
```

**Parameters**

- _Record_ \- The service record to free.

**See Also**

- FspLaunchRegGetRecord


</blockquote>
</details>

<details>
<summary>
<b>FspLaunchRegGetRecord</b> - Get a service registry record.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspLaunchRegGetRecord( 
    PWSTR ClassName,
    PWSTR Agent, 
    FSP_LAUNCH_REG_RECORD **PRecord);  
```

**Parameters**

- _ClassName_ \- The service class name.
- _Agent_ \- The name of the agent that is retrieving the service record. This API matches
the supplied Agent against the Agent in the service record and it only returns
the record if they match. Pass NULL to match any Agent.
- _PRecord_ \- Pointer to a record pointer. Memory for the service record will be allocated
and a pointer to it will be stored at this address. This memory must be later
freed using FspLaunchRegFreeRecord.

**Return Value**

STATUS\_SUCCESS or error code.

**See Also**

- FspLaunchRegFreeRecord


</blockquote>
</details>

<details>
<summary>
<b>FspLaunchRegSetRecord</b> - Add/change/delete a service registry record.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspLaunchRegSetRecord( 
    PWSTR ClassName, 
    const FSP_LAUNCH_REG_RECORD *Record);  
```

**Parameters**

- _ClassName_ \- The service class name.
- _Record_ \- The record to set in the registry. If NULL, the registry record is deleted.

**Return Value**

STATUS\_SUCCESS or error code.


</blockquote>
</details>

### Typedefs

<details>
<summary>
<b>FSP_LAUNCH_REG_RECORD</b> - Service registry record.
</summary>
<blockquote>
<br/>

```c
typedef struct _FSP_LAUNCH_REG_RECORD { 
    PWSTR Agent; 
    PWSTR Executable; 
    PWSTR CommandLine; 
    PWSTR WorkDirectory; 
    PWSTR RunAs; 
    PWSTR Security; 
    PWSTR AuthPackage; 
    PWSTR Stderr; 
    PVOID Reserved0[4]; 
    ULONG JobControl; 
    ULONG Credentials; 
    ULONG AuthPackageId; 
    ULONG Recovery; 
    ULONG Reserved1[4]; 
    UINT8 Buffer[]; 
} FSP_LAUNCH_REG_RECORD;  
```


</blockquote>
</details>


<br/>
<p align="center">
<sub>
Copyright © 2015-2021 Bill Zissimopoulos
<br/>
Generated with <a href="https://github.com/billziss-gh/prettydoc">prettydoc</a>
</sub>
</p>
