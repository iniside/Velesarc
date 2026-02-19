#include "ArcMassStateTreeRunEnvQueryTask.h"

#include "ArcMassEQSResultStore.h"
#include "ArcMassEQSTypes.h"
#include "EnvQueryItemType_SmartObject.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassEQSBlueprintLibrary.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "Items/EnvQueryItemType_MassEntityHandle.h"
#include "SmartObjects/ArcEnvQueryItemType_EntitySmartObject.h"
#include "SmartObjects/ArcMassSmartObjectTypes.h"

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
	UArcMassEQSResultStore* ResultStoreSubsystem = Context.GetWorld()->GetSubsystem<UArcMassEQSResultStore>();
	
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

	//if (InstanceData.TriggerQuery.GetDispatcher().IsValid())
	{
		Context.BindDelegate(InstanceData.TriggerQuery, FSimpleDelegate::CreateWeakLambda(MassSubsystem, [bSignal = bSignalOnResultChange, WeakContext = Context.MakeWeakExecutionContext()
			, SignalSubsystem, Entity = MassCtx.GetEntity()
			, ResultStoreSubsystem, bFinish = bFinishOnEnd]()
		{
			// Rerun the EQS query here
			const FStateTreeStrongExecutionContext StrongCtx = WeakContext.MakeStrongExecutionContext();
			{
				if (FInstanceDataType* InstData = StrongCtx.GetInstanceDataPtr<FInstanceDataType>())
				{
					UWorld* LocalWorld = SignalSubsystem->GetWorld();
					UMassEntitySubsystem* MassSubsystem = LocalWorld->GetSubsystem<UMassEntitySubsystem>();
					FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
					
					AActor* Owner = nullptr;
					if (FMassActorFragment* MassActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity))
					{
						Owner = MassActorFragment->GetMutable();
					}
					// Recreate and execute the query
					FEnvQueryRequest NewRequest(InstData->QueryTemplate, Owner, Entity, SignalSubsystem->GetWorld());
					for (FAIDynamicParam& DynamicParam : InstData->QueryConfig)
					{
						NewRequest.SetDynamicParam(DynamicParam, nullptr);
					}

					float interval = 0;
					auto RecursiveCallback = FQueryFinishedSignature::CreateStatic(&FArcMassStateTreeRunEnvQueryTask::ExecuteQueryCallback,
						WeakContext
						, SignalSubsystem
						, Entity
						, ResultStoreSubsystem
						, bFinish
						, 0.f//interval
						, bSignal);
					
					InstData->RequestId = NewRequest.Execute(InstData->RunMode, RecursiveCallback);
				}
			}
		}));
	}
	
	auto QueryLambda = FQueryFinishedSignature::CreateLambda(
		[this, WeakContext = Context.MakeWeakExecutionContext(), SignalSubsystem, Entity = MassCtx.GetEntity(), ResultStoreSubsystem, World = Context.GetWorld()]
		(TSharedPtr<FEnvQueryResult> QueryResult) mutable
		{
			const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			if (FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>())
			{
				InstanceDataPtr->RequestId = INDEX_NONE;

				bool bSuccess = false;
				if (QueryResult && QueryResult->IsSuccessful())
				{
					auto [VectorPtr, ActorPtr, ArrayOfVector, ArrayOfActor, EntityPtr, EntityArrayPtr, SmartObjectSlotEQSItemPtr] = InstanceDataPtr->Result.GetPtrTupleFromStrongExecutionContext<FVector,AActor*, TArray<FVector>, TArray<AActor*>, FMassEnvQueryEntityInfoBlueprintWrapper, TArray<FMassEnvQueryEntityInfoBlueprintWrapper>, FArcMassSmartObjectItem>(StrongContext);

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
					else if (SmartObjectSlotEQSItemPtr)
					{
						UArcEnvQueryItemType_EntitySmartObject* DefTypeOb = QueryResult->ItemType->GetDefaultObject<UArcEnvQueryItemType_EntitySmartObject>();
						check(DefTypeOb != nullptr);
						
						USmartObjectSubsystem* SmartObjectSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
						for (const FEnvQueryItem& Item : QueryResult->Items)
						{
							FArcMassSmartObjectItem SmartObjectItem = DefTypeOb->GetValue(QueryResult->RawData.GetData() + Item.DataOffset);
							*SmartObjectSlotEQSItemPtr = SmartObjectItem;
							//FConstStructView OwnerData = SmartObjectSubsystem->GetOwnerData(SmartObjectItem.SmartObjectHandle);
							//if (OwnerData.GetScriptStruct() == TBaseStructure<FMassEntityHandle>::Get())
							//{
							//	FMassEntityHandle OwnerEntity = OwnerData.Get<FMassEntityHandle>();
							//	FArcMassSmartObjectItem ArcItem(SmartObjectItem.Location, SmartObjectItem.SmartObjectHandle, SmartObjectItem.SlotHandle, OwnerEntity);
							//	*SmartObjectSlotEQSItemPtr = ArcItem;
							//}
							//else
							//{
							//	FArcMassSmartObjectItem ArcItem(SmartObjectItem.Location, SmartObjectItem.SmartObjectHandle, SmartObjectItem.SlotHandle);
							//	*SmartObjectSlotEQSItemPtr = ArcItem;	
							//}
							
							break;
						}

						if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
						{
							ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).EntityResult = *EntityPtr;
						}
					}
					bSuccess = true;
				}

				if (!bSuccess)
				{
					auto [VectorPtr, ActorPtr, ArrayOfVector, ArrayOfActor, EntityPtr, EntityArrayPtr, SmartObjectSlotEQSItemPtr] = InstanceDataPtr->Result.GetPtrTupleFromStrongExecutionContext<FVector,AActor*, TArray<FVector>, TArray<AActor*>, FMassEnvQueryEntityInfoBlueprintWrapper, TArray<FMassEnvQueryEntityInfoBlueprintWrapper>, FArcMassSmartObjectItem>(StrongContext);
					
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
					else if (SmartObjectSlotEQSItemPtr)
					{
						*SmartObjectSlotEQSItemPtr = FArcMassSmartObjectItem();
					}
				}

				if (bFinishOnEnd)
				{
					StrongContext.FinishTask(bSuccess ? EStateTreeFinishTaskType::Succeeded : EStateTreeFinishTaskType::Failed);	
				}
					

				SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
				
			}
		});

	

	if (!bFinishOnEnd)
	{
		auto Func = FQueryFinishedSignature::CreateStatic(&FArcMassStateTreeRunEnvQueryTask::ExecuteQueryCallback,
		Context.MakeWeakExecutionContext(), SignalSubsystem, MassCtx.GetEntity(), ResultStoreSubsystem, bFinishOnEnd, Interval, bSignalOnResultChange);
	
		InstanceData.RequestId = Request.Execute(InstanceData.RunMode, Func);
		return EStateTreeRunStatus::Running;
	}
	else
	{
		InstanceData.RequestId = Request.Execute(InstanceData.RunMode, QueryLambda);
	}
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

