// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "MassEntityManager.h"
#include "MassEntityTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ArcCompositeVisualization.generated.h"

class UStaticMesh;
class UMaterialInterface;

// ---------------------------------------------------------------------------
// Data Types
// ---------------------------------------------------------------------------

/** Per-mesh-component cached data for a composite visualization entity. */
USTRUCT()
struct ARCMASS_API FArcCompositeVisInstanceEntry
{
	GENERATED_BODY()

	/** The static mesh for this component. */
	UPROPERTY()
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** Transform of this mesh component relative to the actor root. */
	UPROPERTY()
	FTransform RelativeTransform = FTransform::Identity;

	/** Materials applied to this mesh component. */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	/** ISM instance index when in ISM mode. INDEX_NONE means not instanced. */
	int32 ISMInstanceId = INDEX_NONE;

	/** Sequential index of this entry within the entity's MeshEntries array. */
	int32 EntryIndex = INDEX_NONE;
};

// ---------------------------------------------------------------------------

/** Per-entity mutable state for composite visualization representation. */
USTRUCT()
struct ARCMASS_API FArcCompositeVisRepFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Cell coordinates (set once on add). */
	FIntVector GridCoords = FIntVector::ZeroValue;

	/** True when the entity is currently represented by a spawned actor rather than ISM instances. */
	bool bIsActorRepresentation = false;

	/** True once mesh data has been captured from the source actor. */
	bool bMeshDataCaptured = false;

	/** One entry per static mesh component found on the source actor. */
	UPROPERTY()
	TArray<FArcCompositeVisInstanceEntry> MeshEntries;
};

template<>
struct TMassFragmentTraits<FArcCompositeVisRepFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------

/** Shared (const) config for all entities of the same composite visualization type. */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcCompositeVisConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** Actor class used when spawning a full actor representation. */
	UPROPERTY(EditAnywhere, Category = "CompositeVisualization")
	TSubclassOf<AActor> ActorClass;

	/** Actor class spawned per cell to own ISM components. Defaults to AActor. */
	UPROPERTY(EditAnywhere, Category = "CompositeVisualization")
	TSubclassOf<AActor> ISMManagerClass;

	/** Whether ISM instances cast shadows. */
	UPROPERTY(EditAnywhere, Category = "CompositeVisualization")
	bool bCastShadows = false;

	FArcCompositeVisConfigFragment();
};

template<>
struct TMassFragmentTraits<FArcCompositeVisConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------

/** Tag to identify composite visualization entities. */
USTRUCT()
struct ARCMASS_API FArcCompositeVisEntityTag : public FMassTag
{
	GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// Mesh Capture
// ---------------------------------------------------------------------------

namespace UE::ArcMass::CompositeVis
{
	/** Inspect an actor's UStaticMeshComponents (excluding UInstancedStaticMeshComponent)
	  * and extract mesh, relative transform, and materials into entries. */
	void CaptureActorMeshData(AActor* Actor, TArray<FArcCompositeVisInstanceEntry>& OutEntries);
}

// ---------------------------------------------------------------------------
// Subsystem
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcCompositeVisSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Add ISM instances for all entries in the given mesh entries array.
	 * Updates ISMInstanceId on each entry in-place.
	 */
	void AddISMInstance(
		const FIntVector& Cell,
		TSubclassOf<AActor> ManagerClass,
		bool bCastShadows,
		const FTransform& WorldTransform,
		FMassEntityHandle OwnerEntity,
		int32 EntryIndex,
		FArcCompositeVisInstanceEntry& InOutEntry);

	/**
	 * Remove a single ISM instance by entry index. Performs swap-fixup so the
	 * swapped entity's stored ISMInstanceId remains correct.
	 */
	void RemoveISMInstance(
		const FIntVector& Cell,
		TSubclassOf<AActor> ManagerClass,
		UStaticMesh* Mesh,
		int32 InstanceId,
		FMassEntityManager& EntityManager);

private:
	/** Identifies the entity+entry that owns a particular ISM instance slot. */
	struct FInstanceOwnerEntry
	{
		FMassEntityHandle Entity;
		int32 EntryIndex = INDEX_NONE;
	};

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

	/** Per-cell manager actor with ISM components and per-instance owner tracking. */
	struct FCellManager
	{
		TObjectPtr<AActor> ManagerActor = nullptr;

		/** Mesh → ISM component on this manager actor. */
		TMap<FObjectKey, TObjectPtr<UInstancedStaticMeshComponent>> ISMComponents;

		/**
		 * Mesh → array of FInstanceOwnerEntry, indexed by ISM instance ID.
		 * Used for swap-fixup during RemoveInstance.
		 */
		TMap<FObjectKey, TArray<FInstanceOwnerEntry>> InstanceOwners;
	};

	UInstancedStaticMeshComponent* GetOrCreateISMComponent(
		FCellManager& Manager,
		UStaticMesh* Mesh,
		const TArray<TObjectPtr<UMaterialInterface>>& Materials,
		bool bCastShadows);

	FCellManager& GetOrCreateCellManager(const FIntVector& Cell, TSubclassOf<AActor> ManagerClass);

	TMap<FCellManagerKey, FCellManager> CellManagers;
};
