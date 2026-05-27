#include "FLinker.h"
#include "Object/Object.h"

//=======================================================
// FLinkerSave
//=======================================================
int32 FLinkerSave::IndexForObject(UObject* Obj)
{
	if (!Obj) return 0;

	// Dedup: if we've seen this object before, return its existing index.
	auto It = ObjectToIndex.find(Obj);
	if (It != ObjectToIndex.end())
	{
		return It->second;
	}

	// First sighting: append to table, assign next 1-based index.
	ObjectTable.push_back(Obj);
	int32 NewIndex = static_cast<int32>(ObjectTable.size());
	ObjectToIndex.insert({ Obj, NewIndex });
	return NewIndex;
}

void FLinkerSave::Finalize()
{
	int32 Count = static_cast<int32>(ObjectTable.size());
	*Inner << Count;

	for (UObject* Obj : ObjectTable)
	{
		FString Path = Obj ? Obj->GetPathName() : FString("None");
		*Inner << Path;
	}

	if (!SaveBuffer.empty())
	{
		Inner->Serialize(SaveBuffer.data(), SaveBuffer.size());
	}

}

void FLinkerSave::Serialize(void* Data, size_t Num)
{
	// All property writes land here. Append to buffer; Finalize flushes later.
	const uint8* Bytes = static_cast<const uint8*>(Data);
	SaveBuffer.insert(SaveBuffer.end(), Bytes, Bytes + Num);
}

FArchive& FLinkerSave::operator<<(UObject*& Obj)
{
	int32 Idx = IndexForObject(Obj);
	*this << Idx;
	return *this;
}


//=======================================================
// FLinkerLoad
//=======================================================
void FLinkerLoad::ReadTable()
{

}

UObject* FLinkerLoad::ObjectForIndex(int32 Index) const
{
	if (Index >= ObjectTable.size()) return nullptr;
	return ObjectTable[Index];
}

void FLinkerLoad::Serialize(void* Data, size_t Num)
{

}

FArchive& FLinkerLoad::operator<<(UObject*& Obj)
{

}

