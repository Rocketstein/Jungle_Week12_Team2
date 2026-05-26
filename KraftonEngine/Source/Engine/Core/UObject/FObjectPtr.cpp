#include "FObjectPtr.h"

FObjectPtr::FObjectPtr(UObject* InObject)
{
	Handle = InObject;
}

FObjectPtr::FObjectPtr(int32 Index)
{
	const auto& Items = GUObjectArray.GetItems();
	if (Index < Items.size()) Handle = Items[Index].Object;
	else Handle = nullptr;
}

FObjectPtr& FObjectPtr::operator=(UObject* Other)
{
	Handle = Other;
}

FObjectPtr& FObjectPtr::operator=(std::nullptr_t)
{
	Handle   = nullptr;
	DebugPtr = nullptr;
}

UObject* FObjectPtr::Get() const
{
	return Handle;
}

bool FObjectPtr::IsValid() const 
{
	if (Handle) return true;

	return false;
}

UClass* FObjectPtr::GetClass() const 
{
	if (!Handle) return nullptr;
	return Handle->GetClass();
}

FName FObjectPtr::GetFName() const
{
	if (!Handle) return nullptr;
	return Handle->GetFName();
}

FString FObjectPtr::GetName() const 
{
	if (!Handle) return "";
	return Handle->GetName();
}

FString FObjectPtr::GetPathName() const
{

}

FObjectPtr FObjectPtr::GetOuter() const
{
	if (!Handle) return nullptr;
	return FObjectPtr(Handle->GetOuter());
}

FObjectPtr FObjectPtr::GetPackage() const
{

}

bool FObjectPtr::IsIn(FObjectPtr SomeOuter) const
{

}

bool FObjectPtr::IsA(const UClass* SomeBase) const
{
	if (!Handle || !SomeBase) return false;
	return Handle->GetClass()->IsChildOf(SomeBase);
}
