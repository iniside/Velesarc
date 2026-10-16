// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcRPGAbilityTask_CascadeIncrement.h"

#include "ArcRPGMechanicsLogs.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Sources/ArcSourceItemResolver.h"
#include "Fragments/ArcRPGItemFragment_CascadingCost.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Mass/EntityManager.h"
#include "StructUtils/InstancedStruct.h"

EStateTreeRunStatus FArcRPGAbilityTask_CascadeIncrement::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	const FMassEntityHandle Entity = AbilityContext.GetEntity();
	const FArcAbilityHandle Handle = AbilityContext.GetAbilityHandle();

	const FArcItemData* ItemPtr = ArcAbilityKit::FindSourceItem(EntityManager, Entity, Handle);
	if (ItemPtr == nullptr)
	{
		UE_LOGF(LogArcRPGMechanics, Warning, "Cascade increment failed: no resolvable source item, entity %ls", *Entity.DebugGetDescription());
		return EStateTreeRunStatus::Failed;
	}

	if (FArcRPGItemInstance_CascadingCost* CascadingInstance =
		ArcItemsHelper::FindMutableInstance<FArcRPGItemInstance_CascadingCost>(ItemPtr))
	{
		CascadingInstance->AddStack();
		const_cast<FArcItemData*>(ItemPtr)->IncrementVersion();
	}
	else
	{
		UE_LOGF(LogArcRPGMechanics, Verbose, "Cascade increment skipped: item has no cascading cost instance, entity %ls", *Entity.DebugGetDescription());
	}

	return EStateTreeRunStatus::Succeeded;
}
