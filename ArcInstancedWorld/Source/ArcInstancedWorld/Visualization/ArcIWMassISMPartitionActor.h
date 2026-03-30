// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorPartition/PartitionActor.h"
#include "Engine/ActorInstanceManagerInterface.h"
#include "Engine/CollisionProfile.h"
#include "MassEntityTypes.h"
#include "MassEntityManager.h"
#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "MassEntityHandle.h"
#include "ArcIWMassISMPartitionActor.generated.h"

class IArcPersistenceBackend;
class UBodySetup;
class UMassEntityConfigAsset;
class UPCGComponent;
class UInstancedStaticMeshComponent;
class UInstancedSkinnedMeshComponent;
struct FMassExecutionContext;
struct FArcMassSkinnedMeshFragment;
struct FArcMassVisualizationMeshConfigFragment;
struct FArcMassPhysicsBodyConfigFragment;

/** Data captured per mesh activation request during the processor gather phase. */
struct FArcIWMeshActivationRequest
{
	FMassEntityHandle SourceEntity;
	FTransform FinalTransform;
	FArcIWMeshEntry MeshEntry;
	int32 MeshIdx = INDEX_NONE;
	int32 HolderSlot = INDEX_NONE;
	bool bHasOverrideMats = false;
};

/**
 * Partition actor that uses Mass MeshEngine for ISM rendering and standalone
 * FBodyInstance for physics (no ISM components at runtime).
 * Implements IActorInstanceManagerInterface for automatic trace hydration.
 */
UCLASS()
class ARCINSTANCEDWORLD_API AArcIWMassISMPartitionActor : public APartitionActor, public IActorInstanceManagerInterface
{
	GENERATED_BODY()

public:
	AArcIWMassISMPartitionActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Look up grid cell size from the WP runtime hash by grid name.
	 *  Falls back to UArcIWSettings::DefaultGridCellSize. */
	static uint32 GetGridCellSize(UWorld* InWorld, FName InGridName);

#if WITH_EDITOR
	virtual uint32 GetDefaultGridSize(UWorld* InWorld) const override { return GetGridCellSize(InWorld, GridName); }
	virtual FGuid GetGridGuid() const override { return GridGuid; }
	void SetGridGuid(const FGuid& InGuid) { GridGuid = InGuid; }
#endif

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostRegisterAllComponents() override;

#if WITH_EDITOR
	/**
	 * Adds an actor instance to this partition cell.
	 * Finds or creates an entry by (ActorClass, SourcePCGComponent) and appends the transform.
	 */
	void AddActorInstance(
		TSubclassOf<AActor> ActorClass,
		const FTransform& WorldTransform,
		const TArray<FArcIWMeshEntry>& MeshDescriptors,
		UMassEntityConfigAsset* InAdditionalConfig,
		UBodySetup* InCollisionBodySetup,
		const FCollisionProfileName& InCollisionProfile,
		const FBodyInstance& InBodyTemplate,
		int32 InRespawnTimeSeconds = 0,
		EArcMassPhysicsBodyType InPhysicsBodyType = EArcMassPhysicsBodyType::Static,
		UPCGComponent* InSourcePCGComponent = nullptr);

	/**
	 * Removes specific instance transforms from an actor class entry.
	 * Indices must be sorted in ascending order. Uses swap-and-pop removal.
	 * Removes the entire FArcIWActorClassData entry if no transforms remain.
	 * Returns the number of transforms actually removed.
	 */
	int32 RemoveActorInstances(
		TSubclassOf<AActor> ActorClass,
		const TArray<int32>& SortedTransformIndices);

	/**
	 * Removes all entries contributed by the given PCG component.
	 * Returns the number of entries removed.
	 */
	int32 RemoveEntriesBySource(UPCGComponent* SourcePCGComponent);

	/**
	 * Removes entries whose SourcePCGComponent was set but is now stale (destroyed).
	 * Returns true if any entries were removed.
	 */
	bool RemoveStaleEntries();

	/** Set the grid name. Called during editor conversion. */
	void SetGridName(FName InGridName) { GridName = InGridName; }

	/** Create editor-only ISM components to preview instances in the level editor.
	 *  These are stripped in PIE/game worlds. */
	void RebuildEditorPreviewISMCs();
	void DestroyEditorPreviewISMCs();
#endif

	/** Returns the stored per-class data entries. */
	const TArray<FArcIWActorClassData>& GetActorClassEntries() const { return ActorClassEntries; }

	/** Returns all Mass entities spawned by this partition actor. */
	const TArray<FMassEntityHandle>& GetSpawnedEntities() const { return SpawnedEntities; }

	FName GetGridName() const { return GridName; }

