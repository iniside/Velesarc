// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassMovementTypes.h"
#include "MassNavigationTypes.h"
#include "MassNavMeshNavigationFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeTypes.h"
#include "MassZoneGraphNavigationFragments.h"
#include "Navigation/ArcWorldNavTypes.h"
#include "ArcHybridPathFollowTask.generated.h"

struct FTransformFragment;
struct FMassMoveTargetFragment;
struct FAgentRadiusFragment;
struct FAgentHeightFragment;
struct FMassMovementParameters;
struct FMassDesiredMovementFragment;
struct FMassRepresentationLODFragment;
struct FArcWorldRouteFragment;
class UZoneGraphSubsystem;
class UArcWorldRouteSubsystem;

/**
 * Instance data for FArcHybridPathFollowTask.
 * Contains navigation parameters and runtime state for LOD-based movement switching.
 */
USTRUCT()
struct FArcHybridPathFollowTaskInstanceData
{
	GENERATED_BODY()

	/** The world-space destination to navigate toward. */
	UPROPERTY(EditAnywhere, Category = Input)
	FVector Destination = FVector::ZeroVector;

	/** Movement style reference determining speed and animation parameters. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassMovementStyleRef MovementStyle;

	/** Multiplier applied to the entity's movement speed. 1.0 = normal speed. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float SpeedScale = 1.0f;

	/** How far ahead on the ZoneGraph lane to place the navmesh carrot point (cm). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float CarrotDistance = 10000.0f;

	/** Maximum width of the navmesh corridor (cm). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float CorridorWidth = 600.0f;

	/** Amount to offset corridor sides from navmesh borders (cm). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float OffsetFromBoundaries = 10.0f;

	/** Distance from final destination to consider arrival (cm). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float ArrivalDistance = 200.0f;

	/** Interval in seconds between lane location syncs when in NavMeshCarrot mode. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float LaneSyncInterval = 2.0f;

	// ---- Runtime state (not editable) ----

	/** Current navigation mode based on LOD. */
	UPROPERTY()
	EArcWorldNavMode NavMode = EArcWorldNavMode::LaneFollow;

	/** Timer tracking when to next sync lane location in NavMeshCarrot mode. */
	UPROPERTY()
	float LaneSyncTimer = 0.0f;

	/** True when a navmesh path has been requested and is being followed. */
	UPROPERTY()
	bool bNavMeshPathActive = false;

	/** Current carrot position sampled ahead on the ZoneGraph lane. */
	UPROPERTY()
	FVector CarrotPosition = FVector::ZeroVector;
};

/**
 * Hybrid path follow task for Mass entities along ZoneGraph routes.
 *
 * Two movement modes based on LOD:
 * - LOD1+ (LaneFollow): Entity is invisible. Advances directly along ZoneGraph lanes,
 *   writing interpolated position to transform.
 * - LOD0 (NavMeshCarrot): Entity is visible with navmesh. Samples a carrot point ahead
 *   on the lane and uses navmesh pathfinding to walk toward it.
 *
 * LOD transitions happen automatically based on FMassRepresentationLODFragment.
 */
USTRUCT(meta = (DisplayName = "Arc Hybrid Path Follow", ToolTip = "Hybrid LOD-based path following: ZoneGraph lanes (LOD1+) or NavMesh carrot (LOD0)."))
struct FArcHybridPathFollowTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FArcHybridPathFollowTaskInstanceData::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	/** Advance entity directly along cached lane data (invisible, LOD1+). */
	EStateTreeRunStatus TickLaneFollow(FStateTreeExecutionContext& Context, float DeltaTime) const;

	/** Follow a navmesh carrot point sampled ahead on the lane (visible, LOD0). */
	EStateTreeRunStatus TickNavMeshCarrot(FStateTreeExecutionContext& Context, float DeltaTime) const;

	/** Compute a carrot position ahead on the current lane by CarrotDistance. */
	FVector ComputeCarrotPosition(const FMassZoneGraphCachedLaneFragment& CachedLane,
								  const FMassZoneGraphLaneLocationFragment& LaneLocation,
								  float CarrotDist) const;

	/** Request a navmesh path from current position to the carrot point. */
	bool RequestNavMeshPath(FStateTreeExecutionContext& Context, const FVector& TargetPosition) const;

	/** Determine the desired nav mode from the current LOD fragment. */
	EArcWorldNavMode DetermineNavMode(const FMassRepresentationLODFragment& LODFragment) const;

	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
	TStateTreeExternalDataHandle<FMassMoveTargetFragment> MoveTargetHandle;
	TStateTreeExternalDataHandle<FAgentRadiusFragment> AgentRadiusHandle;
	TStateTreeExternalDataHandle<FAgentHeightFragment> AgentHeightHandle;
	TStateTreeExternalDataHandle<FMassDesiredMovementFragment> DesiredMovementHandle;
	TStateTreeExternalDataHandle<FMassMovementParameters> MovementParamsHandle;
	TStateTreeExternalDataHandle<FMassNavMeshShortPathFragment> ShortPathHandle;
	TStateTreeExternalDataHandle<FMassNavMeshCachedPathFragment> CachedPathHandle;
	TStateTreeExternalDataHandle<FMassZoneGraphLaneLocationFragment> LaneLocationHandle;
	TStateTreeExternalDataHandle<FMassZoneGraphCachedLaneFragment> CachedLaneHandle;
	TStateTreeExternalDataHandle<FMassRepresentationLODFragment> LODFragmentHandle;
	TStateTreeExternalDataHandle<FArcWorldRouteFragment> RouteFragmentHandle;
	TStateTreeExternalDataHandle<UZoneGraphSubsystem> ZoneGraphSubsystemHandle;
	TStateTreeExternalDataHandle<UArcWorldRouteSubsystem> RouteSubsystemHandle;
};
