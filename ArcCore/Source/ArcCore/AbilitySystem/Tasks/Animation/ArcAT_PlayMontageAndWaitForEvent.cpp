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

#include "ArcCore/AbilitySystem/Tasks/Animation/ArcAT_PlayMontageAndWaitForEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemLog.h"
#include "Animation/AnimInstance.h"
#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "ArcCore/AbilitySystem/ArcCoreGameplayAbility.h"

UArcAT_PlayMontageAndWaitForEvent::UArcAT_PlayMontageAndWaitForEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DesiredPlayTime = -1.f;
	bStopWhenAbilityEnds = true;
}

UArcCoreAbilitySystemComponent* UArcAT_PlayMontageAndWaitForEvent::GetTargetASC()
{
	return Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
}

void UArcAT_PlayMontageAndWaitForEvent::OnMontageBlendingOut(UAnimMontage* Montage
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
			OnInterrupted.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}
	else
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnBlendOut.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}

	if (bCleanOnBlendOut)
	{
		UArcCoreAbilitySystemComponent* ArcASC = GetTargetASC();
		if (ArcASC)
		{
			ArcASC->RemoveGameplayEventTagContainerDelegate(EventTags, EventHandle);
			ArcASC->RemoveMontageFromItem(MontageToPlay);
		}
	}
}

void UArcAT_PlayMontageAndWaitForEvent::OnAbilityCancelled()
{
	// TODO: Merge this fix back to engine, it was calling the wrong callback

	if (StopPlayingMontage())
	{
		// Let the BP handle the interrupt as well
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}
}

void UArcAT_PlayMontageAndWaitForEvent::OnMontageEnded(UAnimMontage* Montage
													   , bool bInterrupted)
{
	if (!bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCompleted.Broadcast(FGameplayTag(), FGameplayEventData());
		}

		UArcCoreAbilitySystemComponent* ArcASC = GetTargetASC();
		if (ArcASC)
		{
			ArcASC->RemoveGameplayEventTagContainerDelegate(EventTags, EventHandle);
			ArcASC->RemoveMontageFromItem(MontageToPlay);
		}
	}
}

void UArcAT_PlayMontageAndWaitForEvent::OnGameplayEvent(FGameplayTag EventTag
														, const FGameplayEventData* Payload)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		FGameplayEventData TempData = *Payload;
		TempData.EventTag = EventTag;

		EventReceived.Broadcast(EventTag, TempData);

		if (TObjectPtr<UAnimMontage>* MontagePtr = EventTagToMontageMap.Find(EventTag))
		{
			UAnimMontage* Montage = MontagePtr->Get();
			if (Montage)
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
				
				FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay);
				if (MontageInstance)
				{
					MontageInstance->OnMontageBlendingOutStarted.Unbind();
					MontageInstance->OnMontageEnded.Unbind();
				}

				DesiredPlayTime = -1.f;
				// Play the new montage
				MontageToPlay = Montage;
				PlayMontage();
			}
		}
	}
}

UArcAT_PlayMontageAndWaitForEvent* UArcAT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
	UGameplayAbility* OwningAbility
	, FName TaskInstanceName
	, UAnimMontage* MontageToPlay
	, FGameplayTagContainer EventTags
	, TMap<FGameplayTag, UAnimMontage*> InEventTagToMontageMap
	, FName StartSection
	, float DesiredPlayTime
	, bool bStopWhenAbilityEnds
	, float AnimRootMotionTranslationScale
	, bool bInClientOnlyMontage
	, bool bInCleanOnBlendOut)
{
	UArcAT_PlayMontageAndWaitForEvent* MyObj = NewAbilityTask<UArcAT_PlayMontageAndWaitForEvent>(OwningAbility, TaskInstanceName);

	Algo::Transform(InEventTagToMontageMap, MyObj->EventTagToMontageMap
		, [](const TPair<FGameplayTag, UAnimMontage*>& Pair)
		{
			return TPair<FGameplayTag, TObjectPtr<UAnimMontage>>(Pair.Key, Pair.Value);
		});
	MyObj->DesiredPlayTime = DesiredPlayTime;
	MyObj->MontageToPlay = MontageToPlay;
	MyObj->EventTags = EventTags;
	MyObj->StartSection = StartSection;
	MyObj->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;
	MyObj->bStopWhenAbilityEnds = bStopWhenAbilityEnds;
	MyObj->bClientOnlyMontage = bInClientOnlyMontage;
	MyObj->bCleanOnBlendOut = bInCleanOnBlendOut;
	
	return MyObj;
}

