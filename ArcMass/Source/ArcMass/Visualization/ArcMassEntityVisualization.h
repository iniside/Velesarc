// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Mass/ArchetypeTypes.h"
#include "Mass/EntityHandle.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "MassSubsystemBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Mesh/ArcRenderMeshFragments.h"
#include "Mesh/ArcISMHolderRegistry.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcMassVisualizationConfigFragments.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMassEntityVisualization.generated.h"

class UStaticMesh;
class USkinnedAsset;
class UMaterialInterface;
class UBodySetup;
class UMassEntityConfigAsset;
class UArcMassEntityConfigAsset;
struct FArcOverrideMaterialsFragment;
struct FMassEntityManager;
struct FMassArchetypeHandle;
struct FMassExecutionContext;
struct FArcMassSkinnedMeshFragment;
struct FArcMassRenderISMSkinnedFragment;
struct FArcMassInstancedSkinnedMeshRenderStateHelper;

// ---------------------------------------------------------------------------
// Signals
// ---------------------------------------------------------------------------

namespace UE::ArcMass::Signals
{
	const FName VisMeshActivated = FName(TEXT("VisMeshActivated"));
	const FName VisMeshDeactivated = FName(TEXT("VisMeshDeactivated"));
}

// ---------------------------------------------------------------------------
// Fragments & Tags
// ---------------------------------------------------------------------------

/** Per-entity mutable state for visualization representation. */
USTRUCT()
struct ARCMASS_API FArcVisRepresentationFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Mesh grid cell coordinates (set once on init). */
	FIntVector MeshGridCoords = FIntVector::ZeroValue;

	/** Physics grid cell coordinates (set once on init, only meaningful for physics-enabled entities). */
	FIntVector PhysicsGridCoords = FIntVector::ZeroValue;

	/** Whether MassEngine mesh rendering is active (visible). */
	bool bHasMeshRendering = false;

};

/** Shared config per entity type — mesh, materials, physics. */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcVisConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Visualization")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	UPROPERTY(EditAnywhere, Category = "Visualization")
	bool bCastShadows = false;

};

template<>
struct TMassFragmentTraits<FArcVisConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/** Tag to identify visualization entities. */
USTRUCT()
struct ARCMASS_API FArcVisEntityTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCMASS_API FArcVisSourceEntityTag : public FMassTag
{
	GENERATED_BODY()
};

/** Per-entity link to its ISM holder and instance index. Added on mesh activate, removed on deactivate. */
USTRUCT()
struct ARCMASS_API FArcVisISMInstanceFragment : public FMassFragment
{
	GENERATED_BODY()

	/** The ISM holder entity this instance belongs to. */
	FMassEntityHandle HolderEntity;

	/** Sparse array index in the holder's FArcRenderISMFragment::PerInstanceSMData. */
	int32 InstanceIndex = INDEX_NONE;
};

/** Component-relative transform extracted from exemplar actor's first UStaticMeshComponent.
 *  Composed into ISM instance transforms at activation: EntityWorld * ComponentRelative. */
USTRUCT()
struct ARCMASS_API FArcVisComponentTransformFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FTransform ComponentRelativeTransform = FTransform::Identity;
};

struct FArcVisSkinnedISMHolderKey
{
	TWeakObjectPtr<const USkinnedAsset> SkinnedAsset;
	bool bCastShadow = false;
	bool bHasOverrideMaterials = false;

	friend bool operator==(const FArcVisSkinnedISMHolderKey& A, const FArcVisSkinnedISMHolderKey& B)
	{
		return A.SkinnedAsset == B.SkinnedAsset && A.bCastShadow == B.bCastShadow && A.bHasOverrideMaterials == B.bHasOverrideMaterials;
	}

	friend uint32 GetTypeHash(const FArcVisSkinnedISMHolderKey& Key)
	{
		uint32 Hash = GetTypeHash(Key.SkinnedAsset);
		Hash = HashCombine(Hash, GetTypeHash(Key.bCastShadow));
		Hash = HashCombine(Hash, GetTypeHash(Key.bHasOverrideMaterials));
		return Hash;
	}
};

