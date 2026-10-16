// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_GetItemFragment.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Sources/ArcSourceItemResolver.h"
#include "Items/ArcItemData.h"
#include "StructUtils/InstancedStruct.h"

EStateTreeRunStatus FArcAbilityTask_GetItemFragment::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	const UScriptStruct* RequestedType = InstanceData.ItemFragment.GetScriptStruct();
	if (RequestedType == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	const FMassEntityHandle Entity = AbilityContext.GetEntity();

	const FArcItemData* ItemData = ArcAbilityKit::FindSourceItem(EntityManager, Entity, AbilityContext.GetAbilityHandle());
	if (ItemData == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	const uint8* FragmentBytes = ItemData->FindFragment(const_cast<UScriptStruct*>(RequestedType));
	if (FragmentBytes == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ItemFragment.InitializeAs(RequestedType, FragmentBytes);
	return EStateTreeRunStatus::Succeeded;
}