void UArcAT_PlayMontageAndWaitForEvent::Activate()
{
	if (Ability == nullptr || MontageToPlay == nullptr)
	{
		OnCompleted.Broadcast(FGameplayTag(), FGameplayEventData());
		return;
	}

	bool bPlayedMontage = false;

	UArcCoreAbilitySystemComponent* ArcASC = GetTargetASC();

	if (ArcASC)
	{
		ACharacter* C = Cast<ACharacter>(GetAvatarActor());
		UCharacterMovementComponent* CMC = C->GetCharacterMovement();

		// we are going to accept root motion montages as is.
		// still we probabaly should do it earlier in execution chain, like
		// when activating ability.
		CMC->bServerAcceptClientAuthoritativePosition = true;
		CMC->bIgnoreClientMovementErrorChecksAndCorrection = true;

		if (C && (C->GetLocalRole() == ROLE_Authority || (
					  C->GetLocalRole() == ROLE_AutonomousProxy && Ability->GetNetExecutionPolicy() ==
					  EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
		{
			C->SetAnimRootMotionTranslationScale(AnimRootMotionTranslationScale);
		}

		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			ENetMode NM = ArcASC->GetNetMode();
			if (bClientOnlyMontage == true && NM == ENetMode::NM_DedicatedServer)
			{
				bPlayMontage = false;
			}
			if (bPlayMontage)
			{
				// Bind to event callback
				EventHandle = ArcASC->AddGameplayEventTagContainerDelegate(EventTags
					, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this
						, &UArcAT_PlayMontageAndWaitForEvent::OnGameplayEvent));

				bPlayedMontage = PlayMontage();
			}
		}
		else
		{
			ABILITY_LOG(Warning
				, TEXT("UAuAT_PlayMontageAndWaitForEvent call to PlayMontage failed!"));
		}
	}
	else
	{
		ABILITY_LOG(Warning
			, TEXT( "UAuAT_PlayMontageAndWaitForEvent called on invalid AbilitySystemComponent"));
	}

	if (!bPlayedMontage)
	{
		ABILITY_LOG(Warning
			, TEXT(
				"UAuAT_PlayMontageAndWaitForEvent called in Ability %s failed to play montage %s; Task Instance Name "
				"%s.")
			, *Ability->GetName()
			, *GetNameSafe(MontageToPlay)
			, *InstanceName.ToString());
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast(FGameplayTag()
				, FGameplayEventData());
		}
	}

	SetWaitingOnAvatar();
}

void UArcAT_PlayMontageAndWaitForEvent::ExternalCancel()
{
	check(AbilitySystemComponent.IsValid());

	OnAbilityCancelled();

	Super::ExternalCancel();
}

void UArcAT_PlayMontageAndWaitForEvent::OnDestroy(bool AbilityEnded)
{
	ACharacter* C = Cast<ACharacter>(GetAvatarActor());
	UCharacterMovementComponent* CMC = C->GetCharacterMovement();

	// we are going to accept root motion montages as is.
	CMC->bServerAcceptClientAuthoritativePosition = false;
	CMC->bIgnoreClientMovementErrorChecksAndCorrection = false;

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

	UArcCoreAbilitySystemComponent* ArcASC = GetTargetASC();
	if (ArcASC)
	{
		ArcASC->RemoveGameplayEventTagContainerDelegate(EventTags, EventHandle);
		ArcASC->RemoveMontageFromItem(MontageToPlay);
	}
	
	Super::OnDestroy(AbilityEnded);
}

