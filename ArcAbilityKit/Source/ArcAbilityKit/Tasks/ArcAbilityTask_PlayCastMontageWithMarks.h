// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Animation/AnimMontage.h"
#include "GameplayTagContainer.h"
#include "StateTreeDelegate.h"
#include "StructUtils/InstancedStruct.h"
#include "Tasks/ArcMontageMarkEntry.h"
#include "ArcAbilityTask_PlayCastMontageWithMarks.generated.h"

UENUM(BlueprintType)
enum class EArcAbilityCastTime : uint8
{
	QuarterSecond UMETA(DisplayName = "0.25 Seconds"),
	HalfSecond UMETA(DisplayName = "0.5 Seconds"),
	ThreeQuarterSecond UMETA(DisplayName = "0.75 Seconds"),
	OneSecond UMETA(DisplayName = "1 Second"),
	TwoSeconds UMETA(DisplayName = "2 Seconds"),
	Custom,
};

enum class EArcAbilityCastMontagePhase : uint8
{
	Start,
	Loop,
	End,
	Completed,
};

struct FArcAbilityCastMontagePhaseMarkRuntime
{
	TArray<float> Times;
	TArray<int32> OriginalIndices;
	int32 NextIndex = 0;
};

USTRUCT()
struct FArcAbilityTask_PlayCastMontageWithMarksInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EArcAbilityCastTime CastTime = EArcAbilityCastTime::OneSecond;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (EditCondition = "CastTime == EArcAbilityCastTime::Custom", ClampMin = "2.0"))
	float CustomCastTime = 2.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimMontage> StartMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimMontage> LoopMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimMontage> EndMontage = nullptr;

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
	float StartInputPlayRate = 1.f;
	float EndInputPlayRate = 1.f;
	float StartEffectivePlayRate = 1.f;
	float LoopEffectivePlayRate = 1.f;
	float EndEffectivePlayRate = 1.f;
	float PhaseElapsedTime = 0.f;
	float LoopCycleElapsedTime = 0.f;
	EArcAbilityCastMontagePhase Phase = EArcAbilityCastMontagePhase::Start;
	FArcAbilityCastMontagePhaseMarkRuntime StartMarkRuntime;
	FArcAbilityCastMontagePhaseMarkRuntime LoopMarkRuntime;
	FArcAbilityCastMontagePhaseMarkRuntime EndMarkRuntime;
	UAnimMontage* ActiveMontage = nullptr;
	bool bUseStart = false;
	bool bUseLoop = false;
	bool bVisualPositionNeedsSync = false;
	bool bTerminalDelegateFired = false;
};

USTRUCT(DisplayName = "Play Cast Montage With Marks", Category = "Arc|Ability|Animation")
struct ARCABILITYKIT_API FArcAbilityTask_PlayCastMontageWithMarks : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_PlayCastMontageWithMarksInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
