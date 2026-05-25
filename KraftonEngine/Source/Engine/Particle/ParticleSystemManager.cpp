#include "Particle/ParticleSystemManager.h"

#include "Asset/AssetPackage.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Object/ObjectFactory.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/ParticleSpriteEmitter.h"
#include "Particle/ParticleSystem.h"
#include "Particle/TypeData/ParticleModuleTypeDataBeam2.h"
#include "Platform/Paths.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <filesystem>

namespace
{
constexpr int32 ParticleSystemVersion = 2;

namespace ParticleKeys
{
	static constexpr const char* Version = "Version";
	static constexpr const char* Emitters = "Emitters";
	static constexpr const char* LODDistances = "LODDistances";
	static constexpr const char* Name = "Name";
	static constexpr const char* InitialAllocationCount = "InitialAllocationCount";
	static constexpr const char* PeakActiveParticles = "PeakActiveParticles";
	static constexpr const char* LODLevels = "LODLevels";
	static constexpr const char* Level = "Level";
	static constexpr const char* bEnabled = "bEnabled";
	static constexpr const char* Required = "Required";
	static constexpr const char* Spawn = "Spawn";
	static constexpr const char* TypeData = "TypeData";
	static constexpr const char* Modules = "Modules";
	static constexpr const char* Type = "Type";
	static constexpr const char* Material = "Material";
	static constexpr const char* EmitterOrigin = "EmitterOrigin";
	static constexpr const char* ScreenAlignment = "ScreenAlignment";
	static constexpr const char* SubImagesHorizontal = "SubImages_Horizontal";
	static constexpr const char* SubImagesVertical = "SubImages_Vertical";
	static constexpr const char* AlphaSource = "AlphaSource";
	static constexpr const char* AlphaThreshold = "AlphaThreshold";
	static constexpr const char* AlphaPower = "AlphaPower";
	static constexpr const char* ColorIntensity = "ColorIntensity";
	static constexpr const char* SortMode = "SortMode";
	static constexpr const char* EmitterDuration = "EmitterDuration";
	static constexpr const char* MaxDrawCount = "MaxDrawCount";
	static constexpr const char* bUseLocalSpace = "bUseLocalSpace";
	static constexpr const char* bKillOnDeactivate = "bKillOnDeactivate";
	static constexpr const char* bKillOnCompleted = "bKillOnCompleted";
	static constexpr const char* Rate = "Rate";
	static constexpr const char* Lifetime = "Lifetime";
	static constexpr const char* LifetimeMin = "LifetimeMin";
	static constexpr const char* LifetimeMax = "LifetimeMax";
	static constexpr const char* StartLocation = "StartLocation";
	static constexpr const char* StartLocationMin = "StartLocationMin";
	static constexpr const char* StartLocationMax = "StartLocationMax";
	static constexpr const char* StartVelocity = "StartVelocity";
	static constexpr const char* StartVelocityMin = "StartVelocityMin";
	static constexpr const char* StartVelocityMax = "StartVelocityMax";
	static constexpr const char* StartColor = "StartColor";
	static constexpr const char* StartAlpha = "StartAlpha";
	static constexpr const char* StartAlphaMin = "StartAlphaMin";
	static constexpr const char* StartAlphaMax = "StartAlphaMax";
	static constexpr const char* EndColor = "EndColor";
	static constexpr const char* EndAlpha = "EndAlpha";
	static constexpr const char* StartSize = "StartSize";
	static constexpr const char* StartSizeMin = "StartSizeMin";
	static constexpr const char* StartSizeMax = "StartSizeMax";
	static constexpr const char* Beam2 = "Beam2";
	static constexpr const char* BeamMethod = "BeamMethod";
	static constexpr const char* InterpolationPoints = "InterpolationPoints";
	static constexpr const char* Sheets = "Sheets";
	static constexpr const char* MaxBeamCount = "MaxBeamCount";
	static constexpr const char* Speed = "Speed";
	static constexpr const char* bAlwaysOn = "bAlwaysOn";
	static constexpr const char* UpVectorStepSize = "UpVectorStepSize";
	static constexpr const char* Distance = "Distance";
	static constexpr const char* SourcePoint = "SourcePoint";
	static constexpr const char* TargetPoint = "TargetPoint";
	static constexpr const char* Width = "Width";
	static constexpr const char* TextureTile = "TextureTile";
	static constexpr const char* TextureTileDistance = "TextureTileDistance";
	static constexpr const char* Color = "Color";
	static constexpr const char* Alpha = "Alpha";
	static constexpr const char* BranchParentName = "BranchParentName";
	static constexpr const char* TaperMethod = "TaperMethod";
	static constexpr const char* TaperFactor = "TaperFactor";
	static constexpr const char* TaperScale = "TaperScale";
	static constexpr const char* bRenderGeometry = "bRenderGeometry";
	static constexpr const char* bRenderDirectLine = "bRenderDirectLine";
	static constexpr const char* bRenderLines = "bRenderLines";
	static constexpr const char* bRenderTessellation = "bRenderTessellation";
	static constexpr const char* TargetData = "TargetData";
	static constexpr const char* TargetName = "TargetName";
	static constexpr const char* TargetPercentage = "TargetPercentage";
}

json::JSON MakeVectorJSON(const FVector& Value)
{
	return json::Array(Value.X, Value.Y, Value.Z);
}

FVector ReadVectorJSON(json::JSON& Object, const char* Key, const FVector& DefaultValue)
{
	if (!Object.hasKey(Key))
	{
		return DefaultValue;
	}

	json::JSON& Value = Object[Key];
	if (Value.JSONType() != json::JSON::Class::Array || Value.length() < 3)
	{
		return DefaultValue;
	}

	return FVector(
		static_cast<float>(Value[0].ToFloat()),
		static_cast<float>(Value[1].ToFloat()),
		static_cast<float>(Value[2].ToFloat()));
}

FString GetParticleMaterialPath(UMaterialInterface* MaterialInterface)
{
	UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : nullptr;
	return Material ? FPaths::MakeProjectRelative(Material->GetAssetPathFileName()) : FString();
}

UParticleModuleTypeDataBase* FindLODTypeDataModule(UParticleLODLevel* LOD)
{
	if (!LOD)
	{
		return nullptr;
	}

	if (LOD->TypeDataModule)
	{
		return LOD->TypeDataModule;
	}

	for (UParticleModule* Module : LOD->Modules)
	{
		if (UParticleModuleTypeDataBase* TypeData = Cast<UParticleModuleTypeDataBase>(Module))
		{
			return TypeData;
		}
	}
	return nullptr;
}

json::JSON SerializeRequiredModule(UParticleModuleRequired* Required)
{
	json::JSON Object = json::JSON::Make(json::JSON::Class::Object);
	if (!Required)
	{
		return Object;
	}

	Object[ParticleKeys::Material] = GetParticleMaterialPath(Required->Material);
	Object[ParticleKeys::EmitterOrigin] = MakeVectorJSON(Required->EmitterOrigin);
	Object[ParticleKeys::ScreenAlignment] = static_cast<int32>(Required->ScreenAlignment);
	Object[ParticleKeys::SubImagesHorizontal] = Required->SubImages_Horizontal;
	Object[ParticleKeys::SubImagesVertical] = Required->SubImages_Vertical;
	Object[ParticleKeys::AlphaSource] = Required->AlphaSource;
	Object[ParticleKeys::AlphaThreshold] = Required->AlphaThreshold;
	Object[ParticleKeys::AlphaPower] = Required->AlphaPower;
	Object[ParticleKeys::ColorIntensity] = Required->ColorIntensity;
	Object[ParticleKeys::SortMode] = static_cast<int32>(Required->SortMode);
	Object[ParticleKeys::EmitterDuration] = Required->EmitterDuration;
	Object[ParticleKeys::MaxDrawCount] = Required->MaxDrawCount;
	Object[ParticleKeys::bUseLocalSpace] = Required->bUseLocalSpace != 0;
	Object[ParticleKeys::bKillOnDeactivate] = Required->bKillOnDeactivate != 0;
	Object[ParticleKeys::bKillOnCompleted] = Required->bKillOnCompleted != 0;
	return Object;
}

json::JSON SerializeSpawnModule(UParticleModuleSpawn* Spawn)
{
	json::JSON Object = json::JSON::Make(json::JSON::Class::Object);
	if (Spawn)
	{
		Object[ParticleKeys::Rate] = Spawn->Rate;
	}
	return Object;
}

json::JSON SerializeTypeDataModule(UParticleModuleTypeDataBase* TypeData)
{
	json::JSON Object = json::JSON::Make(json::JSON::Class::Object);
	if (!TypeData)
	{
		return Object;
	}

	if (UParticleModuleTypeDataBeam2* Beam = Cast<UParticleModuleTypeDataBeam2>(TypeData))
	{
		Object[ParticleKeys::Type] = ParticleKeys::Beam2;
		Object[ParticleKeys::BeamMethod] = static_cast<int32>(Beam->BeamMethod);
		Object[ParticleKeys::InterpolationPoints] = Beam->InterpolationPoints;
		Object[ParticleKeys::Sheets] = Beam->Sheets;
		Object[ParticleKeys::MaxBeamCount] = Beam->MaxBeamCount;
		Object[ParticleKeys::Speed] = Beam->Speed;
		Object[ParticleKeys::bAlwaysOn] = Beam->bAlwaysOn;
		Object[ParticleKeys::UpVectorStepSize] = Beam->UpVectorStepSize;
		Object[ParticleKeys::Distance] = Beam->Distance;
		Object[ParticleKeys::SourcePoint] = MakeVectorJSON(Beam->SourcePoint);
		Object[ParticleKeys::TargetPoint] = MakeVectorJSON(Beam->TargetPoint);
		Object[ParticleKeys::Width] = Beam->Width;
		Object[ParticleKeys::TextureTile] = Beam->TextureTile;
		Object[ParticleKeys::TextureTileDistance] = Beam->TextureTileDistance;
		Object[ParticleKeys::Color] = MakeVectorJSON(Beam->Color);
		Object[ParticleKeys::Alpha] = Beam->Alpha;
		Object[ParticleKeys::BranchParentName] = Beam->BranchParentName.ToString();
		Object[ParticleKeys::TaperMethod] = static_cast<int32>(Beam->TaperMethod);
		Object[ParticleKeys::TaperFactor] = Beam->TaperFactor;
		Object[ParticleKeys::TaperScale] = Beam->TaperScale;
		Object[ParticleKeys::bRenderGeometry] = Beam->bRenderGeometry;
		Object[ParticleKeys::bRenderDirectLine] = Beam->bRenderDirectLine;
		Object[ParticleKeys::bRenderLines] = Beam->bRenderLines;
		Object[ParticleKeys::bRenderTessellation] = Beam->bRenderTessellation;

		json::JSON Targets = json::Array();
		for (const FBeamTargetData& Target : Beam->TargetData)
		{
			json::JSON TargetObject = json::JSON::Make(json::JSON::Class::Object);
			TargetObject[ParticleKeys::TargetName] = Target.TargetName.ToString();
			TargetObject[ParticleKeys::TargetPercentage] = Target.TargetPercentage;
			Targets.append(TargetObject);
		}
		Object[ParticleKeys::TargetData] = Targets;
	}

	return Object;
}

const char* GetSerializableModuleType(UParticleModule* Module)
{
	if (Module->IsA<UParticleModuleLifetime>()) return "Lifetime";
	if (Module->IsA<UParticleModuleLocation>()) return "InitialLocation";
	if (Module->IsA<UParticleModuleVelocity>()) return "InitialVelocity";
	if (Module->IsA<UParticleModuleColor>()) return "InitialColor";
	if (Module->IsA<UParticleModuleColorOverLife>()) return "ColorOverLife";
	if (Module->IsA<UParticleModuleSize>()) return "InitialSize";
	return nullptr;
}

json::JSON SerializeModule(UParticleModule* Module)
{
	json::JSON Object = json::JSON::Make(json::JSON::Class::Object);
	if (!Module)
	{
		return Object;
	}

	const char* Type = GetSerializableModuleType(Module);
	if (!Type)
	{
		return Object;
	}

	Object[ParticleKeys::Type] = Type;
	Object[ParticleKeys::bEnabled] = Module->bEnabled != 0;

	if (UParticleModuleLifetime* Lifetime = Cast<UParticleModuleLifetime>(Module))
	{
		Object[ParticleKeys::Lifetime] = Lifetime->Lifetime;
		Object[ParticleKeys::LifetimeMin] = Lifetime->LifetimeMin;
		Object[ParticleKeys::LifetimeMax] = Lifetime->LifetimeMax;
	}
	else if (UParticleModuleLocation* Location = Cast<UParticleModuleLocation>(Module))
	{
		Object[ParticleKeys::StartLocation] = MakeVectorJSON(Location->StartLocation);
		Object[ParticleKeys::StartLocationMin] = MakeVectorJSON(Location->StartLocationMin);
		Object[ParticleKeys::StartLocationMax] = MakeVectorJSON(Location->StartLocationMax);
	}
	else if (UParticleModuleVelocity* Velocity = Cast<UParticleModuleVelocity>(Module))
	{
		Object[ParticleKeys::StartVelocity] = MakeVectorJSON(Velocity->StartVelocity);
		Object[ParticleKeys::StartVelocityMin] = MakeVectorJSON(Velocity->StartVelocityMin);
		Object[ParticleKeys::StartVelocityMax] = MakeVectorJSON(Velocity->StartVelocityMax);
	}
	else if (UParticleModuleColor* Color = Cast<UParticleModuleColor>(Module))
	{
		Object[ParticleKeys::StartColor] = MakeVectorJSON(Color->StartColor);
		Object[ParticleKeys::StartAlpha] = Color->StartAlpha;
		Object[ParticleKeys::StartAlphaMin] = Color->StartAlphaMin;
		Object[ParticleKeys::StartAlphaMax] = Color->StartAlphaMax;
	}
	else if (UParticleModuleColorOverLife* ColorOverLife = Cast<UParticleModuleColorOverLife>(Module))
	{
		Object[ParticleKeys::EndColor] = MakeVectorJSON(ColorOverLife->ColorOverLife);
		Object[ParticleKeys::EndAlpha] = ColorOverLife->AlphaOverLife;
	}
	else if (UParticleModuleSize* Size = Cast<UParticleModuleSize>(Module))
	{
		Object[ParticleKeys::StartSize] = MakeVectorJSON(Size->StartSize);
		Object[ParticleKeys::StartSizeMin] = MakeVectorJSON(Size->StartSizeMin);
		Object[ParticleKeys::StartSizeMax] = MakeVectorJSON(Size->StartSizeMax);
	}

	return Object;
}

json::JSON SerializeLODLevel(UParticleLODLevel* LOD)
{
	json::JSON Object = json::JSON::Make(json::JSON::Class::Object);
	if (!LOD)
	{
		return Object;
	}

	Object[ParticleKeys::Level] = LOD->Level;
	Object[ParticleKeys::bEnabled] = LOD->bEnabled != 0;
	Object[ParticleKeys::Required] = SerializeRequiredModule(LOD->RequiredModule);
	Object[ParticleKeys::Spawn] = SerializeSpawnModule(LOD->SpawnModule);
	json::JSON TypeDataObject = SerializeTypeDataModule(FindLODTypeDataModule(LOD));
	if (TypeDataObject.hasKey(ParticleKeys::Type))
	{
		Object[ParticleKeys::TypeData] = TypeDataObject;
	}

	json::JSON Modules = json::Array();
	for (UParticleModule* Module : LOD->Modules)
	{
		json::JSON ModuleObject = SerializeModule(Module);
		if (ModuleObject.hasKey(ParticleKeys::Type))
		{
			Modules.append(ModuleObject);
		}
	}
	Object[ParticleKeys::Modules] = Modules;
	return Object;
}

json::JSON SerializeEmitter(UParticleEmitter* Emitter)
{
	json::JSON Object = json::JSON::Make(json::JSON::Class::Object);
	if (!Emitter)
	{
		return Object;
	}

	Object[ParticleKeys::Name] = Emitter->GetEmitterName().ToString();
	Object[ParticleKeys::InitialAllocationCount] = Emitter->InitialAllocationCount;
	Object[ParticleKeys::PeakActiveParticles] = Emitter->PeakActiveParticles;

	json::JSON LODLevels = json::Array();
	for (UParticleLODLevel* LOD : Emitter->LODLevels)
	{
		LODLevels.append(SerializeLODLevel(LOD));
	}
	Object[ParticleKeys::LODLevels] = LODLevels;
	return Object;
}

json::JSON SerializeParticleSystem(UParticleSystem* ParticleSystem)
{
	json::JSON Root = json::JSON::Make(json::JSON::Class::Object);
	Root[ParticleKeys::Version] = ParticleSystemVersion;

	if (ParticleSystem)
	{
		ParticleSystem->NormalizeLODData();
	}

	json::JSON LODDistances = json::Array();
	if (ParticleSystem)
	{
		for (float Distance : ParticleSystem->GetLODDistances())
		{
			LODDistances.append(Distance);
		}
	}
	Root[ParticleKeys::LODDistances] = LODDistances;

	json::JSON Emitters = json::Array();
	if (ParticleSystem)
	{
		for (UParticleEmitter* Emitter : ParticleSystem->Emitters)
		{
			Emitters.append(SerializeEmitter(Emitter));
		}
	}
	Root[ParticleKeys::Emitters] = Emitters;
	return Root;
}

UParticleModuleRequired* DeserializeRequiredModule(json::JSON& Object, UParticleLODLevel* Outer)
{
	UParticleModuleRequired* Required = GUObjectArray.CreateObject<UParticleModuleRequired>(Outer);
	if (Object.hasKey(ParticleKeys::Material))
	{
		const FString MaterialPath = Object[ParticleKeys::Material].ToString();
		if (!MaterialPath.empty())
		{
			Required->Material = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);
		}
	}
	Required->EmitterOrigin = ReadVectorJSON(Object, ParticleKeys::EmitterOrigin, Required->EmitterOrigin);
	if (Object.hasKey(ParticleKeys::ScreenAlignment))
	{
		const int32 Value = static_cast<int32>(Object[ParticleKeys::ScreenAlignment].ToInt());
		Required->ScreenAlignment = static_cast<EParticleScreenAlignment>(std::clamp(Value, 0, static_cast<int32>(PSA_MAX) - 1));
	}
	if (Object.hasKey(ParticleKeys::SubImagesHorizontal)) Required->SubImages_Horizontal = std::max(1, static_cast<int32>(Object[ParticleKeys::SubImagesHorizontal].ToInt()));
	if (Object.hasKey(ParticleKeys::SubImagesVertical)) Required->SubImages_Vertical = std::max(1, static_cast<int32>(Object[ParticleKeys::SubImagesVertical].ToInt()));
	if (Object.hasKey(ParticleKeys::AlphaSource)) Required->AlphaSource = std::clamp(static_cast<int32>(Object[ParticleKeys::AlphaSource].ToInt()), 0, 1);
	if (Object.hasKey(ParticleKeys::AlphaThreshold)) Required->AlphaThreshold = std::clamp(static_cast<float>(Object[ParticleKeys::AlphaThreshold].ToFloat()), 0.0f, 1.0f);
	if (Object.hasKey(ParticleKeys::AlphaPower)) Required->AlphaPower = std::max(0.001f, static_cast<float>(Object[ParticleKeys::AlphaPower].ToFloat()));
	if (Object.hasKey(ParticleKeys::ColorIntensity)) Required->ColorIntensity = std::max(0.0f, static_cast<float>(Object[ParticleKeys::ColorIntensity].ToFloat()));
	if (Object.hasKey(ParticleKeys::SortMode))
	{
		const int32 Value = static_cast<int32>(Object[ParticleKeys::SortMode].ToInt());
		Required->SortMode = static_cast<EParticleSortMode>(std::clamp(Value, 0, static_cast<int32>(PSORTMODE_MAX) - 1));
	}
	if (Object.hasKey(ParticleKeys::EmitterDuration)) Required->EmitterDuration = std::max(0.0f, static_cast<float>(Object[ParticleKeys::EmitterDuration].ToFloat()));
	if (Object.hasKey(ParticleKeys::MaxDrawCount)) Required->MaxDrawCount = std::max(0, static_cast<int32>(Object[ParticleKeys::MaxDrawCount].ToInt()));
	if (Object.hasKey(ParticleKeys::bUseLocalSpace)) Required->bUseLocalSpace = Object[ParticleKeys::bUseLocalSpace].ToBool();
	if (Object.hasKey(ParticleKeys::bKillOnDeactivate)) Required->bKillOnDeactivate = Object[ParticleKeys::bKillOnDeactivate].ToBool();
	if (Object.hasKey(ParticleKeys::bKillOnCompleted)) Required->bKillOnCompleted = Object[ParticleKeys::bKillOnCompleted].ToBool();
	return Required;
}

