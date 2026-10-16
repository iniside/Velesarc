// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcHybridPathFollowTask.h"
#include "MassAIBehaviorTypes.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassRepresentationFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "Navigation/ArcPathFollow.h"
#include "Navigation/ArcWorldNavTypes.h"
#include "Navigation/ArcWorldRouteSubsystem.h"
#include "StateTreeLinker.h"
#include "ZoneGraphQuery.h"
#include "ZoneGraphSubsystem.h"

namespace ArcHybridPathFollowPrivate
{

EStateTreeRunStatus ToRunStatus(ArcPathFollow::EResult Result)
{
	switch (Result)
	{
	case ArcPathFollow::EResult::Succeeded: return EStateTreeRunStatus::Succeeded;
	case ArcPathFollow::EResult::Failed:    return EStateTreeRunStatus::Failed;
	default:                                return EStateTreeRunStatus::Running;
	}
}

} // namespace ArcHybridPathFollowPrivate

bool FArcHybridPathFollowTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransformHandle);
	Linker.LinkExternalData(MoveTargetHandle);
	Linker.LinkExternalData(AgentRadiusHandle);
	Linker.LinkExternalData(AgentHeightHandle);
	Linker.LinkExternalData(DesiredMovementHandle);
	Linker.LinkExternalData(MovementParamsHandle);
	Linker.LinkExternalData(CachedPathHandle);
	Linker.LinkExternalData(ShortPathHandle);
	Linker.LinkExternalData(LaneLocationHandle);
	Linker.LinkExternalData(CachedLaneHandle);
	Linker.LinkExternalData(LODFragmentHandle);
	Linker.LinkExternalData(RouteFragmentHandle);
	Linker.LinkExternalData(ZoneGraphSubsystemHandle);
	Linker.LinkExternalData(RouteSubsystemHandle);

	return true;
}

