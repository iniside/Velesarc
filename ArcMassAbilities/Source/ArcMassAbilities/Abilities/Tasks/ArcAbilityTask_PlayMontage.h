// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Animation/AnimInstance.h"
#include "StateTreeExecutionTypes.h"
#include "ArcAbilityTask_PlayMontage.generated.h"

class UAnimMontage;

USTRUCT()
struct FArcAbilityTask_PlayMontageInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ToolTip = "Override duration. 0 = use montage length."))
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName StartSection = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bLoop = false;

	// Local elapsed time accumulator — updated each Tick instead of polling the sparse fragment.
	float ElapsedTime = 0.f;

	// Resolved duration captured at EnterState. Equals authored Duration if > 0; otherwise
	// montage-play-length / PlayRate. Tick must compare against this, not the authored
	// Duration field — an authored 0 would otherwise Succeed on the first Tick.
	float ResolvedDuration = 0.f;

	// Set by the Montage_SetEndDelegate callback when an early interrupt fires.
	// Tick checks this to return Succeeded immediately, matching the old anim_stopped branch.
	bool bInterrupted = false;

	// Scheduled-tick handle for the natural-end wake (skipped for looping montages).
	UE::StateTree::FScheduledTickHandle ScheduledTickHandle;

	// Anim-end delegate — stored to allow unbind in ExitState.
	FOnMontageEnded EndedDelegate;
};

USTRUCT(DisplayName = "Play Montage", Category = "Arc|Ability|Animation")
struct ARCMASSABILITIES_API FArcAbilityTask_PlayMontage : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	FArcAbilityTask_PlayMontage()
	{
		// Drive scheduling via AddScheduledTickRequest (in EnterState). Without this,
		// GetNextScheduledTick short-circuits to MakeEveryFrames because the state's
		// bCachedRequestTick is set from bShouldCallTick && bConsideredForScheduling.
		bConsideredForScheduling = false;
	}

	virtual bool DrivesScheduledTick() const override { return true; }

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FArcAbilityTask_PlayMontageInstanceData::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
