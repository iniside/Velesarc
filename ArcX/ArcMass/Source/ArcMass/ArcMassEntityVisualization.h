// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "MassEntityManager.h"
#include "MassEntityTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ArcMassEntityVisualization.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMassEntityConfigAsset;
class AArcVisPartitionActor;

// ---------------------------------------------------------------------------
// Signals
// ---------------------------------------------------------------------------

namespace UE::ArcMass::Signals
{
	const FName VisualizationCellActivated = FName(TEXT("VisCellActivated"));
	const FName VisualizationCellDeactivated = FName(TEXT("VisCellDeactivated"));
}

// ---------------------------------------------------------------------------
// Fragments & Tags
// ---------------------------------------------------------------------------

/** Per-entity mutable state for visualization representation. */
USTRUCT()
struct ARCMASS_API FArcVisRepresentationFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Cell coordinates (set once on add). */
	FIntVector GridCoords = FIntVector::ZeroValue;

	/** ISM instance index when in ISM mode (non-partition path). */
	int32 ISMInstanceId = INDEX_NONE;

	/** Stable slot index in the partition actor (partition path). */
	int32 PartitionSlotIndex = INDEX_NONE;

	/** Current representation state. */
	bool bIsActorRepresentation = false;

	/** Mesh currently used for ISM representation. Used by lifecycle mesh switching
	  * to know which mesh to remove. nullptr means using base config mesh. */
	UPROPERTY()
	TObjectPtr<UStaticMesh> CurrentISMMesh = nullptr;
};
template<>
struct TMassFragmentTraits<FArcVisRepresentationFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/** Shared config per entity type — mesh, materials, actor class. */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcVisConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Visualization")
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY(EditAnywhere, Category = "Visualization")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	UPROPERTY(EditAnywhere, Category = "Visualization")
	TSubclassOf<AActor> ActorClass;

	/** Actor class spawned per cell to own ISM components. If not set, uses a plain AActor. */
	UPROPERTY(EditAnywhere, Category = "Visualization")
	TSubclassOf<AActor> ISMManagerClass;

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
class ARCMASS_API UArcEntityVisualizationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Grid ---
	FArcVisualizationGrid& GetGrid() { return Grid; }
	const FArcVisualizationGrid& GetGrid() const { return Grid; }

	void SetCellSize(float NewCellSize);

	// --- Entity Registration ---
	void RegisterEntity(FMassEntityHandle Entity, const FVector& Position);
	void UnregisterEntity(FMassEntityHandle Entity, const FIntVector& Cell);

	// --- ISM Management ---
	/** Add an ISM instance on the per-cell manager actor. */
	int32 AddISMInstance(const FIntVector& Cell, TSubclassOf<AActor> ManagerClass, UStaticMesh* Mesh, const TArray<TObjectPtr<UMaterialInterface>>& Materials, bool bCastShadows, const FTransform& Transform, FMassEntityHandle OwnerEntity);
	/** Remove an ISM instance. Updates the swapped entity's stored ID via EntityManager. */
	void RemoveISMInstance(const FIntVector& Cell, TSubclassOf<AActor> ManagerClass, UStaticMesh* Mesh, int32 InstanceId, FMassEntityManager& EntityManager);
	/** Get the manager actor for a given cell and class, or nullptr. */
	AActor* GetCellManagerActor(const FIntVector& Cell, TSubclassOf<AActor> ManagerClass) const;

	// --- Cell Tracking ---
	void UpdatePlayerCell(const FIntVector& NewCell);
	FIntVector GetLastPlayerCell() const { return LastPlayerCell; }
	bool IsActiveCellCoord(const FIntVector& Cell) const;
	void GetActiveCells(TArray<FIntVector>& OutCells) const;

	// --- Config ---
	float GetSwapRadius() const { return SwapRadius; }
	void SetSwapRadius(float NewRadius);
	int32 GetSwapRadiusCells() const { return SwapRadiusCells; }

private:
	void RecomputeSwapRadiusCells();

	/** Key for identifying a cell manager: cell coords + manager actor class. */
	struct FCellManagerKey
	{
		FIntVector Cell;
		TWeakObjectPtr<UClass> ManagerClass;

		friend bool operator==(const FCellManagerKey& A, const FCellManagerKey& B)
		{
			return A.Cell == B.Cell && A.ManagerClass == B.ManagerClass;
		}

		friend uint32 GetTypeHash(const FCellManagerKey& Key)
		{
			return HashCombine(GetTypeHash(Key.Cell), GetTypeHash(Key.ManagerClass.Get()));
		}
	};

	/** Per-cell manager actor with its ISM components and owner tracking. */
	struct FCellManager
	{
		TObjectPtr<AActor> ManagerActor;

		/** Mesh → ISM component on this manager actor. */
		TMap<FObjectKey, TObjectPtr<UInstancedStaticMeshComponent>> ISMComponents;

		/** Mesh → (instance index → owning entity). For RemoveInstance swap fixup. */
		TMap<FObjectKey, TArray<FMassEntityHandle>> InstanceOwners;
	};

	UInstancedStaticMeshComponent* GetOrCreateISMComponent(FCellManager& Manager, UStaticMesh* Mesh, const TArray<TObjectPtr<UMaterialInterface>>& Materials, bool bCastShadows);
	FCellManager& GetOrCreateCellManager(const FIntVector& Cell, TSubclassOf<AActor> ManagerClass);
	
	FArcVisualizationGrid Grid;
	TMap<FCellManagerKey, FCellManager> CellManagers;

	/** Fallback manager for entities without a custom ISMManagerClass. */
	FCellManager GlobalManager;

	FIntVector LastPlayerCell = FIntVector(TNumericLimits<int32>::Max());

	UPROPERTY()
	float SwapRadius = 5000.f;

	int32 SwapRadiusCells = 5;
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
