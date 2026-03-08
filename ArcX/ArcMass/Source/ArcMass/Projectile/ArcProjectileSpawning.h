// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "ArcProjectileSpawning.generated.h"

class UMassEntityConfigAsset;

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
};

// ---------------------------------------------------------------------------
// Spawn processor — runs once at spawn time to initialize fragments
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcProjectileSpawnProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcProjectileSpawnProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

// ---------------------------------------------------------------------------
// Static helper
// ---------------------------------------------------------------------------

namespace UE::ArcMass::Projectile
{
	ARCMASS_API void BatchSpawnProjectiles(
		UWorld* World,
		UMassEntityConfigAsset* EntityConfig,
		const FArcProjectileSpawnData& SpawnData,
		TArray<FMassEntityHandle>& OutEntities
	);
}
