// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "UObject/Object.h"
#include "ArcMassSetBoolTask.generated.h"

/** Instance data for FArcMassSetBoolTask. Holds the target bool reference and the value to assign. */
USTRUCT()
struct FArcMassSetBoolTaskInstanceData
{
	GENERATED_BODY()

	/** Property reference to the bool that will be modified. Must be bound to a bool property in the StateTree. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (RefType = "bool"))
	FStateTreePropertyRef BoolToSet;

	/** The value to assign to the referenced bool property. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bNewValue = false;
};

/**
 * Instant task that sets a bool property reference to a specified value.
 * When bSetOnExit is true, the bool is set when exiting the state instead of (or in addition to) entering.
 */
USTRUCT(DisplayName="Arc Mass Set Bool Task", meta = (Category = "Arc|Common", ToolTip = "Instant task. Sets a bool property reference to a specified value. Optionally sets on state exit."))
struct FArcMassSetBoolTask: public FMassStateTreeTaskBase
{
	GENERATED_BODY()

public:

	using FInstanceDataType = FArcMassSetBoolTaskInstanceData;

	FArcMassSetBoolTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** If true, the bool value is set when the state is exited rather than only on enter. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bSetOnExit = false;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};