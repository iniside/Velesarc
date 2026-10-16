// Copyright Lukasz Baran. All Rights Reserved.

#include "Input/ArcMassCustomInputAction_PushWidgetToLayer.h"

#include "CommonUIExtensions.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassCustomInputAction_PushWidgetToLayer)

void FArcMassCustomInputAction_PushWidgetToLayer::OnTriggered(
	APawn* Pawn,
	const FInputActionValue& /*Value*/,
	FMassEntityHandle /*Entity*/) const
{
	if (Pawn == nullptr)
	{
		return;
	}

	if (!LayerID.IsValid())
	{
		return;
	}

	APlayerController* PC = Pawn->GetController<APlayerController>();
	if (PC == nullptr)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PC->Player);
	if (LocalPlayer == nullptr)
	{
		return;
	}

	TSubclassOf<UCommonActivatableWidget> ConcreteClass = WidgetClass.Get();
	if (ConcreteClass == nullptr)
	{
		ConcreteClass = WidgetClass.LoadSynchronous();
	}

	if (ConcreteClass == nullptr)
	{
		return;
	}

	UCommonUIExtensions::PushContentToLayer_ForPlayer(LocalPlayer, LayerID, ConcreteClass);
}
