// Copyright Lukasz Baran. All Rights Reserved.

#include "Navigation/ArcPathFollow.h"
#include "ArcMass/Spatial/ArcMassInfluenceMapping.h"
#include "Navigation/ArcInfluenceQueryFilter.h"
#include "MassAIBehaviorTypes.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "NavCorridor.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavMesh/NavMeshPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"
#include "Navigation/ArcWorldRouteSubsystem.h"
#include "ZoneGraphQuery.h"
#include "ZoneGraphSubsystem.h"

DEFINE_LOG_CATEGORY(LogArcPathDiag);

namespace ArcPathFollowPrivate
{

#if WITH_MASSGAMEPLAY_DEBUG
constexpr bool bBehaviorLogging = true;
#else
constexpr bool bBehaviorLogging = false;
#endif

float ComputeDesiredSpeed(const ArcPathFollow::FContext& Ctx, const ArcPathFollow::FParams& Params)
{
	const float StyleSpeed = Ctx.MovementParams.GenerateDesiredSpeed(Params.MovementStyle, Ctx.Entity.Index) * Params.SpeedScale;
	return FMath::Min(FMath::Min(StyleSpeed, Ctx.MovementParams.MaxSpeed), Ctx.DesiredMovement.DesiredMaxSpeedOverride);
}

void SyncLaneLocationFromPosition(const ArcPathFollow::FContext& Ctx, const ArcPathFollow::FLaneContext& LaneCtx, bool bRefreshLaneLength)
{
	FZoneGraphLaneLocation NearestLocation;
	if (!LaneCtx.RouteSubsystem.FindNearestLane(Ctx.Transform.GetTransform().GetLocation(), 5000.0f, NearestLocation))
	{
		return;
	}

	LaneCtx.LaneLocation.LaneHandle = NearestLocation.LaneHandle;
	LaneCtx.LaneLocation.DistanceAlongLane = NearestLocation.DistanceAlongLane;

	if (!bRefreshLaneLength)
	{
		return;
	}

	const FZoneGraphStorage* Storage = LaneCtx.ZoneGraphSubsystem.GetZoneGraphStorage(NearestLocation.LaneHandle.DataHandle);
	if (Storage)
	{
		UE::ZoneGraph::Query::GetLaneLength(*Storage, NearestLocation.LaneHandle, LaneCtx.LaneLocation.LaneLength);
	}
}

bool TrimPathEnd(TArray<FNavPathPoint>& PathPoints, const float StopShortDistance)
{
	double Remaining = static_cast<double>(StopShortDistance);
	for (int32 SegmentIndex = PathPoints.Num() - 2; SegmentIndex >= 0; --SegmentIndex)
	{
		const FVector SegmentStart = PathPoints[SegmentIndex].Location;
		const FVector SegmentEnd = PathPoints[SegmentIndex + 1].Location;
		const double SegmentLength = FVector::Dist(SegmentStart, SegmentEnd);
		if (SegmentLength > Remaining)
		{
			PathPoints[SegmentIndex + 1].Location = FNavMeshNodeFlags(PathPoints[SegmentIndex].Flags).IsNavLink()
				? SegmentStart
				: SegmentEnd + (SegmentStart - SegmentEnd) * (Remaining / SegmentLength);
			PathPoints.SetNum(SegmentIndex + 2);
			return true;
		}
		Remaining -= SegmentLength;
	}
	return false;
}

} // namespace ArcPathFollowPrivate

