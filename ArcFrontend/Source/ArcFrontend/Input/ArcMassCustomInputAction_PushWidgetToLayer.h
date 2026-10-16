// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "GameplayTagContainer.h"
#include "Input/ArcMassInputActionConfig.h"

#include "ArcMassCustomInputAction_PushWidgetToLayer.generated.h"

class APawn;
struct FInputActionValue;

/**
 * Custom input action that, on Triggered, pushes the configured
 * UCommonActivatableWidget class onto the UI layer identified by LayerID
 * for the pawn's local player.
 */
USTRUCT(BlueprintType, DisplayName = "Push Widget To Layer")
struct ARCFRONTEND_API FArcMassCustomInputAction_PushWidgetToLayer
	: public FArcMassCustomInputAction
{
	GENERATED_BODY()

	// The UI layer this widget will be pushed onto (e.g. UI.Layer.Menu).
	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (Categories = "UI.Layer"))
	FGameplayTag LayerID;

	// The activatable widget class to push.
	UPROPERTY(EditDefaultsOnly, Category = "UI", meta = (AssetBundles = "Client"))
	TSoftClassPtr<UCommonActivatableWidget> WidgetClass;

	virtual void OnTriggered(APawn* Pawn, const FInputActionValue& Value, FMassEntityHandle Entity) const override;
};
