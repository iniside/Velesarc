// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Actions/ArcTacticsAction.h"
#include "CoreMinimal.h"
#include "Decision/ArcTacticsNode.h"

#include "ArcTacticsSequenceAction.generated.h"

struct FArcTQSQueryInstance;

USTRUCT(BlueprintType, meta = (ToolTip = "Runtime state of a Tactical Action Sequence: the current step and its async request generations."))
struct ARCTACTICS_API FArcTacticsSequenceActionInstanceData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 StepIndex = INDEX_NONE;

	// Request generations live on the container so a completion requested by an earlier step never matches a later one.
	UPROPERTY()
	uint32 RequestGeneration = 0;

	UPROPERTY()
	uint32 RequestDecisionGeneration = 0;

	UPROPERTY()
	bool bActiveStepsEntered = false;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Tactical Action Sequence", ToolTip = "Runs its child steps in order as one selectable action."))
struct ARCTACTICS_API FArcTacticsSequenceAction : public FArcTacticsActionNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsActionStatus Enter(FArcTacticsActionContext& Context, FStructView InstanceData) const override;
	virtual EArcTacticsActionStatus Tick(FArcTacticsActionContext& Context, FStructView InstanceData) const override;
	virtual void Cancel(
		FArcTacticsActionContext& Context,
		FStructView InstanceData,
		EArcTacticsActionCancelMode CancelMode) const override;
	virtual void OnAsyncQueryCompleted(
		FArcTacticsActionContext& Context,
		FStructView InstanceData,
		const FArcTacticsAsyncRequestToken& Token,
		FArcTQSQueryInstance& CompletedQuery) const override;
	virtual EArcTacticsActionTickLane GetTickLane(const FArcTacticsActionView& Action) const override;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const override;
	virtual bool BlocksDecision(const FArcTacticsActionView& Action) const override;
	virtual double GetNextWakeTime(const FArcTacticsActionView& Action) const override;
	virtual int32 GetCurrentStepIndex(const FArcTacticsActionView& Action) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
#endif
};