namespace ArcPathFollow
{

bool RequestPath(
	const FContext& Ctx,
	const FParams& Params,
	const FVector& Destination,
	bool bDisplayDebug,
	FRequestPathResult* OutResult)
{
	if (OutResult)
	{
		*OutResult = FRequestPathResult();
	}

	const FVector AgentNavLocation = Ctx.Transform.GetTransform().GetLocation();
	const FNavAgentProperties NavAgentProperties(Ctx.AgentRadius.Radius, Ctx.AgentHeight.Height);

	UE_LOG(LogArcPathDiag, Log,
		TEXT("RequestPath [%s]: start=%s dest=%s straightDist=%.0f agentRadius=%.0f agentHeight=%.0f"),
		*Ctx.Entity.DebugGetDescription(), *AgentNavLocation.ToCompactString(), *Destination.ToCompactString(),
		FVector::Dist(AgentNavLocation, Destination), Ctx.AgentRadius.Radius, Ctx.AgentHeight.Height);

	UNavigationSystemV1* NavSys = Cast<UNavigationSystemV1>(Ctx.World.GetNavigationSystem());
	if (!NavSys)
	{
		UE_LOG(LogArcPathDiag, Warning, TEXT("RequestPath [%s]: FAIL - no navigation system."), *Ctx.Entity.DebugGetDescription());
		UE_CLOG(ArcPathFollowPrivate::bBehaviorLogging, LogMassBehavior, Warning,
			TEXT("ArcPathFollow: entity [%s] missing navigation system."), *Ctx.Entity.DebugGetDescription());
		return false;
	}

	const ANavigationData* NavData = NavSys->GetNavDataForProps(NavAgentProperties, AgentNavLocation);
	if (!NavData)
	{
		UE_LOG(LogArcPathDiag, Warning,
			TEXT("RequestPath [%s]: FAIL - no NavData for agent radius=%.0f height=%.0f at %s. No navmesh matches this agent profile."),
			*Ctx.Entity.DebugGetDescription(), Ctx.AgentRadius.Radius, Ctx.AgentHeight.Height, *AgentNavLocation.ToCompactString());
		UE_CLOG(ArcPathFollowPrivate::bBehaviorLogging, LogMassBehavior, Warning,
			TEXT("ArcPathFollow: entity [%s] invalid NavData."), *Ctx.Entity.DebugGetDescription());
		return false;
	}

	FSharedConstNavQueryFilter QueryFilter;
#if WITH_RECAST
	// A dtQueryFilter-derived implementation only means anything on a Recast navmesh.
	if (Params.VisibilityGridIndex >= 0 && Ctx.QuerierTeamId != INDEX_NONE && Cast<const ARecastNavMesh>(NavData))
	{
		UArcInfluenceMappingSubsystem* InfluenceSubsystem = Ctx.World.GetSubsystem<UArcInfluenceMappingSubsystem>();
		const FArcInfluenceTiledGrid* TiledGrid =
			InfluenceSubsystem && Params.VisibilityGridIndex < InfluenceSubsystem->GetGridCount()
				? InfluenceSubsystem->GetTiledGrid(Params.VisibilityGridIndex)
				: nullptr;
		if (TiledGrid)
		{
			FArcInfluenceRecastQueryFilter VisibilityFilter;

			// SetFilterImplementation replaces the implementation outright, so seed the navmesh's
			// authored area costs and flags first. isVirtual is left alone: it must stay true for
			// getVirtualCost to dispatch.
			if (const INavigationQueryFilterInterface* DefaultImpl = NavData->GetDefaultQueryFilterImpl())
			{
				float AreaCosts[RECAST_MAX_AREAS];
				float FixedAreaCosts[RECAST_MAX_AREAS];
				DefaultImpl->GetAllAreaCosts(AreaCosts, FixedAreaCosts, RECAST_MAX_AREAS);
				for (int32 AreaIndex = 0; AreaIndex < RECAST_MAX_AREAS; AreaIndex++)
				{
					const uint8 AreaType = static_cast<uint8>(AreaIndex);
					VisibilityFilter.SetAreaCost(AreaType, AreaCosts[AreaIndex]);
					VisibilityFilter.SetFixedAreaEnteringCost(AreaType, FixedAreaCosts[AreaIndex]);
				}
				VisibilityFilter.SetIncludeFlags(DefaultImpl->GetIncludeFlags());
				VisibilityFilter.SetExcludeFlags(DefaultImpl->GetExcludeFlags());
				VisibilityFilter.setHeuristicScale(DefaultImpl->GetHeuristicScale());
			}

			VisibilityFilter.TiledGrid = TiledGrid;
			VisibilityFilter.QuerierTeamId = Ctx.QuerierTeamId;
			VisibilityFilter.CostMultiplier = Params.VisibilityCostMultiplier;

			// Clone the nav data's default filter for its search-node budget, then swap in our
			// implementation — SetFilterImplementation deep-copies through CreateCopy().
			FSharedNavQueryFilter InstanceFilter = NavData->GetDefaultQueryFilter()->GetCopy();
			InstanceFilter->SetFilterImplementation(&VisibilityFilter);
			QueryFilter = InstanceFilter;
		}
	}
#endif

	if (!QueryFilter.IsValid() && *Params.NavigationFilterClass)
	{
		QueryFilter = UNavigationQueryFilter::GetQueryFilter(*NavData, NavSys, Params.NavigationFilterClass);
	}

	FPathFindingQuery Query(NavSys, *NavData, AgentNavLocation, Destination, QueryFilter);
	if (!Query.NavData.IsValid())
	{
		Query.NavData = NavSys->GetNavDataForProps(NavAgentProperties, Query.StartLocation);
	}

	FPathFindingResult Result(ENavigationQueryResult::Error);
	if (Query.NavData.IsValid())
	{
		UE_CLOG(bDisplayDebug, LogMassBehavior, Verbose, TEXT("ArcPathFollow: requesting synchronous path (filter: %s)"),
			*Params.NavigationFilterClass ? *Params.NavigationFilterClass->GetName() : TEXT("Default"));

		Result = Query.NavData->FindPath(NavAgentProperties, Query);
	}

	UE_LOG(LogArcPathDiag, Log,
		TEXT("RequestPath [%s]: FindPath result=%s points=%d partial=%d"),
		*Ctx.Entity.DebugGetDescription(), *UEnum::GetValueAsString(Result.Result),
		Result.Path.IsValid() ? Result.Path->GetPathPoints().Num() : -1,
		Result.Path.IsValid() ? static_cast<int32>(Result.Path->IsPartial()) : -1);

	if (!Result.IsSuccessful())
	{
		UE_CLOG(ArcPathFollowPrivate::bBehaviorLogging, LogMassBehavior, Warning,
			TEXT("ArcPathFollow: entity [%s] failed to find a path, result: %s (start: %s, end: %s)"),
			*Ctx.Entity.DebugGetDescription(), *UEnum::GetValueAsString(Result.Result),
			*Query.StartLocation.ToCompactString(), *Query.EndLocation.ToCompactString());
		return false;
	}

	const bool bNavPathPartial = Result.Path->IsPartial();
	if (OutResult)
	{
		OutResult->bNavPathPartial = bNavPathPartial;
	}

	if (Params.StopShortDistance > 0.0f && !bNavPathPartial
		&& !ArcPathFollowPrivate::TrimPathEnd(Result.Path->GetPathPoints(), Params.StopShortDistance))
	{
		UE_LOG(LogArcPathDiag, Log,
			TEXT("RequestPath [%s]: path length within stop-short distance %.0f - already at end, NO Move action."),
			*Ctx.Entity.DebugGetDescription(), Params.StopShortDistance);
		if (OutResult)
		{
			OutResult->bAlreadyAtEnd = true;
		}
		return true;
	}

	Result.Path->RemoveOverlappingPoints(FNavCorridor::OverlappingPointTolerance);
	UE_LOG(LogArcPathDiag, Log, TEXT("RequestPath [%s]: points after RemoveOverlappingPoints=%d"),
		*Ctx.Entity.DebugGetDescription(), Result.Path.Get()->GetPathPoints().Num());
	if (Result.Path.Get()->GetPathPoints().Num() <= 1)
	{
		UE_LOG(LogArcPathDiag, Warning,
			TEXT("RequestPath [%s]: path collapsed to %d point(s) - returning SUCCESS with no corridor, no short path and NO Move action."),
			*Ctx.Entity.DebugGetDescription(), Result.Path.Get()->GetPathPoints().Num());
		if (OutResult)
		{
			OutResult->bAlreadyAtEnd = !bNavPathPartial;
		}
		return true;
	}

	Ctx.CachedPath.NavPath = Result.Path;
	Ctx.CachedPath.PathSource = EMassNavigationPathSource::NavMesh;

	Ctx.CachedPath.Corridor = MakeShared<FNavCorridor>();
	const FSharedConstNavQueryFilter CorridorFilter = Query.QueryFilter ? Query.QueryFilter : NavData->GetDefaultQueryFilter();

	FNavCorridorParams CorridorParams;
	CorridorParams.SetFromWidth(Params.CorridorWidth);
	CorridorParams.PathOffsetFromBoundaries = Params.OffsetFromBoundaries;

	Ctx.CachedPath.Corridor->BuildFromPath(*Ctx.CachedPath.NavPath, CorridorFilter, CorridorParams);

	UE_LOG(LogArcPathDiag, Log, TEXT("RequestPath [%s]: corridor built, portals=%d (width=%.0f offset=%.0f)"),
		*Ctx.Entity.DebugGetDescription(), Ctx.CachedPath.Corridor->Portals.Num(),
		Params.CorridorWidth, Params.OffsetFromBoundaries);

	const bool bShortPathOk = Ctx.ShortPath.RequestShortPath(Ctx.CachedPath.Corridor, 0, 0, Params.EndDistanceThreshold);

	UE_CLOG(!bShortPathOk || Ctx.ShortPath.NumPoints == 0, LogArcPathDiag, Warning,
		TEXT("RequestPath [%s]: RequestShortPath produced NO usable path (ok=%d points=%d partial=%d). ")
		TEXT("IsDone() is true immediately, so the executor's update branch never fires and the body never advances."),
		*Ctx.Entity.DebugGetDescription(), static_cast<int32>(bShortPathOk),
		static_cast<int32>(Ctx.ShortPath.NumPoints), static_cast<int32>(Ctx.ShortPath.bPartialResult));

	Ctx.CachedPath.NavPathNextStartIndex = static_cast<uint16>(FMath::Max(
		Ctx.ShortPath.NumPoints - FMassNavMeshShortPathFragment::NumPointsBeyondUpdate - FMassNavMeshCachedPathFragment::NumLeadingPoints, 0));

	const float StyleSpeed = Ctx.MovementParams.GenerateDesiredSpeed(Params.MovementStyle, Ctx.Entity.Index) * Params.SpeedScale;
	const float FinalSpeed = ArcPathFollowPrivate::ComputeDesiredSpeed(Ctx, Params);
	UE_LOG(LogArcPathDiag, Log,
		TEXT("RequestPath [%s]: shortPath points=%d endReached=%.0f endIntent=%d partial=%d done=%d | speed style=%.0f max=%.0f override=%.0f final=%.0f | nextStartIdx=%d"),
		*Ctx.Entity.DebugGetDescription(), static_cast<int32>(Ctx.ShortPath.NumPoints),
		Ctx.ShortPath.EndReachedDistance, static_cast<int32>(Ctx.ShortPath.EndOfPathIntent),
		static_cast<int32>(Ctx.ShortPath.bPartialResult), static_cast<int32>(Ctx.ShortPath.IsDone()),
		StyleSpeed, Ctx.MovementParams.MaxSpeed, Ctx.DesiredMovement.DesiredMaxSpeedOverride, FinalSpeed,
		static_cast<int32>(Ctx.CachedPath.NavPathNextStartIndex));

	UE_CLOG(FinalSpeed <= KINDA_SMALL_NUMBER, LogArcPathDiag, Warning,
		TEXT("RequestPath [%s]: desired speed is %.3f - the body has a path it can never walk."),
		*Ctx.Entity.DebugGetDescription(), FinalSpeed);

	Ctx.MoveTarget.DesiredSpeed.Set(FinalSpeed);
	Ctx.MoveTarget.CreateNewAction(EMassMovementAction::Move, Ctx.World);

	UE_LOG(LogArcPathDiag, Log, TEXT("RequestPath [%s]: Move action issued - %s"),
		*Ctx.Entity.DebugGetDescription(), *Ctx.MoveTarget.ToString());

	return true;
}

bool UpdateShortPath(const FContext& Ctx, const FParams& Params)
{
	UE_LOG(LogArcPathDiag, Log, TEXT("UpdateShortPath [%s]: from startIdx=%d leading=%d"),
		*Ctx.Entity.DebugGetDescription(), static_cast<int32>(Ctx.CachedPath.NavPathNextStartIndex),
		static_cast<int32>(Ctx.CachedPath.NumLeadingPoints));

	Ctx.ShortPath.RequestShortPath(
		Ctx.CachedPath.Corridor, Ctx.CachedPath.NavPathNextStartIndex, Ctx.CachedPath.NumLeadingPoints, Params.EndDistanceThreshold);

	Ctx.CachedPath.NavPathNextStartIndex += static_cast<uint16>(FMath::Max(
		Ctx.ShortPath.NumPoints - FMassNavMeshShortPathFragment::NumPointsBeyondUpdate - FMassNavMeshCachedPathFragment::NumLeadingPoints, 0));

	UE_LOG(LogArcPathDiag, Log, TEXT("UpdateShortPath [%s]: points=%d done=%d partial=%d nextStartIdx=%d"),
		*Ctx.Entity.DebugGetDescription(), static_cast<int32>(Ctx.ShortPath.NumPoints),
		static_cast<int32>(Ctx.ShortPath.IsDone()), static_cast<int32>(Ctx.ShortPath.bPartialResult),
		static_cast<int32>(Ctx.CachedPath.NavPathNextStartIndex));

	return true;
}

bool IsArrived(const FVector& Location, const FVector& Destination, float ArrivalDistance)
{
	const double Threshold = static_cast<double>(ArrivalDistance);
	return FVector::DistSquared(Location, Destination) <= Threshold * Threshold;
}

FVector ComputeCarrotPosition(
	const FMassZoneGraphCachedLaneFragment& CachedLane,
	const FMassZoneGraphLaneLocationFragment& LaneLocation,
	float CarrotDistance)
{
	const float TargetDistance = FMath::Min(LaneLocation.DistanceAlongLane + CarrotDistance, CachedLane.LaneLength);

	FVector CarrotPoint;
	FVector CarrotTangent;
	CachedLane.GetPointAndTangentAtDistance(TargetDistance, CarrotPoint, CarrotTangent);

	return CarrotPoint;
}

EResult TickLaneFollow(const FContext& Ctx, const FLaneContext& LaneCtx, const FParams& Params, float DeltaTime)
{
	const float DesiredSpeed = ArcPathFollowPrivate::ComputeDesiredSpeed(Ctx, Params);

	LaneCtx.LaneLocation.DistanceAlongLane += DesiredSpeed * DeltaTime;

	if (LaneCtx.LaneLocation.DistanceAlongLane >= LaneCtx.LaneLocation.LaneLength)
	{
		if (!LaneCtx.Route.AdvanceToNextLane())
		{
			LaneCtx.LaneLocation.DistanceAlongLane = LaneCtx.LaneLocation.LaneLength;
			return EResult::Succeeded;
		}

		const FZoneGraphLaneHandle NextLaneHandle = LaneCtx.Route.GetCurrentLaneHandle();
		const FZoneGraphStorage* Storage = LaneCtx.ZoneGraphSubsystem.GetZoneGraphStorage(NextLaneHandle.DataHandle);
		if (!Storage)
		{
			UE_CLOG(ArcPathFollowPrivate::bBehaviorLogging, LogMassBehavior, Warning,
				TEXT("ArcPathFollow: entity [%s] has no storage for next lane."), *Ctx.Entity.DebugGetDescription());
			return EResult::Failed;
		}

		const float OvershootDistance = LaneCtx.LaneLocation.DistanceAlongLane - LaneCtx.LaneLocation.LaneLength;

		LaneCtx.LaneLocation.LaneHandle = NextLaneHandle;
		LaneCtx.LaneLocation.DistanceAlongLane = OvershootDistance;
		UE::ZoneGraph::Query::GetLaneLength(*Storage, NextLaneHandle, LaneCtx.LaneLocation.LaneLength);

		LaneCtx.CachedLane.CacheLaneData(*Storage, NextLaneHandle, 0.0f, LaneCtx.LaneLocation.LaneLength, 0.0f);
	}

	FVector InterpolatedPosition;
	FVector InterpolatedTangent;
	LaneCtx.CachedLane.GetPointAndTangentAtDistance(LaneCtx.LaneLocation.DistanceAlongLane, InterpolatedPosition, InterpolatedTangent);

	FTransform& MutableTransform = Ctx.Transform.GetMutableTransform();
	MutableTransform.SetLocation(InterpolatedPosition);
	if (!InterpolatedTangent.IsNearlyZero())
	{
		MutableTransform.SetRotation(InterpolatedTangent.ToOrientationQuat());
	}

	Ctx.MoveTarget.Center = InterpolatedPosition;
	Ctx.MoveTarget.Forward = InterpolatedTangent.GetSafeNormal();
	Ctx.MoveTarget.DesiredSpeed.Set(DesiredSpeed);

	return EResult::Running;
}

EResult TickNavMeshCarrot(
	const FContext& Ctx,
	const FLaneContext& LaneCtx,
	const FLaneState& State,
	const FParams& Params,
	const FVector& Destination,
	float DeltaTime)
{
	const FVector NewCarrotPosition = ComputeCarrotPosition(LaneCtx.CachedLane, LaneCtx.LaneLocation, Params.CarrotDistance);

	if (!State.bNavMeshPathActive)
	{
		if (!RequestPath(Ctx, Params, NewCarrotPosition, false))
		{
			UE_CLOG(ArcPathFollowPrivate::bBehaviorLogging, LogMassBehavior, Warning,
				TEXT("ArcPathFollow: entity [%s] failed to request navmesh path to carrot."), *Ctx.Entity.DebugGetDescription());
			return EResult::Failed;
		}
		State.bNavMeshPathActive = true;
		State.CarrotPosition = NewCarrotPosition;
	}
	else if (FVector::Dist(NewCarrotPosition, State.CarrotPosition) > Params.CarrotDistance * 0.5f)
	{
		if (RequestPath(Ctx, Params, NewCarrotPosition, false))
		{
			State.CarrotPosition = NewCarrotPosition;
		}
	}

	if (Ctx.ShortPath.IsDone())
	{
		if (Ctx.ShortPath.bPartialResult)
		{
			UpdateShortPath(Ctx, Params);
		}
		else
		{
			State.bNavMeshPathActive = false;

			ArcPathFollowPrivate::SyncLaneLocationFromPosition(Ctx, LaneCtx, true);

			if (LaneCtx.LaneLocation.DistanceAlongLane >= LaneCtx.LaneLocation.LaneLength - Params.ArrivalDistance)
			{
				if (!LaneCtx.Route.AdvanceToNextLane())
				{
					if (IsArrived(Ctx.Transform.GetTransform().GetLocation(), Destination, Params.ArrivalDistance))
					{
						return EResult::Succeeded;
					}
				}
				else
				{
					const FZoneGraphLaneHandle NextLaneHandle = LaneCtx.Route.GetCurrentLaneHandle();
					const FZoneGraphStorage* Storage = LaneCtx.ZoneGraphSubsystem.GetZoneGraphStorage(NextLaneHandle.DataHandle);
					if (Storage)
					{
						LaneCtx.LaneLocation.LaneHandle = NextLaneHandle;
						LaneCtx.LaneLocation.DistanceAlongLane = 0.0f;
						UE::ZoneGraph::Query::GetLaneLength(*Storage, NextLaneHandle, LaneCtx.LaneLocation.LaneLength);

						LaneCtx.CachedLane.CacheLaneData(*Storage, NextLaneHandle, 0.0f, LaneCtx.LaneLocation.LaneLength, 0.0f);
					}
				}
			}
		}
	}

	State.LaneSyncTimer += DeltaTime;
	if (State.LaneSyncTimer >= Params.LaneSyncInterval)
	{
		State.LaneSyncTimer = 0.0f;
		ArcPathFollowPrivate::SyncLaneLocationFromPosition(Ctx, LaneCtx, false);
	}

	return EResult::Running;
}

} // namespace ArcPathFollow
