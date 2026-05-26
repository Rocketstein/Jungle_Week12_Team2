#pragma once
#include "Core/CoreTypes.h"
#include "Math/Vector.h"

class AActor;
class UPrimitiveComponent;

struct FParticleEventCollideData
{
	int32 EmitterIndex = -1;
	int32 ParticleIndex = -1;
	uint16 ParticleDirectIndex = 0;
	uint32 ParticleId = 0;

	FVector Location = FVector::ZeroVector;
	FVector OldLocation = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector Normal = FVector::ZeroVector;
	float EmitterTime = 0.0f;
	float ParticleRelativeTime = 0.0f;
	float HitTime = 0.0f;
	bool bParticleWasKilled = false;

	AActor* HitActor = nullptr;
	UPrimitiveComponent* HitComponent = nullptr;
};
