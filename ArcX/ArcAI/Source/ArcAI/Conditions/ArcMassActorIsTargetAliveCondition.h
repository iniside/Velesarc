// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "UObject/Object.h"
#include "ArcMassActorIsTargetAliveCondition.generated.h"

/**
 * 
 */
USTRUCT()
struct FArcMassActorIsTargetAliveConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> TargetInput;
};

USTRUCT(DisplayName="Arc Mass Actor Is Target Alive Condition")
struct FArcMassActorIsTargetAliveConditionCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassActorIsTargetAliveConditionInstanceData;

	FArcMassActorIsTargetAliveConditionCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};