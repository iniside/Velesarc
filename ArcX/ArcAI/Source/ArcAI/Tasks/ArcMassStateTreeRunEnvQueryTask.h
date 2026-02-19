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

USTRUCT()
struct FArcMassStateTreeRunEnvQueryInstanceData
{
	GENERATED_BODY()

	// Result of the query. If an array is binded, it will output all the created values otherwise it will output the best one.
	UPROPERTY(EditAnywhere, Category = Out, meta = (RefType = "/Script/CoreUObject.Vector, /Script/Engine.Actor, /Script/MassEntity.MassEntityHandle, /Script/MassEQS.MassEnvQueryEntityInfoBlueprintWrapper, /Script/ArcAI.ArcMassSmartObjectItem", CanRefToArray))
	FStateTreePropertyRef Result;

	// The query template to run
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UEnvQuery> QueryTemplate;

	// Query config associated with the query template.
	UPROPERTY(EditAnywhere, EditFixedSize, Category = Parameter)
	TArray<FAIDynamicParam> QueryConfig;

	/** determines which item will be stored (All = only first matching) */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TEnumAsByte<EEnvQueryRunMode::Type> RunMode = EEnvQueryRunMode::SingleResult;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bStoreInEQSStore = false;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FName StoreName;
	
	FMassEntityHandle EntityHandle;
	int32 RequestId = INDEX_NONE;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnResultChanged;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateListener TriggerQuery;
};

/**
* Task that runs an async environment query and outputs the result to an outside parameter. Supports Actor and vector types EQS.
* The task is usually run in a sibling state to the result user will be with the data being stored in the parent state's parameters.
* - Parent (Has an EQS result parameter)
*	- Run Env Query (If success go to Use Query Result)
*	- Use Query Result
*/
USTRUCT(meta = (DisplayName = "Arc Run Mass Env Query", Category = "Arc|Common"))
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
    
	UPROPERTY(EditAnywhere)
	bool bFinishOnEnd = true;

	// How often should  EQS re run after finish. -1 = Never.
	UPROPERTY(EditAnywhere, Meta = (EditCondition="bFinishOnEnd==false"))
	float Interval = -1;

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
