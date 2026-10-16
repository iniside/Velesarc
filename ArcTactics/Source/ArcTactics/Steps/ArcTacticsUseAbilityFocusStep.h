// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Actions/ArcTacticsMovement.h"
#include "CoreMinimal.h"
#include "Steps/ArcTacticsUseAbilityStep.h"

#include "ArcTacticsUseAbilityFocusStep.generated.h"

USTRUCT(BlueprintType, meta = (ToolTip = "Inputs, parameters and runtime state of a Use Ability Focus step."))
struct ARCTACTICS_API FArcTacticsUseAbilityFocusStepInstanceData : public FArcTacticsUseAbilityStepInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ToolTip = "Tolerance and timeout for turning to face the target location."))
	FArcTacticsFacingSettings Facing;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Use Ability Focus", ToolTip = "Turns the entity to face the target location, then activates the ability."))
struct ARCTACTICS_API FArcTacticsUseAbilityFocusStep : public FArcTacticsUseAbilityStep
{
	GENERATED_BODY()

	static constexpr uint8 StageFacing = 0;
	static constexpr uint8 StageAcquireHold = 1;
	static constexpr uint8 StageWaitingForStart = 2;
	static constexpr uint8 StageUsing = 3;

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual EArcTacticsActionStatus Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const override;
	virtual EArcTacticsActionTickLane GetTickLane(FConstStructView InstanceData) const override;
	virtual bool BlocksDecision(FConstStructView InstanceData) const override;
};
