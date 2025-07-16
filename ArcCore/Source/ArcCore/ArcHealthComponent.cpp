/**
 * This file is part of ArcX.
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcHealthComponent.h"

#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "ArcEventTags.h"
#include "ArcWorldDelegates.h"
#include "GameplayTagsManager.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Pawn/ArcPawnExtensionComponent.h"

void UArcAttributeHandlingComponent::OnRegister()
{
	Super::OnRegister();
	IsPawnComponentReadyToInitialize();
}

void UArcAttributeHandlingComponent::BeginPlay()
{
	Super::BeginPlay();
	IsPawnComponentReadyToInitialize();

	
	UArcWorldDelegates::Get(this)->BroadcastActorOnComponentBeginPlay(
		FArcComponentChannelKey(GetOwner(), GetClass())
		, this);
}

bool UArcAttributeHandlingComponent::IsPawnComponentReadyToInitialize()
{
	if (bHealthInitialized == true)
	{
		return true;
	}

	if (GetOwner() == nullptr)
	{
		return false;
	}

	ArcASC = GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	if (ArcASC.IsValid() == false)
	{
		if (APawn* P = GetPawn<APawn>())
		{
			APlayerState* ArcPS = P->GetPlayerState();
			if (ArcPS != nullptr)
			{
				ArcASC = ArcPS->FindComponentByClass<UArcCoreAbilitySystemComponent>();
			}
		}
	}

	if (ArcASC.IsValid() == false)
	{
		bHealthInitialized = false;
		return false;
	}

	FOnGameplayAttributeValueChange& HealthAttributeDelegate = ArcASC->GetGameplayAttributeValueChangeDelegate(
		TrackedAttribute);
	AttributeChangedHandle = HealthAttributeDelegate.AddUObject(this
		, &UArcAttributeHandlingComponent::HandleAttributeChangedDelegate);
	bHealthInitialized = true;

	return bHealthInitialized;
}

DEFINE_LOG_CATEGORY(LogArcDeath);

void UArcHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UArcHealthComponent, DeathState);
}

void UArcHealthComponent::OnRep_DeathState(EArcDeathState OldDeathState)
{
	const EArcDeathState NewDeathState = DeathState;

	// Revert the death state for now since we rely on StartDeath and FinishDeath to
	// change it.
	DeathState = OldDeathState;

	if (OldDeathState > NewDeathState)
	{
		// The server is trying to set us back but we've already predicted past the server
		// state.
		UE_LOG(LogArcDeath
			, Warning
			, TEXT( "OnRep_DeathState: Predicted past server death state [%d] -> [%d] for owner [%s].")
			, (uint8)OldDeathState
			, (uint8)NewDeathState
			, *GetNameSafe(GetOwner()));
		return;
	}

	if (OldDeathState == EArcDeathState::NotDead)
	{
		if (NewDeathState == EArcDeathState::DeathStarted)
		{
			StartDeath();
		}
		else if (NewDeathState == EArcDeathState::DeathFinished)
		{
			StartDeath();
			FinishDeath();
		}
		else
		{
			UE_LOG(LogArcDeath
				, Error
				, TEXT( "OnRep_DeathState: Invalid death transition [%d] -> [%d] for owner [%s].")
				, (uint8)OldDeathState
				, (uint8)NewDeathState
				, *GetNameSafe(GetOwner()));
		}
	}
	else if (OldDeathState == EArcDeathState::DeathStarted)
	{
		if (NewDeathState == EArcDeathState::DeathFinished)
		{
			FinishDeath();
		}
		else
		{
			UE_LOG(LogArcDeath
				, Error
				, TEXT( "OnRep_DeathState: Invalid death transition [%d] -> [%d] for owner [%s].")
				, (uint8)OldDeathState
				, (uint8)NewDeathState
				, *GetNameSafe(GetOwner()));
		}
	}

	ensureMsgf((DeathState == NewDeathState)
		, TEXT("OnRep_DeathState: Death transition failed [%d] -> [%d] for owner [%s].")
		, (uint8)OldDeathState
		, (uint8)NewDeathState
		, *GetNameSafe(GetOwner()));
}

UArcHealthComponent::UArcHealthComponent()
{
}

void UArcHealthComponent::StartDeath()
{
	if (DeathState != EArcDeathState::NotDead)
	{
		return;
	}

	DeathState = EArcDeathState::DeathStarted;
	APawn* P = Cast<APawn>(GetOwner());
	if (P)
	{
		CachedController = P->GetController();
		OnDeathStart(P
			, P->GetController());
	}
	OnDeathStarted.Broadcast(GetOwner());

	if (P && P->GetLocalRole() == ROLE_Authority)
	{
		P->DetachFromControllerPendingDestroy();
		if (UArcPawnExtensionComponent* PawnExt = UArcPawnExtensionComponent::Get(GetOwner()))
		{
			if (PawnExt->GetArcAbilitySystemComponent() && PawnExt->GetArcAbilitySystemComponent()->GetAvatarActor() ==
				GetOwner())
			{
				PawnExt->UninitializeAbilitySystem();
			}
		}
	}

	FTimerHandle Unused;
	GetWorld()->GetTimerManager().SetTimer(Unused
		, FTimerDelegate::CreateUObject(this
			, &UArcHealthComponent::FinishDeath)
		, TimeToDie
		, false);

	GetOwner()->ForceNetUpdate();
}

void UArcHealthComponent::FinishDeath()
{
	if (DeathState != EArcDeathState::DeathStarted)
	{
		return;
	}

	DeathState = EArcDeathState::DeathFinished;

	APawn* P = Cast<APawn>(GetOwner());
	if (P)
	{
		OnDeathFinished.Broadcast(GetOwner());
		OnDeathFinish(P
			, CachedController.Get());
	}
	if (HasAuthority())
	{
		if (P)
		{
			if (UArcPawnExtensionComponent* PawnExt = UArcPawnExtensionComponent::Get(GetOwner()))
			{
				if (PawnExt->GetArcAbilitySystemComponent() && PawnExt->GetArcAbilitySystemComponent()->GetAvatarActor()
					== GetOwner())
				{
					PawnExt->UninitializeAbilitySystem();
				}
			}
		}
		if (bDestroyOnDeath)
		{
			GetOwner()->Destroy(true);
		}
	}
	GetOwner()->ForceNetUpdate();
}

void UArcHealthComponent::HandleAttributeChanged(const FOnAttributeChangeData& InChangeData)
{
	if (InChangeData.NewValue <= 0.f)
	{
		// StartDeath();

		UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(GetWorld());

		FArcDeathMessage Message;
		MessageSystem.BroadcastMessage(Arcx::EventTags::GameplayEvent_Death
			, Message);

		GetOwner()->OnEndPlay.AddDynamic(this
			, &UArcHealthComponent::HandleOwnerEndPlay);
		FOnGameplayAttributeValueChange& HealthAttributeDelegate = ArcASC->GetGameplayAttributeValueChangeDelegate(
			TrackedAttribute);
		HealthAttributeDelegate.Remove(AttributeChangedHandle);
		StartDeath();

		if (ArcASC.IsValid())
		{
			// Send the "GameplayEvent.Death" gameplay event through the owner's ability
			// system.  This can be used to trigger a death gameplay ability.
			{
				FGameplayEventData Payload;
				Payload.EventTag = Arcx::EventTags::GameplayEvent_Death;
				// Payload.Instigator = DamageInstigator;
				// Payload.Target = AbilitySystemComponent->GetAvatarActor();
				// Payload.OptionalObject = DamageEffectSpec.Def;
				// Payload.ContextHandle = DamageEffectSpec.GetEffectContext();
				// Payload.InstigatorTags =
				// *DamageEffectSpec.CapturedSourceTags.GetAggregatedTags();
				// Payload.TargetTags =
				// *DamageEffectSpec.CapturedTargetTags.GetAggregatedTags();
				// Payload.EventMagnitude = DamageMagnitude;

				FScopedPredictionWindow NewScopedWindow(ArcASC.Get()
					, true);
				ArcASC->HandleGameplayEvent(Payload.EventTag
					, &Payload);
			}
		}

		GetOwner()->ForceNetUpdate();
	}
}

void UArcHealthComponent::HandleOwnerEndPlay(AActor* Actor
											 , EEndPlayReason::Type EndPlayReason)
{
	FinishDeath();
}

UArcGameplayAbility_Death::UArcGameplayAbility_Death()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	bAutoStartDeath = true;

	UGameplayTagsManager::Get().CallOrRegister_OnDoneAddingNativeTagsDelegate(FSimpleDelegate::CreateUObject(this
		, &ThisClass::DoneAddingNativeTags));
}

void UArcGameplayAbility_Death::DoneAddingNativeTags()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// Add the ability trigger tag as default to the CDO.
		FAbilityTriggerData TriggerData;
		TriggerData.TriggerTag = Arcx::EventTags::GameplayEvent_Death;
		TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(TriggerData);
	}
}

void UArcGameplayAbility_Death::ActivateAbility(const FGameplayAbilitySpecHandle Handle
												, const FGameplayAbilityActorInfo* ActorInfo
												, const FGameplayAbilityActivationInfo ActivationInfo
												, const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);

	UArcCoreAbilitySystemComponent* LyraASC = CastChecked<UArcCoreAbilitySystemComponent>(
		ActorInfo->AbilitySystemComponent.Get());

	// FGameplayTagContainer AbilityTypesToIgnore;
	// AbilityTypesToIgnore.AddTag(FLyraGameplayTags::Get().Ability_Behavior_SurvivesDeath);

	// Cancel all abilities and block others from starting.
	// LyraASC->CancelAbilities(nullptr, &AbilityTypesToIgnore, this);
	LyraASC->CancelAbilities(nullptr
		, nullptr
		, this);

	SetCanBeCanceled(false);

	// if (!ChangeActivationGroup(ELyraAbilityActivationGroup::Exclusive_Blocking))
	//{
	//	UE_LOG(LogLyraAbilitySystem, Error,
	// TEXT("UArcGameplayAbility_Death::ActivateAbility: Ability [%s] failed to
	// change activation group to blocking."), *GetName());
	// }

	if (bAutoStartDeath)
	{
		StartDeath();
	}

	Super::ActivateAbility(Handle
		, ActorInfo
		, ActivationInfo
		, TriggerEventData);
}

void UArcGameplayAbility_Death::EndAbility(const FGameplayAbilitySpecHandle Handle
										   , const FGameplayAbilityActorInfo* ActorInfo
										   , const FGameplayAbilityActivationInfo ActivationInfo
										   , bool bReplicateEndAbility
										   , bool bWasCancelled)
{
	check(ActorInfo);

	// Always try to finish the death when the ability ends in case the ability doesn't.
	// This won't do anything if the death hasn't been started.
	FinishDeath();

	Super::EndAbility(Handle
		, ActorInfo
		, ActivationInfo
		, bReplicateEndAbility
		, bWasCancelled);
}

void UArcGameplayAbility_Death::StartDeath()
{
	if (UArcHealthComponent* HealthComponent = UArcHealthComponent::FindHealthComponent(GetAvatarActorFromActorInfo()))
	{
		if (HealthComponent->GetDeathState() == EArcDeathState::NotDead)
		{
			HealthComponent->StartDeath();
		}
	}
}

void UArcGameplayAbility_Death::FinishDeath()
{
	if (UArcHealthComponent* HealthComponent = UArcHealthComponent::FindHealthComponent(GetAvatarActorFromActorInfo()))
	{
		if (HealthComponent->GetDeathState() == EArcDeathState::DeathStarted)
		{
			HealthComponent->FinishDeath();
		}
	}
}
