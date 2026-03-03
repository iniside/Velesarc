// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSpawnerDeathCleanupProcessor.h"

#include "ArcEntitySpawnerSubsystem.h"
#include "ArcEntitySpawnerTypes.h"
#include "ArcMass/ArcMassLifecycle.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSpawnerDeathCleanupProcessor)

UArcSpawnerDeathCleanupProcessor::UArcSpawnerDeathCleanupProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server);
}

void UArcSpawnerDeathCleanupProcessor::InitializeInternal(UObject& Owner,
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::LifecycleDead);
}

void UArcSpawnerDeathCleanupProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcSpawnedByFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcMassPersistenceFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcSpawnerDeathCleanupProcessor::SignalEntities(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	FMassSignalNameLookup& EntitySignals)
{
	UArcEntitySpawnerSubsystem* SpawnerSub =
		EntityManager.GetWorld()->GetSubsystem<UArcEntitySpawnerSubsystem>();
	if (!SpawnerSub)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context,
		[SpawnerSub](FMassExecutionContext& Ctx)
		{
			const auto SpawnedByFragments =
				Ctx.GetFragmentView<FArcSpawnedByFragment>();
			const auto PersistFragments =
				Ctx.GetFragmentView<FArcMassPersistenceFragment>();

			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				const FArcSpawnedByFragment& SpawnedBy = SpawnedByFragments[i];
				const FArcMassPersistenceFragment& Persist = PersistFragments[i];

				if (!SpawnedBy.SpawnerGuid.IsValid() || !Persist.PersistenceGuid.IsValid())
				{
					continue;
				}

				SpawnerSub->OnSpawnedEntityDied(
					SpawnedBy.SpawnerGuid,
					Persist.PersistenceGuid,
					Ctx.GetEntity(i));
			}
		});
}
