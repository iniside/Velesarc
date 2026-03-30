// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SmartObjectTypes.h"
#include "GameplayTagContainer.h"
#include "ArcInteractionDisplayData.generated.h"

/**
 * SmartObject slot definition data that describes how an interaction
 * should be displayed in the UI. Author this on SO slot DefinitionData arrays.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionDisplayData : public FSmartObjectDefinitionData
{
	GENERATED_BODY()

	/** Label shown in the interaction UI (e.g. "Harvest", "Open"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText DisplayName;

	/** Tag identifying the interaction type, usable for icon/input prompt mapping. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FGameplayTag InteractionTag;
};
