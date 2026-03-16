// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGA_Interact.h"

#include "ArcCoreInteractionSourceComponent.h"
#include "ArcInteractionContextData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

UArcGA_Interact::UArcGA_Interact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UArcGA_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UArcCoreInteractionSourceComponent* InteractionSource = nullptr;
	if (ActorInfo && ActorInfo->OwnerActor.IsValid())
	{
		InteractionSource = ActorInfo->OwnerActor->FindComponentByClass<UArcCoreInteractionSourceComponent>();
	}

	if (!InteractionSource || !InteractionSource->GetCurrentTarget())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PerformInteraction(InteractionSource);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UArcGA_Interact::PerformInteraction(UArcCoreInteractionSourceComponent* InteractionSource)
{
	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());

	FArcInteractionContextData CtxData;
	CtxData.InteractorPawn = AvatarPawn;

	if (AvatarPawn)
	{
		CtxData.PlayerController = Cast<APlayerController>(AvatarPawn->GetController());
		CtxData.PlayerState = AvatarPawn->GetPlayerState();

		FVector EyeLocation;
		FRotator EyeRotation;
		AvatarPawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);
		CtxData.InteractionLocation = EyeLocation;
		CtxData.InteractionDirection = EyeRotation.Vector();
	}

	FInteractionContext Context;
	Context.Target = InteractionSource->GetCurrentTarget();
	Context.ContextData = FInstancedStruct::Make(CtxData);
	Context.Target->BeginInteraction(Context);
}