	/** Record an instance as removed and destroy its Mass entity.
	 *  Gameplay code calls this — do NOT call during DespawnEntities (stream-out).
	 *  @param ClassIndex Index into ActorClassEntries.
	 *  @param TransformIndex Index into ActorClassEntries[ClassIndex].InstanceTransforms. */
	void MarkInstanceRemoved(int32 ClassIndex, int32 TransformIndex);

	/** Spawn entities for all expired pending respawns. Called by UArcIWRespawnSubsystem. */
	void RestoreExpiredRemovals(int64 NowUtc);

	/** Check if a holder slot index is within bounds. */
	bool IsValidHolderSlot(int32 Slot) const { return ISMHolderEntities.IsValidIndex(Slot); }

	/** Hydrate a specific entity -- terminate physics bodies, spawn actor via pool. No-op if already hydrated. */
	void HydrateEntity(FMassEntityHandle EntityHandle);

	/** Activate entity — add ISM instances to holders. */
	void ActivateEntity(
		FMassEntityHandle Entity,
		FArcIWInstanceFragment& Instance,
		const FArcIWVisConfigFragment& Config,
		const FTransform& WorldTransform,
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context);

	/** Deactivate entity — remove ISM instances from holders. */
	void DeactivateEntity(
		FMassEntityHandle Entity,
		FArcIWInstanceFragment& Instance,
		const FArcIWVisConfigFragment& Config,
		FMassEntityManager& EntityManager);

	/** Batch-activate ISM instances. Adds to existing holders immediately,
	 *  pushes one deferred command for all new holders. */
	void ActivateMesh(
		TArray<FArcIWMeshActivationRequest>& Requests,
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context);

	/** Add ISM instances to holder entities. Called when entity enters MeshAddRadius.
	 *  If the holder entity doesn't exist yet, queues a deferred command to create it. */
	void ActivateMesh(
		FMassEntityHandle Entity,
		FArcIWInstanceFragment& Instance,
		const FArcIWVisConfigFragment& Config,
		const FTransform& WorldTransform,
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context);

	/** Remove ISM instances from holder entities. Called when entity exits MeshRemoveRadius. */
	void DeactivateMesh(
		FMassEntityHandle Entity,
		FArcIWInstanceFragment& Instance,
		const FArcIWVisConfigFragment& Config,
		FMassEntityManager& EntityManager);

	/** Add a SimpleVis entity's mesh to the ISM holder. Creates holder via deferred command on first call. */
	void ActivateSimpleVisMesh(
		FMassEntityHandle Entity,
		FArcIWInstanceFragment& Instance,
		const FArcIWVisConfigFragment& Config,
		const FTransform& WorldTransform,
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context);

	/** Remove a SimpleVis entity's mesh from the ISM holder. */
	void DeactivateSimpleVisMesh(
		FMassEntityHandle Entity,
		FArcIWInstanceFragment& Instance,
		FMassEntityManager& EntityManager);

	/** Add a SimpleVis skinned entity's mesh to the skinned ISM holder. Creates holder via deferred command.
	 *  Returns the holder entity handle for dirty tracking when the holder already exists;
	 *  returns invalid handle when holder creation is deferred (the deferred command handles flush). */
	FMassEntityHandle ActivateSimpleVisSkinnedMesh(
		FMassEntityHandle Entity,
		FArcIWInstanceFragment& Instance,
		const FArcIWVisConfigFragment& Config,
		const FArcMassSkinnedMeshFragment& SkinnedMeshFragment,
		const FArcMassVisualizationMeshConfigFragment& MeshConfigFragment,
		const FTransform& WorldTransform,
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context);

	/** Remove a SimpleVis skinned entity's mesh from the skinned ISM holder.
	 *  Returns the holder entity handle for dirty tracking by the caller. */
	FMassEntityHandle DeactivateSimpleVisSkinnedMesh(
		FMassEntityHandle Entity,
		FArcIWInstanceFragment& Instance,
		FMassEntityManager& EntityManager);

	/** Activate SimpleVis mesh with an explicit mesh entry (lifecycle override path). */
	void ActivateSimpleVisMesh(
		FMassEntityHandle Entity,
		FArcIWInstanceFragment& Instance,
		const FArcIWMeshEntry& ExplicitMeshEntry,
		const FTransform& WorldTransform,
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context);

	/** Activate SimpleVis skinned mesh with an explicit mesh entry (lifecycle override path).
	 *  Returns holder entity handle for dirty tracking. */
	FMassEntityHandle ActivateSimpleVisSkinnedMesh(
		FMassEntityHandle Entity,
		FArcIWInstanceFragment& Instance,
		const FArcIWMeshEntry& ExplicitMeshEntry,
		const FTransform& WorldTransform,
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context);

	/** Atomic static SimpleVis mesh swap — deactivate old, activate with new mesh. */
	void SwapSimpleVisMesh(
		FMassEntityHandle Entity,
		FArcIWInstanceFragment& Instance,
		const FArcIWMeshEntry& NewMeshEntry,
		const FTransform& WorldTransform,
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context);

