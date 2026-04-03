#pragma once
#include "Mass/EntityElementTypes.h"
#include "MassProcessor.h"
#include "MassStateTreeTypes.h"

#include "ArcMassWaitTask.generated.h"

/** Sparse fragment storing per-entity wait timer state. Used by UArcMassWaitTaskProcessor to tick down the wait. */
USTRUCT()
struct FArcMassWaitTaskFragment : public FMassSparseFragment
{
	GENERATED_BODY()

public:
	/** Total wait duration in seconds. Set when the wait begins. */
	float Duration = 0;

	/** Elapsed time in seconds since the wait started. Compared against Duration to detect completion. */
	float Time = 0;
};

/** Sparse tag added to entities that have an active wait in progress. Used by UArcMassWaitTaskProcessor to query waiting entities. */
USTRUCT()
struct FArcMassWaitTaskTag : public FMassSparseTag
{
	GENERATED_BODY()
};

/** Instance data for FArcMassWaitTask. Holds the configured wait duration and elapsed time. */
USTRUCT()
struct FArcMassWaitTaskInstanceData
{
	GENERATED_BODY()

	/** Duration to wait in seconds before the task completes. A value of 0 or negative means wait indefinitely (requires an external state transition to stop). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 0.f;

	/** Internal: elapsed time in seconds since the wait started. Compared against Duration each tick. */
	float Time = 0;
};

/**
 * Latent StateTree task that waits for a specified duration before completing.
 * EnterState returns Running and starts the timer. Tick accumulates elapsed time. The task
 * completes (Succeeded) after Duration seconds. If Duration is 0 or negative, the task runs
 * indefinitely until an external state transition stops it. The entity stands idle at its
 * current navmesh location while waiting. This should be the primary task in its state.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Wait", Category = "Arc|Common", ToolTip = "Latent task that waits for a specified duration. Returns Running until time elapses."))
struct FArcMassWaitTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassWaitTaskInstanceData;

public:
	FArcMassWaitTask();

protected:
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

class UMassSignalSubsystem;

/**
 * Mass processor that ticks down wait timers on entities with FArcMassWaitTaskTag.
 * When an entity's elapsed time exceeds its configured duration, signals the entity's
 * StateTree to resume execution.
 */
UCLASS(MinimalAPI)
class UArcMassWaitTaskProcessor : public UMassProcessor
{
	GENERATED_BODY()

protected:
	UArcMassWaitTaskProcessor();

	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& ) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/** Query for entities that have the wait tag and fragment, used to tick and check completion. */
	FMassEntityQuery EntityQuery_Conditional;

	/** Cached reference to the signal subsystem for sending completion signals to StateTree. */
	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem = nullptr;
};