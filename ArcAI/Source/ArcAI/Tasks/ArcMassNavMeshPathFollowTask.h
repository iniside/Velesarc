// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEQSBlueprintLibrary.h"
#include "MassMovementTypes.h"
#include "MassNavigationTypes.h"
#include "MassNavMeshNavigationFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeTypes.h"
#include "ArcMassNavMeshPathFollowTask.generated.h"

struct FTransformFragment;
struct FMassMoveTargetFragment;
struct FAgentRadiusFragment;
struct FAgentHeightFragment;
struct FMassMovementParameters;
struct FMassDesiredMovementFragment;

/** Instance data for FArcMassNavMeshPathFollowTask. Contains navigation parameters and optional goal tracking settings. */
USTRUCT()
struct FArcMassNavMeshPathFollowTaskInstanceData
{
	GENERATED_BODY()

	/** The target location to navigate toward. Supports both fixed positions and actor-based targets. */
	UPROPERTY(EditAnywhere, Category = Input)
	FMassTargetLocation TargetLocation;

	/** Movement style reference determining speed and animation parameters. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassMovementStyleRef MovementStyle;

	/** Multiplier applied to the entity's movement speed. 1.0 = normal speed. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float SpeedScale = 1.f;
	
	/** Maximum width of the corridor to use. */
	UPROPERTY(EditAnywhere, Category = Parameter)
    float CorridorWidth = 600.f;

	/** Amount to offset corridor sides from navmesh borders. Used to keep movement away for navmesh borders. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float OffsetFromBoundaries = 10.f;

	/** Distance from the end of path used to confirm that the destination is reached. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float EndDistanceThreshold = 20.f;
	
	/** Optional entity wrapper to track as the goal actor. When set, the destination updates to follow this entity's position. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEnvQueryEntityInfoBlueprintWrapper TrackedEntity;

	/** If true, monitors the goal actor/entity for position changes and re-paths when it moves significantly. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bTrackGoalChange = false;

	/** Minimum distance the goal must move before triggering a repath. Only used when bTrackGoalChange is true. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float MinGoalDistanceToRepath = 100.f;
};

/**
 * Latent StateTree task for Mass-only NavMesh pathfollowing. Does not require an actor or AIController.
 * Finds a path to TargetLocation using Mass NavMesh corridor navigation, starts a move action,
 * and follows the path by updating the short path segment as needed. EnterState requests the path
 * and returns Running. Tick updates path progress. Completes (Succeeded) when the destination is
 * reached. Supports goal actor tracking via TrackedEntity for dynamic re-pathing.
 * This should be the primary task in its state.
 */
USTRUCT(meta = (DisplayName = "Arc NavMesh Path Follow", ToolTip = "Latent Mass-only NavMesh pathfollowing task. No actor required. Returns Running until path is completed."))
struct FArcMassNavMeshPathFollowTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FArcMassNavMeshPathFollowTaskInstanceData::StaticStruct(); };

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	bool RequestPath(FStateTreeExecutionContext& Context, const FMassTargetLocation& InTargetLocation) const;
	bool UpdateShortPath(FStateTreeExecutionContext& Context) const;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	/** Handle to the entity's transform fragment for reading current position. */
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;

	/** Handle to the entity's move target fragment for setting movement action and destination. */
	TStateTreeExternalDataHandle<FMassMoveTargetFragment> MoveTargetHandle;

	/** Handle to the entity's agent radius for corridor width calculations. */
	TStateTreeExternalDataHandle<FAgentRadiusFragment> AgentRadiusHandle;

	/** Handle to the entity's agent height for navigation queries. */
	TStateTreeExternalDataHandle<FAgentHeightFragment> AgentHeightHandle;

	/** Handle to the entity's desired movement fragment for setting movement direction and speed. */
	TStateTreeExternalDataHandle<FMassDesiredMovementFragment> DesiredMovementHandle;

	/** Handle to the entity's movement parameters (max speed, acceleration, etc.). */
	TStateTreeExternalDataHandle<FMassMovementParameters> MovementParamsHandle;

	/** Handle to the short path fragment holding a small segment of the full navmesh path for local steering. */
	TStateTreeExternalDataHandle<FMassNavMeshShortPathFragment> ShortPathHandle;

	/** Handle to the cached full navmesh path for long-distance corridor navigation. */
	TStateTreeExternalDataHandle<FMassNavMeshCachedPathFragment> CachedPathHandle;
};