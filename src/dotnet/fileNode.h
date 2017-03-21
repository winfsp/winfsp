#include<winfsp\winfsp.h>
#include <sddl.h>

using namespace System;
using namespace System::Collections::Concurrent;
using namespace System::Threading;
#define GET_PROPERTY(type,PropName,var) property type PropName { \
										type get() { \
											return var;}}

#define GET_SET_PROPERTY(type,PropName,var) property type PropName { \
											type get() { \
												return var;}\
											void set(type val) {\
												var = val;}}
#pragma once
namespace WinFspNet {


	public ref struct VolInfo {
	public:
		UINT64 TotalSize;
		UINT64 FreeSize;
		String^ VolumeLable;
	};
	[System::Runtime::InteropServices::StructLayout(System::Runtime::InteropServices::LayoutKind::Sequential)]
	public ref struct FileInfo {
		UINT32 FileAttributes;
		UINT32 ReparseTag;
		UINT64 AllocationSize;
		UINT64 FileSize;
		UINT64 CreationTime;
		UINT64 LastAccessTime;
		UINT64 LastWriteTime;
		UINT64 ChangeTime;
		UINT64 IndexNumber;
		UINT32 HardLinks;
	};
	

	public ref class SecuirtyDescriptor {
		IntPtr^ SecurityDescriptorPtr;
		ULONG Length;
		String^ Sddl;

		static bool ConvertToString(PSECURITY_DESCRIPTOR desc, ULONG length, String^% descriptorStr, PULONG error) {
			LPSTR sddlPtr = NULL;
			if (ConvertSecurityDescriptorToStringSecurityDescriptorA(desc, SDDL_REVISION_1, BACKUP_SECURITY_INFORMATION, &sddlPtr, &length)) {
				descriptorStr = gcnew String(sddlPtr);
				return true;
			}
			else {
				*error = GetLastError();
				return false;
			}
		}

		static bool ConvertToDescriptor(String^ descriptrStr, PSECURITY_DESCRIPTOR* ptrDesc, PULONG length, PULONG error) {
			LPCSTR sddlPtr = (LPCSTR)System::Runtime::InteropServices::Marshal::StringToHGlobalAnsi(descriptrStr).ToPointer();
			if (ConvertStringSecurityDescriptorToSecurityDescriptorA(sddlPtr, SDDL_REVISION_1, ptrDesc, length)) {
				return true;
			}
			else {
				*error = GetLastError();
				return false;
			}
		}
	public:
		SecuirtyDescriptor(PSECURITY_DESCRIPTOR ptr, ULONG length) {
			SecurityDescriptorPtr = IntPtr::IntPtr(ptr);
			Length = length;
			ULONG error = 00;
			if (!ConvertToString(ptr, length, Sddl, &error)) {
				Console::WriteLine("ConvertToString Security descriptor failed to convert Error {0}", error);
			}

		}
		SecuirtyDescriptor(String^ sddl) {
			PSECURITY_DESCRIPTOR pDes;
			ULONG length;
			ULONG error = 00;

			if (ConvertToDescriptor(sddl, &pDes, &length, &error))
			{
				Length = length;
				SecurityDescriptorPtr = IntPtr::IntPtr(pDes);
				Sddl = sddl;
			}
			Console::WriteLine("ConvertToString Security descriptor failed to convert Error {0}", sddl, error);
		}
		~SecuirtyDescriptor() {
			PSECURITY_DESCRIPTOR sdes = (PSECURITY_DESCRIPTOR)SecurityDescriptorPtr->ToPointer();
			if (sdes != NULL)
				free(sdes);

		}
		String^ ToString() override {
			PSECURITY_DESCRIPTOR desc = (PSECURITY_DESCRIPTOR)SecurityDescriptorPtr->ToPointer();
			ULONG error = 00;
			if (!ConvertToString(desc, Length, Sddl, &error)) {
				Console::WriteLine("ConvertToString Security descriptor failed to convert Error {0}", error);
				return nullptr;
			}
			return Sddl;

		}
	};
	
	public ref class FileNode {
		FileInfo^ info;		
		String^ fn;
	    INT64 RefCount;
		
	public:
		SecuirtyDescriptor^ Security;
		Object^ UserContext;
		GET_PROPERTY(String^, FileName, fn)
		GET_PROPERTY(INT64, OpenCount, RefCount)
		GET_SET_PROPERTY(FileInfo^, Info, info)
		FileNode(String^ fileName) {
			info = gcnew FileInfo();
			fn = fileName;
			RefCount = 0;
		}
		inline void IncreamentRef() {
			Interlocked::Increment(RefCount);
		}
		inline void DecreamentRef() {
			Interlocked::Decrement(RefCount);
		}

	};
	public ref struct DirItemInfo {
		String^ FileName;
		FileNode^ Info;
	};
	public ref class FileOpenContext {
		FileNode^ NodeVal;
		UINT64 IdVal;
	public:
		Object^ UserContext2;
		GET_PROPERTY(FileNode^, Node, NodeVal)
		GET_PROPERTY(UINT64, NodeId, IdVal)
		FileOpenContext(FileNode^ node, UINT id) {
			NodeVal = node;
			IdVal = id;
		}
	};
	
	ref class FileNodeCollection
	{
	private:
		INT64 _currentId=1;
		ConcurrentDictionary<UINT64, FileOpenContext^>^ contextDict;
		ConcurrentDictionary<String^, FileNode^>^ nodeDic;
	public:
		FileNodeCollection() {
			contextDict = gcnew ConcurrentDictionary<UINT64, FileOpenContext^> ();
			nodeDic= gcnew ConcurrentDictionary<String^, FileNode^>();
		}
		~FileNodeCollection() {
		
		}
		FileNode^ GetOrCreateFileNode(String^ FileName);
		FileNode^ GetFileNode(String^ FileName);
		FileOpenContext^ GetFileOpenContext(UINT64 Id);		
		FileOpenContext^ CreateNewContext(String^ fileName);
		void FileNodeCollection::RemoveContext(UINT64 Id);
	};

}