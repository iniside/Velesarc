// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "MassEntityManager.h"
#include "MassEntityTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ArcMobileVisualization.generated.h"

class UStaticMesh;
class UMaterialInterface;

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EArcMobileVisLOD : uint8
{
	Actor			UMETA(DisplayName = "Actor"),
	StaticMesh		UMETA(DisplayName = "Static Mesh"),
	None			UMETA(DisplayName = "None")
};

UENUM(BlueprintType)
enum class EArcMobileVisCellUpdatePolicy : uint8
{
	DistanceThreshold	UMETA(DisplayName = "Distance Threshold"),
	EveryTick			UMETA(DisplayName = "Every Tick")
};

// ---------------------------------------------------------------------------
// Signals
// ---------------------------------------------------------------------------

namespace UE::ArcMass::Signals
{
	const FName MobileVisUpgradeToActor   = FName(TEXT("MobVisUpgradeToActor"));
	const FName MobileVisDowngradeToISM   = FName(TEXT("MobVisDowngradeToISM"));
	const FName MobileVisUpgradeToISM     = FName(TEXT("MobVisUpgradeToISM"));
	const FName MobileVisDowngradeToNone  = FName(TEXT("MobVisDowngradeToNone"));
	const FName MobileVisEntityCellChanged = FName(TEXT("MobVisEntityCellChanged"));
}

// ---------------------------------------------------------------------------
// Fragments
// ---------------------------------------------------------------------------

/** Per-entity LOD tracking for mobile visualization. */
USTRUCT()
struct ARCMASS_API FArcMobileVisLODFragment : public FMassFragment
{
	GENERATED_BODY()

	EArcMobileVisLOD CurrentLOD = EArcMobileVisLOD::None;
	EArcMobileVisLOD PrevLOD    = EArcMobileVisLOD::None;
};

/** Per-entity mutable state for mobile visualization representation. */
USTRUCT()
struct ARCMASS_API FArcMobileVisRepFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Current cell coordinates in the spatial grid. */
	FIntVector GridCoords = FIntVector::ZeroValue;

	/** Region coordinate derived from cell position. */
	FIntVector RegionCoord = FIntVector::ZeroValue;

	/** ISM instance index when represented as an instanced static mesh. */
	int32 ISMInstanceId = INDEX_NONE;

	/** True when this entity is represented by a full actor. */
	bool bIsActorRepresentation = false;

	/** Mesh currently used for ISM representation. nullptr means using base config mesh. */
	UPROPERTY()
	TObjectPtr<UStaticMesh> CurrentISMMesh = nullptr;

	/** Last known world position, used for cell-change detection. */
	FVector LastPosition = FVector::ZeroVector;
};

template<>
struct TMassFragmentTraits<FArcMobileVisRepFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/** Shared config per entity type -- mesh, materials, actor class, ISM manager. */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcMobileVisConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "MobileVisualization")
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY(EditAnywhere, Category = "MobileVisualization")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	UPROPERTY(EditAnywhere, Category = "MobileVisualization")
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, Category = "MobileVisualization")
	TSubclassOf<AActor> ISMManagerClass;

	UPROPERTY(EditAnywhere, Category = "MobileVisualization")
	bool bCastShadows = false;
};

template<>
struct TMassFragmentTraits<FArcMobileVisConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/** Shared distance thresholds per entity type -- controls LOD transition radii. */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcMobileVisDistanceConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** World-unit radius within which entities use Actor representation. */
	UPROPERTY(EditAnywhere, Category = "MobileVisualization", meta = (ClampMin = "0"))
	float ActorRadius = 5000.f;

	/** World-unit radius within which entities use ISM representation. Must be >= ActorRadius. */
	UPROPERTY(EditAnywhere, Category = "MobileVisualization", meta = (ClampMin = "0"))
	float ISMRadius = 15000.f;

	/** Hysteresis percentage applied to transition thresholds to prevent rapid LOD flickering. */
	UPROPERTY(EditAnywhere, Category = "MobileVisualization", meta = (ClampMin = "0", ClampMax = "50"))
	float HysteresisPercent = 10.f;

	/** Policy determining when entity cell assignments are re-evaluated. */
	UPROPERTY(EditAnywhere, Category = "MobileVisualization")
	EArcMobileVisCellUpdatePolicy CellUpdatePolicy = EArcMobileVisCellUpdatePolicy::DistanceThreshold;
};

template<>
struct TMassFragmentTraits<FArcMobileVisDistanceConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------
// Tags
// ---------------------------------------------------------------------------

/** Tag to identify mobile visualization entities. */
USTRUCT()
struct ARCMASS_API FArcMobileVisEntityTag : public FMassTag
{
	GENERATED_BODY()
};

