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



#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "AbilitySystem/Delegates/ArcPlayMontageAndWaitForEventDynamic.h"
#include "Animation/AnimMontage.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "ArcAT_WaitActivationTimeWithAnimation.generated.h"

USTRUCT()
struct FArcItemFragment_ActivationTimeMontages : public FArcItemFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game"))
	TSoftObjectPtr<UAnimMontage> StartMontage;

	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game"))
	TSoftObjectPtr<UAnimMontage> LoopMontage;

	// Optional montage that will loop, when activation time is reached, and we wait for user confirmation.
	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game"))
	TSoftObjectPtr<UAnimMontage> LoopActivatedMontage;;
	
	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game"))
	TSoftObjectPtr<UAnimMontage> EndMontage;

	UPROPERTY(EditAnywhere)
	bool bBroadcastEventTagsOnInputConfirm = false;

	UPROPERTY(EditAnywhere)
	bool bReachedActivationTime = false;

	UPROPERTY(EditAnywhere)
	FGameplayTag ActivationTag;
};

/**
 * 
 */
UCLASS()
class ARCCORE_API UArcAT_WaitActivationTimeWithAnimation : public UAbilityTask
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability|Tasks"
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitActivationTimeWithAnimation* WaitActivationTimeWithAnimation (UGameplayAbility* OwningAbility
																					, FName TaskInstanceName
																					, bool bInWaitForInputConfirm
																					, bool bInBroadcastEventTagsOnInputConfirm
																					, FGameplayTagContainer InEventTags
																					, FGameplayTag InActivationTag
																					, FGameplayTagContainer InputConfirmedRequiredTags
																					, FGameplayTagContainer InputDenyRequiredTags);

	virtual void Activate() override;

	void HandleOnActivationFinished();
	void HandleFinished();
	
	void OnStartMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	void HandleActivationTimeChanged(UArcCoreGameplayAbility* InAbility, float NewTime);

	void OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

	UFUNCTION()
	void HandleOnInputPressed(FGameplayAbilitySpec& InSpec);

	bool bWaitForInputConfirm = false;
	bool bBroadcastEventTagsOnInputConfirm = false;
	bool bReachedActivationTime = false;

	FGameplayTagContainer InputConfirmedRequiredTags;
	FGameplayTagContainer InputDenyRequiredTags;
	
	FGameplayTagContainer EventTags;
	FGameplayTag ActivationTag;

	FDelegateHandle EventHandle;
	FTimerHandle ActivationTimerHandle;

	FDelegateHandle InputDelegateHandle;
	
	FOnMontageEnded MontageEndedDelegate;
	FOnMontageBlendingOutStarted BlendingOutDelegate;
	
	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnEventReceived;

	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnActivationFinished;
	
	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnInputConfirmed;
};