UParticleModuleSpawn* DeserializeSpawnModule(json::JSON& Object, UParticleLODLevel* Outer)
{
	UParticleModuleSpawn* Spawn = GUObjectArray.CreateObject<UParticleModuleSpawn>(Outer);
	Spawn->bEnabled = true;
	if (Object.hasKey(ParticleKeys::Rate))
	{
		Spawn->Rate = std::max(0.0f, static_cast<float>(Object[ParticleKeys::Rate].ToFloat()));
	}
	return Spawn;
}

UParticleModuleTypeDataBase* DeserializeTypeDataModule(json::JSON& Object, UParticleLODLevel* Outer)
{
	if (!Object.hasKey(ParticleKeys::Type))
	{
		return nullptr;
	}

	const FString Type = Object[ParticleKeys::Type].ToString();
	if (Type != ParticleKeys::Beam2)
	{
		return nullptr;
	}

	UParticleModuleTypeDataBeam2* Beam = GUObjectArray.CreateObject<UParticleModuleTypeDataBeam2>(Outer);
	if (Object.hasKey(ParticleKeys::BeamMethod))
	{
		const int32 Value = static_cast<int32>(Object[ParticleKeys::BeamMethod].ToInt());
		Beam->BeamMethod = static_cast<EBeam2Method>(std::clamp(Value, 0, static_cast<int32>(PEB2M_MAX) - 1));
	}
	if (Object.hasKey(ParticleKeys::InterpolationPoints)) Beam->InterpolationPoints = std::max(1, static_cast<int32>(Object[ParticleKeys::InterpolationPoints].ToInt()));
	if (Object.hasKey(ParticleKeys::Sheets)) Beam->Sheets = std::max(1, static_cast<int32>(Object[ParticleKeys::Sheets].ToInt()));
	if (Object.hasKey(ParticleKeys::MaxBeamCount)) Beam->MaxBeamCount = std::max(1, static_cast<int32>(Object[ParticleKeys::MaxBeamCount].ToInt()));
	if (Object.hasKey(ParticleKeys::Speed)) Beam->Speed = std::max(0.0f, static_cast<float>(Object[ParticleKeys::Speed].ToFloat()));
	if (Object.hasKey(ParticleKeys::bAlwaysOn)) Beam->bAlwaysOn = Object[ParticleKeys::bAlwaysOn].ToBool();
	if (Object.hasKey(ParticleKeys::UpVectorStepSize)) Beam->UpVectorStepSize = std::max(0, static_cast<int32>(Object[ParticleKeys::UpVectorStepSize].ToInt()));
	if (Object.hasKey(ParticleKeys::Distance)) Beam->Distance = std::max(0.0f, static_cast<float>(Object[ParticleKeys::Distance].ToFloat()));
	Beam->SourcePoint = ReadVectorJSON(Object, ParticleKeys::SourcePoint, Beam->SourcePoint);
	Beam->TargetPoint = ReadVectorJSON(Object, ParticleKeys::TargetPoint, Beam->TargetPoint);
	if (Object.hasKey(ParticleKeys::Width)) Beam->Width = std::max(0.0f, static_cast<float>(Object[ParticleKeys::Width].ToFloat()));
	if (Object.hasKey(ParticleKeys::TextureTile)) Beam->TextureTile = std::max(1, static_cast<int32>(Object[ParticleKeys::TextureTile].ToInt()));
	if (Object.hasKey(ParticleKeys::TextureTileDistance)) Beam->TextureTileDistance = std::max(0.0f, static_cast<float>(Object[ParticleKeys::TextureTileDistance].ToFloat()));
	Beam->Color = ReadVectorJSON(Object, ParticleKeys::Color, Beam->Color);
	if (Object.hasKey(ParticleKeys::Alpha)) Beam->Alpha = std::clamp(static_cast<float>(Object[ParticleKeys::Alpha].ToFloat()), 0.0f, 1.0f);
	if (Object.hasKey(ParticleKeys::BranchParentName)) Beam->BranchParentName = FName(Object[ParticleKeys::BranchParentName].ToString());
	if (Object.hasKey(ParticleKeys::TaperMethod))
	{
		const int32 Value = static_cast<int32>(Object[ParticleKeys::TaperMethod].ToInt());
		Beam->TaperMethod = static_cast<EBeamTaperMethod>(std::clamp(Value, 0, static_cast<int32>(PEBTM_MAX) - 1));
	}
	if (Object.hasKey(ParticleKeys::TaperFactor)) Beam->TaperFactor = std::max(0.0f, static_cast<float>(Object[ParticleKeys::TaperFactor].ToFloat()));
	if (Object.hasKey(ParticleKeys::TaperScale)) Beam->TaperScale = std::max(0.0f, static_cast<float>(Object[ParticleKeys::TaperScale].ToFloat()));
	if (Object.hasKey(ParticleKeys::bRenderGeometry)) Beam->bRenderGeometry = Object[ParticleKeys::bRenderGeometry].ToBool();
	if (Object.hasKey(ParticleKeys::bRenderDirectLine)) Beam->bRenderDirectLine = Object[ParticleKeys::bRenderDirectLine].ToBool();
	if (Object.hasKey(ParticleKeys::bRenderLines)) Beam->bRenderLines = Object[ParticleKeys::bRenderLines].ToBool();
	if (Object.hasKey(ParticleKeys::bRenderTessellation)) Beam->bRenderTessellation = Object[ParticleKeys::bRenderTessellation].ToBool();

	if (Object.hasKey(ParticleKeys::TargetData))
	{
		for (auto& TargetObject : Object[ParticleKeys::TargetData].ArrayRange())
		{
			FBeamTargetData Target;
			if (TargetObject.hasKey(ParticleKeys::TargetName))
			{
				Target.TargetName = FName(TargetObject[ParticleKeys::TargetName].ToString());
			}
			if (TargetObject.hasKey(ParticleKeys::TargetPercentage))
			{
				Target.TargetPercentage = std::clamp(static_cast<float>(TargetObject[ParticleKeys::TargetPercentage].ToFloat()), 0.0f, 100.0f);
			}
			Beam->TargetData.push_back(Target);
		}
	}

	return Beam;
}

