// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "UObject/Object.h"
#include "ArcMassSetBoolTask.generated.h"

USTRUCT()
struct FArcMassSetBoolTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (RefType = "bool"))
	FStateTreePropertyRef BoolToSet;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bNewValue = false;
};

USTRUCT(DisplayName="Arc Mass Set Bool Task", meta = (Category = "Arc|Common"))
struct FArcMassSetBoolTask: public FMassStateTreeTaskBase
{
	GENERATED_BODY()

public:
	
	using FInstanceDataType = FArcMassSetBoolTaskInstanceData;

	FArcMassSetBoolTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bSetOnExit = false;
};