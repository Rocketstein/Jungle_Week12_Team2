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
	int32 Count = 0;
	*Inner << Count;

	ObjectTable.clear();
	ObjectTable.reserve(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		FString Path;
		*Inner << Path;
		ObjectTable.push_back(FindObjectByPath(Path));
		// Note: FindObjectByPath returning nullptr is fine. We store nullptr
		// in that slot, and any property pointing at it deserializes to null.
	}
}

UObject* FLinkerLoad::ObjectForIndex(int32 Index) const
{
	if (Index <= 0) return nullptr;                              // 0 = null sentinel
	if (Index > static_cast<int32>(ObjectTable.size())) return nullptr;
	return ObjectTable[Index - 1];
}

void FLinkerLoad::Serialize(void* Data, size_t Num)
{
	Inner->Serialize(Data, Num);
}

FArchive& FLinkerLoad::operator<<(UObject*& Obj)
{
	int32 Idx = 0;
	*this << Idx;
	Obj = ObjectForIndex(Idx);
	return *this;
}

