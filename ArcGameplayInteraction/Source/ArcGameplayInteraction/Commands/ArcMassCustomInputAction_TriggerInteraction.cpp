// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassCustomInputAction_TriggerInteraction.h"

#include "Commands/ArcInteractCommand.h"
#include "GameplayTagContainer.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/ArcCoreInteractionSourceComponent.h"
#include "InteractableTargetInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassCustomInputAction_TriggerInteraction)

void FArcMassCustomInputAction_TriggerInteraction::OnTriggered(APawn* Pawn, const FInputActionValue& /*Value*/, FMassEntityHandle /*Entity*/) const
{
	if (Pawn == nullptr)
	{
		return;
	}

	APlayerController* PC = Pawn->GetController<APlayerController>();
	if (PC == nullptr)
	{
		return;
	}

	APlayerState* PS = Pawn->GetPlayerState();
	if (PS == nullptr)
	{
		return;
	}

	UArcCoreInteractionSourceComponent* Source = PS->FindComponentByClass<UArcCoreInteractionSourceComponent>();
	if (Source == nullptr)
	{
		return;
	}

	TScriptInterface<IInteractionTarget> CurrentTarget = Source->GetCurrentTarget();
	FMassEntityHandle EntityTarget = Source->GetCurrentEntityTarget();
	UObject* TargetObject = CurrentTarget.GetObject();

	if (TargetObject == nullptr && !EntityTarget.IsValid())
	{
		return;
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	Pawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	const bool bSent = Arcx::SendServerCommand<FArcInteractCommand>(PC
		, Pawn
		, TargetObject
		, EntityTarget
		, EyeLocation
		, EyeRotation.Vector()
		, FGameplayTag()
		, INDEX_NONE);

	if (bSent)
	{
		Source->NotifyTriggerInteraction();
	}
}