UParticleModule* DeserializeModule(json::JSON& Object, UParticleLODLevel* Outer)
{
	if (!Object.hasKey(ParticleKeys::Type))
	{
		return nullptr;
	}

	const FString Type = Object[ParticleKeys::Type].ToString();
	UParticleModule* Module = nullptr;

	if (Type == "Lifetime")
	{
		UParticleModuleLifetime* Lifetime = GUObjectArray.CreateObject<UParticleModuleLifetime>(Outer);
		if (Object.hasKey(ParticleKeys::Lifetime))
		{
			Lifetime->Lifetime = std::max(0.0f, static_cast<float>(Object[ParticleKeys::Lifetime].ToFloat()));
			Lifetime->LifetimeMin = Lifetime->Lifetime;
			Lifetime->LifetimeMax = Lifetime->Lifetime;
		}
		if (Object.hasKey(ParticleKeys::LifetimeMin)) Lifetime->LifetimeMin = std::max(0.0f, static_cast<float>(Object[ParticleKeys::LifetimeMin].ToFloat()));
		if (Object.hasKey(ParticleKeys::LifetimeMax)) Lifetime->LifetimeMax = std::max(0.0f, static_cast<float>(Object[ParticleKeys::LifetimeMax].ToFloat()));
		Module = Lifetime;
	}
	else if (Type == "InitialLocation")
	{
		UParticleModuleLocation* Location = GUObjectArray.CreateObject<UParticleModuleLocation>(Outer);
		Location->StartLocation = ReadVectorJSON(Object, ParticleKeys::StartLocation, Location->StartLocation);
		Location->StartLocationMin = ReadVectorJSON(Object, ParticleKeys::StartLocationMin, Location->StartLocation);
		Location->StartLocationMax = ReadVectorJSON(Object, ParticleKeys::StartLocationMax, Location->StartLocation);
		Module = Location;
	}
	else if (Type == "InitialVelocity")
	{
		UParticleModuleVelocity* Velocity = GUObjectArray.CreateObject<UParticleModuleVelocity>(Outer);
		Velocity->StartVelocity = ReadVectorJSON(Object, ParticleKeys::StartVelocity, Velocity->StartVelocity);
		Velocity->StartVelocityMin = ReadVectorJSON(Object, ParticleKeys::StartVelocityMin, Velocity->StartVelocity);
		Velocity->StartVelocityMax = ReadVectorJSON(Object, ParticleKeys::StartVelocityMax, Velocity->StartVelocity);
		Module = Velocity;
	}
	else if (Type == "InitialColor")
	{
		UParticleModuleColor* Color = GUObjectArray.CreateObject<UParticleModuleColor>(Outer);
		Color->StartColor = ReadVectorJSON(Object, ParticleKeys::StartColor, Color->StartColor);
		if (Object.hasKey(ParticleKeys::StartAlpha))
		{
			Color->StartAlpha = std::clamp(static_cast<float>(Object[ParticleKeys::StartAlpha].ToFloat()), 0.0f, 1.0f);
			Color->StartAlphaMin = Color->StartAlpha;
			Color->StartAlphaMax = Color->StartAlpha;
		}
		if (Object.hasKey(ParticleKeys::StartAlphaMin)) Color->StartAlphaMin = std::clamp(static_cast<float>(Object[ParticleKeys::StartAlphaMin].ToFloat()), 0.0f, 1.0f);
		if (Object.hasKey(ParticleKeys::StartAlphaMax)) Color->StartAlphaMax = std::clamp(static_cast<float>(Object[ParticleKeys::StartAlphaMax].ToFloat()), 0.0f, 1.0f);
		Module = Color;
	}
	else if (Type == "ColorOverLife")
	{
		UParticleModuleColorOverLife* ColorOverLife = GUObjectArray.CreateObject<UParticleModuleColorOverLife>(Outer);
		ColorOverLife->ColorOverLife = ReadVectorJSON(Object, ParticleKeys::EndColor, ColorOverLife->ColorOverLife);
		if (Object.hasKey(ParticleKeys::EndAlpha))
		{
			ColorOverLife->AlphaOverLife = std::clamp(static_cast<float>(Object[ParticleKeys::EndAlpha].ToFloat()), 0.0f, 1.0f);
		}
		Module = ColorOverLife;
	}
	else if (Type == "InitialSize")
	{
		UParticleModuleSize* Size = GUObjectArray.CreateObject<UParticleModuleSize>(Outer);
		Size->StartSize = ReadVectorJSON(Object, ParticleKeys::StartSize, Size->StartSize);
		Size->StartSizeMin = ReadVectorJSON(Object, ParticleKeys::StartSizeMin, Size->StartSize);
		Size->StartSizeMax = ReadVectorJSON(Object, ParticleKeys::StartSizeMax, Size->StartSize);
		Module = Size;
	}

	if (Module && Object.hasKey(ParticleKeys::bEnabled))
	{
		Module->bEnabled = Object[ParticleKeys::bEnabled].ToBool();
	}
	return Module;
}

