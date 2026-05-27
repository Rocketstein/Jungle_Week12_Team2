#include "FObjectProperty.h"
#include "Core/UObject/FObjectPtr.h"

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