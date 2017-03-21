#pragma once
#include<winfsp\winfsp.h>
#include<sddl.h>
#include "FileNode.h"

using System::Runtime::InteropServices::Marshal;
#define COPY_FILE_INFO(fInfo,PtrInfo)  IntPtr pnt = Marshal::AllocHGlobal(Marshal::SizeOf(fInfo)); \
									   Marshal::StructureToPtr(fInfo, pnt, false); \
									   memcpy(PtrInfo, pnt.ToPointer(), sizeof FSP_FSCTL_FILE_INFO);Marshal::FreeHGlobal(pnt);
									   

using namespace System;

namespace WinFspNet {

	public ref struct OpenFileInfo {
		OpenFileInfo(FSP_FSCTL_FILE_INFO* ptr) {

		}
	};


	public ref class ReadDirectoryContext {			
	public:
		String^ Marker;
		ULONG Length;		
	internal:
		PVOID BufferRet;		
		PULONG PBytesTransferred;			
	};
	public ref class ReadDirectoryBuffer {
	public:
		ReadDirectoryContext^ Context;
	internal:		
		ReadDirectoryBuffer(ReadDirectoryContext^ context) {
			Context = context;
		}		
	public:
		bool AddItem(String^ fname, FileInfo^ Info);
		void SetEof();
	};

	public ref class ReadFileBuffer {		
	public:
		IntPtr^ BufferRet;
		UINT64 Offset;
		ULONG Length;
		PULONG ByteTransfered;
		ReadFileBuffer(PVOID buffer, ULONG length, UINT64 offset, PULONG byteTransfered);
		void FillBuffer(array<byte>^% buffer, ULONG retBufferOffset, ULONG length); ;
	internal:
		ULONG GetByetTransfered();

	};

	public ref class WriteFileBuffer {
		PVOID Buffer;
		PULONG PBytesTransferred;
		array<System::Byte>^ data;
	public:
		const bool WriteToEndOfFile;
		const bool ConstrainedIo;
		const UINT64 Offset;
		const ULONG Length;

		void SetTransferedBytes(ULONG byteCount);
		array<byte>^ GetData();
		WriteFileBuffer::WriteFileBuffer(bool eof, bool constraindedIol,PVOID buffer, UINT64 offset, ULONG length, PULONG pByteTtransferred) :
			WriteToEndOfFile(eof),
			ConstrainedIo(constraindedIol),
			Offset(offset),
			Length(length),
			Buffer(buffer),
			PBytesTransferred(pByteTtransferred)

		{
			*pByteTtransferred = 0;
		}


	};

}