/** Pending ISM activation request — collected during processor execution, resolved in deferred command. */
struct FArcPendingISMActivation
{
	FMassEntityHandle SourceEntity;
	FIntVector Cell;
	FTransform WorldTransform;
	FArcMassStaticMeshConfigFragment StaticMeshConfigFrag;
	FArcMassVisualizationMeshConfigFragment VisMeshConfigFrag;
	FArcOverrideMaterialsFragment OverrideMats;
	bool bHasOverrideMats = false;
};

struct FArcPendingSkinnedISMActivation
{
	FMassEntityHandle SourceEntity;
	FIntVector Cell;
	FTransform WorldTransform;
	FArcMassSkinnedMeshFragment SkinnedMeshFrag;
	FArcMassVisualizationMeshConfigFragment VisMeshConfigFrag;
	FArcOverrideMaterialsFragment OverrideMats;
	bool bHasOverrideMats = false;
};

// ---------------------------------------------------------------------------
// Spatial Hash Grid (simple, for fixed-position entities)
// ---------------------------------------------------------------------------

struct ARCMASS_API FArcVisualizationGrid
{
	TMap<FIntVector, TArray<FMassEntityHandle>> CellEntities;
	float CellSize = 1000.f;

	FIntVector WorldToCell(const FVector& WorldPos) const
	{
		const float InvCellSize = 1.f / CellSize;
		return FIntVector(
			FMath::FloorToInt(WorldPos.X * InvCellSize),
			FMath::FloorToInt(WorldPos.Y * InvCellSize),
			FMath::FloorToInt(WorldPos.Z * InvCellSize)
		);
	}

	void AddEntity(FMassEntityHandle Entity, const FVector& WorldPos)
	{
		const FIntVector Cell = WorldToCell(WorldPos);
		CellEntities.FindOrAdd(Cell).Add(Entity);
	}

	void RemoveEntity(FMassEntityHandle Entity, const FIntVector& Cell)
	{
		if (TArray<FMassEntityHandle>* Entities = CellEntities.Find(Cell))
		{
			Entities->RemoveSwap(Entity);
			if (Entities->IsEmpty())
			{
				CellEntities.Remove(Cell);
			}
		}
	}

	const TArray<FMassEntityHandle>* GetEntitiesInCell(const FIntVector& Cell) const
	{
		return CellEntities.Find(Cell);
	}

	void GetCellsInRadius(const FIntVector& Center, int32 RadiusCells, TArray<FIntVector>& OutCells) const
	{
		OutCells.Reset();
		for (int32 X = Center.X - RadiusCells; X <= Center.X + RadiusCells; X++)
		{
			for (int32 Y = Center.Y - RadiusCells; Y <= Center.Y + RadiusCells; Y++)
			{
				for (int32 Z = Center.Z - RadiusCells; Z <= Center.Z + RadiusCells; Z++)
				{
					OutCells.Add(FIntVector(X, Y, Z));
				}
			}
		}
	}

	void Clear()
	{
		CellEntities.Empty();
	}
};

