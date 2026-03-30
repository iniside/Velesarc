// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGA_Interact.h"

#include "Interaction/ArcCoreInteractionSourceComponent.h"
#include "Interaction/ArcInteractionContextData.h"
#include "ArcEntityInteractionComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcGA_Interact)

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

	if (!InteractionSource || (!InteractionSource->GetCurrentTarget() && !InteractionSource->GetCurrentEntityTarget().IsValid()))
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

	// Actor target path — existing behavior
	if (InteractionSource->GetCurrentTarget())
	{
		FInteractionContext Context;
		Context.Target = InteractionSource->GetCurrentTarget();
		Context.ContextData = FInstancedStruct::Make(CtxData);
		Context.Target->BeginInteraction(Context);
		return;
	}

	// Entity target path — delegate to UArcEntityInteractionComponent
	FMassEntityHandle EntityTarget = InteractionSource->GetCurrentEntityTarget();
	if (EntityTarget.IsValid())
	{
		AActor* OwnerActor = GetActorInfo().OwnerActor.Get();
		if (OwnerActor)
		{
			UArcEntityInteractionComponent* EntityInteractionComp = OwnerActor->FindComponentByClass<UArcEntityInteractionComponent>();
			if (EntityInteractionComp)
			{
				EntityInteractionComp->ExecuteEntityInteraction(EntityTarget, CtxData);
			}
		}
	}
}
