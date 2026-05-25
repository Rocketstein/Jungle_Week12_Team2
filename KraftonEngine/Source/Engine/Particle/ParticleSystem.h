#pragma once

#include "Object/Object.h"
#include "ParticleSystem.generated.h"

class UParticleEmitter;

UCLASS()
class UFXSystemAsset : public UObject
{
public:
	GENERATED_BODY(UFXSystemAsset)
};

UCLASS()
class UParticleSystem : public UFXSystemAsset
{
public:
	GENERATED_BODY(UParticleSystem)

	const FString& GetAssetPathFileName() const override { return AssetPathFileName; }
	void SetAssetPathFileName(const FString& InPath) { AssetPathFileName = InPath; }

	TArray<UParticleEmitter*> Emitters;

private:
	FString AssetPathFileName;
};
