// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorPartition/PartitionActor.h"
#include "Misc/Guid.h"
#include "Town/ArcTownVolume.h"

#include "ArcTownPartitionActor.generated.h"

class UInstancedStaticMeshComponent;
class USplineMeshComponent;
class UStaticMeshComponent;

/** Adoption preflight rejects any other final name, so the emitter has to build its names from this. */
inline constexpr TCHAR ArcTownGeneratedISMPrefix[] = TEXT("ArcTownISM_");

inline constexpr TCHAR ArcTownRoadSegmentPrefix[] = TEXT("ArcTownRoadSeg_");

inline constexpr TCHAR ArcTownRoadNodePrefix[] = TEXT("ArcTownRoadNode_");

/**
 * World Partition cell container for one town's baked building instances. One actor exists per
 * (town, grid size, cell): the first two axes are folded into GridGuid, which is the only identity
 * axis UActorPartitionSubsystem matches on beyond class and cell.
 */
UCLASS()
class ARCTOWN_API AArcTownPartitionActor : public APartitionActor
{
	GENERATED_BODY()

public:
	AArcTownPartitionActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** No EditAnywhere: a details row per instance transform hangs the editor on selection. */
	UPROPERTY(BlueprintReadOnly, Instanced, NonTransactional, Category = "Instances")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> StaticMeshInstances;

	UPROPERTY(BlueprintReadOnly, Instanced, NonTransactional, Category = "Instances")
	TArray<TObjectPtr<USplineMeshComponent>> RoadSegments;

	/** A junction is laid, not deformed: its mesh carries the crossing itself, so it is a plain static mesh component. */
	UPROPERTY(BlueprintReadOnly, Instanced, NonTransactional, Category = "Instances")
	TArray<TObjectPtr<UStaticMeshComponent>> RoadNodes;

	/** TownId of the owner, readable without resolving the soft pointer; the sole key town teardown matches on. */
	UPROPERTY()
	FGuid OwnerGuid;

	/** Soft by construction: a hard reference is a World Partition ActorDesc reference and would force the whole town always-loaded. */
	UPROPERTY()
	TSoftObjectPtr<AArcTownVolume> OwningTown;

#if WITH_EDITORONLY_DATA
	/** Soft plus GUID for the same reason OwningTown is soft: a hard reference is an ActorDesc reference. */
	UPROPERTY(NonTransactional)
	TArray<FArcTownEmittedActor> OwnedBuildingActors;
#endif

	virtual void PostLoad() override;
	virtual void Destroyed() override;

#if WITH_EDITOR
	//~ Begin APartitionActor Interface
	virtual uint32 GetDefaultGridSize(UWorld* InWorld) const override { return 25600; }
	virtual FGuid GetGridGuid() const override { return GridGuid; }
	void SetGridGuid(const FGuid& InGuid) { GridGuid = InGuid; }
	//~ End APartitionActor Interface

	/** Cosmetic Outliner nesting under the owning town; it adds no attachment and no ActorDesc reference. */
	virtual AActor* GetSceneOutlinerParent() const override;

	/**
	 * Commits fully populated, unregistered instance components outered to this actor on the game
	 * thread. Every final name must start with ArcTownGeneratedISMPrefix; the caller validates the
	 * complete candidate set before this swap.
	 */
	bool AdoptPreparedInstanceComponents(
		TArray<TObjectPtr<UInstancedStaticMeshComponent>>&& InComponents,
		TArray<FName>&& InFinalNames,
		FText& OutError);

	bool AdoptPreparedRoadComponents(
		TArray<TObjectPtr<USplineMeshComponent>>&& InComponents,
		TArray<FName>&& InFinalNames,
		FText& OutError);

	bool AdoptPreparedRoadNodeComponents(
		TArray<TObjectPtr<UStaticMeshComponent>>&& InComponents,
		TArray<FName>&& InFinalNames,
		FText& OutError);

	bool AdoptPreparedBuildingActors(TArray<FArcTownEmittedActor>&& InActors, FText& OutError);

	int32 DestroyGeneratedRoadSegments();

	int32 DestroyGeneratedRoadNodes();
#endif

#if WITH_EDITORONLY_DATA
	/** Distinguishes the partition-actor families of different towns and grid sizes. */
	UPROPERTY()
	FGuid GridGuid;
#endif
};
