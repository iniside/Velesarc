// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "MassEntityTypes.h"
#include "MassSpawnerTypes.h"
#include "ArcEntitySpawnerTypes.h"
#include "ArcEntitySpawner.generated.h"

struct FStreamableHandle;
struct FArcTQSTargetItem;
struct FArcTQSQueryInstance;
class UArcTQSQueryDefinition;
class UMassAgentComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArcEntitySpawnerOnSpawningFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArcEntitySpawnerOnDespawningFinished);

/**
 * Mass entity spawner with TQS query integration and optional StateTree control.
 *
 * Two spawn modes:
 * - Default: runs a TQS query for spawn positions, then spawns on BeginPlay.
 * - StateTree: a Mass agent runs a StateTree that controls spawning via custom tasks.
 *
 * Unlike AMassSpawner, this spawner holds the EntityCreationContext alive while running
 * initializers, allowing them to modify fragments and queue deferred commands before
 * observers fire.
 */
UCLASS(hidecategories = (Object, Actor, Input, Rendering, LOD, Cooking, Collision, HLOD, Partition))
class ARCENTITYSPAWNER_API AArcEntitySpawner : public AActor
{
	GENERATED_BODY()

public:
	AArcEntitySpawner();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;

public:
	// --- Public API (callable from both modes and StateTree tasks) ---

	/** Start spawning in Default mode. Uses the configured SpawnQuery. */
	UFUNCTION(BlueprintCallable, Category = "Mass|Spawn")
	void DoSpawning();

	/** Spawn entities at locations from TQS query results. */
	void SpawnEntitiesFromResults(TConstArrayView<FArcTQSTargetItem> Results);

	/** Spawn entities at explicit world locations. Uses configured EntityTypes. */
	void SpawnEntitiesAtLocations(int32 InCount, TConstArrayView<FVector> Locations);

	/** Despawn all entities spawned by this spawner. */
	UFUNCTION(BlueprintCallable, Category = "Mass|Spawn")
	void DoDespawning();

	/** Despawn specific entities if they were spawned by this spawner. */
	void DespawnEntities(TConstArrayView<FMassEntityHandle> Entities);

	/** Despawn a single entity if it was spawned by this spawner. Returns true if found and removed. */
	bool DespawnEntity(const FMassEntityHandle Entity);

	/** Get all currently tracked spawned entity handles. */
	void GetAllSpawnedEntities(TArray<FMassEntityHandle>& OutEntities) const;

	/** Called by UArcEntitySpawnerSubsystem when a spawned entity dies. */
	void OnSpawnedEntityDied(const FGuid& EntityGuid, FMassEntityHandle EntityHandle);

	/** Get this spawner's stable GUID. */
	const FGuid& GetSpawnerGuid() const { return SpawnerGuid; }

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
	int32 GetSpawnCount() const;
	void RunInitializers(FMassEntityManager& EntityManager, TArrayView<const FMassEntityHandle> Entities);

	/** Internal: spawn entities from a template, set transforms, run initializers. */
	void SpawnEntitiesInternal(TConstArrayView<FVector> Locations);

	/** Callback for TQS query completion in Default mode. */
	void OnTQSQueryCompleted(FArcTQSQueryInstance& CompletedQuery);

	/** Start StateTree mode: enable Mass agent component. */
	void BeginPlayStateTree();

	/** Start Default mode: run TQS query and spawn. */
	void BeginPlayDefault();

	/** Stamp SpawnerGuid and track PersistenceGuid on freshly spawned entities. */
	void TrackSpawnedEntities(FMassEntityManager& EntityManager, TArrayView<const FMassEntityHandle> Entities);

protected:
	// --- Shared properties (both modes) ---

	/** Spawn mode. Default uses TQS query + auto-spawn. StateTree uses Mass agent. */
	UPROPERTY(EditAnywhere, Category = "Mass|Spawn")
	EArcEntitySpawnMode SpawnMode = EArcEntitySpawnMode::Default;

	/** Entity types to spawn with proportion weights. */
	UPROPERTY(EditAnywhere, Category = "Mass|Spawn")
	TArray<FMassSpawnedEntityType> EntityTypes;

	/** Post-spawn initializers that run while the creation context is still alive. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Mass|Spawn")
	TArray<TObjectPtr<UArcEntityInitializer>> PostSpawnInitializers;

	// --- Default mode properties ---

	/** Number of entities to spawn. */
	UPROPERTY(EditAnywhere, Category = "Mass|Spawn|Default", meta = (EditCondition = "SpawnMode == EArcEntitySpawnMode::Default", EditConditionHides))
	int32 Count = 10;

	/** Scale multiplier for spawn count. */
	UPROPERTY(EditAnywhere, Category = "Mass|Spawn|Default", meta = (EditCondition = "SpawnMode == EArcEntitySpawnMode::Default", EditConditionHides))
	float SpawningCountScale = 1.0f;

	/** TQS query definition for generating spawn locations. */
	UPROPERTY(EditAnywhere, Category = "Mass|Spawn|Default", meta = (EditCondition = "SpawnMode == EArcEntitySpawnMode::Default", EditConditionHides))
	TObjectPtr<UArcTQSQueryDefinition> SpawnQuery;

	/** Whether to auto-spawn on BeginPlay in Default mode. */
	UPROPERTY(EditAnywhere, Category = "Mass|Spawn|Default", meta = (EditCondition = "SpawnMode == EArcEntitySpawnMode::Default", EditConditionHides))
	uint32 bAutoSpawnOnBeginPlay : 1;

	// --- StateTree mode properties ---

	/** Mass agent component for StateTree mode. Configure its EntityConfig with StateTree + Transform traits. */
	UPROPERTY(VisibleAnywhere, Category = "Mass|Spawn|StateTree", meta = (EditCondition = "SpawnMode == EArcEntitySpawnMode::StateTree", EditConditionHides))
	TObjectPtr<UMassAgentComponent> AgentComponent;

	// --- Persistence properties ---

	/** Stable GUID for this spawner. Generated once, persisted across save/load. */
	UPROPERTY(SaveGame)
	FGuid SpawnerGuid;

	/** Stable GUIDs of all currently alive spawned entities. */
	UPROPERTY(SaveGame)
	TArray<FGuid> SpawnedEntityGuids;

	/** Whether this spawner has performed its initial spawn. */
	UPROPERTY(SaveGame)
	bool bHasSpawned = false;

	/** Whether to automatically spawn fresh replacements when entities die. */
	UPROPERTY(EditAnywhere, Category = "Mass|Spawn|Persistence")
	bool bAutoRespawnOnDeath = true;

private:
	struct FSpawnedEntities
	{
		FMassEntityTemplateID TemplateID;
		TArray<FMassEntityHandle> Entities;
	};

	TArray<FSpawnedEntities> AllSpawnedEntities;
	TSharedPtr<FStreamableHandle> StreamingHandle;
	FDelegateHandle SimulationStartedHandle;
	int32 ActiveTQSQueryId = INDEX_NONE;
};
