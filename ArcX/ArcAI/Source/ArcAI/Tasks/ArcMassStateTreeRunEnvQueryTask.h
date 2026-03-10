#pragma once
#include "MassProcessorDependencySolver.h"
#include "MassSignalProcessorBase.h"
#include "MassStateTreeTypes.h"
#include "StateTreePropertyRef.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/EnvQueryTypes.h"

#include "ArcMassStateTreeRunEnvQueryTask.generated.h"

class UEnvQuery;
class UArcMassEQSResultStore;
class UMassSignalSubsystem;

/** Instance data for FArcMassStateTreeRunEnvQueryTask. Holds the query template, config, and output result. */
USTRUCT()
struct FArcMassStateTreeRunEnvQueryInstanceData
{
	GENERATED_BODY()

	/** Output result of the EQS query. Supports Vector, Actor, MassEntityHandle, and MassEnvQueryEntityInfoBlueprintWrapper types. If bound to an array, outputs all matching items; otherwise outputs the best single result. */
	UPROPERTY(EditAnywhere, Category = Out, meta = (RefType = "/Script/CoreUObject.Vector, /Script/Engine.Actor, /Script/MassEntity.MassEntityHandle, /Script/MassEQS.MassEnvQueryEntityInfoBlueprintWrapper, /Script/ArcAI.ArcMassSmartObjectItem", CanRefToArray))
	FStateTreePropertyRef Result;

	/** The EQS query template asset to execute. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UEnvQuery> QueryTemplate;

	/** Runtime configuration parameters for the query template. Entries must match the query's named params. */
	UPROPERTY(EditAnywhere, EditFixedSize, Category = Parameter)
	TArray<FAIDynamicParam> QueryConfig;

	/** Determines how many items are stored from the query results. SingleResult returns the best match; AllMatching returns all passing items. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TEnumAsByte<EEnvQueryRunMode::Type> RunMode = EEnvQueryRunMode::SingleResult;

	/** If true, stores the query result in the EQS result store subsystem under StoreName for later retrieval. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bStoreInEQSStore = false;

	/** Name key used to store and retrieve the result in the EQS result store. Only used when bStoreInEQSStore is true. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FName StoreName;

	/** Internal: the entity handle for the entity running this query. */
	FMassEntityHandle EntityHandle;

	/** Internal: the active EQS request ID. INDEX_NONE when no query is pending. */
	int32 RequestId = INDEX_NONE;

	/** Delegate dispatched when the EQS result changes (new best item differs from previous). Useful for reacting to result updates during repeated queries. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnResultChanged;

	/** Delegate listener that triggers a new EQS query when invoked. Allows external events to force a re-query. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateListener TriggerQuery;
};

/**
 * Latent StateTree task that runs an asynchronous EQS query and outputs results via property ref binding.
 * Supports Actor, Vector, MassEntityHandle, and MassEnvQueryEntityInfoBlueprintWrapper result types.
 *
 * EnterState kicks off the async query and returns Running. The task signals completion when results arrive.
 * When bFinishOnEnd is false, the task remains Running and can re-run the query at the specified Interval.
 * Use bSignalOnResultChange to fire the OnResultChanged delegate when the best result changes between runs.
 *
 * Typical usage pattern: run in a sibling state, storing results in a parent state's parameters:
 *   - Parent (Has an EQS result parameter)
 *     - Run Env Query (on success, transition to Use Query Result)
 *     - Use Query Result
 *
 * This should be the primary task in its state since it is latent.
 */
USTRUCT(meta = (DisplayName = "Arc Run Mass Env Query", Category = "Arc|Common", ToolTip = "Latent async EQS query task. Returns Running until results arrive. Supports repeated queries via Interval."))
struct FArcMassStateTreeRunEnvQueryTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassStateTreeRunEnvQueryInstanceData;

	FArcMassStateTreeRunEnvQueryTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	static void ExecuteQueryCallback(TSharedPtr<FEnvQueryResult> QueryResult, FStateTreeWeakExecutionContext WeakContext, UMassSignalSubsystem* SignalSubsystem, FMassEntityHandle Entity
		, UArcMassEQSResultStore* ResultStoreSubsystem, bool bFinishOnEnd, float Interval, bool bInSignalOnResultChange);
    
	/** If true, the task completes (Succeeded) once the first query result arrives. If false, the task stays Running and can re-run periodically. */
	UPROPERTY(EditAnywhere)
	bool bFinishOnEnd = true;

	/** How often (in seconds) the EQS query re-runs after completion. Only used when bFinishOnEnd is false. -1 means never re-run. */
	UPROPERTY(EditAnywhere, Meta = (EditCondition="bFinishOnEnd==false"))
	float Interval = -1;

	/** If true, dispatches the OnResultChanged delegate when the best query result changes between re-runs. Only used when bFinishOnEnd is false. */
	UPROPERTY(EditAnywhere, Meta = (EditCondition="bFinishOnEnd==false"))
	bool bSignalOnResultChange = true;

//#if WITH_EDITOR
//	UE_API virtual void PostEditInstanceDataChangeChainProperty(const FPropertyChangedChainEvent& PropertyChangedEvent, FStateTreeDataView InstanceDataView) override;
//	UE_API virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
//	virtual FName GetIconName() const override
//	{
//		return FName("StateTreeEditorStyle|Node.Find");
//	}
//	virtual FColor GetIconColor() const override
//	{
//		return UE::StateTree::Colors::Grey;
//	}
//#endif // WITH_EDITOR

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

/** Gathers actors perceived by context */
//UCLASS(meta = (DisplayName = "Arc Mass Perceived Actors"), MinimalAPI)
//class UEnvQueryGenerator_ArcMassPerceivedActors : public UEnvQueryGenerator
//{
//	GENERATED_UCLASS_BODY()
//
//protected:
//	/** If set will be used to filter results */
//	UPROPERTY(EditDefaultsOnly, Category=Generator)
//	TSubclassOf<AActor> AllowedActorClass;
//
//	/** Additional distance limit imposed on the items generated. Perception's range limit still applies. */
//	UPROPERTY(EditDefaultsOnly, Category=Generator)
//	FAIDataProviderFloatValue SearchRadius;
//
//	/** The perception listener to use as a source of information */
//	UPROPERTY(EditAnywhere, Category=Generator)
//	TSubclassOf<UEnvQueryContext> ListenerContext;
//
//	/** If set will be used to filter gathered results so that only actors perceived with a given sense are considered */
//	UPROPERTY(EditAnywhere, Category=Generator)
//	TSubclassOf<UAISense> SenseToUse;
//
//	/**
//	 * Indicates whether to include all actors known via perception (TRUE) or just the ones actively being perceived 
//	 * at the moment (example "currently visible" as opposed to "seen and the perception stimulus haven't expired yet").
//	 * @see FAIStimulus.bExpired
//	 */
//	UPROPERTY(EditAnywhere, Category=Generator)
//	bool bIncludeKnownActors = true;
//
//	AIMODULE_API virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;
//
//	AIMODULE_API virtual FText GetDescriptionTitle() const override;
//	AIMODULE_API virtual FText GetDescriptionDetails() const override;
//};
