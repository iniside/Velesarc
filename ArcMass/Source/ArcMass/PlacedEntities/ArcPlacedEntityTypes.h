// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcPlacedEntityTypes.generated.h"

class UMassEntityConfigAsset;

#if WITH_EDITOR
struct ARCMASS_API FArcPlacedEntityEditorEvents
{
	static FSimpleMulticastDelegate OnInstanceTransformChanged;
};
#endif // WITH_EDITOR

/** Per-instance fragment overrides. Each FInstancedStruct is a full copy of one mutable fragment. */
USTRUCT()
struct ARCMASS_API FArcPlacedEntityFragmentOverrides
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "PlacedEntities")
	TArray<FInstancedStruct> FragmentValues;
};

/** Single entity config with its placed instance transforms. */
USTRUCT()
struct ARCMASS_API FArcPlacedEntityEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "PlacedEntities")
	TObjectPtr<UMassEntityConfigAsset> EntityConfig = nullptr;

	UPROPERTY(EditAnywhere, Category = "PlacedEntities")
	TArray<FTransform> InstanceTransforms;

	/**
	 * Sparse per-instance fragment overrides.
	 * Key = index into InstanceTransforms. Only instances with overrides have entries.
	 * Managed by UArcPlacedEntityProxyEditingObject — not directly editable in details panel.
	 */
	UPROPERTY()
	TMap<int32, FArcPlacedEntityFragmentOverrides> PerInstanceOverrides;

	// Identity of whatever generated this entry: the PCG element stamps its
	// UPCGMassPlacedEntityManagedResource OwnerGuid, ArcForest stamps its volume id.
	// Default-constructed (!IsValid()) for hand-placed entries — those are immune to generated cleanup.
	UPROPERTY()
	FGuid OwnerGuid;
};

/** Per-entry set of removed instance indices. Parallel to FArcPlacedEntityEntry::InstanceTransforms. */
USTRUCT()
struct ARCMASS_API FArcPlacedEntityRemovals
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TSet<int32> RemovedInstances;
};

/** One pending respawn record: when to respawn, and which (entry, transform) pair to restore. */
USTRUCT()
struct ARCMASS_API FArcPlacedEntityPendingRespawn
{
	GENERATED_BODY()

	/** Unix timestamp (seconds) at which this instance should respawn. */
	UPROPERTY(SaveGame)
	int64 RespawnAtUtc = 0;

	/** Index into AArcPlacedEntityPartitionActor::Entries. */
	UPROPERTY(SaveGame)
	int32 ClassIndex = INDEX_NONE;

	/** Index into FArcPlacedEntityEntry::InstanceTransforms. */
	UPROPERTY(SaveGame)
	int32 TransformIndex = INDEX_NONE;
};
