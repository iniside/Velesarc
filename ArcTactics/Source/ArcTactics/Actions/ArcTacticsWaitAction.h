// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Actions/ArcTacticsAction.h"
#include "CoreMinimal.h"
#include "Decision/ArcTacticsNode.h"

#include "ArcTacticsWaitAction.generated.h"

USTRUCT(BlueprintType, meta = (ToolTip = "Settings and runtime state of a Wait action."))
struct ARCTACTICS_API FArcTacticsWaitActionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ArcTacticsParameter, ClampMin = "0.0", Units = "s", ToolTip = "Seconds to wait before the action succeeds; zero or less succeeds immediately."))
	float Duration = 2.0f;

	UPROPERTY()
	double EndTime = 0.0;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Wait", ToolTip = "Does nothing for Duration seconds, then succeeds. Higher-scoring actions can interrupt it."))
struct ARCTACTICS_API FArcTacticsWaitAction : public FArcTacticsActionNode
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
	virtual EArcTacticsActionTickLane GetTickLane(const FArcTacticsActionView& Action) const override;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const override;
	virtual bool BlocksDecision(const FArcTacticsActionView& Action) const override;
	virtual double GetNextWakeTime(const FArcTacticsActionView& Action) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override;
	virtual FColor GetIconColor() const override;
#endif
};