/** Tag to identify mobile visualization source entities (e.g. player cameras / observers). */
USTRUCT()
struct ARCMASS_API FArcMobileVisSourceTag : public FMassTag
{
	GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// Subsystem
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMobileVisSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Cell size configuration ---
	float GetCellSize() const { return CellSize; }
	void SetCellSize(float NewCellSize);

	int32 GetRegionExtent() const { return RegionExtent; }
	void SetRegionExtent(int32 NewExtent);

	// --- Grid utilities ---
	FIntVector WorldToCell(const FVector& WorldPos) const;
	FIntVector CellToRegion(const FIntVector& CellCoord) const;

	// --- Entity grid ---
	void RegisterEntity(FMassEntityHandle Entity, const FIntVector& Cell);
	void UnregisterEntity(FMassEntityHandle Entity, const FIntVector& Cell);
	void MoveEntityCell(FMassEntityHandle Entity, const FIntVector& OldCell, const FIntVector& NewCell);
	const TArray<FMassEntityHandle>* GetEntitiesInCell(const FIntVector& Cell) const;

	// --- Source grid ---
	void RegisterSource(FMassEntityHandle Entity, const FIntVector& Cell);
	void UnregisterSource(FMassEntityHandle Entity, const FIntVector& Cell);
	void MoveSourceCell(FMassEntityHandle Entity, const FIntVector& OldCell, const FIntVector& NewCell);
	const TMap<FMassEntityHandle, FIntVector>& GetSourcePositions() const { return SourceCellPositions; }

	// --- LOD evaluation ---
	EArcMobileVisLOD EvaluateEntityLOD(const FIntVector& EntityCell,
		int32 ActorRadiusCells, int32 ISMRadiusCells,
		int32 ActorRadiusCellsLeave, int32 ISMRadiusCellsLeave,
		EArcMobileVisLOD CurrentLOD) const;
	void GetEntityCellsInRadius(const FIntVector& Center, int32 RadiusCells, TArray<FIntVector>& OutCells) const;

	// --- ISM management ---
	int32 AddISMInstance(const FIntVector& RegionCoord, TSubclassOf<AActor> ManagerClass, UStaticMesh* Mesh, const TArray<TObjectPtr<UMaterialInterface>>& Materials, bool bCastShadows, const FTransform& Transform, FMassEntityHandle OwnerEntity);
	void RemoveISMInstance(const FIntVector& RegionCoord, TSubclassOf<AActor> ManagerClass, UStaticMesh* Mesh, int32 InstanceId, FMassEntityManager& EntityManager);
	void UpdateISMTransform(const FIntVector& RegionCoord, TSubclassOf<AActor> ManagerClass, UStaticMesh* Mesh, int32 InstanceId, const FTransform& NewTransform);

private:
	// --- Region manager ---
	struct FRegionKey
	{
		FIntVector RegionCoord;
		TWeakObjectPtr<UClass> ManagerClass;

		friend bool operator==(const FRegionKey& A, const FRegionKey& B)
		{
			return A.RegionCoord == B.RegionCoord && A.ManagerClass == B.ManagerClass;
		}

		friend uint32 GetTypeHash(const FRegionKey& Key)
		{
			return HashCombine(GetTypeHash(Key.RegionCoord), GetTypeHash(Key.ManagerClass.Get()));
		}
	};

	struct FRegionManager
	{
		TObjectPtr<AActor> ManagerActor;

		/** Mesh -> ISM component on this manager actor. */
		TMap<FObjectKey, TObjectPtr<UInstancedStaticMeshComponent>> ISMComponents;

		/** Mesh -> (instance index -> owning entity). For RemoveInstance swap fixup. */
		TMap<FObjectKey, TArray<FMassEntityHandle>> InstanceOwners;
	};

	UInstancedStaticMeshComponent* GetOrCreateISMComponent(FRegionManager& Manager, UStaticMesh* Mesh, const TArray<TObjectPtr<UMaterialInterface>>& Materials, bool bCastShadows);
	FRegionManager& GetOrCreateRegionManager(const FIntVector& RegionCoord, TSubclassOf<AActor> ManagerClass);

	// --- Data ---

	/** Spatial grid: cell coordinate -> entities in that cell. */
	TMap<FIntVector, TArray<FMassEntityHandle>> EntityCells;

	/** Source entity handle -> cell coordinate. */
	TMap<FMassEntityHandle, FIntVector> SourceCellPositions;

	/** Region-scoped ISM managers. */
	TMap<FRegionKey, FRegionManager> RegionManagers;

	/** World-space size of a single grid cell. */
	UPROPERTY()
	float CellSize = 1000.f;

	/** Number of cells from the center cell to include in a region (half-extent). */
	int32 RegionExtent = 2;

	/** Total number of cells per region side (2 * RegionExtent + 1). */
	int32 RegionStride = 5;
};
