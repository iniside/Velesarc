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

#include "AbilitySystem/Tasks/Animation/ArcAT_WaitReleasePlayMontageAndWait.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemLog.h"
#include "AbilitySystem/ArcAbilityTargetingComponent.h"
#include "Animation/AnimInstance.h"


#include "ArcCore/AbilitySystem/Actors/ArcTargetingVisualizationActor.h"

UArcAT_WaitReleasePlayMontageAndWait::UArcAT_WaitReleasePlayMontageAndWait(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Rate = 1.f;
	bStopWhenAbilityEnds = true;
	bTickingTask = true;
}

UArcCoreAbilitySystemComponent* UArcAT_WaitReleasePlayMontageAndWait::GetTargetASC()
{
	return Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
}

void UArcAT_WaitReleasePlayMontageAndWait::OnMontageBlendingOut(UAnimMontage* Montage
																, bool bInterrupted)
{
	if (Montage == MontageToPlay)
	{
		AbilitySystemComponent->ClearAnimatingAbility(Ability);

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
				, FGameplayEventData()
				, 0);
		}
	}
	else
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnBlendOut.Broadcast(FGameplayTag()
				, FGameplayEventData()
				, 0);
		}
	}
}

void UArcAT_WaitReleasePlayMontageAndWait::OnAbilityCancelled()
{
	// TODO: Merge this fix back to engine, it was calling the wrong callback

	if (StopPlayingMontage())
	{
		// Let the BP handle the interrupt as well
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast(FGameplayTag()
				, FGameplayEventData()
				, 0);
		}
	}
}

void UArcAT_WaitReleasePlayMontageAndWait::OnMontageEnded(UAnimMontage* Montage
														  , bool bInterrupted)
{
	if (!bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCompleted.Broadcast(FGameplayTag()
				, FGameplayEventData()
				, 0);
		}
	}

	EndTask();
}

void UArcAT_WaitReleasePlayMontageAndWait::OnGameplayEvent(FGameplayTag EventTag
														   , const FGameplayEventData* Payload)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		FGameplayEventData TempData = *Payload;
		TempData.EventTag = EventTag;

		EventReceived.Broadcast(EventTag
			, TempData
			, RelesedTime);
	}
}

UArcAT_WaitReleasePlayMontageAndWait* UArcAT_WaitReleasePlayMontageAndWait::WaitReleasePlayMontageAndWait(
	UGameplayAbility* OwningAbility
	, FName TaskInstanceName
	, UAnimMontage* MontageToPlay
	, FGameplayTagContainer EventTags
	, TSubclassOf<class AArcTargetingVisualizationActor> InVisualizationActorClass
	, class UArcTargetingObject* InConfig
	, float Rate
	, FName SectionToJumpOnConfirm
	, bool bStopWhenAbilityEnds
	, float AnimRootMotionTranslationScale)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(Rate);

	UArcAT_WaitReleasePlayMontageAndWait* MyObj = NewAbilityTask<UArcAT_WaitReleasePlayMontageAndWait>(OwningAbility
		, TaskInstanceName);
	MyObj->MontageToPlay = MontageToPlay;
	MyObj->EventTags = EventTags;
	MyObj->Rate = Rate;
	MyObj->JumpToSectionOnConfirm = SectionToJumpOnConfirm;
	MyObj->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;
	MyObj->bStopWhenAbilityEnds = bStopWhenAbilityEnds;
	MyObj->VisualizationActorClass = InVisualizationActorClass;
	MyObj->Config = InConfig;
	return MyObj;
}

