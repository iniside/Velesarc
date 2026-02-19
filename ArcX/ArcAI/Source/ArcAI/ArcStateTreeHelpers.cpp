// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcStateTreeHelpers.h"

#include "AsyncMessageBindingComponent.h"
#include "AsyncMessageSystemBase.h"
#include "AsyncMessageWorldSubsystem.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"
#include "Engine/Engine.h"
#include "Perception/ArcAISense_GameplayAbility.h"

void UArcStateTreeHelpers::SendGameplayAbilityEvent(FGameplayTag MessageId, const FGameplayTagContainer& AbilityTags, AActor* AffectedActor, AActor* Instigator)
{
	if (!AffectedActor)
	{
		return;
	}
	
	UWorld* World = AffectedActor->GetWorld();
	if (!World)
	{
		return;
	}
	TSharedPtr<FAsyncMessageSystemBase> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem(World);
	if (!Sys.IsValid())
	{
		return;
	}
	
	IAsyncMessageBindingEndpointInterface* Interface = Cast<IAsyncMessageBindingEndpointInterface>(AffectedActor);
	if (!Interface)
	{
		Interface = AffectedActor->FindComponentByInterface<IAsyncMessageBindingEndpointInterface>();
	}
	if (!Interface)
	{
		return;
	}
	TWeakPtr<FAsyncMessageBindingEndpoint> WeakEndpoint = Interface->GetEndpoint();

	FArcAIGameplayAbilityEvent Event(AbilityTags, AffectedActor, Instigator);
	FInstancedStruct Payload = FInstancedStruct::Make<FArcAIGameplayAbilityEvent>(Event);
	Sys->QueueMessageForBroadcast(MessageId, Payload, WeakEndpoint);
}

void UArcStateTreeHelpers::SendGameplayAbilityEventInRadius(FGameplayTag MessageId, const FVector& Location, float Radius
	, const FGameplayTagContainer& AbilityTags, AActor* Instigator)
{
	if (!Instigator)
	{
		return;
	}
	
	UWorld* World = Instigator->GetWorld();
	if (!World)
	{
		return;
	}
	
	TSharedPtr<FAsyncMessageSystemBase> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem(World);
	if (!Sys.IsValid())
	{
		return;
	}
	
	UArcMassSpatialHashSubsystem* SpatialHash = World->GetSubsystem<UArcMassSpatialHashSubsystem>();
	if (!SpatialHash)
	{
		return;
	}
	
	UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
	const FMassEntityManager& EM = MES->GetEntityManager();
	
	
	TArray<FArcMassEntityInfo> Entities = SpatialHash->QueryEntitiesInRadius(Location, Radius);
	
	for (const FArcMassEntityInfo& Entity : Entities)
	{
		FMassActorFragment* ActorFragment = EM.GetFragmentDataPtr<FMassActorFragment>(Entity.Entity);
		if (ActorFragment && ActorFragment->Get())
		{
			IAsyncMessageBindingEndpointInterface* Interface = Cast<IAsyncMessageBindingEndpointInterface>(ActorFragment->GetMutable());
			if (!Interface)
			{
				Interface = ActorFragment->Get()->FindComponentByInterface<IAsyncMessageBindingEndpointInterface>();
			}
			if (!Interface)
			{
				continue;
			}
			TWeakPtr<FAsyncMessageBindingEndpoint> WeakEndpoint = Interface->GetEndpoint();

			FArcAIGameplayAbilityEvent Event(AbilityTags, ActorFragment->GetMutable(), Instigator);
			FInstancedStruct Payload = FInstancedStruct::Make<FArcAIGameplayAbilityEvent>(Event);
			Sys->QueueMessageForBroadcast(MessageId, Payload, WeakEndpoint);		
		}
	}
	
}
