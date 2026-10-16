// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Decision/ArcTacticsNode.h"
#include "Mass/EntityHandle.h"

#include "ArcTacticsFocusActiveStep.generated.h"

USTRUCT(BlueprintType, meta = (ToolTip = "Inputs and parameters of a Focus active step."))
struct ARCTACTICS_API FArcTacticsFocusActiveStepInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsOptional, ToolTip = "Entity to focus on unless Use Target Location is set."))
	FMassEntityHandle Target;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (ArcTacticsInput, ArcTacticsOptional, ToolTip = "World location in cm to focus on when Use Target Location is set."))
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ArcTacticsParameter, ToolTip = "Focuses on Target Location instead of the target entity."))
	bool bUseTargetLocation = false;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Focus", ToolTip = "Focuses the entity on the target entity or location while the action is active and clears the focus when the action ends."))
struct ARCTACTICS_API FArcTacticsFocusActiveStep : public FArcTacticsActiveStepNode
{
	GENERATED_BODY()

	virtual const UScriptStruct* GetInstanceDataType() const override;
	virtual FText GetDisplayName() const override;
	virtual bool Enter(FArcTacticsActionContext& Context, FStructView InstanceData) const override;
	virtual void Exit(FArcTacticsActionContext& Context, FStructView InstanceData) const override;
	virtual EArcTacticsExecutionRequirement GetRequirements(FConstStructView InstanceData) const override;
};
