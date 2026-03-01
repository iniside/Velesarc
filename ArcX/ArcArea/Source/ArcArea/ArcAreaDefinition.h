// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArcAreaSlotDefinition.h"
#include "ArcAreaDefinition.generated.h"

/**
 * Data asset defining an area type.
 * Referenced by UArcAreaTrait on SmartObject Mass entities.
 */
UCLASS(BlueprintType, Blueprintable)
class ARCAREA_API UArcAreaDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Human-readable name for this area (e.g., "Blacksmith Forge", "Guard Post"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	FText DisplayName;

	/** Tags describing this area type (e.g., "Area.Forge", "Area.Market"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	FGameplayTagContainer AreaTags;

	/** Slot definitions — each slot is one NPC work position. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	TArray<FArcAreaSlotDefinition> Slots;
};
