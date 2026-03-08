// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcProjectileVisualization.h"

#include "ArcProjectileFragments.h"

#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcProjectileVisualization)

// ---------------------------------------------------------------------------
// Init Observer
// ---------------------------------------------------------------------------

UArcProjectileActorInitObserver::UArcProjectileActorInitObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcProjectileTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcProjectileActorInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcProjectileConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcProjectileTag>(EMassFragmentPresence::All);
}

void UArcProjectileActorInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context,
		[World](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const FArcProjectileConfigFragment& Config = Ctx.GetConstSharedFragment<FArcProjectileConfigFragment>();

		if (!Config.ActorClass)
		{
			return;
		}

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AActor* NewActor = World->SpawnActor<AActor>(Config.ActorClass, EntityTransform, SpawnParams);
			if (NewActor)
			{
				ActorFragments[EntityIt].SetAndUpdateHandleMap(Entity, NewActor, true);

				if (NewActor->Implements<UArcProjectileActorInterface>())
				{
					IArcProjectileActorInterface::Execute_OnProjectileEntityCreated(NewActor, Entity);
				}
			}
		}
	});
}

// ---------------------------------------------------------------------------
// Deinit Observer
// ---------------------------------------------------------------------------

UArcProjectileActorDeinitObserver::UArcProjectileActorDeinitObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcProjectileTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcProjectileActorDeinitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddTagRequirement<FArcProjectileTag>(EMassFragmentPresence::All);
}

void UArcProjectileActorDeinitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ObserverQuery.ForEachEntityChunk(Context,
		[](FMassExecutionContext& Ctx)
	{
		TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FMassActorFragment& ActorFragment = ActorFragments[EntityIt];
			if (AActor* Actor = ActorFragment.GetMutable())
			{
				Actor->Destroy();
			}
			ActorFragment.ResetAndUpdateHandleMap();
		}
	});
}
