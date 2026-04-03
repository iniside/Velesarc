// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "MassEntityManager.h"
#include "MassEntityTemplate.h"
#include "MassArchetypeTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ArcIWVisualizationSubsystem.generated.h"

struct FArcIWMeshEntry;
struct FArcIWActorClassData;
struct FMassEntityTemplateData;

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

	// --- Domain Grids ---
	FIntVector WorldToMeshCell(const FVector& WorldPos) const;
	FIntVector WorldToPhysicsCell(const FVector& WorldPos) const;
	FIntVector WorldToActorCell(const FVector& WorldPos) const;

	void RegisterMeshEntity(FMassEntityHandle Entity, const FVector& Position);
	void UnregisterMeshEntity(FMassEntityHandle Entity, const FIntVector& Cell);
	const TArray<FMassEntityHandle>* GetMeshEntitiesInCell(const FIntVector& Cell) const;

	void RegisterPhysicsEntity(FMassEntityHandle Entity, const FVector& Position);
	void UnregisterPhysicsEntity(FMassEntityHandle Entity, const FIntVector& Cell);
	const TArray<FMassEntityHandle>* GetPhysicsEntitiesInCell(const FIntVector& Cell) const;

	void RegisterActorEntity(FMassEntityHandle Entity, const FVector& Position);
	void UnregisterActorEntity(FMassEntityHandle Entity, const FIntVector& Cell);
	const TArray<FMassEntityHandle>* GetActorEntitiesInCell(const FIntVector& Cell) const;

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
	void UpdateMeshPlayerCell(const FIntVector& NewCell);
	void UpdatePhysicsPlayerCell(const FIntVector& NewCell);
	void UpdateActorPlayerCell(const FIntVector& NewCell);
	FIntVector GetLastMeshPlayerCell() const { return LastMeshPlayerCell; }
	FIntVector GetLastPhysicsPlayerCell() const { return LastPhysicsPlayerCell; }
	FIntVector GetLastActorPlayerCell() const { return LastActorPlayerCell; }
	bool HasActorGrid() const { return bActorGridEnabled; }
	static void GetCellsInRadius(const FIntVector& Center, int32 RadiusCells, TArray<FIntVector>& OutCells);

	// --- Mesh Grid Config ---
	float GetMeshCellSize() const { return MeshCellSize; }
	float GetMeshAddRadius() const { return MeshAddRadius; }
	int32 GetMeshAddRadiusCells() const { return MeshAddRadiusCells; }
	float GetMeshRemoveRadius() const { return MeshRemoveRadius; }
	int32 GetMeshRemoveRadiusCells() const { return MeshRemoveRadiusCells; }
	bool IsMeshCell(const FIntVector& MeshCell) const;
	bool IsWithinMeshRemoveRadius(const FIntVector& MeshCell) const;

	// --- Physics Grid Config ---
	float GetPhysicsCellSize() const { return PhysicsCellSize; }
	float GetPhysicsAddRadius() const { return PhysicsAddRadius; }
	int32 GetPhysicsAddRadiusCells() const { return PhysicsAddRadiusCells; }
	float GetPhysicsRemoveRadius() const { return PhysicsRemoveRadius; }
	int32 GetPhysicsRemoveRadiusCells() const { return PhysicsRemoveRadiusCells; }
	bool IsPhysicsCell(const FIntVector& PhysicsCell) const;
	bool IsWithinPhysicsRemoveRadius(const FIntVector& PhysicsCell) const;

	// --- Actor Grid Config ---
	float GetActorCellSize() const { return ActorCellSize; }
	float GetActorHydrationRadius() const { return ActorHydrationRadius; }
	int32 GetActorHydrationRadiusCells() const { return ActorHydrationRadiusCells; }
	float GetActorDehydrationRadius() const { return ActorDehydrationRadius; }
	int32 GetActorDehydrationRadiusCells() const { return ActorDehydrationRadiusCells; }
	bool IsActorCell(const FIntVector& ActorCell) const;
	bool IsWithinDehydrationRadius(const FIntVector& ActorCell) const;

	// --- Debug ---
	const TMap<FIntVector, TArray<FMassEntityHandle>>& GetMeshCellEntities() const { return MeshCellEntities; }
	const TMap<FIntVector, TArray<FMassEntityHandle>>& GetPhysicsCellEntities() const { return PhysicsCellEntities; }
	const TMap<FIntVector, TArray<FMassEntityHandle>>& GetActorCellEntities() const { return ActorCellEntities; }

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

	// --- Entity Template & Holder Archetype Cache ---
