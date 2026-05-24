#include "Particle/ParticleSystemManager.h"

#include "Asset/AssetPackage.h"
#include "Object/ObjectFactory.h"
#include "Particle/ParticleSystem.h"
#include "Platform/Paths.h"

#include <filesystem>

UParticleSystem* FParticleSystemManager::Load(const FString& Path)
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto It = LoadedParticleSystems.find(NormalizedPath);
	if (It != LoadedParticleSystems.end())
	{
		return It->second;
	}

	if (!FAssetPackage::IsAssetPackagePath(NormalizedPath))
	{
		return nullptr;
	}

	FAssetImportMetadata Metadata;
	FString Payload;
	if (!FAssetPackage::LoadStringPayload(NormalizedPath, EAssetPackageType::ParticleSystem, Metadata, Payload))
	{
		return nullptr;
	}

	UParticleSystem* ParticleSystem = GUObjectArray.CreateObject<UParticleSystem>();
	ParticleSystem->SetAssetPathFileName(NormalizedPath);
	ParticleSystem->SetFName(FName(FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(NormalizedPath)).stem().wstring())));

	LoadedParticleSystems.emplace(NormalizedPath, ParticleSystem);
	return ParticleSystem;
}

UParticleSystem* FParticleSystemManager::Find(const FString& Path) const
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
	auto It = LoadedParticleSystems.find(NormalizedPath);
	return It != LoadedParticleSystems.end() ? It->second : nullptr;
}

bool FParticleSystemManager::Save(UParticleSystem* ParticleSystem)
{
	if (!ParticleSystem)
	{
		return false;
	}

	const FString Path = FPaths::MakeProjectRelative(ParticleSystem->GetAssetPathFileName());
	if (Path.empty())
	{
		return false;
	}

	FAssetImportMetadata Metadata;
	return FAssetPackage::SaveStringPayload(Path, EAssetPackageType::ParticleSystem, Metadata, "{}");
}
