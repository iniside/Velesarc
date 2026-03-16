// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "UObject/Object.h"
#include "ArcMassTickStateTreeTask.generated.h"

/** Instance data for FArcMassTickStateTreeTask. Empty -- this task has no per-instance parameters. */
USTRUCT()
struct FArcMassTickStateTreeTaskInstanceData
{
	GENERATED_BODY()
};

/**
 * Task that enables StateTree tick processing for the entity while the state is active.
 * On EnterState, it adds the tick tag to the entity so the Mass StateTree processor will tick it each frame.
 * On ExitState, it removes the tick tag, stopping per-frame ticking.
 * Returns Running from EnterState to keep the state active.
 */
USTRUCT(DisplayName="Arc Mass Tick StateTree Task", meta = (Category="AI|Common", ToolTip = "Enables StateTree tick processing for the entity while this state is active. Adds tick tag on enter, removes on exit."))
struct FArcMassTickStateTreeTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcMassTickStateTreeTaskInstanceData;

	FArcMassTickStateTreeTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};