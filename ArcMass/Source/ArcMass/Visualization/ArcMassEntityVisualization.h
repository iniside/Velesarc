// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassArchetypeTypes.h"
#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"
#include "MassSubsystemBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Mesh/MassEngineMeshFragments.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcMassVisualizationConfigFragments.h"
#include "ArcMassEntityVisualization.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UBodySetup;
class UMassEntityConfigAsset;
struct FMassOverrideMaterialsFragment;
struct FMassEntityManager;
struct FMassArchetypeHandle;
struct FMassExecutionContext;

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

	/** Current representation state. */
	bool bIsActorRepresentation = false;

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

/** Actor class for visualization — entities with this fragment support actor swapping. */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcVisActorConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Visualization")
	TSubclassOf<AActor> ActorClass;
};

template<>
struct TMassFragmentTraits<FArcVisActorConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/** Marks a pre-placed actor that should be reused during ISM<->Actor swaps instead of spawning/destroying. */
USTRUCT()
struct ARCMASS_API FArcVisPrePlacedActorFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Weak reference to the pre-placed actor in the level. */
	TWeakObjectPtr<AActor> PrePlacedActor;
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

	/** Sparse array index in the holder's FMassRenderISMFragment::PerInstanceSMData. */
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

/** Key for ISM holder lookup — identifies a unique mesh configuration. */
struct FArcVisISMHolderKey
{
	TWeakObjectPtr<const UStaticMesh> Mesh;
	bool bCastShadow = false;
	bool bHasOverrideMaterials = false;

	friend bool operator==(const FArcVisISMHolderKey& A, const FArcVisISMHolderKey& B)
	{
		return A.Mesh == B.Mesh && A.bCastShadow == B.bCastShadow && A.bHasOverrideMaterials == B.bHasOverrideMaterials;
	}

	friend uint32 GetTypeHash(const FArcVisISMHolderKey& Key)
	{
		uint32 Hash = GetTypeHash(Key.Mesh);
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
	FMassOverrideMaterialsFragment OverrideMats;
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

	// --- ISM Holders (unchanged) ---
	FMassEntityHandle FindOrCreateISMHolder(
		const FIntVector& Cell,
		const FArcMassStaticMeshConfigFragment& StaticMeshConfigFrag,
		const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag,
		const FMassOverrideMaterialsFragment* OverrideMats,
		FMassEntityManager& EntityManager);

	int32 AddISMInstance(FMassEntityHandle HolderEntity, const FTransform& WorldTransform, FMassEntityManager& EntityManager);
	void RemoveISMInstance(FMassEntityHandle HolderEntity, int32 InstanceIndex, const FIntVector& Cell, FMassEntityManager& EntityManager);
	void DestroyCellHolders(const FIntVector& Cell, FMassEntityManager& EntityManager);
	void BatchActivateISMEntities(TArray<FArcPendingISMActivation>&& PendingActivations, FMassExecutionContext& Context);
	void InitializeISMHolderArchetypes(FMassEntityManager& EntityManager);

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

	TMap<FIntVector, TMap<FArcVisISMHolderKey, FMassEntityHandle>> CellISMHolders;
	FMassArchetypeHandle ISMHolderArchetype;
	FMassArchetypeHandle ISMHolderArchetypeWithMats;
	bool bISMHolderArchetypesInitialized = false;
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
	static FMassEntityHandle SpawnVisEntity(UObject* WorldContextObject, const UMassEntityConfigAsset* EntityConfig, FTransform Transform);
};
