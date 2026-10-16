// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"

class UWorld;
struct FHitResult;

namespace ArcCollisionHelpers
{
	/** Sphere overlap test, any hit (blocking OR overlapping triggers true). */
	ARCMASS_API bool OverlapAnySphere(
		const UWorld* World,
		const FVector& Center,
		float Radius,
		ECollisionChannel Channel,
		const FCollisionQueryParams& Params);

	/** Sphere overlap test, blocking hit only. */
	ARCMASS_API bool OverlapBlockingSphere(
		const UWorld* World,
		const FVector& Center,
		float Radius,
		ECollisionChannel Channel,
		const FCollisionQueryParams& Params);

	/** Line trace, single hit. OutHit populated on success. Returns true if a hit was registered. */
	ARCMASS_API bool LineTrace(
		const UWorld* World,
		FHitResult& OutHit,
		const FVector& Start,
		const FVector& End,
		ECollisionChannel Channel,
		const FCollisionQueryParams& Params);
}
