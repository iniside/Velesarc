// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaSlotHandleIsValidCondition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAreaSlotHandleIsValidCondition)

bool FArcAreaSlotHandleIsValidCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const bool bIsValid = InstanceData.SlotHandle.IsValid();
	return bInvert ? !bIsValid : bIsValid;
}
