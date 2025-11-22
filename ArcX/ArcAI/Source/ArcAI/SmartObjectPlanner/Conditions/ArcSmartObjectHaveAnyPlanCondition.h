#pragma once
#include "MassStateTreeTypes.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanResponse.h"

#include "ArcSmartObjectHaveAnyPlanCondition.generated.h"

USTRUCT()
struct FArcSmartObjectHaveAnyPlanConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	FArcSmartObjectPlanResponse PlanInput;
};

USTRUCT(DisplayName="Arc Smart Object Have Any Plan")
struct FArcSmartObjectHaveAnyPlanCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()
	
public:
	using FInstanceDataType = FArcSmartObjectHaveAnyPlanConditionInstanceData;

	FArcSmartObjectHaveAnyPlanCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
	
	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};