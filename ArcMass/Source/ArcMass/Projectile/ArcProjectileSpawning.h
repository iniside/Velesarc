// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMass/Actions/ArcMassActionFragment.h"
#include "Mass/EntityHandle.h"
#include "ArcProjectileSpawning.generated.h"

class UArcMassEntityConfigAsset;

// ---------------------------------------------------------------------------
// Spawn data — per-entity initialization arrays
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcProjectileSpawnData
{
	GENERATED_BODY()

	TArray<FTransform> Transforms;
	TArray<FVector> Velocities;
	TArray<TWeakObjectPtr<UObject>> Instigators;
	TArray<TArray<TWeakObjectPtr<AActor>>> IgnoredActors;
	TArray<FMassEntityHandle> TargetEntities;
	TArray<FArcMassActionFragment> ActionFragments;
};

// ---------------------------------------------------------------------------
// Static helper
// ---------------------------------------------------------------------------

namespace UE::ArcMass::Projectile
{
	ARCMASS_API void BatchSpawnProjectiles(
		UWorld* World,
		UArcMassEntityConfigAsset* EntityConfig,
		const FArcProjectileSpawnData& SpawnData,
		TArray<FMassEntityHandle>& OutEntities
	);
}
