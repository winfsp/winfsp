
#include <winfsp/winfsp.h>
#include <vcclr.h>  
#include "interop.h"
#include "FileNode.h"
#define DEFAULT_SECTOR_SIZE               512
#define DEFAULT_SECTORS_PER_ALLOCATION_UNIT 1

#define DEFAULT_MAX_IRP_CAPACITY 1000
#define DEFAULT_IRP_TIMOUT 10*60*1000 
#define DEFAULT_TRANSACT_TIMEOUT 10*1000 

#define GET_FILESYSTEM(f) WinFspInstances::Instance->GetFileSystem((UINT_PTR)f)
using namespace System;
using namespace System::Runtime;
using System::Runtime::InteropServices::Marshal;
#pragma once
namespace WinFspNet {

	public enum class CleanUpFlags: ULONG{	
		FspCleanupDelete = 0x01,
		FspCleanupSetAllocationSize = 0x02,
		FspCleanupSetArchiveBit = 0x10,
		FspCleanupSetLastAccessTime = 0x20,
		FspCleanupSetLastWriteTime = 0x40,
		FspCleanupSetChangeTime = 0x80,
	};
	public ref class FileAttirutes {
	public:
		static const UINT32 WINFSP_FILE_ATTRIBUTE_READONLY = 0x00000001;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_HIDDEN = 0x00000002;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_SYSTEM = 0x00000004;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_DIRECTORY = 0x00000010;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_ARCHIVE = 0x00000020;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_DEVICE = 0x00000040;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_NORMAL = 0x00000080;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_TEMPORARY = 0x00000100;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_SPARSE_FILE = 0x00000200;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_REPARSE_POINT = 0x00000400;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_COMPRESSED = 0x00000800;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_OFFLINE = 0x00001000;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_NOT_CONTENT_INDEXED = 0x00002000;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_ENCRYPTED = 0x00004000;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_INTEGRITY_STREAM = 0x00008000;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_VIRTUAL = 0x00010000;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_NO_SCRUB_DATA = 0x00020000;
		static const UINT32 WINFSP_FILE_ATTRIBUTE_EA = 0x00040000;
		static const UINT32 INVALID_ATTRIBUTE = 4294967295;
	};
	public ref class NT_STATUS
	{
	public:
		//NT STATUS
		static const UINT32 SUCCESS = 0x00000000;

		static const UINT32 MORE_ENTRIES = 0x00000105;
		static const UINT32 NO_SUCH_DEVICE = 0xC000000E;
		static const UINT32 NO_SUCH_FILE = 0xC000000F;
		static const UINT32 NO_MORE_FILES = 0x80000006;
		static const UINT32 OBJECT_NAME_INVALID = 0xC0000033;
		static const UINT32 OBJECT_NAME_NOT_FOUND = 0xC0000034;
		static const UINT32 OBJECT_NAME_COLLISION = 0xC0000035;
		static const UINT32 OBJECT_PATH_NOT_FOUND = 0xC000003A;
		static const UINT32 FILE_IS_A_DIRECTORY = 0xC00000BA;
		static const UINT32 NOT_A_DIRECTORY = 0xC0000103;
		static const UINT32 NOT_SUPPORTED = 0xC00000BB;
		static const UINT32 NONCONTINUABLE_EXCEPTION = 0xC0000025;
		static const UINT32 BUFFER_OVERFLOW = 0x80000005;
		static const UINT32 DIRECTORY_NOT_EMPTY = 0xC0000101;
		static const UINT32 END_OF_FILE = 0xC0000011;
		static const UINT32 NOT_IMPLEMENTED = 0xC0000002;
		static const UINT32 REQUEST_NOT_ACCEPTED = 0xC00000D0;
		static const UINT32 INVALID_PARAMETER = 0xC000000D;
		static const UINT32 ACCESS_DENIED = 0xC0000022;
	};
	public ref class CreateOption {
	public:
		static const UINT32 WINFSP_FILE_DIRECTORY_FILE = 0x00000001;
		static	const UINT32 WINFSP_FILE_WRITE_THROUGH = 0x00000002;
		static	const UINT32 WINFSP_FILE_SEQUENTIAL_ONLY = 0x00000004;
		static const UINT32 WINFSP_FILE_NO_INTERMEDIATE_BUFFERING = 0x00000008;

		static const UINT32 WINFSP_FILE_SYNCHRONOUS_IO_ALERT = 0x00000010;
		static const UINT32 WINFSP_FILE_SYNCHRONOUS_IO_NONALERT = 0x00000020;
		static const UINT32 WINFSP_FILE_NON_DIRECTORY_FILE = 0x00000040;
		static const UINT32 WINFSP_FILE_CREATE_TREE_CONNECTION = 0x00000080;

		static const UINT32 WINFSP_FILE_COMPLETE_IF_OPLOCKED = 0x00000100;
		static const UINT32 WINFSP_FILE_NO_EA_KNOWLEDGE = 0x00000200;
		static const UINT32 WINFSP_FILE_OPEN_REMOTE_INSTANCE = 0x00000400;
		static const UINT32 WINFSP_FILE_RANDOM_ACCESS = 0x00000800;

		static const UINT32 WINFSP_FILE_DELETE_ON_CLOSE = 0x00001000;
		static const UINT32 WINFSP_FILE_OPEN_BY_FILE_ID = 0x00002000;
		static const UINT32 WINFSP_FILE_OPEN_FOR_BACKUP_INTENT = 0x00004000;
		static const UINT32 WINFSP_FILE_NO_COMPRESSION = 0x00008000;
	};

