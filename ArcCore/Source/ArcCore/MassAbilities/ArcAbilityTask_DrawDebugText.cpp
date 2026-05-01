// Copyright Lukasz Baran. All Rights Reserved.

#include "MassAbilities/ArcAbilityTask_DrawDebugText.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "MassEntityManager.h"
#include "Mass/EntityFragments.h"
#include "DrawDebugHelpers.h"

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

	return EStateTreeRunStatus::Succeeded;
}
