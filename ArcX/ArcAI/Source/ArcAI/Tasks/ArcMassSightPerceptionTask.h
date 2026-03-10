// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "Perception/ArcMassPerception.h"
#include "UObject/Object.h"
#include "ArcMassSightPerceptionTask.generated.h"

/** Instance data for FArcMassSightPerceptionTask. Holds the list of perceived entities and a change notification delegate. */
USTRUCT()
struct FArcMassSightPerceptionTaskInstanceData
{
	GENERATED_BODY()

	/** Output array of entities currently perceived by the entity's sight sense. Updated on EnterState. */
	UPROPERTY(EditAnywhere, Category = Output)
	TArray<FArcPerceivedEntity> PerceivedEntities;

	/** Delegate dispatcher that fires when the perception results change, allowing downstream nodes to react. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnResultChanged;
};

/**
 * Instant task that queries the entity's sight perception and outputs the list of perceived entities.
 * Fires OnResultChanged when the perception list changes, enabling event-driven StateTree transitions.
 */
USTRUCT(DisplayName="Arc Mass Sight Perception Task", meta = (Category = "Arc|Perception", ToolTip = "Instant task. Queries sight perception and outputs the list of perceived entities."))
struct FArcMassSightPerceptionTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcMassSightPerceptionTaskInstanceData;

	FArcMassSightPerceptionTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

/** Instance data for FArcGetActorFromArrayPropertyFunction. Holds the input array and the selected output actor. */
USTRUCT()
struct FArcGetActorFromArrayFunctionInstanceData
{
	GENERATED_BODY()

	/** Input array of actors to select from. Typically bound from a perception or query result. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<TObjectPtr<AActor>> Input;

	/** The selected actor from the input array (first element by default). */
	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<AActor> Output;
};

/**
 * Property function that extracts a single actor from an array of actors (selects the first element).
 * Useful for converting perception result arrays into a single actor reference for downstream tasks.
 */
USTRUCT(DisplayName = "Arc Get Actor From Array", meta = (Category = "Arc|Perception", ToolTip = "Property function. Extracts a single actor from an array of actors."))
struct FArcGetActorFromArrayPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGetActorFromArrayFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;
};

/**
 * Snapshot of a selected/perceived entity with location, distance, perception strength, and timing data.
 * Used as output from perception selection tasks and property functions.
 */
USTRUCT(BlueprintType)
struct ARCAI_API FArcSelectedEntity
{
	GENERATED_BODY()

	/** Mass entity handle of the selected entity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FMassEntityHandle Entity;

	/** The actor associated with the selected entity, if one exists (may be null for entity-only targets). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<AActor> Actor;

	/** The last known world-space location of the entity. Updated each time the entity is perceived. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector LastKnownLocation = FVector::ZeroVector;

	/** Distance from the perceiving entity to this entity, in world units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Distance = 0.0f;

	/** Perception strength/score for this entity. Higher values indicate stronger/more confident perception. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Strength = 0.0f;

	/** Time in seconds since this entity was last perceived. Zero if currently being perceived. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TimeSinceLastSeen = 0.0f;

	/** World time (seconds) when this entity was last perceived. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	double LastTimeSeen = 0.0f;

	/** True if this entity is currently within the perceiver's active perception range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bIsBeingPerceived = false;
};

/** Instance data for FArcGetPerceivedEntityFromArrayPropertyFunction. Converts perceived entity array to a selected entity. */
USTRUCT()
struct FArcGetPerceivedEntityFromArrayFunctionInstanceData
{
	GENERATED_BODY()

	/** Input array of perceived entities from a perception task. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<FArcPerceivedEntity> Input;

	/** The selected entity info extracted from the input array (first element), converted to FArcSelectedEntity format. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcSelectedEntity Output;
};

/**
 * Property function that extracts entity info from a perceived entity array and converts it to FArcSelectedEntity.
 * Selects the first element from the input array and populates the output with entity handle, actor, location, and perception data.
 */
USTRUCT(DisplayName = "Arc Get Perceived Entity From Array", meta = (Category = "Arc|Perception", ToolTip = "Property function. Extracts entity info from a perceived entity array into FArcSelectedEntity format."))
struct FArcGetPerceivedEntityFromArrayPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGetPerceivedEntityFromArrayFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;
	
};
/** Instance data for FArcMassPerceptionSelectActorTask. Handles input/output entities, trigger listeners, and data store binding. */
USTRUCT()
struct FArcMassPerceptionSelectActorInstanceData
{
	GENERATED_BODY()

	/** The perceived entity to evaluate/select from. Typically bound from a perception property function output. */
	UPROPERTY(EditAnywhere, Category = Input)
	FArcSelectedEntity PerceivedEntity;

	/** Output: the currently selected perceived entity after evaluation. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcSelectedEntity OutPerceivedEntity;

	/** Delegate listener that triggers a new search/selection pass when fired by upstream perception tasks. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateListener TriggerSearch;

	/** Delegate dispatcher that fires when the selected actor changes, allowing downstream nodes to react. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnSelectedActorChanged;

	/** Optional gameplay tag referencing a global data store entry. Used to persist or share the selected entity across the StateTree. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (Categories = "GlobalStore"))
	FGameplayTag DataStoreTag;
};

/**
 * Task that selects and tracks an actor from perception results. Listens for perception changes via TriggerSearch
 * and fires OnSelectedActorChanged when the selected target changes. Can persist the selection to a global data store
 * via DataStoreTag. Returns Running to stay active and re-evaluate on Tick.
 */
USTRUCT(DisplayName="Arc Mass Perception Select Actor Task", meta = (Category = "Arc|Perception", ToolTip = "Selects and tracks an actor from perception results. Fires events when the selected target changes."))
struct FArcMassPerceptionSelectActorTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcMassPerceptionSelectActorInstanceData;

	FArcMassPerceptionSelectActorTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};