	public ref  class WinFspFileSystem {
		FileNodeCollection^ _fileNodes;
		FSP_FILE_SYSTEM* FileSystem = NULL;
	internal:
		GET_PROPERTY(FileNodeCollection^, Nodes, _fileNodes)
	public:

		interface class WinFspMinimalOperation {
		public:
			void Cleanup(WinFspFileSystem^ FileSystem, FileOpenContext^ Context, String^ FileName,ULONG Flag);
			void Close(WinFspFileSystem^ FileSystem, FileOpenContext^ Context);
			UINT32 GetVolumeInfo(WinFspFileSystem^ FileSystem, VolInfo^% VolumeInfo);
			UINT32 SetVolumeLabel(WinFspFileSystem^ FileSystem, String^ VolumeLabel, VolInfo^% VolumeInfo);
			//SecurityDescriptor
			UINT32 GetSecurityByName(WinFspFileSystem^ FileSystem, String^ FileName, UINT32% PFileAttributes, SecuirtyDescriptor^ SecurityDescriptor);

			UINT32 Create(WinFspFileSystem^ FileSystem, String^ FileName, UINT32 CreateOptions, UINT32 GrantedAccess, UINT32 FileAttributes, SecuirtyDescriptor^ SecurityDescriptor, UINT64 AllocationSize, FileOpenContext^ Context);
			UINT32 Open(WinFspFileSystem^ FileSystem, String^ FileName, UINT32 CreateOptions, UINT32 GrantedAccess, FileOpenContext^ Context);
			UINT32 Overwrite(WinFspFileSystem^ FileSystem, FileOpenContext^ Context, UINT32 FileAttributes, bool ReplaceFileAttributes);
			UINT32 Read(WinFspFileSystem^ FileSystem, FileOpenContext^ Context, ReadFileBuffer^ buffer);
			UINT32 Write(WinFspFileSystem^ FileSystem, FileOpenContext^ Context, WriteFileBuffer^ buffer);
			UINT32 Flush(WinFspFileSystem^ FileSystem, FileOpenContext^ Context);
			UINT32 GetFileInfo(WinFspFileSystem^ FileSystem, FileOpenContext^ Context);
			UINT32 SetBasicInfo(WinFspFileSystem^ FileSystem, FileOpenContext^ Context, UINT32 FileAttributes, UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime);
			UINT32 SetFileSize(WinFspFileSystem^ FileSystem, FileOpenContext^ Context, UINT64 NewSize, bool SetAllocationSize);
			UINT32 CanDelete(WinFspFileSystem^ FileSystem, FileOpenContext^ Context, String^ FileName);
			UINT32 Rename(WinFspFileSystem^ FileSystem, FileOpenContext^ Context, String^ FileName, String^ NewFileName, bool ReplaceIfExists);
			UINT32 SetSecurity(WinFspFileSystem^ FileSystem, FileOpenContext^ Context, SECURITY_INFORMATION SecurityInformation, SecuirtyDescriptor^ ModificationDescriptor);
			UINT32 ReadDirectory(WinFspFileSystem^ FileSystem, FileOpenContext^ Context, String^ Pattern, ReadDirectoryBuffer^ dirBuffer);
			UINT32 GetSecurity(WinFspFileSystem^ FileSystem, FileOpenContext^ Context, SecuirtyDescriptor^% descriptor);
		};

		interface class WinFspReparseOperation {
		public:
			NTSTATUS ResolveReparsePoints(WinFspFileSystem^ FileSystem, String^ FileName, UINT32 ReparsePointIndex, bool ResolveLastPathComponent, PIO_STATUS_BLOCK PIoStatus, PVOID Buffer, PSIZE_T PSize);
			NTSTATUS GetReparsePoint(WinFspFileSystem^ FileSystem, UINT_PTR FileNodeId, String^ FileName, PVOID Buffer, PSIZE_T PSize);
			NTSTATUS SetReparsePoint(WinFspFileSystem^ FileSystem, UINT_PTR FileNodeId, String^ FileName, PVOID Buffer, SIZE_T Size);
			NTSTATUS DeleteReparsePoint(WinFspFileSystem^ FileSystem, UINT_PTR FileNodeId, String^ FileName, PVOID Buffer, SIZE_T Size);
		};
		interface class WinFspStreamOperation {
			//Streams
			NTSTATUS GetStreamInfo(WinFspFileSystem^ FileSystem, UINT_PTR FileNodeId, PVOID Buffer, ULONG Length, PULONG PBytesTransferred);
		};

		WinFspMinimalOperation^ BasicOperaions = nullptr;
		WinFspReparseOperation^ ReparseOpeation = nullptr;
		WinFspStreamOperation^ StreamOperation = nullptr;
		WinFspFileSystem(FSP_FILE_SYSTEM* fs, WinFspMinimalOperation^ operations, WinFspReparseOperation^ reparseOpeation, WinFspStreamOperation^ stream) {
			FileSystem = fs;
			BasicOperaions = operations;
			ReparseOpeation = reparseOpeation;
			StreamOperation = stream;
			_fileNodes = gcnew FileNodeCollection();
		}
	};



