// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcInteractCommand.h"

#include "ArcEntityInteractionComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InteractableTargetInterface.h"
#include "Interaction/ArcInteractionContextData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcInteractCommand)

bool FArcInteractCommand::CanSendCommand() const
{
	if (InteractorPawn == nullptr)
	{
		return false;
	}
	return TargetObject != nullptr || EntityTarget.IsValid();
}

bool FArcInteractCommand::Execute(UObject* /*WorldContext*/)
{
	if (InteractorPawn == nullptr)
	{
		return false;
	}

	FArcInteractionContextData CtxData;
	CtxData.InteractorPawn = InteractorPawn;
	CtxData.PlayerController = Cast<APlayerController>(InteractorPawn->GetController());
	CtxData.PlayerState = InteractorPawn->GetPlayerState();
	CtxData.InteractionLocation = InteractionLocation;
	CtxData.InteractionDirection = InteractionDirection;

	if (TargetObject != nullptr)
	{
		TScriptInterface<IInteractionTarget> Target(TargetObject);
		if (Target.GetInterface() == nullptr)
		{
			return false;
		}

		FInteractionContext Context;
		Context.Target = Target;
		Context.ContextData = FInstancedStruct::Make(CtxData);
		Target->BeginInteraction(Context);
		return true;
	}

	if (EntityTarget.IsValid())
	{
		APlayerState* PS = CtxData.PlayerState.Get();
		if (PS == nullptr)
		{
			return false;
		}

		UArcEntityInteractionComponent* EntityInteractionComp = PS->FindComponentByClass<UArcEntityInteractionComponent>();
		if (EntityInteractionComp == nullptr)
		{
			return false;
		}

		return EntityInteractionComp->ExecuteEntityInteraction(EntityTarget, CtxData, OptionTag, SlotIndex);
	}

	return false;
}
