// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArcRegionDefinition.generated.h"

/**
 * Data asset defining a region type.
 */
UCLASS(BlueprintType, Blueprintable)
class ARCSETTLEMENT_API UArcRegionDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	FGameplayTagContainer RegionTags;
};
