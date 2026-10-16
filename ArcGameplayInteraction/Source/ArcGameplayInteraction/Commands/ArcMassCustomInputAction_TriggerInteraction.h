// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Input/ArcMassInputActionConfig.h"

#include "ArcMassCustomInputAction_TriggerInteraction.generated.h"

class APawn;
struct FInputActionValue;

/**
 * Custom input action that, on Triggered, sends FArcInteractCommand to the
 * server with the locally-resolved interaction target read from
 * UArcCoreInteractionSourceComponent on the pawn's PlayerState. Lightweight
 * non-GAS replacement for the retired UArcGA_Interact ability.
 */
USTRUCT(BlueprintType, DisplayName = "Trigger Interaction")
struct ARCGAMEPLAYINTERACTION_API FArcMassCustomInputAction_TriggerInteraction
	: public FArcMassCustomInputAction
{
	GENERATED_BODY()

	virtual void OnTriggered(APawn* Pawn, const FInputActionValue& Value, FMassEntityHandle Entity) const override;
};
