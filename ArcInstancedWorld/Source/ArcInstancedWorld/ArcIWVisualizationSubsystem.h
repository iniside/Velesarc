// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "MassEntityManager.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ArcIWVisualizationSubsystem.generated.h"

struct FArcIWMeshEntry;

// ---------------------------------------------------------------------------
// UArcIWVisualizationSubsystem
// ---------------------------------------------------------------------------

/**
 * World subsystem managing the visualization grid, per-cell ISM manager actors,
 * and composite ISM instance add/remove with swap-fixup.
 *
 * Not created on dedicated servers.
 */
UCLASS()
class ARCINSTANCEDWORLD_API UArcIWVisualizationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- USubsystem ---
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Grid ---
	FIntVector WorldToCell(const FVector& WorldPos) const;
	void RegisterEntity(FMassEntityHandle Entity, const FVector& Position);
	void UnregisterEntity(FMassEntityHandle Entity, const FIntVector& Cell);
	const TArray<FMassEntityHandle>* GetEntitiesInCell(const FIntVector& Cell) const;

	// --- Composite ISM Management ---

	/**
	 * Add ISM instances for a composite entity. For each mesh entry, gets/creates
	 * an ISM component on the cell manager actor, adds the instance with
	 * WorldTransform * RelativeTransform, and writes back the instance ID.
	 *
	 * @param Cell            Grid cell for this entity.
	 * @param EntityHandle    The owning Mass entity.
	 * @param WorldTransform  World transform of the entity root.
	 * @param MeshDescriptors Per-component mesh data (mesh, materials, relative transform).
	 * @param InOutISMInstanceIds  Array to receive per-entry ISM instance IDs (resized to match MeshDescriptors).
	 */
	void AddCompositeISMInstances(
		const FIntVector& Cell,
		FMassEntityHandle EntityHandle,
		const FTransform& WorldTransform,
		const TArray<FArcIWMeshEntry>& MeshDescriptors,
		TArray<int32>& InOutISMInstanceIds,
		int32 InClassIndex = INDEX_NONE,
		int32 InEntityIndex = INDEX_NONE);

	/**
	 * Remove ISM instances for a composite entity. Performs swap-fixup so the
	 * swapped entity's FArcIWInstanceFragment::ISMInstanceIds stays correct.
	 *
	 * @param Cell            Grid cell.
	 * @param MeshDescriptors Mesh data (must match the order used during Add).
	 * @param ISMInstanceIds  Per-entry ISM instance IDs to remove.
	 * @param EntityManager   Entity manager for swap-fixup fragment writes.
	 */
	void RemoveCompositeISMInstances(
		FMassEntityHandle EntityHandle,
		const TArray<FArcIWMeshEntry>& MeshDescriptors,
		const TArray<int32>& ISMInstanceIds,
		FMassEntityManager& EntityManager);

	// --- Cell Tracking ---
	void UpdatePlayerCell(const FIntVector& NewCell);
	FIntVector GetLastPlayerCell() const { return LastPlayerCell; }
	bool IsCellActive(const FIntVector& Cell) const;
	void GetActiveCells(TArray<FIntVector>& OutCells) const;
	void GetCellsInRadius(const FIntVector& Center, int32 RadiusCells, TArray<FIntVector>& OutCells) const;

	// --- Config ---
	float GetCellSize() const { return CellSize; }
	float GetSwapRadius() const { return SwapRadius; }
	int32 GetSwapRadiusCells() const { return SwapRadiusCells; }
	float GetActorRadius() const { return ActorRadius; }
	int32 GetActorRadiusCells() const { return ActorRadiusCells; }
	float GetDehydrationRadius() const { return DehydrationRadius; }
	int32 GetDehydrationRadiusCells() const { return DehydrationRadiusCells; }
	bool IsActorCell(const FIntVector& Cell) const;
	bool IsWithinDehydrationRadius(const FIntVector& Cell) const;

	/** Read-only access to the entity grid for debugging. */
	const TMap<FIntVector, TArray<FMassEntityHandle>>& GetCellEntities() const { return CellEntities; }

	/** Register a partition actor as the ISM component owner for its entities. */
	void RegisterPartitionActor(AActor* PartitionActor, const TArray<FMassEntityHandle>& OwnedEntities);
	/** Unregister a partition actor. */
	void UnregisterPartitionActor(AActor* PartitionActor);

	// --- Trace Resolution ---

	/**
	 * Resolve a hit result on an ArcIW ISM component to the Mass entity that owns that instance.
	 * Uses FHitResult.Component (the ISM component) and FHitResult.Item (the ISM instance index).
	 * If bHydrateOnHit is enabled, also spawns the actor for the hit entity.
	 *
	 * @param HitResult  The hit result from a trace.
	 * @return The entity handle, or invalid handle if the hit is not on an ArcIW ISM.
	 */
	FMassEntityHandle ResolveHitToEntity(const FHitResult& HitResult);

	/**
	 * Hydrate a specific entity — remove ISM, spawn actor via pool.
	 * No-op if entity is already in actor representation.
	 *
	 * @param EntityHandle  The entity to hydrate.
	 */
	void HydrateEntity(FMassEntityHandle EntityHandle);

	/**
	 * Resolve an ISM component + instance index to the Mass entity that owns it.
	 *
	 * @param ISMComponent  The instanced static mesh component that was hit.
	 * @param InstanceIndex  The instance index within that ISM component (FHitResult.Item).
	 * @return The entity handle, or invalid handle if not found.
	 */
	FMassEntityHandle ResolveISMInstanceToEntity(const UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex) const;

	/** Resolve ISM instance to full owner info including class/entity indices for composite index building. */
	bool ResolveISMInstanceToOwner(const UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex,
		FMassEntityHandle& OutEntity, int32& OutClassIndex, int32& OutEntityIndex) const;

