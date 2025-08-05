// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCoreInstancedActorsSubsystem.h"

#include "InstancedActorsVisualizationProcessor.h"
#include "MassRepresentationFragments.h"
#include "Algo/NoneOf.h"
#include "EngineUtils.h"
#include "UObject/UObjectBase.h"

AArcCoreInstancedActorsManager::AArcCoreInstancedActorsManager()
{
	InstancedActorsDataClass = UArcCoreInstancedActorsData::StaticClass();
}

UInstancedActorsData* AArcCoreInstancedActorsManager::CreateNextInstanceActorData(TSubclassOf<AActor> ActorClass
	, const FInstancedActorsTagSet& AdditionalInstanceTags)
{
	//UArcCoreInstancedActorsSubsystem* Subsystem = GetWorld()->GetSubsystem<UArcCoreInstancedActorsSubsystem>();
	UArcCoreInstancedActorsSettings* Settings = GetMutableDefault<UArcCoreInstancedActorsSettings>();
	if (Settings)
	{
		if (TSubclassOf<UInstancedActorsData>* IADClass = Settings->ActorToDataClassMap.Find(ActorClass))
		{
			TSubclassOf<UInstancedActorsData> C = *IADClass;
			UClass* DD = nullptr;
			
			const FString InstanceDataNameStr = FString::Printf(TEXT("%s_%s"), *C->GetFName().ToString(), *ActorClass->GetFName().ToString());
			const FName UniqueName = MakeUniqueObjectName(this, C, FName(InstanceDataNameStr));

			UInstancedActorsData* NewInstanceData = NewObject<UInstancedActorsData>(this, C, UniqueName);

			// @todo it's conceivable the NextInstanceDataID will overflow. We need to use some handle system in place instead. 
			NewInstanceData->ID = NextInstanceDataID++;
			NewInstanceData->ActorClass = ActorClass;
			//NewInstanceData->AdditionalTags = AdditionalInstanceTags;

			check(Algo::NoneOf(PerActorClassInstanceData, [NewInstanceData](UInstancedActorsData* InstanceData)
				{
					return InstanceData->ID == NewInstanceData->ID;
				}));

			return NewInstanceData;	
		}
		
	}
	
	return Super::CreateNextInstanceActorData(ActorClass, AdditionalInstanceTags);
}

AActor* AArcCoreInstancedActorsManager::ArcFindActor(const FActorInstanceHandle& Handle)
{
	return FindActor(Handle);
}

void UArcCoreInstancedActorsData::ApplyInstanceDelta(FMassEntityManager& EntityManager, const FInstancedActorsDelta& InstanceDelta
													 , TArray<FInstancedActorsInstanceIndex>& OutEntitiesToRemove)
{
	Super::ApplyInstanceDelta(EntityManager, InstanceDelta, OutEntitiesToRemove);

	FMassEntityHandle Handle = GetEntityHandleForIndex(InstanceDelta.GetInstanceIndex());
	if (InstanceDelta.GetCurrentLifecyclePhaseIndex() == 5)
	{
	}
}

UArcCoreInstancedActorsSubsystem::UArcCoreInstancedActorsSubsystem()
{
	InstancedActorsManagerClass = AArcCoreInstancedActorsManager::StaticClass();
}

void UArcGameFeatureAction_RegisterActorToInstanceData::OnGameFeatureRegistering()
{
	for (const TPair<TSubclassOf<AActor>, TSubclassOf<UInstancedActorsData>>& Pair : ActorToDataClassMap)
	{
		GetMutableDefault<UArcCoreInstancedActorsSettings>()->ActorToDataClassMap.FindOrAdd(Pair.Key) = Pair.Value;	
	}
	
}

void UArcGameFeatureAction_RegisterActorToInstanceData::OnGameFeatureUnregistering()
{
	for (const TPair<TSubclassOf<AActor>, TSubclassOf<UInstancedActorsData>>& Pair : ActorToDataClassMap)
	{
		GetMutableDefault<UArcCoreInstancedActorsSettings>()->ActorToDataClassMap.Remove(Pair.Key);	
	}
}
