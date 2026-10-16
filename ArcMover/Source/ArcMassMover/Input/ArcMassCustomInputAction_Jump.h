// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Input/ArcMassInputActionConfig.h"

#include "ArcMassCustomInputAction_Jump.generated.h"

class APawn;
struct FInputActionValue;

USTRUCT(BlueprintType, DisplayName = "Mover Jump")
struct ARCMASSMOVER_API FArcMassCustomInputAction_Jump
	: public FArcMassCustomInputAction
{
	GENERATED_BODY()

	virtual void OnStarted(APawn* Pawn, const FInputActionValue& Value, FMassEntityHandle Entity) const override;
	virtual void OnCanceled(APawn* Pawn, const FInputActionValue& Value, FMassEntityHandle Entity) const override;
	virtual void OnCompleted(APawn* Pawn, const FInputActionValue& Value, FMassEntityHandle Entity) const override;
};
