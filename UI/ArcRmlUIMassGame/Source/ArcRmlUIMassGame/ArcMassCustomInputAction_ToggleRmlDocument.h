// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcRmlDocumentRef.h"
#include "Input/ArcMassInputActionConfig.h"

#include "ArcMassCustomInputAction_ToggleRmlDocument.generated.h"

class APawn;
struct FInputActionValue;

USTRUCT(DisplayName = "Toggle RML Document")
struct FArcMassCustomInputAction_ToggleRmlDocument : public FArcMassCustomInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	FArcRmlDocumentRef Document;

	virtual void OnStarted(APawn* Pawn, const FInputActionValue& Value, FMassEntityHandle Entity) const override;
};
