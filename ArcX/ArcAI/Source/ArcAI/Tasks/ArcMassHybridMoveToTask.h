#pragma once

#include "AISystem.h"
#include "MassStateTreeTypes.h"
#include "AITypes.h"
#include "MassProcessor.h"
#include "MassActorSubsystem.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "MassTranslator.h"

#include "ArcMassHybridMoveToTask.generated.h"

class AAIController;
class AActor;
class UNavigationQueryFilter;
class UAITask_MoveTo;
class IGameplayTaskOwnerInterface;

/** Instance data for FArcMassHybridMoveToTask. Contains parameters for both Mass NavMesh and AIController-based movement. */
USTRUCT()
struct FArcMassHybridMoveToTaskInstanceData
{
	GENERATED_BODY()

	/** The AIController used for actor-based movement fallback. Must be bound from the entity's actor. */
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	/** World-space destination to move toward. Used when TargetActor is not set. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FVector Destination = FVector::Zero();

	/** Optional target actor to move toward. When set, movement tracks this actor instead of Destination. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<AActor> TargetActor;

	/** fixed distance added to threshold between AI and goal location in destination reach test */
	UPROPERTY(EditAnywhere, Category = Parameter, meta=(ClampMin = "0.0", UIMin="0.0"))
	float AcceptableRadius = GET_AI_CONFIG_VAR(AcceptanceRadius);

	/** if the task is expected to react to changes to location in input
	 *	this property can be used to tweak sensitivity of the mechanism. Value is 
	 *	recommended to be less than AcceptableRadius */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition="bTrackMovingGoal"))
	float DestinationMoveTolerance = 0.f;

	/** Navigation query filter class for pathfinding. "None" will result in the default filter being used. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TSubclassOf<UNavigationQueryFilter> FilterClass;

	/** Whether the AI is allowed to strafe while moving toward the destination. Only applies to AIController movement. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bAllowStrafe = GET_AI_CONFIG_VAR(bAllowStrafing);

	/** if set, use incomplete path when goal can't be reached */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bAllowPartialPath = GET_AI_CONFIG_VAR(bAcceptPartialPaths);

	/** if set, path to goal actor will update itself when actor moves */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bTrackMovingGoal = true;

	/** if set, the goal location will need to be navigable */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bRequireNavigableEndLocation = true;

	/** if set, goal location will be projected on navigation data (navmesh) before using */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bProjectGoalLocation = true;

	/** if set, radius of AI's capsule will be added to threshold between AI and goal location in destination reach test  */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bReachTestIncludesAgentRadius = GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap);
	
	/** if set, radius of goal's capsule will be added to threshold between AI and goal location in destination reach test  */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bReachTestIncludesGoalRadius = GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap);

	/** Acceptance radius when moving toward a TargetActor. Determines how close the AI must get to the actor. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta=(ClampMin = "0.0", UIMin="0.0"))
	float TargetAcceptanceRadius = 40.f;

	/** Internal: the active AITask_MoveTo instance for AIController-based movement. Managed by the task. */
	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> MoveToTask = nullptr;

	/** Internal: the gameplay task owner interface used by the AITask_MoveTo. */
	UPROPERTY(Transient)
	TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner = nullptr;

	/** Target location for Mass NavMesh pathfollowing. Used when bPreferMassNavMesh is true. */
	UPROPERTY(EditAnywhere, Category = Input)
	FMassTargetLocation TargetLocation;

	/** Movement style reference determining speed and animation parameters for Mass movement. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassMovementStyleRef MovementStyle;

	/** Multiplier applied to the entity's movement speed during Mass NavMesh pathfollowing. */
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

	/** Whether the Mass NavMesh pathfollowing requires the end location to be on navigable space. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bMassRequireNavigableEndLocation = false;

	/** If set, Mass NavMesh pathfollowing will accept a partial path when the full destination is unreachable. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bMassAllowPartialPath = true;

	/** When true, prefer lightweight Mass NavMesh pathfollowing. When false, fall back to full AIController MoveTo. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bPreferMassNavMesh = true;

	/** Internal: the final acceptance radius computed from AcceptableRadius and agent/goal radii settings. */
	float CalculatedAcceptanceRadius = 0.f;
};

/**
 * Latent StateTree task that combines Mass NavMesh pathfollowing with AIController MoveTo.
 * When bPreferMassNavMesh is true, the entity uses lightweight Mass NavMesh corridor-based
 * pathfollowing (no actor required for movement). Otherwise, it falls back to full AIController
 * MoveTo via AITask_MoveTo. EnterState starts movement and returns Running. The task completes
 * when the destination is reached or movement fails. This should be the primary task in its state.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Hybrid Move To", Category = "AI|Action", ToolTip = "Latent hybrid movement task combining Mass NavMesh pathfollowing and AIController MoveTo. Returns Running until destination reached."))
struct FArcMassHybridMoveToTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
public:
	using FInstanceDataType = FArcMassHybridMoveToTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FArcMassHybridMoveToTask();

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	virtual UAITask_MoveTo* PrepareMoveToTask(FStateTreeExecutionContext& Context, AAIController& Controller, UAITask_MoveTo* ExistingTask, FAIMoveRequest& MoveRequest) const;
	virtual EStateTreeRunStatus PerformMoveTask(FStateTreeExecutionContext& Context, AAIController& Controller) const;

	bool RequestPath(FStateTreeExecutionContext& Context, const FMassTargetLocation& TargetLocation) const;
	bool UpdateShortPath(FStateTreeExecutionContext& Context) const;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	/** Handle to the entity's actor fragment, used to retrieve the AActor for AIController-based movement. */
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorFragment;

	/** Handle to the entity's move target fragment, used to set movement action and destination. */
	TStateTreeExternalDataHandle<FMassMoveTargetFragment> MoveTargetHandle;

	/** Handle to the entity's transform fragment, used for position queries during Mass pathfollowing. */
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;

	/** Handle to the entity's agent radius, used for corridor width and collision calculations. */
	TStateTreeExternalDataHandle<FAgentRadiusFragment> AgentRadiusHandle;

	/** Handle to the entity's desired movement fragment, used to set movement direction and speed. */
	TStateTreeExternalDataHandle<FMassDesiredMovementFragment> DesiredMovementHandle;

	/** Handle to the entity's movement parameters (max speed, etc.). */
	TStateTreeExternalDataHandle<FMassMovementParameters> MovementParamsHandle;

	/** Handle to the short path fragment holding a small segment of the full navmesh path for steering. */
	TStateTreeExternalDataHandle<FMassNavMeshShortPathFragment> ShortPathHandle;

	/** Handle to the cached full navmesh path used for long-distance navigation. */
	TStateTreeExternalDataHandle<FMassNavMeshCachedPathFragment> CachedPathHandle;
};