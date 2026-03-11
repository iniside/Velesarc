// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "ArcAreaHasVacancyCondition.generated.h"

USTRUCT()
struct FArcAreaHasVacancyConditionInstanceData
{
	GENERATED_BODY()
};

/**
 * Condition: true if the area has any vacant slot.
 */
USTRUCT(meta = (DisplayName = "Arc Area Has Vacancy", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaHasVacancyCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaHasVacancyConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};