bool UArcAT_PlayMontageAndWaitForEvent::PlayMontage()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	if (AnimInstance != nullptr)
	{
		UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Ability);
		UArcCoreAbilitySystemComponent* ArcASC = GetTargetASC();
		ENetMode NM = ArcASC->GetNetMode();
		if (bClientOnlyMontage == true && NM == ENetMode::NM_DedicatedServer)
		{
			bPlayMontage = false;
		}
		if (bPlayMontage)
		{
			float MontageLenght = 0.f;

			if (!StartSection.IsNone())
			{
				int32 Idx = MontageToPlay->GetSectionIndex(StartSection);
				if (Idx != INDEX_NONE)
				{
					MontageLenght = MontageToPlay->GetSectionLength(Idx);		
				}
				else
				{
					MontageLenght = MontageToPlay->GetSectionLength(Idx);
				}
			}
			else
			{
				MontageLenght = MontageToPlay->GetSectionLength(0);
			}
			
			float PlayRate = 1.0f;

			
			if (DesiredPlayTime > 0.0f)
			{
				PlayRate = MontageLenght / DesiredPlayTime;
			}

			bool bPlayed = false;

			bool bIsAlreadyPlaying = AnimInstance->Montage_IsPlaying(MontageToPlay);
			// I'm not sure. Jump to section, and set bPlayed=true, return from node
			// and make other node to handle jumping ?
			if (bIsAlreadyPlaying && StartSection.IsNone() == false)
			{
				ArcASC->AnimMontageJumpToSection(MontageToPlay
					, StartSection);
				bPlayed = true;
			}
			else
			{
				const UArcItemDefinition* ItemDef = ArcAbility->NativeGetOwnerItemData();
				bPlayed = ArcASC->PlayAnimMontage(Ability
					, MontageToPlay
					, PlayRate
					, StartSection
					, 0
					, bClientOnlyMontage
					, ItemDef);
			}

			if (bPlayed)
			{
				UArcCoreGameplayAbility* ArcCoreAbility = Cast<UArcCoreGameplayAbility>(Ability);
				const FArcItemData* ItemData = ArcCoreAbility->GetOwnerItemEntryPtr();
				if (ItemData)
				{
					ArcASC->AddMontageToItem(MontageToPlay, ItemData);	
				}
				
				// Playing a montage could potentially fire off a callback into game
				// code which could kill this ability! Early out if we are  pending
				// kill.
				if (ShouldBroadcastAbilityTaskDelegates() == false)
				{
					return true;
				}

				CancelledHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this, &UArcAT_PlayMontageAndWaitForEvent::OnAbilityCancelled);

				BlendingOutDelegate.BindUObject(this, &UArcAT_PlayMontageAndWaitForEvent::OnMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

				MontageEndedDelegate.BindUObject(this, &UArcAT_PlayMontageAndWaitForEvent::OnMontageEnded);
				AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, MontageToPlay);

				
				return true;
			}
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAuAT_PlayMontageAndWaitForEvent call to PlayMontage failed!"));
		return false;
	}

	return false;
}

bool UArcAT_PlayMontageAndWaitForEvent::StopPlayingMontage()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return false;
	}

	ACharacter* C = Cast<ACharacter>(GetAvatarActor());
	UCharacterMovementComponent* CMC = C->GetCharacterMovement();

	// we are going to accept root motion montages as is.
	// still we probabaly should do it earlier in execution chain, like
	// when activating ability.
	CMC->bServerAcceptClientAuthoritativePosition = false;
	CMC->bIgnoreClientMovementErrorChecksAndCorrection = false;

	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	if (AnimInstance == nullptr)
	{
		return false;
	}

	// Check if the montage is still playing
	// The ability would have been interrupted, in which case we should automatically stop
	// the montage
	UArcCoreAbilitySystemComponent* ArcASC = GetTargetASC();
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
			ArcASC->RemoveMontageFromItem(MontageToPlay);
			return true;
		}
	}
	// else if(AbilitySystemComponent)
	//{
	//	AbilitySystemComponent->CurrentMontageStop();
	//	FAnimMontageInstance* MontageInstance =
	// AnimInstance->GetActiveInstanceForMontage(MontageToPlay); 	if (MontageInstance)
	//	{
	//		MontageInstance->OnMontageBlendingOutStarted.Unbind();
	//		MontageInstance->OnMontageEnded.Unbind();
	//	}
	// }

	return false;
}

FString UArcAT_PlayMontageAndWaitForEvent::GetDebugString() const
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
