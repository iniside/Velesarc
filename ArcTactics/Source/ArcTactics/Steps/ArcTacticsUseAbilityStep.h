// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Abilities/ArcAbilityHandle.h"
#include "Actions/ArcTacticsAction.h"
#include "Actions/ArcTacticsMovement.h"
#include "CoreMinimal.h"
#include "Decision/ArcTacticsNode.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"

#include "ArcTacticsUseAbilityStep.generated.h"

class UArcAbilityDefinition;

USTRUCT(BlueprintType, meta = (ToolTip = "Inputs, parameters and runtime state of a Use Ability step."))
struct ARCTACTICS_API FArcTacticsUseAbilityStepInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsOptional, ToolTip = "Target entity whose current position is the target location unless Use Target Location is set."))
	FMassEntityHandle Target;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsOptional, ToolTip = "World location in cm used as the target location when Use Target Location is set."))
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ToolTip = "Ability definition the step activates; the entity must hold exactly one grant of it."))
	TObjectPtr<UArcAbilityDefinition> AbilityDefinition = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ToolTip = "Uses Target Location as the target location instead of the target entity position."))
	bool bUseTargetLocation = false;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ToolTip = "Movement policy and timeouts for the ability."))
	FArcTacticsAbilitySettings Ability;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ClampMin = "0.0", Units = "cm", ToolTip = "Maximum distance in cm to the target location when the ability activates; 0 disables the check."))
	float MaxRange = 0.0f;

	UPROPERTY()
	uint8 Stage = 0;

	UPROPERTY()
	FArcTacticsMovementRuntimeData MovementRuntime;

	UPROPERTY()
	FArcAbilityHandle AbilityHandle;

	UPROPERTY()
	FGameplayTag AbilityInputTag;

	UPROPERTY()
	double AbilityDeadline = 0.0;

	UPROPERTY()
	bool bAbilityInputPending = false;

	UPROPERTY()
	bool bAbilityDeadlineSet = false;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Use Ability", ToolTip = "Activates the ability; fails if the target location is farther than Max Range."))
struct ARCTACTICS_API FArcTacticsUseAbilityStep : public FArcTacticsStepNode
{
	GENERATED_BODY()

	static constexpr uint8 StageAcquireHold = 0;
	static constexpr uint8 StageWaitingForStart = 1;
	static constexpr uint8 StageUsing = 2;

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual bool Enter(FArcTacticsStepContext& Context, FStructView InstanceData) const override;
	virtual EArcTacticsActionStatus Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const override;
	virtual void Cancel(
		FArcTacticsStepContext& Context,
		FStructView InstanceData,
		EArcTacticsActionCancelMode CancelMode) const override;
	virtual EArcTacticsActionTickLane GetTickLane(FConstStructView InstanceData) const override;
	virtual bool BlocksDecision(FConstStructView InstanceData) const override;
	virtual double GetNextWakeTime(FConstStructView InstanceData) const override;
};
