// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeInstanceData.h"
#include "StateTreeReference.h"
#include "StateTreeTypes.h"

#include "ArcCueBehavior.h"
#include "Cues/ArcCueStateTreeContext.h"

#include "ArcCueBehavior_StateTree.generated.h"

USTRUCT()
struct ARCABILITYKIT_API FArcCueBehaviorState_StateTree : public FArcCueBehaviorState
{
	GENERATED_BODY()

	UPROPERTY()
	FStateTreeInstanceData InstanceData;

	UPROPERTY()
	FArcCueStateTreeContext CueContext;

	EStateTreeRunStatus LastRunStatus = EStateTreeRunStatus::Unset;
};

USTRUCT(BlueprintType, meta = (DisplayName = "StateTree"))
struct ARCABILITYKIT_API FArcCueBehavior_StateTree : public FArcCueBehavior
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "StateTree", meta = (Schema = "/Script/ArcAbilityKit.ArcCueStateTreeSchema"))
	FStateTreeReference StateTreeRef;

	virtual void OnExecute(const FArcCueContext& Context) const override;
	virtual TInstancedStruct<FArcCueBehaviorState> OnApplication(const FArcCueContext& Context) const override;
	virtual void OnStateTreeEvent(const FArcCueContext& Context, const FStateTreeEvent& Event, TInstancedStruct<FArcCueBehaviorState>& State) const override;
	virtual void OnTick(const FArcCueContext& Context, FArcCueBehaviorState& State, float DeltaTime) const override;
	virtual void OnRemoved(const FArcCueContext& Context, FArcCueBehaviorState& State) const override;
	virtual bool WantsTick() const override { return true; }
};
