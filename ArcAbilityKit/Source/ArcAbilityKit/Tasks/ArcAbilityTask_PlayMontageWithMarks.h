// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Animation/AnimMontage.h"
#include "GameplayTagContainer.h"
#include "StateTreeDelegate.h"
#include "StructUtils/InstancedStruct.h"
#include "Tasks/ArcMontageMarkEntry.h"
#include "ArcAbilityTask_PlayMontageWithMarks.generated.h"

USTRUCT()
struct FArcAbilityTask_PlayMontageWithMarksInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName StartSection = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bSyncMarksFromMontageOnSave = false;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TArray<FArcMontageMarkEntry> Marks;

	/** Broadcast from the AnimInstance lambda when the engine reports BlendingOut. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FStateTreeDelegateDispatcher OnBlendingOut;

	/** Broadcast from the AnimInstance lambda when the engine reports the montage ended; task finishes there. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FStateTreeDelegateDispatcher OnEnded;

	/** Rewritten before each per-mark Dispatcher broadcast so listeners can read the firing mark's payload. */
	UPROPERTY(VisibleAnywhere, Category = "Output")
	FGameplayTag LastFiredTag;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FInstancedStruct LastFiredPayload;

	// Captured at EnterState (= MontageLength / PlayRate) for cycle-wrap math.
	float Duration = 0.f;
	// Local accumulator, replaces fragment ElapsedTime.
	float ElapsedTime = 0.f;
	// Index into SortedMarkTimes / SortedMarkOriginalIndices.
	int32 NextMarkIdx = 0;
	// Sorted ascending in [0, Duration); built once at EnterState.
	TArray<float> SortedMarkTimes;
	TArray<int32> SortedMarkOriginalIndices;

	/** Native C++ delegate handles bound to AnimInstance for this montage; unbound on ExitState. */
	FOnMontageBlendingOutStarted BlendingOutDelegate;
	FOnMontageEnded EndedDelegate;
};

USTRUCT(DisplayName = "Play Montage With Marks", Category = "Arc|Ability|Animation")
struct ARCABILITYKIT_API FArcAbilityTask_PlayMontageWithMarks : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_PlayMontageWithMarksInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
