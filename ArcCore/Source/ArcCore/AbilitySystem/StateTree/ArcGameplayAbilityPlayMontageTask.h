// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeDelegate.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "Animation/AnimMontage.h"
#include "UObject/Object.h"
#include "ArcGameplayAbilityPlayMontageTask.generated.h"

class UAbilitySystemComponent;
class UAnimMontage;

USTRUCT()
struct FArcGameplayAbilityPlayMontageTaskInstanceData
{
	GENERATED_BODY()

	/** The ability system component used to play the montage and send gameplay events. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** The animation montage to play. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UAnimMontage> MontageToPlay;

	/** Delegate broadcast when the montage finishes playing or is interrupted. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnMontageEnded;

	/** Optional montage section name to jump to after starting playback. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FName SectionName = NAME_None;

	/** Gameplay tag sent as gameplay event when the montage ends. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag EndEventTag;

	/** When true, gathers UArcAnimNotify_MarkGameplayEvent notifies from the montage and fires them as gameplay events during playback. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bGatherNotifies = false;

	/** Blend out time in seconds when stopping the montage. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float EndBlendTime = 0.1;

	/** When true, uses the Duration parameter instead of the montage's natural length to calculate play rate. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bUseCustomDuration = false;

	/** Custom duration in seconds. The montage play rate is adjusted so it finishes in this time. Only used when bUseCustomDuration is true. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 0;

	/** When true, the montage loops until the state is exited. The task stays Running indefinitely. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bIsLooping = false;

	UPROPERTY()
	TArray<FArcNotifyEvent> NotifyEvents;

	UPROPERTY()
	float CurrentTime = 0;

	UPROPERTY()
	float PlayedMontageLength = 0;

	FOnMontageEnded MontageEndedDelegate;
	FOnMontageSectionChanged SectionChangedDelegate;
};

/**
 * Plays an animation montage on the ability system component's avatar actor.
 * Supports custom duration (adjusting play rate), section jumps, looping, notify gathering, and end event broadcasting.
 * When not looping, succeeds after the montage duration elapses. When looping, stays Running until the state exits.
 * Stops the montage on exit if looping.
 */
USTRUCT(meta = (DisplayName = "Play Montage", Tooltip = "Plays an animation montage on the ability system component's avatar actor. Supports custom duration (adjusting play rate), section jumps, looping, notify gathering, and end event broadcasting. When not looping, succeeds after the montage duration elapses. When looping, stays Running until the state exits. Stops the montage on exit if looping."))
struct FArcGameplayAbilityPlayMontageTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcGameplayAbilityPlayMontageTaskInstanceData;

	FArcGameplayAbilityPlayMontageTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};