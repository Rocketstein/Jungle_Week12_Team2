#pragma once

#include "Component/PrimitiveComponent.h"
#include "FXSystemComponent.generated.h"

class UFXSystemAsset;

UCLASS(HiddenInComponentList)
class UFXSystemComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UFXSystemComponent)

	virtual UFXSystemAsset* GetFXSystemAsset() const { return nullptr; }
};
