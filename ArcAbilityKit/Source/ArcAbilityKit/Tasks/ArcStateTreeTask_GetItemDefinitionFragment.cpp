// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcStateTreeTask_GetItemDefinitionFragment.h"

#include "Items/ArcItemDefinitionData.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeTypes.h"

#include "Cues/ArcCueStateTreeContext.h"

EStateTreeRunStatus FArcStateTreeTask_GetItemDefinitionFragment::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	const UScriptStruct* FragmentType = InstanceData.ItemFragment.GetScriptStruct();
	if (FragmentType == nullptr)
	{
		UE_LOGF(LogStateTree, Warning, "Get Item Definition Fragment: no fragment type is selected.");
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ItemFragment.InitializeAs(FragmentType);

	if (!FragmentType->IsChildOf(FArcItemFragment::StaticStruct()))
	{
		UE_LOGF(LogStateTree, Warning, "Get Item Definition Fragment: selected type '%ls' is not an item fragment.",
			*FragmentType->GetName());
		return EStateTreeRunStatus::Failed;
	}

	const FStateTreeDataView ContextData = Context.GetContextDataByName(ArcAbilityKit::CueStateTree::ContextName);
	const FArcCueStateTreeContext* CueContext = ContextData.GetPtr<FArcCueStateTreeContext>();
	if (CueContext == nullptr)
	{
		UE_LOGF(LogStateTree, Warning, "Get Item Definition Fragment: cue context is unavailable.");
		return EStateTreeRunStatus::Failed;
	}

	const FArcItemDefinitionData* ItemDefinition = FArcItemDefinitionData::Find(CueContext->SourceContext.SourceItemDefinition);
	if (ItemDefinition == nullptr)
	{
		UE_LOGF(LogStateTree, Warning, "Get Item Definition Fragment: source item definition is null.");
		return EStateTreeRunStatus::Failed;
	}

	const uint8* FragmentBytes = ItemDefinition->GetFragment(FragmentType);
	if (FragmentBytes == nullptr)
	{
		UE_LOGF(LogStateTree, Warning, "Get Item Definition Fragment: item definition '%ls' has no '%ls' fragment.",
			*CueContext->SourceContext.SourceItemDefinition.ToString(), *FragmentType->GetName());
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ItemFragment.InitializeAs(FragmentType, FragmentBytes);
	return EStateTreeRunStatus::Running;
}
