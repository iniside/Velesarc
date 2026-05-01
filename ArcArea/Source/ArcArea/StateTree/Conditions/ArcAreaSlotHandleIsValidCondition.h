// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "ArcAreaTypes.h"
#include "ArcAreaSlotHandleIsValidCondition.generated.h"

USTRUCT()
struct FArcAreaSlotHandleIsValidConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	FArcAreaSlotHandle SlotHandle;
};

/**
 * StateTree condition: checks whether the bound FArcAreaSlotHandle is valid.
 */
USTRUCT(meta = (DisplayName = "Arc Is Area Slot Handle Valid", Category = "Arc|Area"))
struct ARCAREA_API FArcAreaSlotHandleIsValidCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaSlotHandleIsValidConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};
