// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_DrawDebugTargeting.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "DrawDebugHelpers.h"
#include "ArcMassAbilitiesLogs.h"

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
		ARCMA_VLOG(World, Warning, TEXT("[Debug] DrawDebugTargeting: no world"));
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.TargetDataHandle.IsValid())
	{
		ARCMA_VLOG(World, Warning, TEXT("[Debug] DrawDebugTargeting: invalid TargetDataHandle"));
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.TargetDataHandle.HasHitResults())
	{
		const TArray<FHitResult>& HitResults = InstanceData.TargetDataHandle.GetHitResults();
		for (const FHitResult& Hit : HitResults)
		{
			DrawDebugSphere(World, Hit.ImpactPoint, InstanceData.Radius,
				12, InstanceData.Color, false, InstanceData.Duration);
			ARCMA_VLOG_LOCATION(World, Hit.ImpactPoint, InstanceData.Radius, InstanceData.Color, TEXT("[Debug] hit"));

			DrawDebugLine(World, Hit.ImpactPoint,
				Hit.ImpactPoint + Hit.ImpactNormal * InstanceData.Radius * 2.f,
				InstanceData.Color, false, InstanceData.Duration);
			ARCMA_VLOG_SEGMENT(World, Hit.ImpactPoint,
				Hit.ImpactPoint + Hit.ImpactNormal * InstanceData.Radius * 2.f,
				InstanceData.Color, TEXT("[Debug] normal"));
		}
	}
	else if (InstanceData.TargetDataHandle.HasEndPoint())
	{
		FVector EndPoint = InstanceData.TargetDataHandle.GetEndPoint();
		DrawDebugSphere(World, EndPoint, InstanceData.Radius,
			12, InstanceData.Color, false, InstanceData.Duration);
		ARCMA_VLOG_LOCATION(World, EndPoint, InstanceData.Radius, InstanceData.Color, TEXT("[Debug] end"));
	}

	return EStateTreeRunStatus::Succeeded;
}
