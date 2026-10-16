// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "InteractionTypes.h"
#include "GameplayTagContainer.h"
#include "ArcInteractionEntityOption.generated.h"

class UInputAction;

/**
 * A concrete interaction option surfaced per-entity to the interaction system.
 * Inherits DisplayName from FInteractionTargetConfiguration.
 * Add one per selectable action when populating FInteractionQueryResults::AvailableInteractions.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionEntityOption : public FInteractionTargetConfiguration
{
	GENERATED_BODY()

	/** Tag identifying this option; mirrors FArcInteractionOption::OptionTag for server dispatch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	FGameplayTag OptionTag;

	/** Input action bound to this option, if any. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	TObjectPtr<const UInputAction> InputAction = nullptr;

	/** Index into the source SO slot's Options array; INDEX_NONE for single-option interactions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	int32 SlotIndex = INDEX_NONE;
};
