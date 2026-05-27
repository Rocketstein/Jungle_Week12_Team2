#include "FObjectProperty.h"
#include "Core/UObject/FObjectPtr.h"
#include "SimpleJSON/json.hpp"

namespace 
{
	// Text key (today): full path name, "Outermost.Outer.Name", matched against
	//                   the object's own GetPathName() walk.
	// Binary key (future): int32 linker index into the archive's export/import
	//                      table. Branch added when FArchive is wired up.
	UObject* ResolveHardReference(const FString& PathKey)
	{
		if (PathKey.empty() || PathKey == FString("None")) return nullptr;

		for (const FUObjectItem& Item : GUObjectArray.GetItems())
		{
			UObject* Obj = Item.Object;
			if (!Obj) continue;
			if (FObjectPtr(Obj).GetPathName() == PathKey) return Obj;
		}
		return nullptr;
	}
} // anonymous namespace

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
	UObject* Obj = static_cast<const FObjectPtr*>(ContainerPtrToValuePtr(Instance))->Get();
	return json::JSON(Obj ? FObjectPtr(Obj).GetPathName() : FString("None"));
}

void FObjectProperty::Deserialize(void* Instance, const json::JSON& Value) const
{
	UObject* Resolved = ResolveHardReference(Value.ToString());
	SetObjectPropertyValue(ContainerPtrToValuePtr(Instance), Resolved);
}

void FObjectProperty::SerializeItem(FArchive& Ar, void* Value, const void* Defaults) const
{
	Ar << *static_cast<FObjectPtr*>(Value);
}