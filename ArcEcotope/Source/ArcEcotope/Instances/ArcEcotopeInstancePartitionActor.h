// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorPartition/PartitionActor.h"
#include "Misc/Guid.h"

#include "ArcEcotopeInstancePartitionActor.generated.h"

class AArcEcotopeVolume;
class UInstancedSkinnedMeshComponent;
class UInstancedStaticMeshComponent;

/**
 * World Partition cell container for one volume's baked forest geometry. One actor exists per
 * (volume, grid size, renderer kind, cell): the first three axes are folded into GridGuid, which is
 * the only identity axis UActorPartitionSubsystem matches on beyond class and cell.
 */
UCLASS()
class ARCECOTOPE_API AArcEcotopeInstancePartitionActor : public APartitionActor
{
	GENERATED_BODY()

public:
	AArcEcotopeInstancePartitionActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Distinguishes the partition-actor families of different volumes, grid sizes and renderer kinds. */
	UPROPERTY()
	FGuid GridGuid;

	/** Soft by construction: a hard reference is a World Partition ActorDesc reference and would cluster this actor with the always-loaded volume. */
	UPROPERTY()
	TSoftObjectPtr<AArcEcotopeVolume> OwningVolume;

	/** VolumeId of the owner, readable without resolving the soft pointer. */
	UPROPERTY()
	FGuid OwnerGuid;

	/** Saved native components which render the authoritative snapshot for this cell. */
	UPROPERTY(Instanced, NonTransactional)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> StaticMeshInstances;

	/** Saved native components which render the authoritative snapshot for this cell. */
	UPROPERTY(Instanced, NonTransactional)
	TArray<TObjectPtr<UInstancedSkinnedMeshComponent>> SkinnedMeshInstances;

	virtual void PostLoad() override;

#if WITH_EDITOR
	//~ Begin APartitionActor Interface
	virtual uint32 GetDefaultGridSize(UWorld* InWorld) const override;
	virtual FGuid GetGridGuid() const override { return GridGuid; }
	/**
	 * Each species picks its own cell side, so a level carries several sizes of this class at once and
	 * the size belongs in their names, not only in their grid guids. Unconditional rather than deferring
	 * to WorldSettings::bIncludeGridSizeInNameForPartitionedActors, which would rename this family on a
	 * world-level toggle nothing here controls.
	 */
	virtual bool ShouldIncludeGridSizeInName(UWorld* InWorld, const FActorPartitionIdentifier& InIdentifier) const override { return true; }
	virtual bool ShouldIncludeGridSizeInLabel() const override { return true; }
	void SetGridGuid(const FGuid& InGuid) { GridGuid = InGuid; }
	//~ End APartitionActor Interface

	/** Cosmetic Outliner nesting under the owning volume; it adds no attachment and no ActorDesc reference. */
	virtual AActor* GetSceneOutlinerParent() const override;

	/**
	 * Commits fully populated, unregistered native instance components outered to this actor. The
	 * caller validates the complete candidate before this game-thread-only swap.
	 */
	bool AdoptPreparedInstanceComponents(
		TArray<TObjectPtr<UInstancedStaticMeshComponent>>&& InStaticComponents,
		TArray<FName>&& InStaticFinalNames,
		TArray<TObjectPtr<UInstancedSkinnedMeshComponent>>&& InSkinnedComponents,
		TArray<FName>&& InSkinnedFinalNames,
		FText& OutError);
#endif
};
