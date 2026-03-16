// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEQSBlueprintLibrary.h"
#include "MassMovementTypes.h"
#include "MassNavigationTypes.h"
#include "MassNavMeshNavigationFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeTypes.h"
#include "ArcMassInfluencePathFollowTask.generated.h"

class UNavigationQueryFilter;
struct FTransformFragment;
struct FMassMoveTargetFragment;
struct FAgentRadiusFragment;
struct FAgentHeightFragment;
struct FMassMovementParameters;
struct FMassDesiredMovementFragment;

USTRUCT()
struct FArcMassInfluencePathFollowTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	FMassTargetLocation TargetLocation;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassMovementStyleRef MovementStyle;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float SpeedScale = 1.f;

	/** Maximum width of the corridor to use. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float CorridorWidth = 600.f;

	/** Amount to offset corridor sides from navmesh borders. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float OffsetFromBoundaries = 10.f;

	/** Distance from the end of path used to confirm that the destination is reached. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float EndDistanceThreshold = 20.f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEnvQueryEntityInfoBlueprintWrapper TrackedEntity;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bTrackGoalChange = false;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float MinGoalDistanceToRepath = 100.f;
};

/**
 * NavMesh path follow task that supports a custom UNavigationQueryFilter.
 *
 * Identical to FArcMassNavMeshPathFollowTask but adds a NavigationFilterClass
 * property. When set (e.g. to UArcInfluenceNavigationQueryFilter), the filter
 * is resolved per-query and passed into FPathFindingQuery so A* uses the
 * custom cost function. When left null, the default navmesh filter is used.
 */
USTRUCT(meta = (DisplayName = "Arc Influence Path Follow"))
struct ARCAI_API FArcMassInfluencePathFollowTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	/** Optional navigation query filter class.
	 *  Set to UArcInfluenceNavigationQueryFilter (or a BP subclass) to
	 *  make pathfinding account for influence map costs. */
	UPROPERTY(EditAnywhere, Category = "Navigation")
	TSubclassOf<UNavigationQueryFilter> NavigationFilterClass;

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FArcMassInfluencePathFollowTaskInstanceData::StaticStruct(); }

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	bool RequestPath(FStateTreeExecutionContext& Context, const FMassTargetLocation& InTargetLocation) const;
	bool UpdateShortPath(FStateTreeExecutionContext& Context) const;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
	TStateTreeExternalDataHandle<FMassMoveTargetFragment> MoveTargetHandle;
	TStateTreeExternalDataHandle<FAgentRadiusFragment> AgentRadiusHandle;
	TStateTreeExternalDataHandle<FAgentHeightFragment> AgentHeightHandle;
	TStateTreeExternalDataHandle<FMassDesiredMovementFragment> DesiredMovementHandle;
	TStateTreeExternalDataHandle<FMassMovementParameters> MovementParamsHandle;
	TStateTreeExternalDataHandle<FMassNavMeshShortPathFragment> ShortPathHandle;
	TStateTreeExternalDataHandle<FMassNavMeshCachedPathFragment> CachedPathHandle;
};
