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

#include "ArcCore/AbilitySystem/Tasks/ArcAT_PlayMontageNotify.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemLog.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Channels/MovieSceneTimeWarpChannel.h"
#include "GameFramework/Character.h"

UArcAT_PlayMontageNotify::UArcAT_PlayMontageNotify(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MontageInstanceID(INDEX_NONE)
	, bInterruptedCalledBeforeBlendingOut(false)
{
	DesiredPlayTime = -1.f;
	bStopWhenAbilityEnds = true;
}

UArcAT_PlayMontageNotify* UArcAT_PlayMontageNotify::PlayMontageNotify(UGameplayAbility* OwningAbility
																	  , FName TaskInstanceName
																	  , class UAnimMontage* InMontageToPlay
																	  , float DesiredPlayTime
																	  , float InStartingPosition
																	  , bool InbStopWhenAbilityEnds
																	  , float InAnimRootMotionTranslationScale
																	  , FName InStartingSection)
{
	UArcAT_PlayMontageNotify* Proxy = NewAbilityTask<UArcAT_PlayMontageNotify>(OwningAbility
		, TaskInstanceName);
	Proxy->MontageToPlay = InMontageToPlay;
	Proxy->DesiredPlayTime = DesiredPlayTime;
	Proxy->StartingPosition = InStartingPosition;
	Proxy->StartingSection = InStartingSection;
	Proxy->bStopWhenAbilityEnds = InbStopWhenAbilityEnds;
	Proxy->AnimRootMotionTranslationScale = InAnimRootMotionTranslationScale;

	return Proxy;
}

void UArcAT_PlayMontageNotify::Activate()
{
	if (Ability == nullptr)
	{
		return;
	}

	bool bPlayedMontage = false;

	if (AbilitySystemComponent.IsValid())
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
		if (AnimInstance != nullptr)
		{

			float Duration = AbilitySystemComponent->PlayMontage(Ability
					, Ability->GetCurrentActivationInfo()
					, MontageToPlay
					, 1.f
					, StartingSection);
					
			if (Duration > 0.f)
			{
				if (DesiredPlayTime > 0.f)
				{
					const float PlayRate = Duration / DesiredPlayTime;
					AnimInstance->Montage_SetPlayRate(MontageToPlay, PlayRate);	
				}
				
				// Playing a montage could potentially fire off a callback into game code
				// which could kill this ability! Early out if we are  pending kill.
				if (ShouldBroadcastAbilityTaskDelegates() == false)
				{
					return;
				}
				if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay))
				{
					MontageInstanceID = MontageInstance->GetInstanceID();
				}

				if (StartingSection != NAME_None)
				{
					AnimInstance->Montage_JumpToSection(StartingSection, MontageToPlay);
				}
				
				InterruptedHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this, &UArcAT_PlayMontageNotify::OnMontageInterrupted);

				BlendingOutDelegate.BindUObject(this, &UArcAT_PlayMontageNotify::OnMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

				MontageEndedDelegate.BindUObject(this, &UArcAT_PlayMontageNotify::OnMontageEnded);
				AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, MontageToPlay);

				AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &UArcAT_PlayMontageNotify::OnNotifyBeginReceived);
				AnimInstance->OnPlayMontageNotifyEnd.AddDynamic(this, &UArcAT_PlayMontageNotify::OnNotifyEndReceived);

				ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
				
				if (Character && (Character->GetLocalRole() == ROLE_Authority || (
									  Character->GetLocalRole() == ROLE_AutonomousProxy
									  && Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
				{
					Character->SetAnimRootMotionTranslationScale(AnimRootMotionTranslationScale);
				}

				bPlayedMontage = true;

				ActorInfo->AbilitySystemComponent->ForceReplication();
			}
		}
		else
		{
			ABILITY_LOG(Warning
				, TEXT("UAbilityTask_PlayMontageAndWait call to PlayMontage failed!"));
		}
	}
	else
	{
		ABILITY_LOG(Warning
			, TEXT( "UAbilityTask_PlayMontageAndWait called on invalid AbilitySystemComponent"));
	}

	if (!bPlayedMontage)
	{
		ABILITY_LOG(Warning
			, TEXT(
				"UAbilityTask_PlayMontageAndWait called in Ability %s failed to play montage %s; Task Instance Name %s."
			)
			, *Ability->GetName()
			, *GetNameSafe(MontageToPlay)
			, *InstanceName.ToString());
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast(NAME_None);
		}
	}

	SetWaitingOnAvatar();
}

