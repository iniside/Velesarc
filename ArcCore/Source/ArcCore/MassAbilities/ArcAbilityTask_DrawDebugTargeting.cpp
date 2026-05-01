// Copyright Lukasz Baran. All Rights Reserved.

#include "MassAbilities/ArcAbilityTask_DrawDebugTargeting.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "DrawDebugHelpers.h"

EStateTreeRunStatus FArcAbilityTask_DrawDebugTargeting::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FArcAbilityTask_DrawDebugTargetingInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_DrawDebugTargetingInstanceData>(*this);

	UWorld* World = AbilityContext.GetMassContext().GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.TargetDataHandle.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.TargetDataHandle.HasHitResults())
	{
		const TArray<FHitResult>& HitResults = InstanceData.TargetDataHandle.GetHitResults();
		for (const FHitResult& Hit : HitResults)
		{
			DrawDebugSphere(World, Hit.ImpactPoint, InstanceData.Radius,
				12, InstanceData.Color, false, InstanceData.Duration);
			DrawDebugLine(World, Hit.ImpactPoint,
				Hit.ImpactPoint + Hit.ImpactNormal * InstanceData.Radius * 2.f,
				InstanceData.Color, false, InstanceData.Duration);
		}
	}
	else if (InstanceData.TargetDataHandle.HasEndPoint())
	{
		DrawDebugSphere(World, InstanceData.TargetDataHandle.GetEndPoint(), InstanceData.Radius,
			12, InstanceData.Color, false, InstanceData.Duration);
	}

	return EStateTreeRunStatus::Succeeded;
}