EStateTreeRunStatus FArcHybridPathFollowTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcHybridPathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcHybridPathFollowTaskInstanceData>(*this);

	FArcWorldRouteFragment& RouteFragment = Context.GetExternalData(RouteFragmentHandle);
	FMassZoneGraphLaneLocationFragment& LaneLocation = Context.GetExternalData(LaneLocationHandle);
	FMassZoneGraphCachedLaneFragment& CachedLane = Context.GetExternalData(CachedLaneHandle);
	const UZoneGraphSubsystem& ZoneGraphSubsystem = Context.GetExternalData(ZoneGraphSubsystemHandle);
	const UArcWorldRouteSubsystem& RouteSubsystem = Context.GetExternalData(RouteSubsystemHandle);

	// If route is not valid, request one
	if (!RouteFragment.bRouteValid)
	{
		const FVector StartLocation = Context.GetExternalData(TransformHandle).GetTransform().GetLocation();

		FZoneGraphTagFilter TagFilter;
		const bool bRouteFound = RouteSubsystem.FindRoute(StartLocation, InstanceData.Destination, TagFilter, RouteFragment);
		if (!bRouteFound || !RouteFragment.bRouteValid)
		{
			MASSBEHAVIOR_LOGF(Warning, "FArcHybridPathFollowTask: Failed to find route.");
			return EStateTreeRunStatus::Failed;
		}
	}

	// Initialize lane location to first lane in route
	const FZoneGraphLaneHandle CurrentLaneHandle = RouteFragment.GetCurrentLaneHandle();
	if (!CurrentLaneHandle.IsValid())
	{
		MASSBEHAVIOR_LOGF(Warning, "FArcHybridPathFollowTask: Invalid lane handle at route index %d.", RouteFragment.CurrentLaneIndex);
		return EStateTreeRunStatus::Failed;
	}

	const FZoneGraphStorage* Storage = ZoneGraphSubsystem.GetZoneGraphStorage(CurrentLaneHandle.DataHandle);
	if (!Storage)
	{
		MASSBEHAVIOR_LOGF(Warning, "FArcHybridPathFollowTask: No ZoneGraph storage for lane handle.");
		return EStateTreeRunStatus::Failed;
	}

	// Set lane location
	LaneLocation.LaneHandle = CurrentLaneHandle;
	LaneLocation.DistanceAlongLane = 0.0f;
	UE::ZoneGraph::Query::GetLaneLength(*Storage, CurrentLaneHandle, LaneLocation.LaneLength);

	// Cache lane data for interpolation
	CachedLane.CacheLaneData(*Storage, CurrentLaneHandle, 0.0f, LaneLocation.LaneLength, 0.0f);

	// Determine initial nav mode from LOD
	const FMassRepresentationLODFragment& LODFragment = Context.GetExternalData(LODFragmentHandle);
	InstanceData.NavMode = DetermineNavMode(LODFragment);
	InstanceData.bNavMeshPathActive = false;
	InstanceData.LaneSyncTimer = 0.0f;

	// Set up move target with desired speed
	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
	const FMassMovementParameters& MovementParams = Context.GetExternalData(MovementParamsHandle);

	float DesiredSpeed = FMath::Min(
		MovementParams.GenerateDesiredSpeed(InstanceData.MovementStyle, MassContext.GetEntity().Index) * InstanceData.SpeedScale,
		MovementParams.MaxSpeed);

	const FMassDesiredMovementFragment& DesiredMovement = Context.GetExternalData(DesiredMovementHandle);
	DesiredSpeed = FMath::Min(DesiredSpeed, DesiredMovement.DesiredMaxSpeedOverride);

	MoveTarget.DesiredSpeed.Set(DesiredSpeed);
	MoveTarget.CreateNewAction(EMassMovementAction::Move, *Context.GetWorld());

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcHybridPathFollowTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcHybridPathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcHybridPathFollowTaskInstanceData>(*this);
	FArcWorldRouteFragment& RouteFragment = Context.GetExternalData(RouteFragmentHandle);
	FTransformFragment& Transform = Context.GetExternalData(TransformHandle);

	if (RouteFragment.IsRouteComplete())
	{
		if (ArcPathFollow::IsArrived(Transform.GetTransform().GetLocation(), InstanceData.Destination, InstanceData.ArrivalDistance))
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}

	const UZoneGraphSubsystem& ZoneGraphSubsystem = Context.GetExternalData(ZoneGraphSubsystemHandle);
	const UArcWorldRouteSubsystem& RouteSubsystem = Context.GetExternalData(RouteSubsystemHandle);
	FMassZoneGraphLaneLocationFragment& LaneLocation = Context.GetExternalData(LaneLocationHandle);
	FMassZoneGraphCachedLaneFragment& CachedLane = Context.GetExternalData(CachedLaneHandle);

	const FMassRepresentationLODFragment& LODFragment = Context.GetExternalData(LODFragmentHandle);
	const EArcWorldNavMode DesiredMode = DetermineNavMode(LODFragment);

	if (DesiredMode != InstanceData.NavMode)
	{
		if (DesiredMode == EArcWorldNavMode::NavMeshCarrot)
		{
			InstanceData.bNavMeshPathActive = false;
		}
		else
		{
			FZoneGraphLaneLocation NearestLocation;
			if (RouteSubsystem.FindNearestLane(Transform.GetTransform().GetLocation(), 5000.0f, NearestLocation))
			{
				LaneLocation.LaneHandle = NearestLocation.LaneHandle;
				LaneLocation.DistanceAlongLane = NearestLocation.DistanceAlongLane;

				const FZoneGraphStorage* Storage = ZoneGraphSubsystem.GetZoneGraphStorage(NearestLocation.LaneHandle.DataHandle);
				if (Storage)
				{
					UE::ZoneGraph::Query::GetLaneLength(*Storage, NearestLocation.LaneHandle, LaneLocation.LaneLength);
					CachedLane.CacheLaneData(*Storage, NearestLocation.LaneHandle, NearestLocation.DistanceAlongLane, LaneLocation.LaneLength, 0.0f);
				}
			}
		}
		InstanceData.NavMode = DesiredMode;
	}

	const ArcPathFollow::FContext PathContext
	{
		*Context.GetWorld(),
		MassContext.GetEntity(),
		Transform,
		Context.GetExternalData(AgentRadiusHandle),
		Context.GetExternalData(AgentHeightHandle),
		Context.GetExternalData(DesiredMovementHandle),
		Context.GetExternalData(MovementParamsHandle),
		Context.GetExternalData(MoveTargetHandle),
		Context.GetExternalData(ShortPathHandle),
		Context.GetExternalData(CachedPathHandle)
	};

	const ArcPathFollow::FLaneContext LaneContext
	{
		ZoneGraphSubsystem,
		RouteSubsystem,
		LaneLocation,
		CachedLane,
		RouteFragment
	};

	ArcPathFollow::FParams Params;
	Params.MovementStyle = InstanceData.MovementStyle;
	Params.SpeedScale = InstanceData.SpeedScale;
	Params.CorridorWidth = InstanceData.CorridorWidth;
	Params.OffsetFromBoundaries = InstanceData.OffsetFromBoundaries;
	Params.EndDistanceThreshold = InstanceData.ArrivalDistance;
	Params.CarrotDistance = InstanceData.CarrotDistance;
	Params.LaneSyncInterval = InstanceData.LaneSyncInterval;
	Params.ArrivalDistance = InstanceData.ArrivalDistance;

	if (InstanceData.NavMode == EArcWorldNavMode::LaneFollow)
	{
		return ArcHybridPathFollowPrivate::ToRunStatus(
			ArcPathFollow::TickLaneFollow(PathContext, LaneContext, Params, DeltaTime));
	}

	const ArcPathFollow::FLaneState LaneState
	{
		InstanceData.LaneSyncTimer,
		InstanceData.bNavMeshPathActive,
		InstanceData.CarrotPosition
	};

	return ArcHybridPathFollowPrivate::ToRunStatus(
		ArcPathFollow::TickNavMeshCarrot(PathContext, LaneContext, LaneState, Params, InstanceData.Destination, DeltaTime));
}

void FArcHybridPathFollowTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
	MoveTarget.CreateNewAction(EMassMovementAction::Stand, *Context.GetWorld());
}

EArcWorldNavMode FArcHybridPathFollowTask::DetermineNavMode(const FMassRepresentationLODFragment& LODFragment) const
{
	// LOD0 (High) = visible entity with navmesh, everything else = lane follow
	if (LODFragment.LOD == EMassLOD::High)
	{
		return EArcWorldNavMode::NavMeshCarrot;
	}
	return EArcWorldNavMode::LaneFollow;
}
