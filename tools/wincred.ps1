Start-Job -ArgumentList $args -ScriptBlock {
param (
    [ValidateSet("get", "set", "del")]
    [string]$Command
)

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
namespace Win32
{
    public class Api
    {
        public static String[] CredRead(
            String TargetName)
        {
            String[] Result = null;
            IntPtr CredentialPtr;
            if (CredReadW(TargetName, 1/*CRED_TYPE_GENERIC*/, 0, out CredentialPtr))
            {
                CREDENTIALW Credential = Marshal.PtrToStructure<CREDENTIALW>(CredentialPtr);
                Result = new String[2]{
                    Credential.UserName,
                    Marshal.PtrToStringUni(
                        Credential.CredentialBlob, (int)Credential.CredentialBlobSize / 2)
                };
                CredFree(CredentialPtr);
            }
            return Result;
        }
        public static Boolean CredWrite(
            String TargetName,
            String UserName,
            String Password)
        {
            CREDENTIALW Credential = new CREDENTIALW{
                Type = 1/*CRED_TYPE_GENERIC*/,
                Persist = 2/*CRED_PERSIST_LOCAL_MACHINE*/,
                TargetName = TargetName,
                UserName = UserName,
                CredentialBlobSize = (UInt32)Password.Length * 2,
                CredentialBlob = Marshal.StringToCoTaskMemUni(Password),
            };
            Boolean Result = CredWriteW(ref Credential, 0);
            Marshal.FreeCoTaskMem(Credential.CredentialBlob);
            return Result;
        }
        public static Boolean CredDelete(
            String TargetName)
        {
            return CredDeleteW(TargetName, 1/*CRED_TYPE_GENERIC*/, 0);
        }
        [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError=true)]
        [return: MarshalAs(UnmanagedType.U1)]
        private static extern Boolean CredReadW(
            String TargetName,
            UInt32 Type,
            UInt32 Flags,
            out IntPtr Credential);
        [DllImport("advapi32.dll")]
        private static extern void CredFree(
            IntPtr Buffer);
        [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError=true)]
        [return: MarshalAs(UnmanagedType.U1)]
        private static extern Boolean CredWriteW(
            ref CREDENTIALW Credential,
            UInt32 Flags);
        [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError=true)]
        [return: MarshalAs(UnmanagedType.U1)]
        private static extern Boolean CredDeleteW(
            String TargetName,
            UInt32 Type,
            UInt32 Flags);
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct CREDENTIALW
        {
            public UInt32 Flags;
            public UInt32 Type;
            public String TargetName;
            public String Comment;
            public UInt64 LastWritten;
            public UInt32 CredentialBlobSize;
            public IntPtr CredentialBlob;
            public UInt32 Persist;
            public UInt32 AttributeCount;
            public IntPtr Attributes;
            public String TargetAlias;
            public String UserName;
        };
    };
};
"@

function Get-WindowsCredential {
    param (
        [Parameter(Mandatory)][string]$TargetName
    )
    return [Win32.Api]::CredRead($TargetName)
}

function Set-WindowsCredential {
    param (
        [Parameter(Mandatory)][string]$TargetName,
        [Parameter(Mandatory)][string]$UserName,
        [Parameter(Mandatory)][string]$Password
    )
    return [Win32.Api]::CredWrite($TargetName, $UserName, $Password)
}

function Delete-WindowsCredential {
    param (
        [Parameter(Mandatory)][string]$TargetName
    )
    return [Win32.Api]::CredDelete($TargetName)
}

$Function = @{
    "get" = "Get-WindowsCredential"
    "set" = "Set-WindowsCredential"
    "del" = "Delete-WindowsCredential"
}[$Command]
& $Function @args

} | Receive-Job -Wait -AutoRemoveJob