void RestoreLegacyDisabledModules(UParticleLODLevel* LOD, bool bAllowLegacyRestore)
{
	if (!bAllowLegacyRestore || !LOD || LOD->Modules.empty())
	{
		return;
	}

	bool bHasEnabledModule = false;
	for (UParticleModule* Module : LOD->Modules)
	{
		if (Module && Module->bEnabled)
		{
			bHasEnabledModule = true;
			break;
		}
	}

	if (bHasEnabledModule)
	{
		return;
	}

	// Early particle assets were saved after module support was added, but before
	// newly created/deserialized modules defaulted to enabled. Treat the "all off"
	// state as legacy data so the particle remains visible when opened.
	for (UParticleModule* Module : LOD->Modules)
	{
		if (Module)
		{
			Module->bEnabled = true;
		}
	}
}

UParticleLODLevel* DeserializeLODLevel(json::JSON& Object, UParticleEmitter* Outer, bool bAllowLegacyRestore)
{
	UParticleLODLevel* LOD = GUObjectArray.CreateObject<UParticleLODLevel>(Outer);
	LOD->SetLevelIndex(Object.hasKey(ParticleKeys::Level) ? static_cast<int32>(Object[ParticleKeys::Level].ToInt()) : 0);
	LOD->bEnabled = Object.hasKey(ParticleKeys::bEnabled) ? Object[ParticleKeys::bEnabled].ToBool() : true;

	if (Object.hasKey(ParticleKeys::Required))
	{
		LOD->RequiredModule = DeserializeRequiredModule(Object[ParticleKeys::Required], LOD);
	}
	else
	{
		LOD->RequiredModule = GUObjectArray.CreateObject<UParticleModuleRequired>(LOD);
	}

	if (Object.hasKey(ParticleKeys::Spawn))
	{
		LOD->SpawnModule = DeserializeSpawnModule(Object[ParticleKeys::Spawn], LOD);
	}
	else
	{
		LOD->SpawnModule = GUObjectArray.CreateObject<UParticleModuleSpawn>(LOD);
	}

	if (Object.hasKey(ParticleKeys::TypeData))
	{
		if (UParticleModuleTypeDataBase* TypeData = DeserializeTypeDataModule(Object[ParticleKeys::TypeData], LOD))
		{
			LOD->TypeDataModule = TypeData;
			LOD->Modules.push_back(TypeData);
		}
	}

	if (Object.hasKey(ParticleKeys::Modules))
	{
		for (auto& ModuleObject : Object[ParticleKeys::Modules].ArrayRange())
		{
			if (UParticleModule* Module = DeserializeModule(ModuleObject, LOD))
			{
				LOD->Modules.push_back(Module);
			}
		}
	}

	RestoreLegacyDisabledModules(LOD, bAllowLegacyRestore);
	LOD->UpdateModuleLists();
	return LOD;
}

