#include "ArcMassEQSTypes.h"

#include "ArcMassEQSResultStore.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassEQSBlueprintLibrary.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "Items/EnvQueryItemType_MassEntityHandle.h"

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

UEnvQueryContext_MassEQSStoreBase::UEnvQueryContext_MassEQSStoreBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UEnvQueryContext_MassEQSStoreBase::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	if (QueryInstance.OwningEntity.IsValid())
	{
		UMassEntitySubsystem* MassSubsystem = QueryInstance.World->GetSubsystem<UMassEntitySubsystem>();
		UArcMassEQSResultStore* ResultStoreSubsystem = QueryInstance.World->GetSubsystem<UArcMassEQSResultStore>();
		if (!ResultStoreSubsystem)
		{
			if (AActor* A = Cast<AActor>(QueryInstance.Owner))
			{
				UEnvQueryItemType_Actor::SetContextHelper(ContextData, A);	
			}	
		}
		if (FArcMassEQSResultTypeMap* TypeMap = ResultStoreSubsystem->EQSResults.Find(QueryInstance.OwningEntity))
		{
			if (FArcMassEQSResults* Result = TypeMap->Results.Find(ResultName))
			{
				switch (InputType)
				{
					case EArcMassEQSInputType::Actor:
					{
						switch (ResultType)
						{
							case EArcMassEQSResultType::Actor:
							{
								if (Result->ActorResult)
								{
									UEnvQueryItemType_Actor::SetContextHelper(ContextData, Result->ActorResult);	
								}
								break;
							}
							case EArcMassEQSResultType::ActorArray:
								break;
							case EArcMassEQSResultType::Vector:
							{
								if (Result->ActorResult)
								{
									UEnvQueryItemType_Point::SetContextHelper(ContextData, Result->ActorResult->GetActorLocation());	
								}
								else
								{
									UEnvQueryItemType_Point::SetContextHelper(ContextData, FNavigationSystem::InvalidLocation);
								}
								break;
							}
							case EArcMassEQSResultType::VectorArray:
								break;
							case EArcMassEQSResultType::Entity:
								break;
							case EArcMassEQSResultType::EntityArray:
								break;
						}
						break;
					}
					case EArcMassEQSInputType::ActorArray:
						switch (ResultType)
						{
						case EArcMassEQSResultType::Actor:
						{
							if (Result->ActorArrayResult.IsEmpty())
							{
								UEnvQueryItemType_Actor::SetContextHelper(ContextData, nullptr);
							}
							else
							{
								UEnvQueryItemType_Actor::SetContextHelper(ContextData, Result->ActorArrayResult[0]);
							}
							break;
						}
						case EArcMassEQSResultType::ActorArray:
								UEnvQueryItemType_Actor::SetContextHelper(ContextData, Result->ActorResult);
								break;
						case EArcMassEQSResultType::Vector:
						{
							if (Result->ActorArrayResult.IsEmpty())
							{
								UEnvQueryItemType_Point::SetContextHelper(ContextData, FNavigationSystem::InvalidLocation);	
							}
							else
							{
								UEnvQueryItemType_Point::SetContextHelper(ContextData, Result->ActorArrayResult[0]->GetActorLocation());
							}
							break;
						}
						case EArcMassEQSResultType::VectorArray:
								if (Result->ActorArrayResult.IsEmpty())
								{
									UEnvQueryItemType_Point::SetContextHelper(ContextData, FNavigationSystem::InvalidLocation);	
								}
								else
								{
									TArray<FVector> VectorArray;
									VectorArray.Reserve(Result->ActorArrayResult.Num());
									for (AActor* Actor : Result->ActorArrayResult)
									{
										VectorArray.Add(Actor->GetActorLocation());
									}
									UEnvQueryItemType_Point::SetContextHelper(ContextData, VectorArray);
								}
								break;
						case EArcMassEQSResultType::Entity:
								break;
						case EArcMassEQSResultType::EntityArray:
								break;
						}
						break;
					case EArcMassEQSInputType::Vector:
						switch (ResultType)
						{
						case EArcMassEQSResultType::Actor:
								break;
						case EArcMassEQSResultType::ActorArray:
								break;
						case EArcMassEQSResultType::Vector:
								UEnvQueryItemType_Point::SetContextHelper(ContextData, Result->VectorResult);
								break;
						case EArcMassEQSResultType::VectorArray:
								UEnvQueryItemType_Point::SetContextHelper(ContextData, Result->VectorResult);
								break;
						case EArcMassEQSResultType::Entity:
								break;
						case EArcMassEQSResultType::EntityArray:
								break;
						}
						break;
					case EArcMassEQSInputType::VectorArray:
						switch (ResultType)
						{
						case EArcMassEQSResultType::Actor:
								break;
						case EArcMassEQSResultType::ActorArray:
								break;
						case EArcMassEQSResultType::Vector:
								if (Result->VectorArrayResult.IsEmpty())
								{
									UEnvQueryItemType_Point::SetContextHelper(ContextData, FNavigationSystem::InvalidLocation);	
								}
								else
								{
									UEnvQueryItemType_Point::SetContextHelper(ContextData, Result->VectorArrayResult[0]);
								}
								break;
						case EArcMassEQSResultType::VectorArray:
								UEnvQueryItemType_Point::SetContextHelper(ContextData, Result->VectorArrayResult);
								break;
						case EArcMassEQSResultType::Entity:
								break;
						case EArcMassEQSResultType::EntityArray:
								break;
						}
						break;
					case EArcMassEQSInputType::Entity:
						switch (ResultType)
						{
						case EArcMassEQSResultType::Actor:
								break;
						case EArcMassEQSResultType::ActorArray:
								break;
						case EArcMassEQSResultType::Vector:
								UEnvQueryItemType_Point::SetContextHelper(ContextData, Result->EntityResult.GetCachedEntityPosition());
								break;
						case EArcMassEQSResultType::VectorArray:
								UEnvQueryItemType_Point::SetContextHelper(ContextData, Result->EntityResult.GetCachedEntityPosition());
								break;
						case EArcMassEQSResultType::Entity:
								ContextData.ValueType = UEnvQueryItemType_MassEntityHandle::StaticClass();
								ContextData.NumValues = 1;
								ContextData.RawData.SetNumUninitialized(sizeof(FMassEnvQueryEntityInfo));
								UEnvQueryItemType_MassEntityHandle::SetValue(ContextData.RawData.GetData(), Result->EntityResult.GetEntityInfo());
								break;
						case EArcMassEQSResultType::EntityArray:
								ContextData.ValueType = UEnvQueryItemType_MassEntityHandle::StaticClass();
								ContextData.NumValues = 1;
								ContextData.RawData.SetNumUninitialized(sizeof(FMassEnvQueryEntityInfo));
								UEnvQueryItemType_MassEntityHandle::SetValue(ContextData.RawData.GetData(), Result->EntityResult.GetEntityInfo());
								break;
						}
						break;
					case EArcMassEQSInputType::EntityArray:
					{
						switch (ResultType)
						{
							case EArcMassEQSResultType::Actor:
								break;
							case EArcMassEQSResultType::ActorArray:
								break;
							case EArcMassEQSResultType::Vector:
								if (Result->EntityArrayResult.IsEmpty())
								{
									UEnvQueryItemType_Point::SetContextHelper(ContextData, FNavigationSystem::InvalidLocation);	
								}
								else
								{
									UEnvQueryItemType_Point::SetContextHelper(ContextData, Result->EntityArrayResult[0].GetCachedEntityPosition());	
								}
								
								break;
							case EArcMassEQSResultType::VectorArray:
							{
								TArray<FVector> VectorArray;
								VectorArray.Reserve(Result->EntityArrayResult.Num());
								for (const FMassEnvQueryEntityInfoBlueprintWrapper& EntityInfo : Result->EntityArrayResult)
								{
									VectorArray.Add(EntityInfo.GetCachedEntityPosition());
								}
								UEnvQueryItemType_Point::SetContextHelper(ContextData, VectorArray);
								break;
							}
							case EArcMassEQSResultType::Entity:
							{
								if (!Result->EntityArrayResult.IsEmpty())
								{
									ContextData.ValueType = UEnvQueryItemType_MassEntityHandle::StaticClass();
									ContextData.NumValues = 1;
									ContextData.RawData.SetNumUninitialized(sizeof(FMassEnvQueryEntityInfo));
									UEnvQueryItemType_MassEntityHandle::SetValue(ContextData.RawData.GetData(), Result->EntityArrayResult[0].GetEntityInfo());	
								}
								
								break;
							}
							case EArcMassEQSResultType::EntityArray:
							{
								ContextData.ValueType = UEnvQueryItemType_MassEntityHandle::StaticClass();
								ContextData.NumValues = 1;
								ContextData.RawData.SetNumUninitialized(sizeof(FMassEnvQueryEntityInfo) * Result->EntityArrayResult.Num());

								uint8* RawData = (uint8*)ContextData.RawData.GetData();
								for (int32 i = 0; i < Result->EntityArrayResult.Num(); ++i)
								{
									UEnvQueryItemType_MassEntityHandle::SetValue(RawData, Result->EntityArrayResult[i].GetEntityInfo());
									RawData += sizeof(FMassEnvQueryEntityInfo);
								}
								break;
							}
						}
						break;
					}
				}	
			}
		}
	}
	else
	{
		if (AActor* A = Cast<AActor>(QueryInstance.Owner))
		{
			UEnvQueryItemType_Actor::SetContextHelper(ContextData, A);	
		}
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