	/** Atomic skinned SimpleVis mesh swap — deactivate old, activate with new mesh.
	 *  Returns holder entity handle for dirty tracking. */
	FMassEntityHandle SwapSimpleVisSkinnedMesh(
		FMassEntityHandle Entity,
		FArcIWInstanceFragment& Instance,
		const FArcIWMeshEntry& NewMeshEntry,
		const FTransform& WorldTransform,
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context);

	//~ IActorInstanceManagerInterface
	virtual int32 ConvertCollisionIndexToInstanceIndex(int32 InIndex, const UPrimitiveComponent* RelevantComponent) const override;
	virtual AActor* FindActor(const FActorInstanceHandle& Handle) override;
	virtual AActor* FindOrCreateActor(const FActorInstanceHandle& Handle) override;
	virtual UClass* GetRepresentedClass(int32 InstanceIndex) const override;
	virtual ULevel* GetLevelForInstance(int32 InstanceIndex) const override;
	virtual FTransform GetTransform(const FActorInstanceHandle& Handle) const override;
	//~

private:
	void SpawnEntities();
	void DespawnEntities();
	void InitializeISMState();
	void DestroyISMHolderEntities();

	/** Save ClassRemovals and PendingRespawns to persistence backend. Called on stream-out (EndPlay). */
	void SaveRemovals();

	/** Load ClassRemovals and PendingRespawns from persistence backend. Called before SpawnEntities(). */
	void LoadRemovals();

	/** Remove expired entries from PendingRespawns and ClassRemovals. Called after LoadRemovals(), before SpawnEntities(). */
	void PurgeExpiredRemovals();

	/** Spawn a single entity from its original class/transform data and clear it from ClassRemovals. */
	void RestoreInstance(int32 ClassIndex, int32 TransformIndex);

	/** World Partition runtime grid name this partition belongs to. */
	UPROPERTY(VisibleAnywhere, Category = "ArcIW")
	FName GridName = TEXT("MainGrid");

#if WITH_EDITORONLY_DATA
	/** Stored grid GUID for World Partition actor desc matching.
	 *  Set via SetGridGuid() in the creation callback — mirrors AInstancedActorsManager pattern. */
	UPROPERTY()
	FGuid GridGuid;
#endif

	/** Per-actor-class serialized data. */
	UPROPERTY(VisibleAnywhere, Category = "ArcIW")
	TArray<FArcIWActorClassData> ActorClassEntries;

	/** Per-class removal tracking. Parallel to ActorClassEntries — same length, same indices.
	 *  Populated from persistence backend on stream-in, updated by MarkInstanceRemoved(). */
	UPROPERTY()
	TArray<FArcIWClassRemovals> ClassRemovals;

	/** Pending respawns sorted ascending by RespawnAtUtc (chronological append order).
	 *  Entries are added by MarkInstanceRemoved, popped from front when expired. */
	UPROPERTY()
	TArray<FArcIWPendingRespawn> PendingRespawns;

	TMap<FIntPoint, FMassEntityHandle> EntityLookupByIndex;

	IArcPersistenceBackend* GetPersistenceBackend() const;
	int32 ComputeMeshSlotBase(int32 ClassIndex) const;
	TArray<FArcIWPendingRespawn> CollectAndClearExpiredRespawns(int64 NowUtc);

	void SaveEntityData();
	static FGuid MakeDeterministicGuid(const FGuid& PartitionGuid, int32 ClassIndex, int32 TransformIndex);

	/** Flat array of all spawned entity handles. The array index is the instance index
	 *  used by IActorInstanceManagerInterface. */
	TArray<FMassEntityHandle> SpawnedEntities;

	/** Mass MeshEngine ISM holder entities -- one per mesh slot across all classes.
	 *  Layout: [Class0.Mesh0, Class0.Mesh1, ..., Class1.Mesh0, ...].
	 *  Each holds FMassRenderStateFragment + FMassRenderISMFragment for rendering. */
	TArray<FMassEntityHandle> ISMHolderEntities;

	/** ISM holder entities for SimpleVis (single-mesh) classes -- one per single-mesh class.
	 *  Indexed by the per-entity MeshSlotBase field. Created lazily on first mesh activation. */
	TArray<FMassEntityHandle> SimpleVisISMHolders;

	/** ISM holder entities for SimpleVis skinned mesh classes. Indexed by skinned mesh slot index. */
	TArray<FMassEntityHandle> SimpleVisSkinnedISMHolders;

#if WITH_EDITORONLY_DATA
	/** Editor-only ISM components for previewing instances. Stripped in PIE/game. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> EditorPreviewISMCs;

	/** Editor-only skinned ISM components for previewing skinned mesh instances. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedSkinnedMeshComponent>> EditorPreviewSkinnedISMCs;
#endif
};
