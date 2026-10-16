// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassMovementTypes.h"
#include "MassNavMeshNavigationFragments.h"
#include "MassZoneGraphNavigationFragments.h"
#include "Mass/EntityHandle.h"
#include "Navigation/ArcWorldNavTypes.h"
#include "Templates/SubclassOf.h"

class UNavigationQueryFilter;
class UZoneGraphSubsystem;
class UArcWorldRouteSubsystem;
struct FTransformFragment;
struct FMassMoveTargetFragment;
struct FAgentRadiusFragment;
struct FAgentHeightFragment;
struct FMassMovementParameters;
struct FMassDesiredMovementFragment;

/** Bring-up diagnostics for navmesh path requests — every branch of RequestPath prints why it did what it did. */
ARCAI_API DECLARE_LOG_CATEGORY_EXTERN(LogArcPathDiag, Log, All);

namespace ArcPathFollow
{
    enum class EResult : uint8
    {
        Running,
        Succeeded,
        Failed
    };

    struct FParams
    {
        FMassMovementStyleRef MovementStyle;
        float SpeedScale = 1.0f;
        float CorridorWidth = 600.0f;
        float OffsetFromBoundaries = 10.0f;
        float EndDistanceThreshold = 20.0f;
        float CarrotDistance = 10000.0f;
        float LaneSyncInterval = 2.0f;
        float ArrivalDistance = 200.0f;
        float StopShortDistance = 0.0f;
        TSubclassOf<UNavigationQueryFilter> NavigationFilterClass;
        /** Tiled influence grid sampled for enemy visibility. INDEX_NONE leaves the path uncosted by visibility. */
        int32 VisibilityGridIndex = INDEX_NONE;
        /** Extra cost of a cell seen by another team, as a fraction of its base cost: 1 doubles it. */
        float VisibilityCostMultiplier = 1.0f;
    };

    struct FRequestPathResult
    {
        bool bAlreadyAtEnd = false;
        bool bNavPathPartial = false;
    };

    struct FContext
    {
        UWorld& World;
        FMassEntityHandle Entity;
        FTransformFragment& Transform;
        const FAgentRadiusFragment& AgentRadius;
        const FAgentHeightFragment& AgentHeight;
        const FMassDesiredMovementFragment& DesiredMovement;
        const FMassMovementParameters& MovementParams;
        FMassMoveTargetFragment& MoveTarget;
        FMassNavMeshShortPathFragment& ShortPath;
        FMassNavMeshCachedPathFragment& CachedPath;
        /** Team of the pathing entity. INDEX_NONE means no team, so no visibility penalty applies. */
        int32 QuerierTeamId = INDEX_NONE;
    };

    struct FLaneContext
    {
        const UZoneGraphSubsystem& ZoneGraphSubsystem;
        const UArcWorldRouteSubsystem& RouteSubsystem;
        FMassZoneGraphLaneLocationFragment& LaneLocation;
        FMassZoneGraphCachedLaneFragment& CachedLane;
        FArcWorldRouteFragment& Route;
    };

    struct FLaneState
    {
        float& LaneSyncTimer;
        bool& bNavMeshPathActive;
        FVector& CarrotPosition;
    };

    ARCAI_API bool RequestPath(
        const FContext& Ctx,
        const FParams& Params,
        const FVector& Destination,
        bool bDisplayDebug,
        FRequestPathResult* OutResult = nullptr);

    ARCAI_API bool UpdateShortPath(const FContext& Ctx, const FParams& Params);

    ARCAI_API bool IsArrived(const FVector& Location, const FVector& Destination, float ArrivalDistance);

    ARCAI_API FVector ComputeCarrotPosition(
        const FMassZoneGraphCachedLaneFragment& CachedLane,
        const FMassZoneGraphLaneLocationFragment& LaneLocation,
        float CarrotDistance);

    ARCAI_API EResult TickLaneFollow(const FContext& Ctx, const FLaneContext& LaneCtx, const FParams& Params, float DeltaTime);

    ARCAI_API EResult TickNavMeshCarrot(
        const FContext& Ctx,
        const FLaneContext& LaneCtx,
        const FLaneState& State,
        const FParams& Params,
        const FVector& Destination,
        float DeltaTime);
}
