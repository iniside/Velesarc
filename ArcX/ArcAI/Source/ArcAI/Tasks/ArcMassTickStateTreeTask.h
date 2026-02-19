// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "UObject/Object.h"
#include "ArcMassTickStateTreeTask.generated.h"

USTRUCT()
struct FArcMassTickStateTreeTaskInstanceData
{
	GENERATED_BODY()
};

USTRUCT(DisplayName="Arc Mass Tick StateTree Task", meta = (Category="AI|Common"))
struct FArcMassTickStateTreeTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcMassTickStateTreeTaskInstanceData;

	FArcMassTickStateTreeTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};