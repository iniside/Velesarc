// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeHandleIsValidCondition.h"

#include "StateTreeExecutionContext.h"

bool FArcKnowledgeHandleIsValidCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	bool bResult = InstanceData.KnowledgeHandle.IsValid();
	return bResult ^ bInvert;
}

#if WITH_EDITOR
FText FArcKnowledgeHandleIsValidCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return bInvert
		? NSLOCTEXT("ArcKnowledge", "HandleIsNotValidDesc", "Knowledge Handle Is NOT Valid")
		: NSLOCTEXT("ArcKnowledge", "HandleIsValidDesc", "Knowledge Handle Is Valid");
}
#endif
