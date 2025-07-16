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

#pragma once
#include "CoreMinimal.h"

#include "Abilities/Tasks/AbilityTask.h"
#include "AbilitySystem/Delegates/ArcPlayMontageAndWaitForEventDynamic.h"
#include "Animation/AnimInstance.h"
#include "ArcAT_PlayMontageAndWaitForEvent.generated.h"

/**
 * This task combines PlayMontageAndWait and WaitForEvent into one task, so you can wait
 * for multiple types of activations such as from a melee combo Much of this code is
 * copied from one of those two ability tasks This is a good task to look at as an example
 * when creating game-specific tasks It is expected that each game will have a set of
 * game-specific tasks to do what they want
 */
UCLASS()
class ARCCORE_API UArcAT_PlayMontageAndWaitForEvent : public UAbilityTask
{
	GENERATED_BODY()

public:
	// Constructor and overrides
	UArcAT_PlayMontageAndWaitForEvent(const FObjectInitializer& ObjectInitializer);

	virtual void Activate() override;

	virtual void ExternalCancel() override;

	virtual FString GetDebugString() const override;

	virtual void OnDestroy(bool AbilityEnded) override;

	/** The montage completely finished playing */
	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnCompleted;

	/** The montage started blending out */
	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnBlendOut;

	/** The montage was interrupted */
	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnInterrupted;

	/** The ability task was explicitly cancelled by another ability */
	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnCancelled;

	/** One of the triggering gameplay events happened */
	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic EventReceived;

	/**
	 * Play a montage and wait for it end. If a gameplay event happens that matches
	 * EventTags (or EventTags is empty), the EventReceived delegate will fire with a tag
	 * and event data. If StopWhenAbilityEnds is true, this montage will be aborted if the
	 * ability ends normally. It is always stopped when the ability is explicitly
	 * cancelled. On normal execution, OnBlendOut is called when the montage is blending
	 * out, and OnCompleted when it is completely done playing OnInterrupted is called if
	 * another montage overwrites this, and OnCancelled is called if the ability or task
	 * is cancelled
	 *
	 * @param TaskInstanceName Set to override the name of this task, for later querying
	 * @param MontageToPlay The montage to play on the character
	 * @param EventTags Any gameplay events matching this tag will activate the
	 * EventReceived callback. If empty, all events will trigger callback
	 * @param Rate Change to play the montage faster or slower
	 * @param bStopWhenAbilityEnds If true, this montage will be aborted if the ability
	 * ends normally. It is always stopped when the ability is explicitly cancelled
	 * @param AnimRootMotionTranslationScale Change to modify size of root motion or set
	 * to 0 to block it entirely
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability|Tasks"
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_PlayMontageAndWaitForEvent* PlayMontageAndWaitForEvent(UGameplayAbility* OwningAbility
																		 , FName TaskInstanceName
																		 , UAnimMontage* MontageToPlay
																		 , FGameplayTagContainer EventTags
																		 , TMap<FGameplayTag, UAnimMontage*> InEventTagToMontageMap 
																		 , FName StartSection = NAME_None
																		 , float DesiredPlayTime = -1.f
																		 , bool bStopWhenAbilityEnds = true
																		 , float AnimRootMotionTranslationScale = 1.f
																		 , bool bInClientOnlyMontage = false
																		 , bool bInCleanOnBlendOut = true);

private:
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<UAnimMontage>> EventTagToMontageMap;
	
	bool bCleanOnBlendOut = true;
	bool bPlayMontage = true;
	/** Montage that is playing */
	UPROPERTY()
	TObjectPtr<UAnimMontage> MontageToPlay;

	/** List of tags to match against gameplay events */
	UPROPERTY()
	FGameplayTagContainer EventTags;

	/** Playback rate */
	UPROPERTY()
	float DesiredPlayTime = -1.f;

	/** Section to start montage from */
	UPROPERTY()
	FName StartSection;

	/** Modifies how root motion movement to apply */
	UPROPERTY()
	float AnimRootMotionTranslationScale;

	/** Rather montage should be aborted if ability ends */
	UPROPERTY()
	bool bStopWhenAbilityEnds;

	bool bClientOnlyMontage = false;

	bool PlayMontage();
	
	/** Checks if the ability is playing a montage and stops that montage, returns true if
	 * a montage was stopped, false if not. */
	bool StopPlayingMontage();

	/** Returns our ability system component */
	class UArcCoreAbilitySystemComponent* GetTargetASC();

	void OnMontageBlendingOut(UAnimMontage* Montage
							  , bool bInterrupted);

	void OnAbilityCancelled();

	void OnMontageEnded(UAnimMontage* Montage
						, bool bInterrupted);

	void OnGameplayEvent(FGameplayTag EventTag
						 , const FGameplayEventData* Payload);

	FOnMontageBlendingOutStarted BlendingOutDelegate;
	FOnMontageEnded MontageEndedDelegate;
	FDelegateHandle CancelledHandle;
	FDelegateHandle EventHandle;
};
