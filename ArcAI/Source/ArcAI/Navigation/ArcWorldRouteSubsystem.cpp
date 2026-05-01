// Copyright Lukasz Baran. All Rights Reserved.

#include "Navigation/ArcWorldRouteSubsystem.h"
#include "ZoneGraphSubsystem.h"
#include "ZoneGraphAStar.h"
#include "ZoneGraphData.h"
#include "GraphAStar.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcWorldRouteSubsystem)

UArcWorldRouteSubsystem::UArcWorldRouteSubsystem()
{
}

void UArcWorldRouteSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UZoneGraphSubsystem>();
	ZoneGraphSubsystem = GetWorld()->GetSubsystem<UZoneGraphSubsystem>();
}

void UArcWorldRouteSubsystem::Deinitialize()
{
	ZoneGraphSubsystem = nullptr;
	Super::Deinitialize();
}

TStatId UArcWorldRouteSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcWorldRouteSubsystem, STATGROUP_Tickables);
}

void UArcWorldRouteSubsystem::Tick(float DeltaTime)
{
}

bool UArcWorldRouteSubsystem::FindNearestLane(
	const FVector& Location,
	const float QueryRadius,
	FZoneGraphLaneLocation& OutLaneLocation) const
{
	if (!ZoneGraphSubsystem)
	{
		return false;
	}

	const FBox QueryBounds = FBox::BuildAABB(Location, FVector(QueryRadius));
	float DistanceSqr = 0.0f;
	return ZoneGraphSubsystem->FindNearestLane(QueryBounds, FZoneGraphTagFilter(), OutLaneLocation, DistanceSqr);
}

bool UArcWorldRouteSubsystem::FindRoute(
	const FVector& StartLocation,
	const FVector& EndLocation,
	const FZoneGraphTagFilter& TagFilter,
	FArcWorldRouteFragment& OutRoute) const
{
	OutRoute.Reset();

	if (!ZoneGraphSubsystem)
	{
		return false;
	}

	constexpr float QueryRadius = 50000.0f;
	const FBox StartBounds = FBox::BuildAABB(StartLocation, FVector(QueryRadius));
	const FBox EndBounds = FBox::BuildAABB(EndLocation, FVector(QueryRadius));

	FZoneGraphLaneLocation StartLaneLocation;
	FZoneGraphLaneLocation EndLaneLocation;
	float StartDistSqr = 0.0f;
	float EndDistSqr = 0.0f;

	if (!ZoneGraphSubsystem->FindNearestLane(StartBounds, TagFilter, StartLaneLocation, StartDistSqr))
	{
		return false;
	}

	if (!ZoneGraphSubsystem->FindNearestLane(EndBounds, TagFilter, EndLaneLocation, EndDistSqr))
	{
		return false;
	}

	if (StartLaneLocation.LaneHandle == EndLaneLocation.LaneHandle)
	{
		OutRoute.RouteLanes.Add(StartLaneLocation.LaneHandle);
		OutRoute.Destination = EndLocation;
		OutRoute.bRouteValid = true;
		return true;
	}

	// Resolve ZoneGraphStorage for the start lane's data handle.
	const AZoneGraphData* ZoneGraphData = ZoneGraphSubsystem->GetZoneGraphData(StartLaneLocation.LaneHandle.DataHandle);
	if (!ZoneGraphData)
	{
		return false;
	}

	const FZoneGraphStorage& Storage = ZoneGraphData->GetStorage();

	// Construct A* start and end nodes with world positions so the heuristic is meaningful.
	const FZoneGraphAStarNode StartNode(StartLaneLocation.LaneHandle.Index, StartLaneLocation.Position);
	const FZoneGraphAStarNode EndNode(EndLaneLocation.LaneHandle.Index, EndLaneLocation.Position);

	FZoneGraphAStarWrapper GraphWrapper(Storage);
	FZoneGraphAStar AStar(GraphWrapper);

	FZoneGraphPathFilter PathFilter(Storage, StartLaneLocation, EndLaneLocation, TagFilter);

	TArray<FZoneGraphAStarWrapper::FNodeRef> ResultPath;
	const EGraphAStarResult AStarResult = AStar.FindPath(StartNode, EndNode, PathFilter, ResultPath);

	if (AStarResult != EGraphAStarResult::SearchSuccess)
	{
		return false;
	}

	OutRoute.RouteLanes.Reserve(ResultPath.Num());
	for (const int32 LaneIndex : ResultPath)
	{
		OutRoute.RouteLanes.Add(FZoneGraphLaneHandle(LaneIndex, StartLaneLocation.LaneHandle.DataHandle));
	}

	OutRoute.Destination = EndLocation;
	OutRoute.bRouteValid = true;
	return true;
}