// ---------------------------------------------------------------------------
// Subsystem
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcEntityVisualizationSubsystem : public UMassSubsystemBase
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldTyp) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void PreDeinitialize() override;
	virtual void Deinitialize() override;

	// --- Mesh Grid ---
	FArcVisualizationGrid& GetMeshGrid() { return MeshGrid; }
	const FArcVisualizationGrid& GetMeshGrid() const { return MeshGrid; }

	void RegisterMeshEntity(FMassEntityHandle Entity, const FVector& Position);
	void UnregisterMeshEntity(FMassEntityHandle Entity, const FIntVector& Cell);

	void UpdateMeshPlayerCell(const FIntVector& NewCell);
	FIntVector GetLastMeshPlayerCell() const { return LastMeshPlayerCell; }
	bool IsMeshActiveCellCoord(const FIntVector& Cell) const;

	float GetMeshActivationRadius() const { return MeshActivationRadius; }
	float GetMeshDeactivationRadius() const { return MeshDeactivationRadius; }
	int32 GetMeshActivationRadiusCells() const { return MeshActivationRadiusCells; }
	int32 GetMeshDeactivationRadiusCells() const { return MeshDeactivationRadiusCells; }

	// --- Physics Grid ---
	FArcVisualizationGrid& GetPhysicsGrid() { return PhysicsGrid; }
	const FArcVisualizationGrid& GetPhysicsGrid() const { return PhysicsGrid; }

	void RegisterPhysicsEntity(FMassEntityHandle Entity, const FVector& Position);
	void UnregisterPhysicsEntity(FMassEntityHandle Entity, const FIntVector& Cell);

	void UpdatePhysicsPlayerCell(const FIntVector& NewCell);
	FIntVector GetLastPhysicsPlayerCell() const { return LastPhysicsPlayerCell; }
	bool IsPhysicsActiveCellCoord(const FIntVector& Cell) const;

	float GetPhysicsActivationRadius() const { return PhysicsActivationRadius; }
	float GetPhysicsDeactivationRadius() const { return PhysicsDeactivationRadius; }
	int32 GetPhysicsActivationRadiusCells() const { return PhysicsActivationRadiusCells; }
	int32 GetPhysicsDeactivationRadiusCells() const { return PhysicsDeactivationRadiusCells; }

	// --- ISM Holders ---
	FMassEntityHandle FindOrCreateISMHolder(
		const FIntVector& Cell,
		const FArcMassStaticMeshConfigFragment& StaticMeshConfigFrag,
		const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag,
		const FArcOverrideMaterialsFragment* OverrideMats,
		FMassEntityManager& EntityManager);

	/**
	 * Add a source entity's instance to the given ISM holder.
	 * SubIndex=0 for single-mesh entities; composite callers pass the per-sub-mesh ordinal
	 * so that multiple instances from the same source entity in the same holder have distinct keys.
	 * Returns the dense index assigned to this instance (INDEX_NONE on failure).
	 * The subsystem is authoritative for dense<->source mapping; callers must not cache
	 * the returned index for later removal — pass the source entity + SubIndex to RemoveISMInstance instead.
	 */
	int32 AddISMInstance(FMassEntityHandle HolderEntity, FMassEntityHandle SourceEntity, int32 SubIndex, const FTransform& WorldTransform, FMassEntityManager& EntityManager, bool bDeferBoundsAndCullingRefresh = false);

	/**
	 * Remove the instance identified by {SourceEntity, SubIndex} from the given ISM holder.
	 * SubIndex=0 for single-mesh entities; composite callers pass the per-sub-mesh ordinal.
	 * Uses swap-with-tail internally — the subsystem's authoritative maps are updated synchronously.
	 * FArcVisISMInstanceFragment::InstanceIndex on the swapped entity is updated via deferred command
	 * (cache only; never read during batch removal).
	 */
	void RemoveISMInstance(FMassEntityHandle HolderEntity, FMassEntityHandle SourceEntity, int32 SubIndex, const FIntVector& Cell, FMassEntityManager& EntityManager);
	void DestroyCellHolders(const FIntVector& Cell, FMassEntityManager& EntityManager);
	void BatchActivateISMEntities(TArray<FArcPendingISMActivation>&& PendingActivations, FMassExecutionContext& Context);

	const TMap<FIntVector, TMap<FArcISMHolderKey, FMassEntityHandle>>& GetCellISMHolders() const { return CellISMHolders; }

	// --- Skinned ISM Holders ---
	FMassEntityHandle FindOrCreateSkinnedISMHolder(
		const FIntVector& Cell,
		const FArcMassSkinnedMeshFragment& SkinnedMeshFrag,
		const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag,
		const FArcOverrideMaterialsFragment* OverrideMats,
		FMassEntityManager& EntityManager);

	/**
	 * Append a skinned instance to the holder and return its dense index.
	 * SourceEntity + SubIndex form the {SourceEntity, SubIndex} key (FArcHolderInstanceKey) recorded
	 * for this dense slot, mirroring FArcSkinnedMeshHolderRegistry. Single-mesh callers pass their
	 * source entity with SubIndex=0; composite callers pass the composite entity with the per-sub-mesh
	 * ordinal. Removal resolves the live dense index from this key, so a sibling swap-remove can never
	 * stale a cached index. The returned dense index is informational only (not used for removal).
	 */
	int32 AddSkinnedISMInstance(FMassEntityHandle HolderEntity, const FTransform& WorldTransform, uint32 AnimationIndex, FMassEntityManager& EntityManager, FMassEntityHandle SourceEntity = FMassEntityHandle(), int32 SubIndex = 0);

	/**
	 * Remove the instance identified by Key ({SourceEntity, SubIndex}) from the given skinned holder.
	 * The current dense index is resolved from the per-holder reverse map (no caller-cached index), so
	 * composite holders with multiple sub-instances are safe across sibling swap-removes. No-ops safely
	 * when the key is unknown.
	 */
	void RemoveSkinnedISMInstance(FMassEntityHandle HolderEntity, const FArcHolderInstanceKey& Key, const FIntVector& Cell, FMassEntityManager& EntityManager);
	void BatchActivateSkinnedISMEntities(TArray<FArcPendingSkinnedISMActivation>&& PendingActivations, FMassExecutionContext& Context);

