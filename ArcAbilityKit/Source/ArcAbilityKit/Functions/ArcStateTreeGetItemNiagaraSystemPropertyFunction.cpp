// Copyright Lukasz Baran. All Rights Reserved.

#include "Functions/ArcStateTreeGetItemNiagaraSystemPropertyFunction.h"

#include "ItemFragments/ArcItemFragment_NiagaraSystem.h"
#include "Items/ArcItemDefinitionData.h"
#include "NiagaraSystem.h"
#include "StateTreeExecutionContext.h"

#include "Cues/ArcCueStateTreeContext.h"

void FArcStateTreeGetItemNiagaraSystemPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);

	Data.Output = nullptr;

	const FStateTreeDataView ContextData = Context.GetContextDataByName(ArcAbilityKit::CueStateTree::ContextName);
	const FArcCueStateTreeContext* CueContext = ContextData.GetPtr<FArcCueStateTreeContext>();
	const FArcItemDefinitionData* ItemDefinition = CueContext != nullptr
		? FArcItemDefinitionData::Find(CueContext->SourceContext.SourceItemDefinition)
		: nullptr;
	if (ItemDefinition == nullptr)
	{
		return;
	}

	const FArcItemFragment_NiagaraSystem* Fragment = ItemDefinition->FindFragment<FArcItemFragment_NiagaraSystem>();
	if (Fragment == nullptr)
	{
		return;
	}

	Data.Output = Fragment->System.LoadSynchronous();
}
