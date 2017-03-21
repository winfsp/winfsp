#include "FileNode.h"


namespace WinFspNet {
	FileNode^ FileNodeCollection::GetOrCreateFileNode(String^ FileName) {
		FileNode^ node=gcnew FileNode(FileName);
		nodeDic->GetOrAdd(FileName, node);
		return node;
	}
	
	FileNode^ FileNodeCollection::GetFileNode(String^ FileName){
		FileNode^ node = nullptr;
		nodeDic->TryGetValue(FileName, node);
		return node;
	}	
	FileOpenContext^ FileNodeCollection::GetFileOpenContext(UINT64 Id) {
		FileOpenContext^ context=nullptr;
		contextDict->TryGetValue(Id, context);
		return context;
	}	
	FileOpenContext^ FileNodeCollection::CreateNewContext(String^ fileName) {
		 FileNode ^node =GetOrCreateFileNode(fileName);
		 UINT Id = Interlocked::Increment(_currentId);
		 FileOpenContext ^context= gcnew FileOpenContext(node, Id);
		 if (contextDict->TryAdd(Id, context))
		 {
			 node->IncreamentRef();
			 return context;
		 }
		 else
			 throw gcnew Exception("Key confilic in open context " + Id);
	}
	void FileNodeCollection::RemoveContext(UINT64 Id) {
		FileOpenContext ^context;
		if (contextDict->TryRemove(Id, context)) {
			context->Node->DecreamentRef();
		}else
			throw gcnew Exception("Context key not found " + Id);
	}
}