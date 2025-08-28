#pragma once
#include "MassEntityHandle.h"
#include "MassNavigationTypes.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeTypes.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreePropertyRef.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "Generators/MassEnvQueryGenerator.h"

#include "ArcMassEQSTypes.generated.h"

USTRUCT()
struct ARCAI_API FArcMassEnvQueryRequest : public FEnvQueryRequest
{
	GENERATED_BODY()

	FMassEntityHandle EntityHandle;
};

UCLASS(MinimalAPI)
class UEnvQueryContext_MassEntityQuerier : public UEnvQueryContext
{
	GENERATED_UCLASS_BODY()
 
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};


UCLASS(MinimalAPI, BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mass Actor Behavior", CommonSchema))
class UArcMassActorStateTreeSchema : public UStateTreeAIComponentSchema
{
	GENERATED_BODY()

public:
	/** Fetches a read-only view of the Mass-relevant requirements of the associated StateTee */
	const TArray<FMassStateTreeDependency>& GetDependencies() const;

protected:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;
	virtual bool Link(FStateTreeLinker& Linker) override;

	UPROPERTY(Transient)
	TArray<FMassStateTreeDependency> Dependencies;
};

USTRUCT()
struct FStateTreeGetMassTargetLocationPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FVector Input = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EMassMovementAction EndOfPathIntent = EMassMovementAction::Move;
	
	UPROPERTY(EditAnywhere, Category = Output)
	FMassTargetLocation Output;;
};

USTRUCT(DisplayName = "Get Mass Target Location")
struct FArcStateTreeMassTargetLocationPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeGetMassTargetLocationPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	//virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const;
#endif
};

USTRUCT()
struct FArcMassStateTreeRunEnvQueryInstanceData
{
	GENERATED_BODY()

	// Result of the query. If an array is binded, it will output all the created values otherwise it will output the best one.
	UPROPERTY(EditAnywhere, Category = Out, meta = (RefType = "/Script/CoreUObject.Vector, /Script/Engine.Actor, /Script/MassEntity.MassEntityHandle", CanRefToArray))
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

	FMassEntityHandle EntityHandle;
	int32 RequestId = INDEX_NONE;
	bool bFinished = false;
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
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
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

UCLASS(meta = (DisplayName = "Arc Mass Entity Handles"), MinimalAPI)
class UArcMassEnvQueryGenerator_MassEntityHandles : public UEnvQueryGenerator
{
	GENERATED_UCLASS_BODY()

public:
	// Begin IMassEQSRequestInterface
	//virtual TUniquePtr<FMassEQSRequestData> GetRequestData(FEnvQueryInstance& QueryInstance) const override;
	//virtual UClass* GetRequestClass() const override { return StaticClass(); }
	//
	//virtual bool TryAcquireResults(FEnvQueryInstance& QueryInstance) const override;
	// ~IMassEQSRequestInterface

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

protected:
	/** Any Entity who is within SearchRadius of any SearchCenter will be acquired */
	UPROPERTY(EditDefaultsOnly, Category=Generator)
	FAIDataProviderFloatValue SearchRadius;

	/** Context of query */
	UPROPERTY(EditAnywhere, Category=Generator)
	TSubclassOf<UEnvQueryContext> SearchCenter;
};