UParticleEmitter* DeserializeEmitter(json::JSON& Object, UParticleSystem* Outer, bool bAllowLegacyRestore)
{
	UParticleSpriteEmitter* Emitter = GUObjectArray.CreateObject<UParticleSpriteEmitter>(Outer);
	Emitter->SetEmitterName(FName(Object.hasKey(ParticleKeys::Name) ? Object[ParticleKeys::Name].ToString() : FString("Particle Emitter")));
	if (Object.hasKey(ParticleKeys::InitialAllocationCount))
	{
		Emitter->InitialAllocationCount = std::max(0, static_cast<int32>(Object[ParticleKeys::InitialAllocationCount].ToInt()));
	}
	if (Object.hasKey(ParticleKeys::PeakActiveParticles))
	{
		Emitter->PeakActiveParticles = std::max(0, static_cast<int32>(Object[ParticleKeys::PeakActiveParticles].ToInt()));
	}

	if (Object.hasKey(ParticleKeys::LODLevels))
	{
		for (auto& LODObject : Object[ParticleKeys::LODLevels].ArrayRange())
		{
			Emitter->LODLevels.push_back(DeserializeLODLevel(LODObject, Emitter, bAllowLegacyRestore));
		}
	}

	if (Emitter->LODLevels.empty())
	{
		json::JSON DefaultLOD = json::Object();
		Emitter->LODLevels.push_back(DeserializeLODLevel(DefaultLOD, Emitter, bAllowLegacyRestore));
	}

	Emitter->UpdateModuleLists();
	return Emitter;
}

