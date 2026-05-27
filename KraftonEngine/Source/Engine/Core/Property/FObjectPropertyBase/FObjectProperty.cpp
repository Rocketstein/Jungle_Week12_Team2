#include "FObjectProperty.h"
#include "Core/UObject/FObjectPtr.h"
#include "SimpleJSON/json.hpp"

UObject* FObjectProperty::GetObjectPropertyValue(void* Addr) const
{
	return static_cast<FObjectPtr*>(Addr)->Get();
}

void FObjectProperty::SetObjectPropertyValue(void* Addr, UObject* Value) const
{
	auto* Ptr = static_cast<FObjectPtr*>(Addr);
	if (!Value || (PropertyClass && !Value->IsA(PropertyClass)))
	{
		Ptr->Reset();
		return;
	}
	*Ptr = Value;
}

json::JSON FObjectProperty::Serialize(const void* Instance) const
{
	return json::JSON(
		static_cast<const FObjectPtr*>(ContainerPtrToValuePtr(Instance))->GetPathName());
}

void FObjectProperty::Deserialize(void* Instance, const json::JSON& Value) const
{
	static_cast<FObjectPtr*>(ContainerPtrToValuePtr(Instance))->SetPath(Value.ToString());
}

void FObjectProperty::SerializeItem(FArchive& Ar, void* Value, const void* Defaults) const
{
	Ar << *static_cast<FObjectPtr*>(Value);
}