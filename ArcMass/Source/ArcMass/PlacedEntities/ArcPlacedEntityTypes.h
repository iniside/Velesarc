// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcPlacedEntityTypes.generated.h"

class UMassEntityConfigAsset;

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
};