void UArcAT_PlayMontageNotify::ExternalCancel()
{
	check(AbilitySystemComponent.IsValid());

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnInterrupted.Broadcast(NAME_None);
	}
	Super::ExternalCancel();
}

bool UArcAT_PlayMontageNotify::IsNotifyValid(FName NotifyName
											 , const FBranchingPointNotifyPayload& BranchingPointNotifyPayload) const
{
	return ((MontageInstanceID != INDEX_NONE) && (BranchingPointNotifyPayload.MontageInstanceID == MontageInstanceID));
}

void UArcAT_PlayMontageNotify::OnMontageInterrupted()
{
	if (StopPlayingMontage())
	{
		// Let the BP handle the interrupt as well
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast(NAME_None);
		}
	}
}

void UArcAT_PlayMontageNotify::OnNotifyBeginReceived(FName NotifyName
													 , const FBranchingPointNotifyPayload& BranchingPointNotifyPayload)
{
	if (IsNotifyValid(NotifyName
		, BranchingPointNotifyPayload))
	{
		OnNotifyBegin.Broadcast(NotifyName);
	}
}

void UArcAT_PlayMontageNotify::OnNotifyEndReceived(FName NotifyName
												   , const FBranchingPointNotifyPayload& BranchingPointNotifyPayload)
{
	if (IsNotifyValid(NotifyName
		, BranchingPointNotifyPayload))
	{
		OnNotifyEnd.Broadcast(NotifyName);
	}
}

void UArcAT_PlayMontageNotify::OnMontageBlendingOut(UAnimMontage* Montage
													, bool bInterrupted)
{
	if (Ability && Ability->GetCurrentMontage() == MontageToPlay)
	{
		if (Montage == MontageToPlay)
		{
			AbilitySystemComponent->ClearAnimatingAbility(Ability);

			// Reset AnimRootMotionTranslationScale
			ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
			if (Character && (Character->GetLocalRole() == ROLE_Authority || (
								  Character->GetLocalRole() == ROLE_AutonomousProxy && Ability->GetNetExecutionPolicy()
								  == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
			{
				Character->SetAnimRootMotionTranslationScale(1.f);
			}
		}
	}

	if (bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast(NAME_None);
		}
	}
	else
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnBlendOut.Broadcast(NAME_None);
		}
	}
}

void UArcAT_PlayMontageNotify::OnMontageEnded(UAnimMontage* Montage
											  , bool bInterrupted)
{
	if (!bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCompleted.Broadcast(NAME_None);
		}
	}
	else
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCompleted.Broadcast(NAME_None);
		}
	}

	EndTask();
}

void UArcAT_PlayMontageNotify::UnbindDelegates()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return;
	}

	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	if (AnimInstance == nullptr)
	{
		return;
	}

	OnCompleted.Clear();
	OnBlendOut.Clear();
	OnInterrupted.Clear();
	AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UArcAT_PlayMontageNotify::OnNotifyBeginReceived);
	AnimInstance->OnPlayMontageNotifyEnd.RemoveDynamic(this, &UArcAT_PlayMontageNotify::OnNotifyEndReceived);
}

bool UArcAT_PlayMontageNotify::StopPlayingMontage()
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

	// Check if the montage is still playing
	// The ability would have been interrupted, in which case we should automatically stop
	// the montage
	if (AbilitySystemComponent.IsValid() && Ability)
	{
		if (AbilitySystemComponent->GetAnimatingAbility() == Ability && AbilitySystemComponent->GetCurrentMontage() ==
			MontageToPlay)
		{
			// Unbind delegates so they don't get called as well
			FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay);
			if (MontageInstance)
			{
				MontageInstance->OnMontageBlendingOutStarted.Unbind();
				MontageInstance->OnMontageEnded.Unbind();
			}

			AbilitySystemComponent->CurrentMontageStop();
			return true;
		}
	}

	return false;
}

void UArcAT_PlayMontageNotify::OnDestroy(bool AbilityEnded)
{
	if (Ability)
	{
		Ability->OnGameplayAbilityCancelled.Remove(InterruptedHandle);
		if (AbilityEnded && bStopWhenAbilityEnds)
		{
			StopPlayingMontage();
		}
	}
	UnbindDelegates();

	Super::OnDestroy(AbilityEnded);
}
