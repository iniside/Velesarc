// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Animation/AnimMontage.h"
#include "StateTreeDelegate.h"
#include "StructUtils/InstancedStruct.h"
#include "Tasks/ArcMontageMarkEntry.h"
#include "ArcAbilityTask_PlayMontageStartLoopEndWithMarks.generated.h"

enum class EArcAbilityMontageStartLoopEndPhase : uint8
{
	Start,
	Loop,
	End,
	Completed,
};

struct FArcAbilityMontagePhaseMarkRuntime
{
	TArray<float> Times;
	TArray<int32> OriginalIndices;
	int32 NextIndex = 0;
};

USTRUCT()
struct FArcAbilityTask_PlayMontageStartLoopEndWithMarksInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimMontage> StartMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimMontage> LoopMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimMontage> EndMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bUseDuration = false;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (EditCondition = "bUseDuration", ClampMin = "0.0"))
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bSyncMarksFromMontagesOnSave = false;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TArray<FArcMontageMarkEntry> StartMarks;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TArray<FArcMontageMarkEntry> LoopMarks;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TArray<FArcMontageMarkEntry> EndMarks;

	UPROPERTY(EditAnywhere, Category = "Output")
	FStateTreeDelegateDispatcher OnMark;

	UPROPERTY(EditAnywhere, Category = "Output")
	FStateTreeDelegateDispatcher OnCompleted;

	UPROPERTY(EditAnywhere, Category = "Output")
	FStateTreeDelegateDispatcher OnInterrupted;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FGameplayTag LastFiredTag;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FInstancedStruct LastFiredPayload;

	float StartDuration = 0.f;
	float LoopDuration = 0.f;
	float EndDuration = 0.f;
	float LoopInterval = 0.f;
	float StartRateScale = 1.f;
	float LoopRateScale = 1.f;
	float EndRateScale = 1.f;
	float PhaseElapsedTime = 0.f;
	float LoopCycleElapsedTime = 0.f;
	EArcAbilityMontageStartLoopEndPhase Phase = EArcAbilityMontageStartLoopEndPhase::Start;
	FArcAbilityMontagePhaseMarkRuntime StartMarkRuntime;
	FArcAbilityMontagePhaseMarkRuntime LoopMarkRuntime;
	FArcAbilityMontagePhaseMarkRuntime EndMarkRuntime;
	UAnimMontage* ActiveMontage = nullptr;
	bool bVisualPositionNeedsSync = false;
	bool bTerminalDelegateFired = false;
};

USTRUCT(DisplayName = "Play Montage Start Loop End With Marks", Category = "Arc|Ability|Animation")
struct ARCABILITYKIT_API FArcAbilityTask_PlayMontageStartLoopEndWithMarks : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_PlayMontageStartLoopEndWithMarksInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