private:
	// --- Internal Types ---

	/** Identifies the entity + mesh entry index that owns a particular ISM instance slot. */
	struct FInstanceOwnerEntry
	{
		FMassEntityHandle Entity;
		int32 MeshEntryIndex = INDEX_NONE;
		/** Stable indices into AArcIWPartitionActor::SpawnedEntities for composite index building. */
		int32 ClassIndex = INDEX_NONE;
		int32 EntityIndex = INDEX_NONE;
	};

	/** ISM components and ownership tracking for a partition actor. */
	struct FPartitionISMData
	{
		TWeakObjectPtr<AActor> OwnerActor;

		/** Mesh -> ISM component on the partition actor. */
		TMap<FObjectKey, TObjectPtr<UInstancedStaticMeshComponent>> ISMComponents;

		/** Mesh -> owner tracking array (indexed by ISM instance ID). */
		TMap<FObjectKey, TArray<FInstanceOwnerEntry>> InstanceOwners;
	};

	// --- Helpers ---
	UInstancedStaticMeshComponent* GetOrCreateISMComponent(
		FPartitionISMData& Data,
		UStaticMesh* Mesh,
		const TArray<TObjectPtr<UMaterialInterface>>& Materials,
		bool bCastShadows);

	FPartitionISMData* FindISMDataForEntity(FMassEntityHandle Entity);

	// --- Data ---
	TMap<FIntVector, TArray<FMassEntityHandle>> CellEntities;

	/** Entity -> partition actor mapping. */
	TMap<FMassEntityHandle, TWeakObjectPtr<AActor>> EntityToPartitionActor;

	/** Partition actor -> ISM data. */
	TMap<TWeakObjectPtr<AActor>, FPartitionISMData> PartitionISMDataMap;

	/** Reverse lookup: ISM component -> {partition actor, mesh key}. Built as ISM components are created. */
	struct FISMComponentInfo
	{
		TWeakObjectPtr<AActor> PartitionActor;
		FObjectKey MeshKey;
	};
	TMap<TWeakObjectPtr<UInstancedStaticMeshComponent>, FISMComponentInfo> ISMComponentLookup;

	FIntVector LastPlayerCell = FIntVector(TNumericLimits<int32>::Max());

	float CellSize = 2000.f;
	float SwapRadius = 10000.f;
	int32 SwapRadiusCells = 5;
	float ActorRadius = 3000.f;
	int32 ActorRadiusCells = 1;
	float DehydrationRadius = 8000.f;
	int32 DehydrationRadiusCells = 4;
};
