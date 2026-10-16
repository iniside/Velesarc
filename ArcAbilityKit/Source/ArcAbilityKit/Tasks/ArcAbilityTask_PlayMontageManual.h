// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Animation/AnimMontage.h"
#include "GameplayTagContainer.h"
#include "StateTreeDelegate.h"
#include "StateTreeExecutionTypes.h"
#include "ArcAbilityTask_PlayMontageManual.generated.h"

UENUM(BlueprintType)
enum class EArcManualMontageEndMode : uint8
{
	FixedDuration,
	NaturalLength,
};

USTRUCT()
struct FArcAbilityTask_PlayMontageManualInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName StartSection = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EArcManualMontageEndMode EndMode = EArcManualMontageEndMode::FixedDuration;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float Duration = 1.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float PlayRate = 1.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float BlendOutTime = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, Category = "Output")
	FStateTreeDelegateDispatcher OnCompleted;

	UPROPERTY(EditAnywhere, Category = "Output")
	FStateTreeDelegateDispatcher OnInterrupted;

	UPROPERTY(EditAnywhere, Category = "Output")
	FStateTreeDelegateDispatcher OnNotify;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FGameplayTag LastFiredTag;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float ElapsedTime = 0.f;

	UPROPERTY()
	bool bLocalCompletionFired = false;

	// Montage end time computed in EnterState; Tick compares ElapsedTime against this.
	float EndTime = 0.f;

	// Sorted-ascending notify trigger times, already divided by EffectivePlayRate
	// so they line up with InstanceData.ElapsedTime.
	TArray<float> SortedNotifyTimes;
	TArray<FGameplayTag> SortedNotifyTags;
	int32 NextNotifyIdx = 0;

	// MontageLength / EffectivePlayRate. Used for cycle-wrap of notify-fire when bLoop.
	float CycleLength = 0.f;

	FOnMontageEnded EndedDelegate;
};

USTRUCT(DisplayName = "Play Montage Manual", Category = "Arc|Ability|Animation")
struct ARCABILITYKIT_API FArcAbilityTask_PlayMontageManual : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_PlayMontageManualInstanceData;

	FArcAbilityTask_PlayMontageManual() {}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual EDataValidationResult Compile(UE::StateTree::ICompileNodeContext& CompileContext) override;
#endif
};
