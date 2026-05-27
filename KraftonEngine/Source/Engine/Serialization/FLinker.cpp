#include "FLinker.h"

//=======================================================
// FLinkerSave
//=======================================================
int32 FLinkerSave::IndexForObject(UObject* Obj)
{
	if (!Obj) return 0;
	ObjectTable.push_back(Obj);
	return ObjectTable.size() - 1;
}

void FLinkerSave::Finalize()
{

}

void FLinkerSave::Serialize(void* Data, size_t Num)
{

}

FArchive& FLinkerSave::operator<<(UObject*& Obj)
{

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

