// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcHybridPathFollowTask.h"
#include "MassAIBehaviorTypes.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassRepresentationFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "NavCorridor.h"
#include "NavigationSystem.h"
#include "Navigation/ArcWorldNavTypes.h"
#include "Navigation/ArcWorldRouteSubsystem.h"
#include "StateTreeLinker.h"
#include "ZoneGraphQuery.h"
#include "ZoneGraphSubsystem.h"

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
			MASSBEHAVIOR_LOG(Warning, TEXT("FArcHybridPathFollowTask: Failed to find route."));
			return EStateTreeRunStatus::Failed;
		}
	}

	// Initialize lane location to first lane in route
	const FZoneGraphLaneHandle CurrentLaneHandle = RouteFragment.GetCurrentLaneHandle();
	if (!CurrentLaneHandle.IsValid())
	{
		MASSBEHAVIOR_LOG(Warning, TEXT("FArcHybridPathFollowTask: Invalid lane handle at route index %d."), RouteFragment.CurrentLaneIndex);
		return EStateTreeRunStatus::Failed;
	}

	const FZoneGraphStorage* Storage = ZoneGraphSubsystem.GetZoneGraphStorage(CurrentLaneHandle.DataHandle);
	if (!Storage)
	{
		MASSBEHAVIOR_LOG(Warning, TEXT("FArcHybridPathFollowTask: No ZoneGraph storage for lane handle."));
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
	FArcHybridPathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcHybridPathFollowTaskInstanceData>(*this);
	const FArcWorldRouteFragment& RouteFragment = Context.GetExternalData(RouteFragmentHandle);

	// Check if route is complete
	if (RouteFragment.IsRouteComplete())
	{
		const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
		const float DistToDestination = FVector::Dist(Transform.GetTransform().GetLocation(), InstanceData.Destination);
		if (DistToDestination <= InstanceData.ArrivalDistance)
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}

	// Check for LOD transition
	const FMassRepresentationLODFragment& LODFragment = Context.GetExternalData(LODFragmentHandle);
	const EArcWorldNavMode DesiredMode = DetermineNavMode(LODFragment);

	if (DesiredMode != InstanceData.NavMode)
	{
		if (DesiredMode == EArcWorldNavMode::NavMeshCarrot)
		{
			// LOD1 -> LOD0: clear navmesh path, will be requested next tick
			InstanceData.bNavMeshPathActive = false;
		}
		else
		{
			// LOD0 -> LOD1: snap to nearest lane
			const UArcWorldRouteSubsystem& RouteSubsystem = Context.GetExternalData(RouteSubsystemHandle);
			const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
			FZoneGraphLaneLocation NearestLocation;
			const bool bFound = RouteSubsystem.FindNearestLane(Transform.GetTransform().GetLocation(), 5000.0f, NearestLocation);
			if (bFound)
			{
				FMassZoneGraphLaneLocationFragment& LaneLocation = Context.GetExternalData(LaneLocationHandle);
				LaneLocation.LaneHandle = NearestLocation.LaneHandle;
				LaneLocation.DistanceAlongLane = NearestLocation.DistanceAlongLane;

				const UZoneGraphSubsystem& ZoneGraphSubsystem = Context.GetExternalData(ZoneGraphSubsystemHandle);
				const FZoneGraphStorage* Storage = ZoneGraphSubsystem.GetZoneGraphStorage(NearestLocation.LaneHandle.DataHandle);
				if (Storage)
				{
					UE::ZoneGraph::Query::GetLaneLength(*Storage, NearestLocation.LaneHandle, LaneLocation.LaneLength);
					FMassZoneGraphCachedLaneFragment& CachedLane = Context.GetExternalData(CachedLaneHandle);
					CachedLane.CacheLaneData(*Storage, NearestLocation.LaneHandle, NearestLocation.DistanceAlongLane, LaneLocation.LaneLength, 0.0f);
				}
			}
		}
		InstanceData.NavMode = DesiredMode;
	}

	// Dispatch to mode-specific tick
	if (InstanceData.NavMode == EArcWorldNavMode::LaneFollow)
	{
		return TickLaneFollow(Context, DeltaTime);
	}
	return TickNavMeshCarrot(Context, DeltaTime);
}

void FArcHybridPathFollowTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
	MoveTarget.CreateNewAction(EMassMovementAction::Stand, *Context.GetWorld());
}

EStateTreeRunStatus FArcHybridPathFollowTask::TickLaneFollow(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcHybridPathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcHybridPathFollowTaskInstanceData>(*this);

	FMassZoneGraphLaneLocationFragment& LaneLocation = Context.GetExternalData(LaneLocationHandle);
	FMassZoneGraphCachedLaneFragment& CachedLane = Context.GetExternalData(CachedLaneHandle);
	FArcWorldRouteFragment& RouteFragment = Context.GetExternalData(RouteFragmentHandle);
	FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
	const FMassMovementParameters& MovementParams = Context.GetExternalData(MovementParamsHandle);

	// Calculate speed
	float DesiredSpeed = FMath::Min(
		MovementParams.GenerateDesiredSpeed(InstanceData.MovementStyle, MassContext.GetEntity().Index) * InstanceData.SpeedScale,
		MovementParams.MaxSpeed);
	const FMassDesiredMovementFragment& DesiredMovement = Context.GetExternalData(DesiredMovementHandle);
	DesiredSpeed = FMath::Min(DesiredSpeed, DesiredMovement.DesiredMaxSpeedOverride);

	// Advance distance along lane
	const float AdvanceDistance = DesiredSpeed * DeltaTime;
	LaneLocation.DistanceAlongLane += AdvanceDistance;

	// Check if we passed the end of the current lane
	if (LaneLocation.DistanceAlongLane >= LaneLocation.LaneLength)
	{
		// Try to advance to next lane in route
		if (RouteFragment.AdvanceToNextLane())
		{
			const FZoneGraphLaneHandle NextLaneHandle = RouteFragment.GetCurrentLaneHandle();
			const UZoneGraphSubsystem& ZoneGraphSubsystem = Context.GetExternalData(ZoneGraphSubsystemHandle);
			const FZoneGraphStorage* Storage = ZoneGraphSubsystem.GetZoneGraphStorage(NextLaneHandle.DataHandle);
			if (!Storage)
			{
				MASSBEHAVIOR_LOG(Warning, TEXT("FArcHybridPathFollowTask: No storage for next lane."));
				return EStateTreeRunStatus::Failed;
			}

			// Carry over remaining distance
			const float OvershootDistance = LaneLocation.DistanceAlongLane - LaneLocation.LaneLength;

			LaneLocation.LaneHandle = NextLaneHandle;
			LaneLocation.DistanceAlongLane = OvershootDistance;
			UE::ZoneGraph::Query::GetLaneLength(*Storage, NextLaneHandle, LaneLocation.LaneLength);

			// Re-cache the new lane
			CachedLane.CacheLaneData(*Storage, NextLaneHandle, 0.0f, LaneLocation.LaneLength, 0.0f);
		}
		else
		{
			// No more lanes — route complete
			LaneLocation.DistanceAlongLane = LaneLocation.LaneLength;
			return EStateTreeRunStatus::Succeeded;
		}
	}

	// Interpolate position from cached lane, write to transform
	FVector InterpolatedPosition;
	FVector InterpolatedTangent;
	CachedLane.GetPointAndTangentAtDistance(LaneLocation.DistanceAlongLane, InterpolatedPosition, InterpolatedTangent);

	FTransform& MutableTransform = Transform.GetMutableTransform();
	MutableTransform.SetLocation(InterpolatedPosition);
	if (!InterpolatedTangent.IsNearlyZero())
	{
		MutableTransform.SetRotation(InterpolatedTangent.ToOrientationQuat());
	}

	// Update move target
	MoveTarget.Center = InterpolatedPosition;
	MoveTarget.Forward = InterpolatedTangent.GetSafeNormal();
	MoveTarget.DesiredSpeed.Set(DesiredSpeed);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcHybridPathFollowTask::TickNavMeshCarrot(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcHybridPathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcHybridPathFollowTaskInstanceData>(*this);

	const FMassZoneGraphLaneLocationFragment& LaneLocation = Context.GetExternalData(LaneLocationHandle);
	const FMassZoneGraphCachedLaneFragment& CachedLane = Context.GetExternalData(CachedLaneHandle);
	FMassNavMeshShortPathFragment& ShortPath = Context.GetExternalData(ShortPathHandle);
	FMassNavMeshCachedPathFragment& CachedPath = Context.GetExternalData(CachedPathHandle);
	FArcWorldRouteFragment& RouteFragment = Context.GetExternalData(RouteFragmentHandle);

	// Compute carrot position ahead on lane
	const FVector NewCarrotPosition = ComputeCarrotPosition(CachedLane, LaneLocation, InstanceData.CarrotDistance);

	// Request navmesh path if none active or carrot moved significantly
	if (!InstanceData.bNavMeshPathActive)
	{
		if (RequestNavMeshPath(Context, NewCarrotPosition))
		{
			InstanceData.bNavMeshPathActive = true;
			InstanceData.CarrotPosition = NewCarrotPosition;
		}
		else
		{
			MASSBEHAVIOR_LOG(Warning, TEXT("FArcHybridPathFollowTask: Failed to request navmesh path to carrot."));
			return EStateTreeRunStatus::Failed;
		}
	}
	else
	{
		const float CarrotMoveDist = FVector::Dist(NewCarrotPosition, InstanceData.CarrotPosition);
		if (CarrotMoveDist > InstanceData.CarrotDistance * 0.5f)
		{
			// Carrot moved significantly, re-request path
			if (RequestNavMeshPath(Context, NewCarrotPosition))
			{
				InstanceData.CarrotPosition = NewCarrotPosition;
			}
		}
	}

	// Check navmesh short path progress
	if (ShortPath.IsDone())
	{
		if (ShortPath.bPartialResult)
		{
			// Update to next short path segment
			ShortPath.RequestShortPath(CachedPath.Corridor, CachedPath.NavPathNextStartIndex,
				CachedPath.NumLeadingPoints, InstanceData.ArrivalDistance);
			CachedPath.NavPathNextStartIndex += static_cast<uint16>(FMath::Max(
				ShortPath.NumPoints - FMassNavMeshShortPathFragment::NumPointsBeyondUpdate - FMassNavMeshCachedPathFragment::NumLeadingPoints, 0));
		}
		else
		{
			// Navmesh path segment done, sync lane location and check for lane end
			InstanceData.bNavMeshPathActive = false;

			// Sync lane location from current position
			const UArcWorldRouteSubsystem& RouteSubsystem = Context.GetExternalData(RouteSubsystemHandle);
			const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
			FZoneGraphLaneLocation NearestLocation;
			const bool bFound = RouteSubsystem.FindNearestLane(Transform.GetTransform().GetLocation(), 5000.0f, NearestLocation);
			if (bFound)
			{
				FMassZoneGraphLaneLocationFragment& MutableLaneLocation = Context.GetExternalData(LaneLocationHandle);
				MutableLaneLocation.LaneHandle = NearestLocation.LaneHandle;
				MutableLaneLocation.DistanceAlongLane = NearestLocation.DistanceAlongLane;

				const UZoneGraphSubsystem& ZoneGraphSubsystem = Context.GetExternalData(ZoneGraphSubsystemHandle);
				const FZoneGraphStorage* Storage = ZoneGraphSubsystem.GetZoneGraphStorage(NearestLocation.LaneHandle.DataHandle);
				if (Storage)
				{
					UE::ZoneGraph::Query::GetLaneLength(*Storage, NearestLocation.LaneHandle, MutableLaneLocation.LaneLength);
				}
			}

			// Check if near end of current lane and should advance route
			if (LaneLocation.DistanceAlongLane >= LaneLocation.LaneLength - InstanceData.ArrivalDistance)
			{
				if (!RouteFragment.AdvanceToNextLane())
				{
					// Final lane completed
					const float DistToDestination = FVector::Dist(Transform.GetTransform().GetLocation(), InstanceData.Destination);
					if (DistToDestination <= InstanceData.ArrivalDistance)
					{
						return EStateTreeRunStatus::Succeeded;
					}
				}
				else
				{
					// Cache next lane
					const FZoneGraphLaneHandle NextLaneHandle = RouteFragment.GetCurrentLaneHandle();
					const UZoneGraphSubsystem& ZoneGraphSubsystem = Context.GetExternalData(ZoneGraphSubsystemHandle);
					const FZoneGraphStorage* Storage = ZoneGraphSubsystem.GetZoneGraphStorage(NextLaneHandle.DataHandle);
					if (Storage)
					{
						FMassZoneGraphLaneLocationFragment& MutableLaneLocation = Context.GetExternalData(LaneLocationHandle);
						MutableLaneLocation.LaneHandle = NextLaneHandle;
						MutableLaneLocation.DistanceAlongLane = 0.0f;
						UE::ZoneGraph::Query::GetLaneLength(*Storage, NextLaneHandle, MutableLaneLocation.LaneLength);

						FMassZoneGraphCachedLaneFragment& MutableCachedLane = Context.GetExternalData(CachedLaneHandle);
						MutableCachedLane.CacheLaneData(*Storage, NextLaneHandle, 0.0f, MutableLaneLocation.LaneLength, 0.0f);
					}
				}
			}
		}
	}

	// Periodic lane sync
	InstanceData.LaneSyncTimer += DeltaTime;
	if (InstanceData.LaneSyncTimer >= InstanceData.LaneSyncInterval)
	{
		InstanceData.LaneSyncTimer = 0.0f;

		const UArcWorldRouteSubsystem& RouteSubsystem = Context.GetExternalData(RouteSubsystemHandle);
		const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
		FZoneGraphLaneLocation NearestLocation;
		const bool bFound = RouteSubsystem.FindNearestLane(Transform.GetTransform().GetLocation(), 5000.0f, NearestLocation);
		if (bFound)
		{
			FMassZoneGraphLaneLocationFragment& MutableLaneLocation = Context.GetExternalData(LaneLocationHandle);
			MutableLaneLocation.LaneHandle = NearestLocation.LaneHandle;
			MutableLaneLocation.DistanceAlongLane = NearestLocation.DistanceAlongLane;
		}
	}

	return EStateTreeRunStatus::Running;
}

FVector FArcHybridPathFollowTask::ComputeCarrotPosition(
	const FMassZoneGraphCachedLaneFragment& CachedLane,
	const FMassZoneGraphLaneLocationFragment& LaneLocation,
	float CarrotDist) const
{
	// Sample a point ahead on the cached lane
	const float TargetDistance = FMath::Min(LaneLocation.DistanceAlongLane + CarrotDist, CachedLane.LaneLength);

	FVector CarrotPoint;
	FVector CarrotTangent;
	CachedLane.GetPointAndTangentAtDistance(TargetDistance, CarrotPoint, CarrotTangent);

	return CarrotPoint;
}

bool FArcHybridPathFollowTask::RequestNavMeshPath(FStateTreeExecutionContext& Context, const FVector& TargetPosition) const
{
	FArcHybridPathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcHybridPathFollowTaskInstanceData>(*this);

	const FAgentRadiusFragment& AgentRadius = Context.GetExternalData(AgentRadiusHandle);
	const FAgentHeightFragment& AgentHeight = Context.GetExternalData(AgentHeightHandle);
	const FVector AgentNavLocation = Context.GetExternalData(TransformHandle).GetTransform().GetLocation();
	const FNavAgentProperties NavAgentProperties(AgentRadius.Radius, AgentHeight.Height);

	UNavigationSystemV1* NavSystem = Cast<UNavigationSystemV1>(Context.GetWorld()->GetNavigationSystem());
	if (!NavSystem)
	{
		MASSBEHAVIOR_LOG(Warning, TEXT("FArcHybridPathFollowTask: Missing navigation system."));
		return false;
	}

	const ANavigationData* NavData = NavSystem->GetNavDataForProps(NavAgentProperties, AgentNavLocation);
	if (!NavData)
	{
		MASSBEHAVIOR_LOG(Warning, TEXT("FArcHybridPathFollowTask: Invalid NavData."));
		return false;
	}

	FPathFindingQuery Query(NavSystem, *NavData, AgentNavLocation, TargetPosition);
	if (!Query.NavData.IsValid())
	{
		Query.NavData = NavSystem->GetNavDataForProps(NavAgentProperties, Query.StartLocation);
	}

	FPathFindingResult Result(ENavigationQueryResult::Error);
	if (Query.NavData.IsValid())
	{
		Result = Query.NavData->FindPath(NavAgentProperties, Query);
	}

	if (!Result.IsSuccessful())
	{
		MASSBEHAVIOR_LOG(Warning, TEXT("FArcHybridPathFollowTask: Failed to find navmesh path (start: %s, end: %s)."),
			*Query.StartLocation.ToCompactString(), *Query.EndLocation.ToCompactString());
		return false;
	}

	Result.Path->RemoveOverlappingPoints(FNavCorridor::OverlappingPointTolerance);
	if (Result.Path.Get()->GetPathPoints().Num() <= 1)
	{
		// Single point path — already at destination
		return true;
	}

	FMassNavMeshCachedPathFragment& CachedPath = Context.GetExternalData(CachedPathHandle);
	CachedPath.NavPath = Result.Path;
	CachedPath.PathSource = EMassNavigationPathSource::NavMesh;

	// Build corridor
	CachedPath.Corridor = MakeShared<FNavCorridor>();
	const FSharedConstNavQueryFilter NavQueryFilter = Query.QueryFilter ? Query.QueryFilter : NavData->GetDefaultQueryFilter();

	FNavCorridorParams CorridorParams;
	CorridorParams.SetFromWidth(InstanceData.CorridorWidth);
	CorridorParams.PathOffsetFromBoundaries = InstanceData.OffsetFromBoundaries;
	CachedPath.Corridor->BuildFromPath(*CachedPath.NavPath, NavQueryFilter, CorridorParams);

	// Request short path
	FMassNavMeshShortPathFragment& ShortPath = Context.GetExternalData(ShortPathHandle);
	ShortPath.RequestShortPath(CachedPath.Corridor, 0, 0, InstanceData.ArrivalDistance);

	CachedPath.NavPathNextStartIndex = static_cast<uint16>(FMath::Max(
		ShortPath.NumPoints - FMassNavMeshShortPathFragment::NumPointsBeyondUpdate - FMassNavMeshCachedPathFragment::NumLeadingPoints, 0));

	// Update move target speed
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
	const FMassMovementParameters& MovementParams = Context.GetExternalData(MovementParamsHandle);

	float DesiredSpeed = FMath::Min(
		MovementParams.GenerateDesiredSpeed(InstanceData.MovementStyle, MassContext.GetEntity().Index) * InstanceData.SpeedScale,
		MovementParams.MaxSpeed);
	const FMassDesiredMovementFragment& DesiredMovement = Context.GetExternalData(DesiredMovementHandle);
	DesiredSpeed = FMath::Min(DesiredSpeed, DesiredMovement.DesiredMaxSpeedOverride);

	MoveTarget.DesiredSpeed.Set(DesiredSpeed);
	MoveTarget.CreateNewAction(EMassMovementAction::Move, *Context.GetWorld());

	return true;
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