void FArcMassStateTreeRunEnvQueryTask::ExecuteQueryCallback(TSharedPtr<FEnvQueryResult> QueryResult, FStateTreeWeakExecutionContext WeakContext, UMassSignalSubsystem* SignalSubsystem
	, FMassEntityHandle Entity, UArcMassEQSResultStore* ResultStoreSubsystem, bool bFinishOnEnd
	, float Interval, bool bInSignalOnResultChange)
{
	const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
	if (FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>())
	{
		InstanceDataPtr->RequestId = INDEX_NONE;

		bool bChanged = false;
		bool bSuccess = false;
		if (QueryResult && QueryResult->IsSuccessful())
		{
			auto [VectorPtr, ActorPtr, ArrayOfVector, ArrayOfActor, EntityPtr, EntityArrayPtr] = InstanceDataPtr->Result.GetPtrTupleFromStrongExecutionContext<FVector,AActor*, TArray<FVector>, TArray<AActor*>, FMassEnvQueryEntityInfoBlueprintWrapper, TArray<FMassEnvQueryEntityInfoBlueprintWrapper>>(StrongContext);

			if (VectorPtr)
			{
				FVector Old = *VectorPtr;
				*VectorPtr = QueryResult->GetItemAsLocation(0);

				bChanged = Old != *VectorPtr;
				
				if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
				{
					ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).VectorResult = *VectorPtr;
				}
			}
			else if (ActorPtr)
			{
				AActor* Old = *ActorPtr;
				*ActorPtr = QueryResult->GetItemAsActor(0);

				bChanged = Old != *ActorPtr;
				if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
				{
					ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).ActorResult = *ActorPtr;
				}
			}
			else if (ArrayOfVector)
			{
				int32 OldSize = ArrayOfVector->Num();
				
				QueryResult->GetAllAsLocations(*ArrayOfVector);

				bChanged = OldSize != ArrayOfVector->Num();
				
				if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
				{
					ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).VectorArrayResult = *ArrayOfVector;
				}
			}
			else if (ArrayOfActor)
			{
				int32 OldSize = ArrayOfActor->Num();
				QueryResult->GetAllAsActors(*ArrayOfActor);

				bChanged = OldSize != ArrayOfActor->Num();
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

					bChanged = EntityInfo.EntityHandle != EntityPtr->GetEntityHandle();
					
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

				int32 OldSize = EntityArrayPtr->Num();

				EntityArrayPtr->Reset(QueryResult->Items.Num());
				for (const FEnvQueryItem& Item : QueryResult->Items)
				{
					FMassEnvQueryEntityInfo EntityInfo = DefTypeOb->GetValue(QueryResult->RawData.GetData() + Item.DataOffset);
					EntityArrayPtr->Add(FMassEnvQueryEntityInfoBlueprintWrapper(EntityInfo));
				}

				bChanged = OldSize != EntityArrayPtr->Num();
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
				FVector Old = *VectorPtr;
				*VectorPtr = FNavigationSystem::InvalidLocation;

				bChanged = Old != *VectorPtr;
				
				if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
				{
					ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).VectorResult = FNavigationSystem::InvalidLocation;
				}
			}
			else if (ActorPtr)
			{
				AActor* Old = *ActorPtr;

				*ActorPtr = nullptr;

				bChanged = Old != *ActorPtr;
				
				if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
				{
					ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).ActorResult = nullptr;
				}
			}
			else if (ArrayOfVector)
			{
				int32 OldSize = ArrayOfVector->Num();
				
				ArrayOfActor->Empty();

				bChanged = OldSize != ArrayOfVector->Num();
				
				if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
				{
					ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).VectorArrayResult.Empty();
				}
			}
			else if (ArrayOfActor)
			{
				int32 OldSize = ArrayOfActor->Num();
				
				ArrayOfVector->Empty();

				bChanged = OldSize != ArrayOfVector->Num();
				if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
				{
					ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).ActorArrayResult.Empty();
				}
			}
			else if (EntityPtr)
			{
				FMassEntityHandle OldEntityHandle = EntityPtr->GetEntityHandle();
				*EntityPtr = FMassEnvQueryEntityInfoBlueprintWrapper();

				bChanged = OldEntityHandle != EntityPtr->GetEntityHandle();
				
				if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
				{
					ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).EntityResult = FMassEnvQueryEntityInfoBlueprintWrapper();
				}
			}
			else if (EntityArrayPtr)
			{
				int32 OldSize = EntityArrayPtr->Num();
				EntityArrayPtr->Empty();

				bChanged = OldSize != EntityArrayPtr->Num();
				if (ResultStoreSubsystem && InstanceDataPtr->bStoreInEQSStore)
                {
                	ResultStoreSubsystem->EQSResults.FindOrAdd(Entity).Results.FindOrAdd(InstanceDataPtr->StoreName).EntityArrayResult.Empty();
                }
			}
		}

		if (bFinishOnEnd)
		{
			StrongContext.FinishTask(bSuccess ? EStateTreeFinishTaskType::Succeeded : EStateTreeFinishTaskType::Failed);		
		}
		else
		{
			if (Interval > 0)
			{
				UWorld* World = SignalSubsystem->GetWorld();
				FTimerManager& TimerManager = World->GetTimerManager();
				FTimerHandle TimerHandle;
              
				// Set timer to rerun the query
				TimerManager.SetTimer(TimerHandle, [WeakContext, SignalSubsystem, Entity, ResultStoreSubsystem, bFinishOnEnd, Interval, bInSignalOnResultChange]()
				{
					// Rerun the EQS query here
					const FStateTreeStrongExecutionContext StrongCtx = WeakContext.MakeStrongExecutionContext();
					{
						if (FInstanceDataType* InstData = StrongCtx.GetInstanceDataPtr<FInstanceDataType>())
						{
							UWorld* LocalWorld = SignalSubsystem->GetWorld();
							UMassEntitySubsystem* MassSubsystem = LocalWorld->GetSubsystem<UMassEntitySubsystem>();
							FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
							
							AActor* Owner = nullptr;
							if (FMassActorFragment* MassActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity))
							{
								Owner = MassActorFragment->GetMutable();
							}
							// Recreate and execute the query
							FEnvQueryRequest NewRequest(InstData->QueryTemplate, Owner, Entity, SignalSubsystem->GetWorld());
							for (FAIDynamicParam& DynamicParam : InstData->QueryConfig)
							{
								NewRequest.SetDynamicParam(DynamicParam, nullptr);
							}
                          
							auto RecursiveCallback = FQueryFinishedSignature::CreateStatic(&FArcMassStateTreeRunEnvQueryTask::ExecuteQueryCallback,
								WeakContext
								, SignalSubsystem
								, Entity
								, ResultStoreSubsystem
								, bFinishOnEnd
								, Interval
								, bInSignalOnResultChange);
							
							InstData->RequestId = NewRequest.Execute(InstData->RunMode, RecursiveCallback);
						}
					}
				}, Interval, false);
			}
		}


		if (bInSignalOnResultChange && bChanged)
		{
			StrongContext.BroadcastDelegate(InstanceDataPtr->OnResultChanged);
			SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});	
		}
		
		
	}
}