	class WinFspFunc
	{

	public:
		// General Function
		static VOID		Cleanup(FSP_FILE_SYSTEM *FileSystem, PVOID FileNode0, PWSTR FileName, ULONG Flags);
		static VOID		Close(FSP_FILE_SYSTEM *FileSystem, PVOID FileNode0);
		static NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem, FSP_FSCTL_VOLUME_INFO *VolumeInfo);
		static NTSTATUS SetVolumeLabel(FSP_FILE_SYSTEM *FileSystem, PWSTR VolumeLabel, FSP_FSCTL_VOLUME_INFO *VolumeInfo);
		static NTSTATUS GetSecurityByName(FSP_FILE_SYSTEM *FileSystem, PWSTR FileName, PUINT32 PFileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize);
		static NTSTATUS Create(FSP_FILE_SYSTEM *FileSystem, PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize, PVOID *PFileNode, FSP_FSCTL_FILE_INFO *FileInfo);
		static NTSTATUS Open(FSP_FILE_SYSTEM *FileSystem, PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, PVOID *PFileNode, FSP_FSCTL_FILE_INFO *FileInfo);
		static NTSTATUS Overwrite(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize, FSP_FSCTL_FILE_INFO *FileInfo);
		static NTSTATUS Read(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length, PULONG PBytesTransferred);
		static NTSTATUS Write(FSP_FILE_SYSTEM *FileSystem, PVOID FileNode0, PVOID Buffer, UINT64 Offset, ULONG Length, BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo, PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo);
		static NTSTATUS Flush(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, FSP_FSCTL_FILE_INFO *FileInfo);
		static NTSTATUS GetFileInfo(FSP_FILE_SYSTEM *FileSystem, PVOID FileNode0, FSP_FSCTL_FILE_INFO *FileInfo);
		static NTSTATUS SetBasicInfo(FSP_FILE_SYSTEM *FileSystem,PVOID FileContext, UINT32 FileAttributes,UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime,FSP_FSCTL_FILE_INFO *FileInfo);
		static NTSTATUS SetFileSize(FSP_FILE_SYSTEM *FileSystem, PVOID FileNode0, UINT64 NewSize, BOOLEAN SetAllocationSize, FSP_FSCTL_FILE_INFO *FileInfo);
		static NTSTATUS CanDelete(FSP_FILE_SYSTEM *FileSystem, PVOID FileNode0, PWSTR FileName);
		static NTSTATUS Rename(FSP_FILE_SYSTEM *FileSystem, PVOID FileNode0, PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists);
		static NTSTATUS SetSecurity(FSP_FILE_SYSTEM *FileSystem, PVOID FileNode0, SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor);
		static NTSTATUS ReadDirectory(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred);
		static NTSTATUS GetSecurity(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize);
		// Reparse point
		static NTSTATUS ResolveReparsePoints(FSP_FILE_SYSTEM *FileSystem, PWSTR FileName, UINT32 ReparsePointIndex, BOOLEAN ResolveLastPathComponent, PIO_STATUS_BLOCK PIoStatus, PVOID Buffer, PSIZE_T PSize);
		static NTSTATUS GetReparsePoint(FSP_FILE_SYSTEM *FileSystem, PVOID FileNode0, PWSTR FileName, PVOID Buffer, PSIZE_T PSize);
		static NTSTATUS SetReparsePoint(FSP_FILE_SYSTEM *FileSystem, PVOID FileNode0, PWSTR FileName, PVOID Buffer, SIZE_T Size);
		static NTSTATUS DeleteReparsePoint(FSP_FILE_SYSTEM *FileSystem, PVOID FileNode0, PWSTR FileName, PVOID Buffer, SIZE_T Size);