public:
	struct FCachedEntityTemplate
	{
		TSharedRef<FMassEntityTemplate> EntityTemplate;
		FMassArchetypeHandle HolderArchetype;
	};

	/** Get or build entity template + holder archetype for an actor class entry.
	 *  Selects the correct vis-type cache map, merges gameplay data if present.
	 *  First call builds and caches; subsequent calls return cached. */
	const FCachedEntityTemplate& EnsureEntityTemplate(
		const FArcIWActorClassData& ClassData, UWorld& World);

private:
	// Cached gameplay template data from AdditionalEntityConfig (per actor class)
	TMap<TObjectPtr<UClass>, FMassEntityTemplateData> GameplayTemplateData;

	// Cached entity templates by vis archetype shape (per actor class)
	TMap<TObjectPtr<UClass>, FCachedEntityTemplate> SimpleVisStaticTemplates;
	TMap<TObjectPtr<UClass>, FCachedEntityTemplate> SimpleVisStaticWithMatsTemplates;
	TMap<TObjectPtr<UClass>, FCachedEntityTemplate> SimpleVisSkinnedTemplates;
	TMap<TObjectPtr<UClass>, FCachedEntityTemplate> SimpleVisSkinnedWithMatsTemplates;
	TMap<TObjectPtr<UClass>, FCachedEntityTemplate> CompositeVisTemplates;

	// --- Internal Types ---

	/** Identifies the entity + mesh entry index that owns a particular ISM instance slot. */
	struct FInstanceOwnerEntry
	{
		FMassEntityHandle Entity;
		int32 MeshEntryIndex = INDEX_NONE;
		/** Stable indices into the partition actor's SpawnedEntities for composite index building. */
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

	// --- Data: Mesh Grid ---
	TMap<FIntVector, TArray<FMassEntityHandle>> MeshCellEntities;
	FIntVector LastMeshPlayerCell = FIntVector(TNumericLimits<int32>::Max());
	float MeshCellSize = 10000.f;
	float MeshAddRadius = 0.f;
	int32 MeshAddRadiusCells = 0;
	float MeshRemoveRadius = 0.f;
	int32 MeshRemoveRadiusCells = 0;

	// --- Data: Physics Grid ---
	TMap<FIntVector, TArray<FMassEntityHandle>> PhysicsCellEntities;
	FIntVector LastPhysicsPlayerCell = FIntVector(TNumericLimits<int32>::Max());
	float PhysicsCellSize = 8000.f;
	float PhysicsAddRadius = 0.f;
	int32 PhysicsAddRadiusCells = 0;
	float PhysicsRemoveRadius = 0.f;
	int32 PhysicsRemoveRadiusCells = 0;

	// --- Data: Actor Grid ---
	bool bActorGridEnabled = false;
	TMap<FIntVector, TArray<FMassEntityHandle>> ActorCellEntities;
	FIntVector LastActorPlayerCell = FIntVector(TNumericLimits<int32>::Max());
	float ActorCellSize = 5000.f;
	float ActorHydrationRadius = 0.f;
	int32 ActorHydrationRadiusCells = 0;
	float ActorDehydrationRadius = 0.f;
	int32 ActorDehydrationRadiusCells = 0;

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
};
