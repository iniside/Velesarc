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

/** FMassNavMeshPathFollowTask movement parameters */
USTRUCT()
struct FArcMassNavMeshPathFollowTaskInstanceData
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

	/** Amount to offset corridor sides from navmesh borders. Used to keep movement away for navmesh borders. */
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

/** Finds a path to TargetLocation, requests a short path, starts a move action and follow the path by updating the short path when needed. */ 
USTRUCT(meta = (DisplayName = "Arc NavMesh Path Follow"))
struct FArcMassNavMeshPathFollowTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FArcMassNavMeshPathFollowTaskInstanceData::StaticStruct(); };

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

	// Hold a small part of a navmesh path
	TStateTreeExternalDataHandle<FMassNavMeshShortPathFragment> ShortPathHandle;

	TStateTreeExternalDataHandle<FMassNavMeshCachedPathFragment> CachedPathHandle;
};