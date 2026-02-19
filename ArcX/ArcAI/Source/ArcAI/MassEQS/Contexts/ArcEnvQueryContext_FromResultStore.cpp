// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEnvQueryContext_FromResultStore.h"

#include "ArcMassEQSResultStore.h"
#include "Engine/World.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "GameFramework/Actor.h"
#include "Items/EnvQueryItemType_MassEntityHandle.h"

void UArcEnvQueryContext_FromResultStore::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	AActor* QueryOwner = Cast<AActor>(QueryInstance.Owner.Get());
	
	UArcMassEQSResultStore* ResultStore = QueryOwner->GetWorld()->GetSubsystem<UArcMassEQSResultStore>();
	
	if (const FArcMassEQSResultTypeMap* ResultMap = ResultStore->EQSResults.Find(QueryInstance.OwningEntity))
	{
		if (const FArcMassEQSResults* Result = ResultMap->Results.Find(ResultName))
		{
			if (Result->EntityResult.GetEntityHandle().IsValid())
			{
				ContextData.ValueType = UEnvQueryItemType_MassEntityHandle::StaticClass();
				ContextData.NumValues = 1;
				ContextData.RawData.SetNumUninitialized(sizeof(FMassEnvQueryEntityInfo));
				UEnvQueryItemType_MassEntityHandle::SetValue(ContextData.RawData.GetData(), Result->EntityResult.GetEntityInfo());
			}
			else if (Result->ActorResult)
			{
				UEnvQueryItemType_Actor::SetContextHelper(ContextData, Result->ActorResult);
			}
			else if (Result->ActorArrayResult.Num() > 0)
			{
				UEnvQueryItemType_Actor::SetContextHelper(ContextData, Result->ActorArrayResult);
			}
			else if (Result->VectorResult != FNavigationSystem::InvalidLocation)
			{
				UEnvQueryItemType_Point::SetContextHelper(ContextData, Result->VectorResult);
			}
			else if (Result->VectorArrayResult.Num() > 0)
			{
				UEnvQueryItemType_Point::SetContextHelper(ContextData, Result->VectorArrayResult);
			}
		}
	}
}

void UArcEnvQueryContext_FromGlobalStore::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	AActor* QueryOwner = Cast<AActor>(QueryInstance.Owner.Get());
	
	UArcMassGlobalDataStore* ResultStore = QueryOwner->GetWorld()->GetSubsystem<UArcMassGlobalDataStore>();
	if (!ResultStore && QueryOwner)
	{
		switch (StoreItemType)
		{
			case EArcStoreItemType::Entity:
			case EArcStoreItemType::Actor:
			{
				UEnvQueryItemType_Actor::SetContextHelper(ContextData, QueryOwner);
				break;
			}
			case  EArcStoreItemType::ActorLocation:
			{	
				UEnvQueryItemType_Point::SetContextHelper(ContextData, QueryOwner->GetActorLocation());
				break;
			}
			case EArcStoreItemType::ActorArray:
			{
				UEnvQueryItemType_Actor::SetContextHelper(ContextData, QueryOwner);
				break;
			}
			case EArcStoreItemType::Location:
			{
				UEnvQueryItemType_Point::SetContextHelper(ContextData, QueryOwner->GetActorLocation());
				break;
			}
			case EArcStoreItemType::LocationArray:
			{
				UEnvQueryItemType_Point::SetContextHelper(ContextData, QueryOwner->GetActorLocation());
				break;
			}
		}
		
		return;
	};
	
	if (const FArcMassDataStoreTypeMap* ResultMap = ResultStore->DataStore.Find(QueryInstance.OwningEntity))
	{
		if (const FArcMassEQSResults* Result = ResultMap->Results.Find(TagKey))
		{
			switch (StoreItemType)
			{
				case EArcStoreItemType::Entity:
				{
					if (Result->EntityResult.GetEntityHandle().IsValid())
					{
						ContextData.ValueType = UEnvQueryItemType_MassEntityHandle::StaticClass();
						ContextData.NumValues = 1;
						ContextData.RawData.SetNumUninitialized(sizeof(FMassEnvQueryEntityInfo));
						UEnvQueryItemType_MassEntityHandle::SetValue(ContextData.RawData.GetData(), Result->EntityResult.GetEntityInfo());
					}
					break;
				}
				case EArcStoreItemType::Actor:
				{
					UEnvQueryItemType_Actor::SetContextHelper(ContextData, Result->ActorResult);
					break;
				}
				case  EArcStoreItemType::ActorLocation:
				{	
					UEnvQueryItemType_Point::SetContextHelper(ContextData, Result->ActorResult->GetActorLocation());
					break;
				}
				case EArcStoreItemType::ActorArray:
				{
					UEnvQueryItemType_Actor::SetContextHelper(ContextData, Result->ActorArrayResult);
					break;
				}
				case EArcStoreItemType::Location:
				{
					UEnvQueryItemType_Point::SetContextHelper(ContextData, Result->VectorResult);
					break;
				}
				case EArcStoreItemType::LocationArray:
				{
					UEnvQueryItemType_Point::SetContextHelper(ContextData, Result->VectorArrayResult);
					break;
				}
			}
			
		}
	}
}
