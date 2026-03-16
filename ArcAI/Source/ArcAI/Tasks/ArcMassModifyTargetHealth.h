// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityHandle.h"
#include "MassStateTreeTypes.h"
#include "ArcMassModifyTargetHealth.generated.h"

/** Defines how a health modification is applied to the target entity. */
UENUM()
enum class EArcHealthOperation : uint8
{
	/** Increase the target's current health by ModifyValue. */
	Add,
	/** Decrease the target's current health by ModifyValue. */
	Subtract,
	/** Set the target's current health to ModifyValue, ignoring the previous value. */
	Override
};

/** Instance data for FArcMassModifyTargetHealthTask. Holds the target entity, operation type, and value. */
USTRUCT()
struct FArcMassModifyTargetHealthTaskInstanceData
{
	GENERATED_BODY()

	/** The Mass entity handle of the target whose health will be modified. Must be bound from a perception or selection result. */
	UPROPERTY(EditAnywhere, Category = Input)
	FMassEntityHandle TargetEntity;

	/** How the health modification is applied: Add, Subtract, or Override. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	EArcHealthOperation Operation = EArcHealthOperation::Subtract;

	/** The amount by which to modify the target's health. Interpretation depends on the Operation type. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float ModifyValue = 0.f;
};

/**
 * Instant task that modifies a target entity's health using the specified operation (Add, Subtract, or Override).
 * The target entity must have a health fragment. Completes immediately on EnterState.
 */
USTRUCT(meta = (DisplayName = "Arc Modify Target Health", Category = "Arc|Mass", ToolTip = "Instant task. Modifies a target entity's health by adding, subtracting, or overriding the value."))
struct FArcMassModifyTargetHealthTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassModifyTargetHealthTaskInstanceData;
	
public:
	FArcMassModifyTargetHealthTask();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};