void UArcAT_WaitReleasePlayMontageAndWait::Activate()
{
	if (Ability == nullptr)
	{
		return;
	}
	RelesedTime = 0;
	PressStartTime = 0;


	const FGameplayAbilityActorInfo* Info = Ability->GetCurrentActorInfo();
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
	if (Info->IsLocallyControlled())
	{
		SetWaitingOnRemotePlayerData();
		SetWaitingOnAvatar();

		InputDelegateHandle = ArcASC->AddOnInputReleasedMap(GetAbilitySpecHandle()
			, FArcAbilityInputDelegate::FDelegate::CreateUObject(this
				, &UArcAT_WaitReleasePlayMontageAndWait::HandleOnInputReleased));
	}
	bool bIsLocal = IsLocallyControlled();
	// two ways to do it.
	// provide separate trace which will be used just for position visualization actor in
	// world. assume that hit at 0 is always from line trace.
	if (bIsLocal)
	{
		if (VisualizationActorClass)
		{
			//FArcTargetResult Hits = ATC->RequestCustomTarget(FArcTargetRequest::Create(Ability->GetCurrentAbilitySpec()->Handle), Config, nullptr);
			//
			//FActorSpawnParameters SpawnParams;
			//SpawnParams.Instigator = Cast<APawn>(GetAvatarActor());
			//SpawnParams.Owner = GetOwnerActor();
			//SpawnParams.bNoFail = true;
			//SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			//VisActor = GetWorld()->SpawnActor<AArcTargetingVisualizationActor>(VisualizationActorClass
			//	, Hits.GetImpactPoint(0)
			//	, FRotator::ZeroRotator
			//	, SpawnParams);
			//VisActor->StartVisualization(Config);
		}
	}

	bool bPlayedMontage = false;
	UArcCoreAbilitySystemComponent* RPGAbilitySystemComponent = GetTargetASC();
	PressStartTime = FPlatformTime::Seconds();
	if (RPGAbilitySystemComponent)
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			// Bind to event callback
			EventHandle = RPGAbilitySystemComponent->AddGameplayEventTagContainerDelegate(EventTags
				, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this
					, &UArcAT_WaitReleasePlayMontageAndWait::OnGameplayEvent));

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
					, &UArcAT_WaitReleasePlayMontageAndWait::OnAbilityCancelled);

				BlendingOutDelegate.BindUObject(this
					, &UArcAT_WaitReleasePlayMontageAndWait::OnMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate
					, MontageToPlay);

				MontageEndedDelegate.BindUObject(this
					, &UArcAT_WaitReleasePlayMontageAndWait::OnMontageEnded);
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
				, TEXT( "UAuAT_WaitConfirmPlayMontageAndVisualize call to PlayMontage failed!"));
		}
	}
	else
	{
		ABILITY_LOG(Warning
			, TEXT( "UAuAT_WaitConfirmPlayMontageAndVisualize called on invalid AbilitySystemComponent"));
	}

	if (!bPlayedMontage)
	{
		ABILITY_LOG(Warning
			, TEXT( "UAuAT_WaitConfirmPlayMontageAndVisualize called in Ability %s failed to play montage %s; Task "
				"Instance Name %s.")
			, *Ability->GetName()
			, *GetNameSafe(MontageToPlay)
			, *InstanceName.ToString());
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast(FGameplayTag()
				, FGameplayEventData()
				, 0);
		}
	}

	SetWaitingOnAvatar();
}

void UArcAT_WaitReleasePlayMontageAndWait::TickTask(float DeltaTime)
{
	//FArcTargetResult Hits = ATC->RequestCustomTarget(FArcTargetRequest::Create(Ability->GetCurrentAbilitySpec()->Handle), Config, nullptr);
	//if (VisActor)
	//{
	//	VisActor->SetActorLocation(Hits.GetImpactPoint(0));
	//}
}

void UArcAT_WaitReleasePlayMontageAndWait::ExternalCancel()
{
	check(AbilitySystemComponent.IsValid());

	OnAbilityCancelled();

	Super::ExternalCancel();
}

void UArcAT_WaitReleasePlayMontageAndWait::OnDestroy(bool AbilityEnded)
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

	if (VisActor)
	{
		VisActor->SetActorHiddenInGame(true);
		VisActor->Destroy();
	}
	if (OnConfirmCallbackDelegateHandle.IsValid() && Ability)
	{
		Ability->OnConfirmDelegate.Remove(OnConfirmCallbackDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}

bool UArcAT_WaitReleasePlayMontageAndWait::StopPlayingMontage()
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

FString UArcAT_WaitReleasePlayMontageAndWait::GetDebugString() const
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

void UArcAT_WaitReleasePlayMontageAndWait::HandleOnInputReleased(FGameplayAbilitySpec& InSpec)
{
	ABILITYTASK_MSG("OnConfirmCallback");
	if (VisActor)
	{
		VisActor->SetActorHiddenInGame(true);
		// VisActor->Destroy();
	}
	Ability->OnWaitingForConfirmInputEnd();
	RelesedTime = FPlatformTime::Seconds() - PressStartTime;
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);

	if (JumpToSectionOnConfirm != NAME_Name)
	{
		ArcASC->AnimMontageJumpToSection(MontageToPlay
			, JumpToSectionOnConfirm);
	}
}