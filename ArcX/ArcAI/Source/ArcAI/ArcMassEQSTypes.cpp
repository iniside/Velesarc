#include "ArcMassEQSTypes.h"

#include "AIController.h"
#include "GameplayTasksComponent.h"
#include "MassActorSubsystem.h"
#include "MassAgentComponent.h"
#include "MassCommonFragments.h"
#include "MassEntityElementTypes.h"
#include "MassEntitySettings.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeTypes.h"
#include "StateTreeConsiderationBase.h"
#include "StateTreeExecutionContext.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "Items/EnvQueryItemType_MassEntityHandle.h"
#include "Tasks/AITask.h"

UEnvQueryContext_MassEntityQuerier::UEnvQueryContext_MassEntityQuerier(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void UEnvQueryContext_MassEntityQuerier::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	if (QueryInstance.OwningEntity.IsValid())
	{
		UMassEntitySubsystem* MassSubsystem = QueryInstance.World->GetSubsystem<UMassEntitySubsystem>();
		FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
		const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(QueryInstance.OwningEntity);
		if (Transform)
		{
			UEnvQueryItemType_Point::SetContextHelper(ContextData, Transform->GetTransform().GetLocation());
		}
	}
}


const TArray<FMassStateTreeDependency>& UArcMassActorStateTreeSchema::GetDependencies() const
{
	return Dependencies;
}

bool UArcMassActorStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return Super::IsStructAllowed(InScriptStruct) ||
		InScriptStruct->IsChildOf(FMassStateTreeEvaluatorBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FMassStateTreeTaskBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FMassStateTreeConditionBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FMassStateTreePropertyFunctionBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreeEvaluatorCommonBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreeConsiderationCommonBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreePropertyFunctionCommonBase::StaticStruct());
}

bool UArcMassActorStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return Super::IsExternalItemAllowed(InStruct) ||
		InStruct.IsChildOf(AActor::StaticClass())
			|| InStruct.IsChildOf(UActorComponent::StaticClass())
			|| InStruct.IsChildOf(UWorldSubsystem::StaticClass())
			|| UE::Mass::IsA<FMassFragment>(&InStruct)
			|| UE::Mass::IsA<FMassSharedFragment>(&InStruct)
			|| UE::Mass::IsA<FMassConstSharedFragment>(&InStruct);
}

bool UArcMassActorStateTreeSchema::Link(FStateTreeLinker& Linker)
{
	Dependencies.Empty();
	UE::MassBehavior::FStateTreeDependencyBuilder Builder(Dependencies);

	auto BuildDependencies = [&Builder](const UStateTree* StateTree)
		{
			for (FConstStructView Node : StateTree->GetNodes())
			{
				if (const FMassStateTreeEvaluatorBase* Evaluator = Node.GetPtr<const FMassStateTreeEvaluatorBase>())
				{
					Evaluator->GetDependencies(Builder);
				}
				else if (const FMassStateTreeTaskBase* Task = Node.GetPtr<const FMassStateTreeTaskBase>())
				{
					Task->GetDependencies(Builder);
				}
				else if (const FMassStateTreeConditionBase* Condition = Node.GetPtr<const FMassStateTreeConditionBase>())
				{
					Condition->GetDependencies(Builder);
				}
				else if (const FMassStateTreePropertyFunctionBase* PropertyFunction = Node.GetPtr<const FMassStateTreePropertyFunctionBase>())
				{
					PropertyFunction->GetDependencies(Builder);
				}
			}
		};

	TArray<const UStateTree*, TInlineAllocator<4>> StateTrees;
	StateTrees.Add(CastChecked<const UStateTree>(GetOuter()));
	for (int32 Index = 0; Index < StateTrees.Num(); ++Index)
	{
		const UStateTree* StateTree = StateTrees[Index];
		BuildDependencies(StateTree);

		// The StateTree::Link order is not deterministic. Build the dependencies and add them to this list.
		for (const FCompactStateTreeState& State : StateTree->GetStates())
		{
			if (State.LinkedAsset && State.Type == EStateTreeStateType::LinkedAsset)
			{
				StateTrees.AddUnique(State.LinkedAsset);
			}
		}
	}

#if UE_WITH_MASS_STATETREE_DEPENDENCIES_DEBUG
	for (const FStateTreeExternalDataDesc& Desc : Linker.GetExternalDataDescs())
	{
		const bool bContains = Dependencies.ContainsByPredicate([&Desc](const FMassStateTreeDependency& Other) { return Desc.Struct == Other.Type; });
		ensureMsgf(bContains, TEXT("Tree %s is missing a mass dependency"), *StateTree->GetPathName());
	}
#endif
	return Super::Link(Linker);
}

void FArcStateTreeMassTargetLocationPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Output.EndOfPathPosition = InstanceData.Input;
	InstanceData.Output.EndOfPathIntent = InstanceData.EndOfPathIntent;
}

FArcMassStateTreeRunEnvQueryTask::FArcMassStateTreeRunEnvQueryTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FArcMassStateTreeRunEnvQueryTask::EnterState(FStateTreeExecutionContext& Context
																 , const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.QueryTemplate)
	{
		return EStateTreeRunStatus::Failed;
	}

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	UMassEntitySubsystem* MassSubsystem = MassCtx.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	AActor* Owner = nullptr;
	if (FMassActorFragment* MassActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(MassCtx.GetEntity()))
	{
		Owner = MassActorFragment->GetMutable();
	}
	FEnvQueryRequest Request(InstanceData.QueryTemplate, Owner, MassCtx.GetEntity(), Context.GetWorld());

	for (FAIDynamicParam& DynamicParam : InstanceData.QueryConfig)
	{
		Request.SetDynamicParam(DynamicParam, nullptr);
	}

	InstanceData.RequestId = Request.Execute(InstanceData.RunMode,
		FQueryFinishedSignature::CreateLambda([WeakContext = Context.MakeWeakExecutionContext()](TSharedPtr<FEnvQueryResult> QueryResult) mutable
		{
			const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			if (FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>())
			{
				InstanceDataPtr->RequestId = INDEX_NONE;

				bool bSuccess = false;
				if (QueryResult && QueryResult->IsSuccessful())
				{
					auto [VectorPtr, ActorPtr, ArrayOfVector, ArrayOfActor] = InstanceDataPtr->Result.GetPtrTupleFromStrongExecutionContext<FVector,AActor*, TArray<FVector>, TArray<AActor*>>(StrongContext);
					if (VectorPtr)
					{
						*VectorPtr = QueryResult->GetItemAsLocation(0);
					}
					else if (ActorPtr)
					{
						*ActorPtr = QueryResult->GetItemAsActor(0);
					}
					else if (ArrayOfVector)
					{
						QueryResult->GetAllAsLocations(*ArrayOfVector);
					}
					else if (ArrayOfActor)
					{
						QueryResult->GetAllAsActors(*ArrayOfActor);
					}

					bSuccess = true;
				}
				InstanceDataPtr->bFinished = true;
				//StrongContext.FinishTask(bSuccess ? EStateTreeFinishTaskType::Succeeded : EStateTreeFinishTaskType::Failed);
				//StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);
			}
		}));

	return InstanceData.RequestId != INDEX_NONE ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FArcMassStateTreeRunEnvQueryTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.bFinished)
	{
		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {MassCtx.GetEntity()});
		return EStateTreeRunStatus::Succeeded;
	}

	SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {MassCtx.GetEntity()});
	return EStateTreeRunStatus::Running;
}

void FArcMassStateTreeRunEnvQueryTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.RequestId != INDEX_NONE)
	{
		if (UEnvQueryManager* QueryManager = UEnvQueryManager::GetCurrent(Context.GetOwner()))
		{
			QueryManager->AbortQuery(InstanceData.RequestId);
		}
		InstanceData.RequestId = INDEX_NONE;
		InstanceData.bFinished = false;
	}
}

UArcMassEnvQueryGenerator_MassEntityHandles::UArcMassEnvQueryGenerator_MassEntityHandles(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ItemType = UEnvQueryItemType_MassEntityHandle::StaticClass();
}

void UArcMassEnvQueryGenerator_MassEntityHandles::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	TArray<FVector> OriginLocations;
	QueryInstance.PrepareContext(SearchCenter, OriginLocations);

	if (OriginLocations.Num() == 0)
	{
		return;
	}

	const UMassEntitySubsystem* MassSubsystem = QueryInstance.World->GetSubsystem<UMassEntitySubsystem>();
}
