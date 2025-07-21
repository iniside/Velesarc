/**
 * This file is part of Velesarc
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

#include "AbilitySystem/Tasks/Animation/ArcAT_WaitPlayMontageUntilInputRelease.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemLog.h"
#include "Animation/AnimInstance.h"
#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "GameFramework/Character.h"

UArcAT_WaitPlayMontageUntilInputRelease::UArcAT_WaitPlayMontageUntilInputRelease(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Rate = 1.f;
	bStopWhenAbilityEnds = true;
}

UArcCoreAbilitySystemComponent* UArcAT_WaitPlayMontageUntilInputRelease::GetTargetASC()
{
	return Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
}

void UArcAT_WaitPlayMontageUntilInputRelease::OnMontageBlendingOut(UAnimMontage* Montage
																   , bool bInterrupted)
{
	if (Montage == MontageToPlay)
	{
		// Reset AnimRootMotionTranslationScale
		ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
		if (Character && (Character->GetLocalRole() == ROLE_Authority || (
							  Character->GetLocalRole() == ROLE_AutonomousProxy && Ability->GetNetExecutionPolicy() ==
							  EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
		{
			Character->SetAnimRootMotionTranslationScale(1.f);
		}
	}

	if (bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast(FGameplayTag()
				, FGameplayEventData());
		}
	}
	else
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnBlendOut.Broadcast(FGameplayTag()
				, FGameplayEventData());
		}
	}
}

void UArcAT_WaitPlayMontageUntilInputRelease::OnAbilityCancelled()
{
	// TODO: Merge this fix back to engine, it was calling the wrong callback

	if (StopPlayingMontage())
	{
		// Let the BP handle the interrupt as well
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast(FGameplayTag()
				, FGameplayEventData());
		}
	}
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
	ArcASC->RemoveOnInputReleasedMap(GetAbilitySpecHandle()
		, InputDelegateHandle);
}

void UArcAT_WaitPlayMontageUntilInputRelease::OnMontageEnded(UAnimMontage* Montage
															 , bool bInterrupted)
{
	if (!bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCompleted.Broadcast(FGameplayTag()
				, FGameplayEventData());
		}
	}
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
	ArcASC->RemoveOnInputReleasedMap(GetAbilitySpecHandle()
		, InputDelegateHandle);
	OnInterrupted.Broadcast(FGameplayTag()
		, FGameplayEventData());
}

void UArcAT_WaitPlayMontageUntilInputRelease::OnGameplayEvent(FGameplayTag EventTag
															  , const FGameplayEventData* Payload)
{
	// if (ShouldBroadcastAbilityTaskDelegates())
	{
		FGameplayEventData TempData = *Payload;
		TempData.EventTag = EventTag;

		EventReceived.Broadcast(EventTag
			, TempData);
	}
}

UArcAT_WaitPlayMontageUntilInputRelease* UArcAT_WaitPlayMontageUntilInputRelease::PlayMontageUntilInputRelease(
	UGameplayAbility* OwningAbility
	, FName TaskInstanceName
	, UAnimMontage* MontageToPlay
	, FGameplayTagContainer EventTags
	, float Rate
	, FName SectionToJump
	, bool bStopWhenAbilityEnds
	, float AnimRootMotionTranslationScale)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(Rate);

	UArcAT_WaitPlayMontageUntilInputRelease* MyObj = NewAbilityTask<UArcAT_WaitPlayMontageUntilInputRelease>(
		OwningAbility
		, TaskInstanceName);
	MyObj->MontageToPlay = MontageToPlay;
	MyObj->EventTags = EventTags;
	MyObj->Rate = Rate;
	MyObj->JumpToSectionOnConfirm = SectionToJump;
	MyObj->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;
	MyObj->bStopWhenAbilityEnds = bStopWhenAbilityEnds;

	return MyObj;
}

void UArcAT_WaitPlayMontageUntilInputRelease::Activate()
{
	if (Ability == nullptr)
	{
		return;
	}
	StartTime = GetWorld()->GetTimeSeconds();

	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
	EventHandle = ArcASC->AddGameplayEventTagContainerDelegate(EventTags
		, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this
			, &UArcAT_WaitPlayMontageUntilInputRelease::OnGameplayEvent));

	SetWaitingOnRemotePlayerData();
	SetWaitingOnAvatar();

	InputDelegateHandle = ArcASC->AddOnInputReleasedMap(GetAbilitySpecHandle()
		, FArcAbilityInputDelegate::FDelegate::CreateUObject(this
			, &UArcAT_WaitPlayMontageUntilInputRelease::OnInputReleased));

	bool bPlayedMontage = false;

	if (ArcASC)
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			// Bind to event callback

			bool bPlayed = ArcASC->PlayAnimMontage(Ability
				, MontageToPlay
				, Rate
				, NAME_None
				, 0
				, false);
			
			if (bPlayed)
			{
				// Playing a montage could potentially fire off a callback into game code
				// which could kill this ability! Early out if we are  pending kill.
				if (ShouldBroadcastAbilityTaskDelegates() == false)
				{
					return;
				}

				CancelledHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this
					, &UArcAT_WaitPlayMontageUntilInputRelease::OnAbilityCancelled);

				BlendingOutDelegate.BindUObject(this
					, &UArcAT_WaitPlayMontageUntilInputRelease::OnMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate
					, MontageToPlay);

				MontageEndedDelegate.BindUObject(this
					, &UArcAT_WaitPlayMontageUntilInputRelease::OnMontageEnded);
				AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate
					, MontageToPlay);

				ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
				if (Character && (Character->GetLocalRole() == ROLE_Authority || (
									  Character->GetLocalRole() == ROLE_AutonomousProxy && Ability->
									  GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
				{
					Character->SetAnimRootMotionTranslationScale(AnimRootMotionTranslationScale);
				}

				bPlayedMontage = true;
			}
		}
		else
		{
			ABILITY_LOG(Warning
				, TEXT( "UAuAT_WaitPlayMontageUntilInputRelease call to PlayMontage failed!"));
		}
	}
	else
	{
		ABILITY_LOG(Warning
			, TEXT( "UAuAT_WaitPlayMontageUntilInputRelease called on invalid AbilitySystemComponent"));
	}

	if (!bPlayedMontage)
	{
		ABILITY_LOG(Warning
			, TEXT(
				"UAuAT_WaitPlayMontageUntilInputRelease called in Ability %s failed to play montage %s; Task Instance "
				"Name %s.")
			, *Ability->GetName()
			, *GetNameSafe(MontageToPlay)
			, *InstanceName.ToString());
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast(FGameplayTag()
				, FGameplayEventData());
		}
	}
}

void UArcAT_WaitPlayMontageUntilInputRelease::ExternalCancel()
{
	check(AbilitySystemComponent.IsValid());

	OnAbilityCancelled();

	Super::ExternalCancel();
}

void UArcAT_WaitPlayMontageUntilInputRelease::OnDestroy(bool AbilityEnded)
{
	// Note: Clearing montage end delegate isn't necessary since its not a multicast and
	// will be cleared when the next montage plays. (If we are destroyed, it will detect
	// this and not do anything)

	// This delegate, however, should be cleared as it is a multicast
	if (Ability)
	{
		Ability->OnGameplayAbilityCancelled.Remove(CancelledHandle);
		if (AbilityEnded && bStopWhenAbilityEnds)
		{
			StopPlayingMontage();
		}
	}

	UArcCoreAbilitySystemComponent* RPGAbilitySystemComponent = GetTargetASC();
	if (RPGAbilitySystemComponent)
	{
		RPGAbilitySystemComponent->RemoveGameplayEventTagContainerDelegate(EventTags
			, EventHandle);
	}
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
	ArcASC->RemoveOnInputReleasedMap(GetAbilitySpecHandle()
		, InputDelegateHandle);
	Super::OnDestroy(AbilityEnded);
}

bool UArcAT_WaitPlayMontageUntilInputRelease::StopPlayingMontage()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return false;
	}

	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	if (AnimInstance == nullptr)
	{
		return false;
	}

	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
	
	// Check if the montage is still playing
	// The ability would have been interrupted, in which case we should automatically stop
	// the montage
	if (ArcASC && Ability)
	{
		{
			// Unbind delegates so they don't get called as well
			FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay);
			if (MontageInstance)
			{
				MontageInstance->OnMontageBlendingOutStarted.Unbind();
				MontageInstance->OnMontageEnded.Unbind();
			}

			ArcASC->StopAnimMontage(MontageToPlay);

			return true;
		}
	}

	return false;
}

FString UArcAT_WaitPlayMontageUntilInputRelease::GetDebugString() const
{
	UAnimMontage* PlayingMontage = nullptr;
	if (Ability)
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();

		if (AnimInstance != nullptr)
		{
			PlayingMontage = AnimInstance->Montage_IsActive(MontageToPlay)
							 ? MontageToPlay.Get()
							 : AnimInstance->GetCurrentActiveMontage();
		}
	}

	return FString::Printf(TEXT("PlayMontageAndWaitForEvent. MontageToPlay: %s  (Currently Playing): %s")
		, *GetNameSafe(MontageToPlay)
		, *GetNameSafe(PlayingMontage));
}

void UArcAT_WaitPlayMontageUntilInputRelease::OnInputReleased(FGameplayAbilitySpec& InSpec)
{
	if (!Ability || !AbilitySystemComponent.IsValid())
	{
		return;
	}

	FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get()
		, IsPredictingClient());

	if (Ability->GetCurrentAbilitySpec()->Ability == InSpec.Ability)
	{
		if (JumpToSectionOnConfirm != NAME_Name)
		{
			UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
			ArcASC->PlayAnimMontage(Ability
				, MontageToPlay
				, 1
				, JumpToSectionOnConfirm
				, 0
				, false);
		}
		InputReleased.Broadcast(FGameplayTag()
			, FGameplayEventData());
	}
	SetWaitingOnRemotePlayerData();
	SetWaitingOnAvatar();
}
