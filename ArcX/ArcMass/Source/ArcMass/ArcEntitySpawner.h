// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "MassSpawnerTypes.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "ArcEntitySpawnerTypes.h"
#include "ArcEntitySpawner.generated.h"

struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArcEntitySpawnerOnSpawningFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArcEntitySpawnerOnDespawningFinished);

/**
 * Mass entity spawner with post-spawn initializer support.
 * Unlike AMassSpawner, this spawner holds the EntityCreationContext alive while running
 * initializers, allowing them to modify fragments and queue deferred commands before
 * observers fire.
 */
UCLASS(hidecategories = (Object, Actor, Input, Rendering, LOD, Cooking, Collision, HLOD, Partition))
class ARCMASS_API AArcEntitySpawner : public AActor
{
	GENERATED_BODY()

public:
	AArcEntitySpawner();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;

public:
	/** Start spawning all configured entity types. */
	UFUNCTION(BlueprintCallable, Category = "Mass|Spawn")
	void DoSpawning();

	/** Despawn all entities spawned by this spawner. */
	UFUNCTION(BlueprintCallable, Category = "Mass|Spawn")
	void DoDespawning();

	/** Despawn a single entity if it was spawned by this spawner. Returns true if found and removed. */
	bool DespawnEntity(const FMassEntityHandle Entity);

	UFUNCTION(BlueprintCallable, Category = "Mass|Spawn")
	int32 GetCount() const { return Count; }

	UFUNCTION(BlueprintCallable, Category = "Mass|Spawn")
	float GetSpawningCountScale() const { return SpawningCountScale; }

	UFUNCTION(BlueprintCallable, Category = "Mass|Spawn")
	void ScaleSpawningCount(float Scale) { SpawningCountScale = Scale; }

	UPROPERTY(BlueprintAssignable)
	FArcEntitySpawnerOnSpawningFinished OnSpawningFinished;

	UPROPERTY(BlueprintAssignable)
	FArcEntitySpawnerOnDespawningFinished OnDespawningFinished;

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Debug")
	void DEBUG_Spawn();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Debug")
	void DEBUG_Clear();
#endif

protected:
	void SpawnGeneratedEntities(TConstArrayView<FMassEntitySpawnDataGeneratorResult> Results);
	void OnSpawnDataGenerationFinished(TConstArrayView<FMassEntitySpawnDataGeneratorResult> Results, FMassSpawnDataGenerator* FinishedGenerator);
	int32 GetSpawnCount() const;

	void RunInitializers(FMassEntityManager& EntityManager, TArrayView<const FMassEntityHandle> Entities);

protected:
	/** Number of entities to spawn. */
	UPROPERTY(EditAnywhere, Category = "Mass|Spawn")
	int32 Count = 10;

	/** Entity types to spawn with proportion weights. Uses the same FMassSpawnedEntityType as AMassSpawner. */
	UPROPERTY(EditAnywhere, Category = "Mass|Spawn")
	TArray<FMassSpawnedEntityType> EntityTypes;

	/** Spawn data generators (EQS, ZoneGraph, etc). Reuses engine's FMassSpawnDataGenerator. */
	UPROPERTY(EditAnywhere, Category = "Mass|Spawn")
	TArray<FMassSpawnDataGenerator> SpawnDataGenerators;

	/** Post-spawn initializers that run while the creation context is still alive. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Mass|Spawn")
	TArray<TObjectPtr<UArcEntityInitializer>> PostSpawnInitializers;

	/** Scale multiplier for spawn count. */
	UPROPERTY(EditAnywhere, Category = "Mass|Spawn")
	float SpawningCountScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Mass|Spawn")
	uint32 bAutoSpawnOnBeginPlay : 1;

private:
	struct FSpawnedEntities
	{
		FMassEntityTemplateID TemplateID;
		TArray<FMassEntityHandle> Entities;
	};

	TArray<FSpawnedEntities> AllSpawnedEntities;
	TArray<FMassEntitySpawnDataGeneratorResult> AllGeneratedResults;
	TSharedPtr<FStreamableHandle> StreamingHandle;
	FDelegateHandle SimulationStartedHandle;
};