void DeserializeParticleSystem(UParticleSystem* ParticleSystem, const FString& Payload)
{
	if (!ParticleSystem || Payload.empty())
	{
		return;
	}

	json::JSON Root = json::JSON::Load(Payload);
	if (Root.JSONType() != json::JSON::Class::Object || !Root.hasKey(ParticleKeys::Emitters))
	{
		return;
	}

	const int32 Version = Root.hasKey(ParticleKeys::Version) ? static_cast<int32>(Root[ParticleKeys::Version].ToInt()) : 1;
	const bool bAllowLegacyRestore = Version < ParticleSystemVersion;

	if (Root.hasKey(ParticleKeys::LODDistances))
	{
		json::JSON& LODDistances = Root[ParticleKeys::LODDistances];
		if (LODDistances.JSONType() == json::JSON::Class::Array)
		{
			for (int32 Index = 0; Index < static_cast<int32>(LODDistances.length()); ++Index)
			{
				const float Distance = std::max(0.0f, static_cast<float>(LODDistances[Index].ToFloat()));
				if (Index == 0)
				{
					ParticleSystem->SetLODDistance(0, Distance);
				}
				else
				{
					ParticleSystem->CreateLOD(Distance);
				}
			}
		}
	}

	for (auto& EmitterObject : Root[ParticleKeys::Emitters].ArrayRange())
	{
		if (UParticleEmitter* Emitter = DeserializeEmitter(EmitterObject, ParticleSystem, bAllowLegacyRestore))
		{
			ParticleSystem->Emitters.push_back(Emitter);
		}
	}

	if (!Root.hasKey(ParticleKeys::LODDistances))
	{
		int32 MaxLODCount = ParticleSystem->GetLODCount();
		for (UParticleEmitter* Emitter : ParticleSystem->Emitters)
		{
			if (Emitter)
			{
				MaxLODCount = std::max(MaxLODCount, static_cast<int32>(Emitter->LODLevels.size()));
			}
		}
		while (ParticleSystem->GetLODCount() < MaxLODCount)
		{
			ParticleSystem->CreateLOD();
		}
	}

	ParticleSystem->NormalizeLODData();
}
}

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
	DeserializeParticleSystem(ParticleSystem, Payload);

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
	json::JSON Root = SerializeParticleSystem(ParticleSystem);
	return FAssetPackage::SaveStringPayload(Path, EAssetPackageType::ParticleSystem, Metadata, Root.dump());
}
