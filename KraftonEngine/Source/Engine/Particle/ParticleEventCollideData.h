#pragma once
#include "Core/CoreTypes.h"
#include "Math/Vector.h"

class AActor;
class UPrimitiveComponent;

struct FParticleEventCollideData
{
	int32 EmitterIndex = -1;
	int32 ParticleIndex = -1;

	FVector Location = FVector::ZeroVector;
	FVector OldLocation = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector Normal = FVector::ZeroVector;

	AActor* HitActor = nullptr;
	UPrimitiveComponent* HitComponent = nullptr;
};
