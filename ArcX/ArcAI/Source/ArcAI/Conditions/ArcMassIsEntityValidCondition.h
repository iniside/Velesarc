#pragma once
#include "MassEQSBlueprintLibrary.h"
#include "MassStateTreeTypes.h"

#include "ArcMassIsEntityValidCondition.generated.h"

USTRUCT()
struct FArcMassIsEntityValidConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEnvQueryEntityInfoBlueprintWrapper TargetInput;
};

USTRUCT(DisplayName="Arc Mass Is Entity Valid")
struct FArcMassIsEntityValidCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()
	
public:
	using FInstanceDataType = FArcMassIsEntityValidConditionInstanceData;

	FArcMassIsEntityValidCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
	
	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};
