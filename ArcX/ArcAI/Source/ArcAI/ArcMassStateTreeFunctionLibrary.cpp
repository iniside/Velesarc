#include "ArcMassStateTreeFunctionLibrary.h"

#include "MassAgentComponent.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeFragments.h"
#include "MassStateTreeSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void UArcMassStateTreeFunctionLibrary::SendStateTreeEvent(UObject* WorldContext, const FStateTreeEvent& Event
														  , const TArray<FMassEntityHandle>& EntityHandles)
{
	UWorld* World = Cast<UWorld>(WorldContext);
	if (!World)
	{
		World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
	}

	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* MassEntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& EntityManager = MassEntitySubsystem->GetMutableEntityManager();
	UMassStateTreeSubsystem* MassStateTreeSubsystem = World->GetSubsystem<UMassStateTreeSubsystem>();
	UMassSignalSubsystem* MassSignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();

	TArray<FMassEntityHandle> EntitiesToSignal;
	EntitiesToSignal.Reserve(EntityHandles.Num());
	for (const FMassEntityHandle& EntityHandle : EntityHandles)
	{
		const FMassStateTreeSharedFragment* StateTreeShared = EntityManager.GetConstSharedFragmentDataPtr<FMassStateTreeSharedFragment>(EntityHandle);
		FMassStateTreeInstanceFragment* Instance = EntityManager.GetFragmentDataPtr<FMassStateTreeInstanceFragment>(EntityHandle);
		if (!StateTreeShared || !Instance)
		{
			continue;
		}

		FStateTreeInstanceData* STData = MassStateTreeSubsystem->GetInstanceData(Instance->InstanceHandle);
		FStateTreeMinimalExecutionContext Context(MassStateTreeSubsystem, StateTreeShared->StateTree, *STData);
		
		Context.SendEvent(Event.Tag, Event.Payload, Event.Origin);
		EntitiesToSignal.Add(EntityHandle);
		
	}

	MassSignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, EntitiesToSignal);
}

void UArcMassStateTreeFunctionLibrary::SendStateTreeEventToActorEntities(UObject* WorldContext, const FStateTreeEvent& Event
	, const TArray<AActor*>& Actors)
{
	TArray<FMassEntityHandle> EntityHandles;
	for (AActor* Actor : Actors)
	{
		if (UMassAgentComponent* MassAgent = Actor->FindComponentByClass<UMassAgentComponent>())
		{
			EntityHandles.Add(MassAgent->GetEntityHandle());
		}
	}

	SendStateTreeEvent(WorldContext, Event, EntityHandles);
}

