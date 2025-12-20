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
#include "Containers/Ticker.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "ArcAT_WaitActivationTimeWithAnimation.generated.h"

class UInputMappingContext;
class UInputAction;

UENUM(BlueprintType)
enum class EArcActivationMontageMode : uint8
{
	// One montage will be played from start to end.
	SingleMontage,
	/**
	 * Started Montage will be played, then loop montage will be played in loop, until there is confirmation
	 * After confirmation end montage will be played.
	 */
	StartEndAndLoopMontage,
	
	// Start montage will be played first then End Montage will be played.
	StartAndEndMontage
};

USTRUCT(BlueprintType, meta = (Category = "Gameplay Ability"))
struct FArcItemFragment_ActivationTimeMontages : public FArcItemFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game"))
	EArcActivationMontageMode Mode = EArcActivationMontageMode::SingleMontage;
	
	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game"))
	FGameplayTagContainer EventTags;
	
	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game"))
	TSoftObjectPtr<UAnimMontage> StartMontage;

	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game", EditConditionHides, EditCondition="Mode == EArcActivationMontageMode::StartEndAndLoopMontage"))
	TSoftObjectPtr<UAnimMontage> LoopMontage;
	
	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game", EditConditionHides, EditCondition="Mode == EArcActivationMontageMode::StartAndEndMontage || Mode == EArcActivationMontageMode::StartEndAndLoopMontage"))
	TSoftObjectPtr<UAnimMontage> EndMontage;

	/**
	 * Montage that will be played after End Montage.
	 */
	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game"))
	TSoftObjectPtr<UAnimMontage> PostEndMontage;
	
	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game"))
	TObjectPtr<UInputMappingContext> InputMappingContext;
	
	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game"))
	int32 InputPriority = 200;
	
	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game"))
	TObjectPtr<UInputAction> InputAction;
	
	UPROPERTY(EditAnywhere)
	bool bBroadcastEventTagsOnInputConfirm = false;

	UPROPERTY(EditAnywhere)
	bool bReachedActivationTime = false;

	/**
	 * If tru it will require pressing input action to confirm activation and then it will play EndMontage and PostEndMontage
	 */
	UPROPERTY(EditAnywhere)
	bool bRequiresInputConfirm = false;
	
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
																					, bool bInBroadcastEventTagsOnInputConfirm
																					, float EndTime
																					, FGameplayTag NotifyEndTimeTag
																					, FGameplayTagContainer InEventTags
																					, FGameplayTag InActivationTag
																					, FGameplayTagContainer InputConfirmedRequiredTags
																					, FGameplayTagContainer InputDenyRequiredTags);

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;
	
	void HandleOnActivationFinished();
	void HandleFinished();
	
	void HandleOnStartMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void HandleOnEndMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void HandleOnPostEndMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	
	void HandleActivationTimeChanged(UArcCoreGameplayAbility* InAbility, float NewTime);

	void OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

	UFUNCTION()
	void HandleOnInputPressed(FGameplayAbilitySpec& InSpec);

	void HandleOnInputTriggered(UInputAction* InInputAction);
	
	EArcActivationMontageMode Mode;
	
	bool bBroadcastEventTagsOnInputConfirm = false;
	bool bReachedActivationTime = false;
	float EndTime = 0;
	bool bFinished = false;
	float StartEndPlayRate = 1.f;
	
	FGameplayTagContainer InputConfirmedRequiredTags;
	FGameplayTagContainer InputDenyRequiredTags;
	
	FGameplayTagContainer EventTags;
	FGameplayTag ActivationTag;
	FGameplayTag NotifyEndTimeTag;
	
	FDelegateHandle EventHandle;
	FTimerHandle ActivationTimerHandle;

	FDelegateHandle InputDelegateHandle;
	
	FOnMontageEnded MontageEndedDelegate;
	FOnMontageEnded StartMontageEndedDelegate;
	
	FOnMontageEnded EndMontageEndedDelegate;
	FOnMontageEnded PostEndMontageEndedDelegate;
	
	TWeakObjectPtr<UAnimMontage> CurrentMontage;
	
	int32 TriggeredHandle;
	
	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnEventReceived;

	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnActivationFinished;

	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnStartMontageStarted;
	
	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnStartMontageEnded;
	
	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnEndMontageStarted;
	
	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnEndMontageEnded;
	
	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnInputConfirmed;
	
	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnActivationTimeChanged;
	

	UPROPERTY(BlueprintAssignable)
	FArcPlayMontageAndWaitForEventDynamic OnPostEndMontageEnded;
	
	bool HandleNotifiesTick(float DeltaTime);
	
	struct Notify
	{
		float OriginalTime = 0;
		float Time = 0;
		FGameplayTag Tag;
		bool bPlayed;
		
		bool bResetPlayRate = false;
		bool bChangePlayRate = false;
		float PlayRate = 1.0f;
	};
	TArray<Notify> Notifies;
	
	bool bPlayNotifiesInLoop = false;
	
	FTSTicker::FDelegateHandle TickerHandle;
	float CurrentNotifyTime = 0;
};
