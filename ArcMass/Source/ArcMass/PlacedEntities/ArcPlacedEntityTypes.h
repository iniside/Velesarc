// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcPlacedEntityTypes.generated.h"

class UMassEntityConfigAsset;

/** Single entity config with its placed instance transforms. */
USTRUCT()
struct ARCMASS_API FArcPlacedEntityEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "PlacedEntities")
	TObjectPtr<UMassEntityConfigAsset> EntityConfig = nullptr;

	UPROPERTY(EditAnywhere, Category = "PlacedEntities")
	TArray<FTransform> InstanceTransforms;
};