#if WITH_EDITOR
	FDelegateHandle MaterialCompilationFinishedHandle;
	FTSTicker::FDelegateHandle PostCompileRebuildTickHandle;
	bool bNeedsPostCompileHolderRebuild = false;
	int32 PostCompileRebuildPollTicks = 0;

	void OnMaterialCompilationFinished(UMaterialInterface* Material);
	void RebuildAllHoldersRenderState();
#endif // WITH_EDITOR

private:
	void RecomputeMeshRadiusCells();
	void RecomputePhysicsRadiusCells();

	FArcVisualizationGrid MeshGrid;
	FIntVector LastMeshPlayerCell = FIntVector(TNumericLimits<int32>::Max());
	UPROPERTY()
	float MeshActivationRadius = 20000.f;
	UPROPERTY()
	float MeshDeactivationRadius = 25000.f;
	int32 MeshActivationRadiusCells = 2;
	int32 MeshDeactivationRadiusCells = 3;

	FArcVisualizationGrid PhysicsGrid;
	FIntVector LastPhysicsPlayerCell = FIntVector(TNumericLimits<int32>::Max());
	UPROPERTY()
	float PhysicsActivationRadius = 15000.f;
	UPROPERTY()
	float PhysicsDeactivationRadius = 18000.f;
	int32 PhysicsActivationRadiusCells = 2;
	int32 PhysicsDeactivationRadiusCells = 3;

	/** Cell → bucket map; bucket keyed by FArcISMHolderKey (mesh + config hash). */
	TMap<FIntVector, TMap<FArcISMHolderKey, FMassEntityHandle>> CellISMHolders;

	/** Owns archetype handles, dense-index maps, and per-holder mechanics. */
	FArcISMHolderRegistry ISMHolderRegistry;

	void InitializeSkinnedISMHolderArchetypes(FMassEntityManager& EntityManager);

	TMap<FIntVector, TMap<FArcVisSkinnedISMHolderKey, FMassEntityHandle>> CellSkinnedISMHolders;

	/**
	 * Per skinned-holder dense-parallel key array, kept strictly parallel to
	 * FArcMassRenderISMSkinnedFragment::PerInstanceData (no holes; swap-with-tail on removal).
	 * SkinnedHolderInstanceKeys[Holder][DenseIndex] -> {SourceEntity, SubIndex}. Mirrors
	 * FArcSkinnedMeshHolderRegistry::HolderInstanceKeys so removal is key-based, not index-based.
	 */
	TMap<FMassEntityHandle, TArray<FArcHolderInstanceKey>> SkinnedHolderInstanceKeys;

	/**
	 * Authoritative reverse map: Holder -> {SourceEntity, SubIndex} -> dense index. Resolves the live
	 * dense index at removal time so a sibling swap-remove never leaves a caller with a stale index
	 * (the composite-holder correctness fix). Both maps are maintained synchronously in lockstep with
	 * the dense PerInstanceData / PerInstanceCustomData / PerInstanceHitProxy swap-remove. Mirrors
	 * FArcSkinnedMeshHolderRegistry::HolderEntityToDenseIndex.
	 */
	TMap<FMassEntityHandle, TMap<FArcHolderInstanceKey, int32>> SkinnedHolderEntityToDenseIndex;

	FMassArchetypeHandle SkinnedISMHolderArchetype;
	FMassArchetypeHandle SkinnedISMHolderArchetypeWithMats;
	bool bSkinnedISMHolderArchetypesInitialized = false;
};

// ---------------------------------------------------------------------------
// Blueprint Function Library
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcVisEntitySpawnLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Spawn a Mass entity from a config asset at the specified location. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|Visualization", meta = (WorldContext = "WorldContextObject"))
	static FMassEntityHandle SpawnVisEntity(UObject* WorldContextObject, UArcMassEntityConfigAsset* EntityConfig, FTransform Transform);
};
