#include "interop.h"


namespace WinFspNet {
	
	/*Read Buffer function*/
	ReadFileBuffer::ReadFileBuffer(PVOID buffer, ULONG length, UINT64 offset, PULONG byteTransfered) {
		BufferRet = IntPtr::IntPtr(buffer);
		Offset = offset;
		Length = length;
		ByteTransfered = byteTransfered;
		*ByteTransfered = 0;
	}

	void ReadFileBuffer::FillBuffer(array<byte>^% buffer, ULONG retBufferOffset, ULONG length) {
		pin_ptr<byte> ptr = &buffer[0];
		PUCHAR ptrRet = (PUCHAR)BufferRet->ToPointer();
		memcpy(ptrRet + retBufferOffset, ptr, length);
		*ByteTransfered += length;
	}
	ULONG ReadFileBuffer::GetByetTransfered() {
		return *ByteTransfered;
	}



	/*Write buffer functions*/
	void WriteFileBuffer::SetTransferedBytes(ULONG byteCount) {
		*PBytesTransferred = byteCount;
	}
	array<byte>^ WriteFileBuffer::GetData() {
		if (data == nullptr) {
			data = gcnew array<System::Byte>(Length);
			pin_ptr<byte> pin_data = &data[0];
			memcpy(pin_data, Buffer, Length);
		}
		return data;
	}
	/*Directorty read buffer function*/
	void ReadDirectoryBuffer::SetEof() {
		FspFileSystemAddDirInfo(0, Context->BufferRet, Context->Length, Context->PBytesTransferred);
	}

	bool ReadDirectoryBuffer::AddItem(String^ fname, FileInfo^ Info) {	
		PWSTR fileName = (PWSTR)System::Runtime::InteropServices::Marshal::StringToHGlobalUni(fname).ToPointer();
		UINT8 *DirInfoBuf=(UINT8*)malloc(sizeof(FSP_FSCTL_DIR_INFO) + fname->Length * sizeof WCHAR);

		FSP_FSCTL_DIR_INFO *DirInfo = (FSP_FSCTL_DIR_INFO *)DirInfoBuf;
		memset(DirInfo->Padding, 0, sizeof DirInfo->Padding);
		
		DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + wcslen(fileName) * sizeof(WCHAR));				
		COPY_FILE_INFO(Info,&DirInfo->FileInfo);								
		memcpy(DirInfo->FileNameBuf, fileName, DirInfo->Size - sizeof(FSP_FSCTL_DIR_INFO));


		//Marshal::FreeHGlobal(IntPtr::IntPtr(fileName));
		return FspFileSystemAddDirInfo(DirInfo, Context->BufferRet, Context->Length, Context->PBytesTransferred);
	}



}