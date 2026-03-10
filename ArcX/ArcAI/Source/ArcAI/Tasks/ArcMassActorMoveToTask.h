#pragma once
#include "AISystem.h"
#include "MassStateTreeTypes.h"
#include "AITypes.h"
#include "MassProcessor.h"
#include "MassActorSubsystem.h"
#include "MassNavigationFragments.h"
#include "MassTranslator.h"

#include "ArcMassActorMoveToTask.generated.h"

class AAIController;
class AActor;
class UNavigationQueryFilter;
class UAITask_MoveTo;
class IGameplayTaskOwnerInterface;

/** Instance data for FArcMassActorMoveToTask. Holds movement parameters and runtime state for AITask_MoveTo. */
USTRUCT()
struct FArcMassActorMoveToTaskInstanceData
{
	GENERATED_BODY()

	/** The AIController that owns this movement request. Must be bound from the entity's actor. */
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	/** World-space destination location to move toward. Used when TargetActor is not set. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FVector Destination = FVector::Zero();

	/** Optional target actor to move toward. When set, the AI will path toward this actor instead of Destination. */
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

	/** Navigation query filter class to use for pathfinding. "None" will result in the default filter being used. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TSubclassOf<UNavigationQueryFilter> FilterClass;

	/** Whether the AI is allowed to strafe while moving toward the destination. */
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

	/** Acceptance radius used when moving toward a TargetActor. Determines how close the AI must get to the actor to consider arrival. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta=(ClampMin = "0.0", UIMin="0.0"))
	float TargetAcceptanceRadius = 40.f;

	/** Internal: the active AITask_MoveTo instance driving the movement. Managed by the task. */
	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> MoveToTask = nullptr;

	/** Internal: the gameplay task owner interface used by the AITask_MoveTo. */
	UPROPERTY(Transient)
	TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner = nullptr;

	/** Internal: the final acceptance radius computed from AcceptableRadius and agent/goal radii settings. */
	float CalculatedAcceptanceRadius = 0.f;
};

/** Mass fragment storing actor-based MoveTo request data on the entity. Used by UArcMassActorMoveToProcessor. */
USTRUCT()
struct FArcMassActorMoveToFragment : public FMassFragment
{
	GENERATED_BODY()

	/** World-space destination for the movement request. */
	UPROPERTY()
	FVector Destination = FVector::Zero();

	/** Distance threshold for considering the destination reached. */
	UPROPERTY()
	float AcceptanceRadius = 32.f;

	/** Tolerance for detecting changes in the destination when tracking a moving goal. */
	UPROPERTY()
	float DestinationMoveTolerance = 0.f;

	/** Whether the path should continuously update when the goal actor moves. */
	UPROPERTY()
	bool bTrackMovingGoal = 0.f;

	/** Optional target actor being moved toward. When valid, movement tracks this actor's position. */
	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	/** The underlying move request configuration passed to the AI movement system. */
	FAIMoveRequest MoveReq;
};

template<>
struct TMassFragmentTraits<FArcMassActorMoveToFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/**
 * Latent StateTree task that moves a Mass entity's actor to a destination using AITask_MoveTo.
 * EnterState starts the MoveTo request and returns Running. The task completes (Succeeded/Failed)
 * when the underlying AITask_MoveTo finishes. This should be the primary task in its state since
 * it occupies the entity until movement completes.
 * Requires the entity to have an actor with an AIController.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Actor Move To", Category = "AI|Action", ToolTip = "Latent task that moves the entity's actor to a destination via AITask_MoveTo. Returns Running until movement completes."))
struct FArcMassActorMoveToTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
public:
	using FInstanceDataType = FArcMassActorMoveToTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FArcMassActorMoveToTask();

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	virtual UAITask_MoveTo* PrepareMoveToTask(FStateTreeExecutionContext& Context, AAIController& Controller, UAITask_MoveTo* ExistingTask, FAIMoveRequest& MoveRequest) const;
	virtual EStateTreeRunStatus PerformMoveTask(FStateTreeExecutionContext& Context, AAIController& Controller) const;


#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	/** Handle to the entity's actor fragment, used to retrieve the AActor for the AIController. */
	TStateTreeExternalDataHandle<FMassActorFragment> MassActorFragment;

	/** Handle to the entity's move target fragment, used to update movement state. */
	TStateTreeExternalDataHandle<FMassMoveTargetFragment> MoveTargetHandle;

	/** Handle to the entity's actor MoveTo fragment, storing per-entity movement request data. */
	TStateTreeExternalDataHandle<FArcMassActorMoveToFragment> MassActorMoveToFragment;
};

/** Instance data for FArcMassDrawDebugSphereTask. Holds the debug sphere location, radius, and display duration. */
USTRUCT()
struct FArcMassDrawDebugSphereTaskInstanceData
{
	GENERATED_BODY()

	/** World-space location at which to draw the debug sphere. */
	UPROPERTY(EditAnywhere, Category = Input)
	FVector Location;

	/** Radius of the debug sphere in world units. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Radius = 20.f;

	/** How long (in seconds) the debug sphere persists in the world. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 20.f;
};

/**
 * Instant StateTree task that draws a debug sphere at a given location.
 * EnterState draws the sphere and immediately returns Succeeded. Useful for visualizing
 * positions during StateTree debugging.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Draw Debug Sphere", Category = "AI|Action", ToolTip = "Instant debug task that draws a sphere at the specified location and immediately succeeds."))
struct FArcMassDrawDebugSphereTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
public:
	using FInstanceDataType = FArcMassDrawDebugSphereTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FArcMassDrawDebugSphereTask();

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};


/** Mass tag added to entities that have an active actor-based MoveTo request in progress. */
USTRUCT()
struct FArcMassActorMoveToTag : public FMassTag
{
	GENERATED_BODY()
};

class UMassSignalSubsystem;

/**
 * Mass processor that monitors entities with FArcMassActorMoveToTag and checks whether their
 * actor-based MoveTo has completed. On completion, signals the entity's StateTree to resume.
 */
UCLASS(MinimalAPI)
class UArcMassActorMoveToProcessor : public UMassProcessor
{
	GENERATED_BODY()

protected:
	UArcMassActorMoveToProcessor();

	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& ) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/** Query for entities that have the MoveTo tag and fragment, used to check completion. */
	FMassEntityQuery EntityQuery_Conditional;

	/** Cached reference to the signal subsystem for sending completion signals to StateTree. */
	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem = nullptr;
};
