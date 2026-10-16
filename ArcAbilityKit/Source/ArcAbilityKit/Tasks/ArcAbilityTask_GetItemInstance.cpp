// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_GetItemInstance.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Sources/ArcSourceItemResolver.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemInstance.h"
#include "Items/ArcItemsHelpers.h"
#include "StructUtils/InstancedStruct.h"

EStateTreeRunStatus FArcAbilityTask_GetItemInstance::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	const UScriptStruct* RequestedType = InstanceData.ItemInstance.GetScriptStruct();
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

	FArcItemInstance* Instance = ArcItemsHelper::FindMutableInstance(ItemData, const_cast<UScriptStruct*>(RequestedType));
	if (Instance == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}
	const uint8* InstanceBytes = reinterpret_cast<const uint8*>(Instance);
	InstanceData.ItemInstance.InitializeAs(RequestedType, InstanceBytes);
	return EStateTreeRunStatus::Succeeded;
}
