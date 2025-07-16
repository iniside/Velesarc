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

#include "Animation/AnimMontage.h"
#include "ArcCore/AbilitySystem/Tasks/ArcAbilityTask.h"
#include "ArcAT_PlayMontageNotify.generated.h"

class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcOnMontagePlayDelegate
	, FName
	, NotifyName);

/**
 *
 */
UCLASS()
class ARCCORE_API UArcAT_PlayMontageNotify : public UArcAbilityTask
{
	GENERATED_BODY()

	// Called when Montage finished playing and wasn't interrupted
	UPROPERTY(BlueprintAssignable)
	FArcOnMontagePlayDelegate OnCompleted;

	// Called when Montage starts blending out and is not interrupted
	UPROPERTY(BlueprintAssignable)
	FArcOnMontagePlayDelegate OnBlendOut;

	// Called when Montage has been interrupted (or failed to play)
	UPROPERTY(BlueprintAssignable)
	FArcOnMontagePlayDelegate OnInterrupted;

	UPROPERTY(BlueprintAssignable)
	FArcOnMontagePlayDelegate OnNotifyBegin;

	UPROPERTY(BlueprintAssignable)
	FArcOnMontagePlayDelegate OnNotifyEnd;

	/**
	 * @brief Plays replicated montage with broadcasting various montage events.
	 * @param OwningAbility 
	 * @param TaskInstanceName 
	 * @param InMontageToPlay 
	 * @param InPlayRate 
	 * @param InStartingPosition 
	 * @param InbStopWhenAbilityEnds 
	 * @param InAnimRootMotionTranslationScale 
	 * @param InStartingSection 
	 * @return 
	 */
	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability|Tasks"
		, meta = (DisplayName = "Play Montage Wait Notify", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility",
			BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_PlayMontageNotify* PlayMontageNotify(UGameplayAbility* OwningAbility
													   , FName TaskInstanceName
													   , class UAnimMontage* InMontageToPlay
													   , float DesiredPlayTime = -1.f
													   , float InStartingPosition = 0.f
													   , bool InbStopWhenAbilityEnds = true
													   , float InAnimRootMotionTranslationScale = 1.0f
													   , FName InStartingSection = NAME_None);

public:
	UArcAT_PlayMontageNotify(const FObjectInitializer& ObjectInitializer);

	//~ Begin UObject Interface
	virtual void OnDestroy(bool AbilityEnded) override;

	//~ End UObject Interface
	virtual void Activate() override;

	virtual void ExternalCancel() override;

protected:
	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* Montage
							  , bool bInterrupted);

	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage
						, bool bInterrupted);

	UFUNCTION()
	void OnNotifyBeginReceived(FName NotifyName
							   , const FBranchingPointNotifyPayload& BranchingPointNotifyPayload);

	UFUNCTION()
	void OnNotifyEndReceived(FName NotifyName
							 , const FBranchingPointNotifyPayload& BranchingPointNotifyPayload);

	UFUNCTION()
	void OnMontageInterrupted();

protected:
	UPROPERTY()
	TObjectPtr<class UAnimMontage> MontageToPlay;

	UPROPERTY()
	float DesiredPlayTime;

	UPROPERTY()
	float StartingPosition;

	UPROPERTY()
	FName StartingSection;

	UPROPERTY()
	bool bStopWhenAbilityEnds;

	UPROPERTY()
	float AnimRootMotionTranslationScale;

private:
	int32 MontageInstanceID;
	uint32 bInterruptedCalledBeforeBlendingOut : 1;

	bool StopPlayingMontage();

	bool IsNotifyValid(FName NotifyName
					   , const FBranchingPointNotifyPayload& BranchingPointNotifyPayload) const;

	void UnbindDelegates();

	FOnMontageBlendingOutStarted BlendingOutDelegate;
	FOnMontageEnded MontageEndedDelegate;
	FDelegateHandle InterruptedHandle;
};
