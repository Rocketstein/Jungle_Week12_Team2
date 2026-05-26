#include "FObjectPtr.h"

FObjectPtr::FObjectPtr(UObject* InObject)
{
	DebugPtr = InObject;
}

FObjectPtr::FObjectPtr(int32 Index)
{
	const auto& Items = GUObjectArray.GetItems();
	if (Index < Items.size()) DebugPtr = Items[Index].Object;
	else DebugPtr = nullptr;
}

FObjectPtr& FObjectPtr::operator=(UObject* Other)
{
	DebugPtr = Other;
}

FObjectPtr& FObjectPtr::operator=(std::nullptr_t)
{
	Handle   = nullptr;
	DebugPtr = nullptr;
}

UObject* FObjectPtr::Get() const
{
	return DebugPtr;
}

bool FObjectPtr::IsValid() const 
{
	if (DebugPtr) return true;

	return false;
}

UClass* FObjectPtr::GetClass() const 
{
	if (!DebugPtr) return nullptr;
	return DebugPtr->GetClass();
}

FName FObjectPtr::GetFName() const
{
	if (!DebugPtr) return nullptr;
	return DebugPtr->GetFName();
}

FString FObjectPtr::GetName() const 
{
	if (!DebugPtr) return nullptr;
	return DebugPtr->GetName();
}

FString FObjectPtr::GetPathName() const
{

}

FObjectPtr FObjectPtr::GetOuter() const
{
	if (!DebugPtr) return nullptr;
	return FObjectPtr(DebugPtr->GetOuter());
}

FObjectPtr FObjectPtr::GetPackage() const
{

}

bool FObjectPtr::IsIn(FObjectPtr SomeOuter) const
{

}

bool FObjectPtr::IsA(const UClass* SomeBase) const
{

}
