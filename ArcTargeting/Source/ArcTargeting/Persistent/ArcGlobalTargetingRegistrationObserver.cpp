#include "Persistent/ArcGlobalTargetingRegistrationObserver.h"

#include "ArcTargetingSubsystem.h"
#include "Persistent/ArcGlobalTargetingFragments.h"
#include "Mass/EntityFragments.h"
#include "Mass/ExecutionContext.h"

UArcGlobalTargetingAddObserver::UArcGlobalTargetingAddObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcGlobalTargetingRegistrationTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcGlobalTargetingAddObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddConstSharedRequirement<FArcGlobalTargetingConfigFragment>();
	ObserverQuery.AddTagRequirement<FArcGlobalTargetingRegistrationTag>(EMassFragmentPresence::All);
}

void UArcGlobalTargetingAddObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcTargetingSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcTargetingSubsystem>();
	if (Subsystem == nullptr)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem](FMassExecutionContext& ChunkContext)
	{
		const FArcGlobalTargetingConfigFragment& Config = ChunkContext.GetConstSharedFragment<FArcGlobalTargetingConfigFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
			for (const FArcGlobalTargetingConfigEntry& Entry : Config.Entries)
			{
				Subsystem->PushPersistentTargeting(Entity, Entry.Tag, Entry.Preset, Entry.MinInterval);
			}
		}
	});
}

UArcGlobalTargetingRemoveObserver::UArcGlobalTargetingRemoveObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcGlobalTargetingRegistrationTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcGlobalTargetingRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddConstSharedRequirement<FArcGlobalTargetingConfigFragment>();
	ObserverQuery.AddTagRequirement<FArcGlobalTargetingRegistrationTag>(EMassFragmentPresence::All);
}

void UArcGlobalTargetingRemoveObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcTargetingSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcTargetingSubsystem>();
	if (Subsystem == nullptr)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem](FMassExecutionContext& ChunkContext)
	{
		const FArcGlobalTargetingConfigFragment& Config = ChunkContext.GetConstSharedFragment<FArcGlobalTargetingConfigFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
			for (const FArcGlobalTargetingConfigEntry& Entry : Config.Entries)
			{
				Subsystem->RemovePersistentTargeting(Entity, Entry.Tag);
			}
		}
	});
}
