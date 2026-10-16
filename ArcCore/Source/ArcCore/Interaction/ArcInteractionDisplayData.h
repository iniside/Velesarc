// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SmartObjectTypes.h"
#include "GameplayTagContainer.h"
#include "ArcInteractionDisplayData.generated.h"

class UInputAction;
class USmartObjectBehaviorDefinition;

/**
 * A single selectable interaction option presented to the player (e.g. "Sit", "Harvest").
 * Embed these in FArcInteractionDisplayData::Options.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionOption
{
	GENERATED_BODY()

	/** Label shown for this option in the interaction UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	FText DisplayName;

	/** Tag identifying this option; used for icon/input-prompt mapping and server dispatch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	FGameplayTag OptionTag;

	/** Input action bound to this option, if any. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	TObjectPtr<const UInputAction> InputAction = nullptr;

	/** Optional per-option behavior override; falls back to the slot's default behavior when null. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	TObjectPtr<USmartObjectBehaviorDefinition> OverrideBehavior = nullptr;
};

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

	/** Selectable options exposed for this interaction slot. Empty means single-option (use DisplayName/InteractionTag). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	TArray<FArcInteractionOption> Options;
};
