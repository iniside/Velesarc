#pragma once
#include "MassStateTreeTypes.h"
#include "StateTreePropertyRef.h"
#include "EnvironmentQuery/EnvQueryTypes.h"

class UEnvQuery;

USTRUCT()
struct FArcMassStateTreeRunEnvQueryInstanceData
{
	GENERATED_BODY()

	// Result of the query. If an array is binded, it will output all the created values otherwise it will output the best one.
	UPROPERTY(EditAnywhere, Category = Out, meta = (RefType = "/Script/CoreUObject.Vector, /Script/Engine.Actor, /Script/MassEntity.MassEntityHandle, /Script/MassEQS.MassEnvQueryEntityInfoBlueprintWrapper", CanRefToArray))
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
};

/**
* Task that runs an async environment query and outputs the result to an outside parameter. Supports Actor and vector types EQS.
* The task is usually run in a sibling state to the result user will be with the data being stored in the parent state's parameters.
* - Parent (Has an EQS result parameter)
*	- Run Env Query (If success go to Use Query Result)
*	- Use Query Result
*/
USTRUCT(meta = (DisplayName = "Run Mass Env Query", Category = "Common"))
struct FArcMassStateTreeRunEnvQueryTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassStateTreeRunEnvQueryInstanceData;

	FArcMassStateTreeRunEnvQueryTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

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