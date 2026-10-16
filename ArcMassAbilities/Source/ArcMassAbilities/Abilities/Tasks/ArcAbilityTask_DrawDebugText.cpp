// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_DrawDebugText.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityFragments.h"
#include "DrawDebugHelpers.h"
#include "ArcMassAbilitiesLogs.h"

EStateTreeRunStatus FArcAbilityTask_DrawDebugText::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FArcAbilityTask_DrawDebugTextInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_DrawDebugTextInstanceData>(*this);

	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	const FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
	if (!TransformFrag)
	{
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = AbilityContext.GetMassContext().GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	FVector Location = TransformFrag->GetTransform().GetLocation() + InstanceData.Offset;

	DrawDebugString(World, Location, InstanceData.Text, nullptr,
		InstanceData.Color, InstanceData.Duration, false, InstanceData.FontScale);

	ARCMA_VLOG_LOCATION(World, Location, 5.f, InstanceData.Color, TEXT("%s"), *InstanceData.Text);
	ARCMA_VLOG(World, Log, TEXT("[Debug] '%s' at %s entity=%s"),
		*InstanceData.Text, *Location.ToString(), *Entity.DebugGetDescription());

	return EStateTreeRunStatus::Succeeded;
}
