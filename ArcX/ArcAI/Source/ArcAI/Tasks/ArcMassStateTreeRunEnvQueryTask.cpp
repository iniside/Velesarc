#include "ArcMassStateTreeRunEnvQueryTask.h"

#include "ArcMassEQSResultStore.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassEQSBlueprintLibrary.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "Items/EnvQueryItemType_MassEntityHandle.h"

FArcMassStateTreeRunEnvQueryTask::FArcMassStateTreeRunEnvQueryTask()
{
	bShouldCallTick = false;
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
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
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

	UArcMassEQSResultStore* ResultStoreSubsystem = Context.GetWorld()->GetSubsystem<UArcMassEQSResultStore>();
	
	InstanceData.RequestId = Request.Execute(InstanceData.RunMode,
		FQueryFinishedSignature::CreateLambda([WeakContext = Context.MakeWeakExecutionContext(), SignalSubsystem, Entity = MassCtx.GetEntity(), ResultStoreSubsystem](TSharedPtr<FEnvQueryResult> QueryResult) mutable
		{
			const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			if (FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>())
			{
				InstanceDataPtr->RequestId = INDEX_NONE;

				bool bSuccess = false;
				if (QueryResult && QueryResult->IsSuccessful())
				{
					auto [VectorPtr, ActorPtr, ArrayOfVector, ArrayOfActor, EntityPtr, EntityArrayPtr] = InstanceDataPtr->Result.GetPtrTupleFromStrongExecutionContext<FVector,AActor*, TArray<FVector>, TArray<AActor*>, FMassEnvQueryEntityInfoBlueprintWrapper, TArray<FMassEnvQueryEntityInfoBlueprintWrapper>>(StrongContext);

					if (VectorPtr)
					{
						*VectorPtr = QueryResult->GetItemAsLocation(0);
						if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
						{
							ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).VectorResult = *VectorPtr;
						}
					}
					else if (ActorPtr)
					{
						*ActorPtr = QueryResult->GetItemAsActor(0);
						if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
						{
							ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).ActorResult = *ActorPtr;
						}
					}
					else if (ArrayOfVector)
					{
						QueryResult->GetAllAsLocations(*ArrayOfVector);
						if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
						{
							ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).VectorArrayResult = *ArrayOfVector;
						}
					}
					else if (ArrayOfActor)
					{
						QueryResult->GetAllAsActors(*ArrayOfActor);
						if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
						{
							ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).ActorArrayResult = *ArrayOfActor;
						}
					}
					else if (EntityPtr)
					{
						UEnvQueryItemType_MassEntityHandle* DefTypeOb = QueryResult->ItemType->GetDefaultObject<UEnvQueryItemType_MassEntityHandle>();
						check(DefTypeOb != nullptr);

						for (const FEnvQueryItem& Item : QueryResult->Items)
						{
							FMassEnvQueryEntityInfo EntityInfo = DefTypeOb->GetValue(QueryResult->RawData.GetData() + Item.DataOffset);
							*EntityPtr = FMassEnvQueryEntityInfoBlueprintWrapper(EntityInfo);
							break;
						}

						if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
						{
							ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).EntityResult = *EntityPtr;
						}
					}
					else if (EntityArrayPtr)
					{
						UEnvQueryItemType_MassEntityHandle* DefTypeOb = QueryResult->ItemType->GetDefaultObject<UEnvQueryItemType_MassEntityHandle>();
						check(DefTypeOb != nullptr);

						TArray<FMassEnvQueryEntityInfoBlueprintWrapper> OutInfo;
						OutInfo.Reserve(OutInfo.Num() + QueryResult->Items.Num());
						
						for (const FEnvQueryItem& Item : QueryResult->Items)
						{
							FMassEnvQueryEntityInfo EntityInfo = DefTypeOb->GetValue(QueryResult->RawData.GetData() + Item.DataOffset);
							EntityArrayPtr->Add(FMassEnvQueryEntityInfoBlueprintWrapper(EntityInfo));
						}

						if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
						{
							ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).EntityArrayResult = *EntityArrayPtr;
						}
					}
					bSuccess = true;
				}

				if (!bSuccess)
				{
					auto [VectorPtr, ActorPtr, ArrayOfVector, ArrayOfActor, EntityPtr, EntityArrayPtr] = InstanceDataPtr->Result.GetPtrTupleFromStrongExecutionContext<FVector,AActor*, TArray<FVector>, TArray<AActor*>, FMassEnvQueryEntityInfoBlueprintWrapper, TArray<FMassEnvQueryEntityInfoBlueprintWrapper>>(StrongContext);
					
					if (VectorPtr)
					{
						*VectorPtr = FNavigationSystem::InvalidLocation;
						if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
						{
							ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).VectorResult = FNavigationSystem::InvalidLocation;
						}
					}
					else if (ActorPtr)
					{
						*ActorPtr = nullptr;
						if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
						{
							ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).ActorResult = nullptr;
						}
					}
					else if (ArrayOfVector)
					{
						ArrayOfActor->Empty();
						if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
						{
							ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).VectorArrayResult.Empty();
						}
					}
					else if (ArrayOfActor)
					{
						ArrayOfVector->Empty();
						if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
						{
							ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).ActorArrayResult.Empty();
						}
					}
					else if (EntityPtr)
					{
						*EntityPtr = FMassEnvQueryEntityInfoBlueprintWrapper();
						if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
						{
							ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).EntityResult = FMassEnvQueryEntityInfoBlueprintWrapper();
						}
					}
					else if (EntityArrayPtr)
					{
						EntityArrayPtr->Empty();
						if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
                        {
                        	ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).EntityArrayResult.Empty();
                        }
					}
				}
				
				StrongContext.FinishTask(bSuccess ? EStateTreeFinishTaskType::Succeeded : EStateTreeFinishTaskType::Failed);

				SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
				
			}
		}));

	return InstanceData.RequestId != INDEX_NONE ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
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
	}
}