		//Streams
		static NTSTATUS GetStreamInfo(FSP_FILE_SYSTEM *FileSystem, PVOID FileNode0, PVOID Buffer, ULONG Length, PULONG PBytesTransferred);
	};


	public   enum MountType {
		NetMount,
		DiskMount
	};

	public ref struct VolumeInfo
	{
		UINT64 TotalSize;
		UINT64 FreeSize;
		String^ VolumeLabel;
	};
	public ref struct WinFspConfig {
		bool IsNetworkMount;
		String^ VolumePrefix;
		String^ RootSecurityDescriptor;
		String^ FileSystemName;
		UINT16 SectorSize;
		UINT16 SectorPerAllocationUnit;
		UINT64 VolumeCreationTime;
		ULONG SerialNumber;
		ULONG FileInfoTimeOut;
		ULONG MaxComponentLength;
		bool CaseSensitive;
		bool CaseSensitiveNames;
		bool UnicodeOnDisk;
		bool PresistACL;
		bool ReparsePoints;
		bool ReparsePointAccessChecks;
		bool NameSteam;
		bool PostCleanupWhenModifiedOnly;
		bool IsReadOnlyVolume;
	};



	public ref class WinFspInstances {
		ConcurrentDictionary<UINT_PTR, WinFspFileSystem^>^ _fspCollection;
		static WinFspInstances^ _instance;
		WinFspInstances() {
			_fspCollection = gcnew ConcurrentDictionary<UINT_PTR, WinFspFileSystem^>();
		}
	public:
		static property WinFspInstances^ Instance {
			WinFspInstances^ get() {
				if (_instance == nullptr)
					_instance = gcnew WinFspInstances();
				return _instance;
			}
		}

		WinFspFileSystem^ GetFileSystem(UINT_PTR Key);
		bool AddFileSystem(UINT_PTR Key, WinFspFileSystem^ fs);
	};

	public ref class WinFsp
	{
	public:
		NTSTATUS StartFs(String^ mountPoint);
		void DeleteFs();
		void StopFs();
		WinFsp(WinFspConfig^ FsConfig, WinFspFileSystem::WinFspMinimalOperation^ operations, WinFspFileSystem::WinFspReparseOperation^ reparseOpeation, WinFspFileSystem::WinFspStreamOperation^ stream);
		inline static UINT64 GetFileTime();
	private:
		IntPtr FileSystemPtr;
		IntPtr VolumeParamsPtr;
		IntPtr FspIntefacePtr;
		IntPtr RootSecurityDescriptor;
		WinFspConfig^ FsConfig;


		NTSTATUS  WinFspCreateFileSystem();
		NTSTATUS  WinFspMountFileSystem();

	};
}