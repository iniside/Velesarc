// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityHandle.h"
#include "MassStateTreeTypes.h"
#include "ArcMassModifyTargetHealth.generated.h"

UENUM()
enum class EArcHealthOperation : uint8
{
	Add,
	Subtract,
	Override
};

USTRUCT()
struct FArcMassModifyTargetHealthTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	FMassEntityHandle TargetEntity;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	EArcHealthOperation Operation = EArcHealthOperation::Subtract;
		
	/** Delay before the task ends. Default (0 or any negative) will run indefinitely, so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float ModifyValue = 0.f;
};

USTRUCT(meta = (DisplayName = "Arc Modify Target Health", Category = "Arc|Mass"))
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