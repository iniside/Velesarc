// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Input/ArcMassInputActionConfig.h"

#include "ArcMassCustomInputAction_Crouch.generated.h"

class APawn;
struct FInputActionValue;

USTRUCT(BlueprintType, DisplayName = "Mover Crouch")
struct ARCMASSMOVER_API FArcMassCustomInputAction_Crouch
	: public FArcMassCustomInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	bool bToggle = true;

	virtual void OnStarted(APawn* Pawn, const FInputActionValue& Value, FMassEntityHandle Entity) const override;
	virtual void OnCanceled(APawn* Pawn, const FInputActionValue& Value, FMassEntityHandle Entity) const override;
	virtual void OnCompleted(APawn* Pawn, const FInputActionValue& Value, FMassEntityHandle Entity) const override;
};
