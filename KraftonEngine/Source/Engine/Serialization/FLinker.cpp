#include "FLinker.h"

//=======================================================
// FLinkerSave
//=======================================================
int32 FLinkerSave::IndexForObject(UObject* Obj)
{

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

}

void FLinkerLoad::Serialize(void* Data, size_t Num)
{

}

FArchive& FLinkerLoad::operator<<(UObject*& Obj)
{

}

