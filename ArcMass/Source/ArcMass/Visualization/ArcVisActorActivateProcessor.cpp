// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisActorActivateProcessor.h"

#include "ArcMassEntityVisualization.h"
#include "ArcMassEntityVisualizationProcessors.h"
#include "ArcVisEntityComponent.h"
#include "ArcVisSettings.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcVisActorActivateProcessor)

UArcVisActorActivateProcessor::UArcVisActorActivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteBefore.Add(UArcVisMeshActivateProcessor::StaticClass()->GetFName());
}

void UArcVisActorActivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisMeshActivated);
}

void UArcVisActorActivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcVisActorConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcVisPrePlacedActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void UArcVisActorActivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	const bool bEnableActorSwapping = GetDefault<UArcVisSettings>()->bEnableActorSwapping;
	if (!bEnableActorSwapping)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisActorActivate);

	EntityQuery.ForEachEntityChunk(Context,
		[World](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FMassActorFragment> ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const FArcVisActorConfigFragment& ActorConfig = Ctx.GetConstSharedFragment<FArcVisActorConfigFragment>();
			TConstArrayView<FArcVisPrePlacedActorFragment> PrePlacedFrags = Ctx.GetFragmentView<FArcVisPrePlacedActorFragment>();

			if (!ActorConfig.ActorClass)
			{
				return;
			}

			const bool bHasPrePlaced = PrePlacedFrags.Num() > 0;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];
				if (Rep.bHasMeshRendering)
				{
					continue;
				}

				const FTransformFragment& TransformFrag = Transforms[EntityIt];
				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				if (bHasPrePlaced)
				{
					const FArcVisPrePlacedActorFragment& PrePlaced = PrePlacedFrags[EntityIt];
					AActor* PrePlacedActor = PrePlaced.PrePlacedActor.Get();
					if (PrePlacedActor)
					{
						PrePlacedActor->SetActorHiddenInGame(false);
						PrePlacedActor->SetActorEnableCollision(true);
						FMassActorFragment& ActorFrag = ActorFrags[EntityIt];
						ActorFrag.SetAndUpdateHandleMap(Entity, PrePlacedActor, /*bIsOwnedByMass=*/false);
						Rep.bIsActorRepresentation = true;
						Rep.bHasMeshRendering = true;

						UArcVisEntityComponent* VisComp = PrePlacedActor->FindComponentByClass<UArcVisEntityComponent>();
						if (VisComp)
						{
							VisComp->NotifyVisActorCreated(Entity);
						}
						continue;
					}
				}

				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AActor* SpawnedActor = World->SpawnActor<AActor>(ActorConfig.ActorClass, TransformFrag.GetTransform(), SpawnParams);
				if (SpawnedActor)
				{
					FMassActorFragment& ActorFrag = ActorFrags[EntityIt];
					ActorFrag.SetAndUpdateHandleMap(Entity, SpawnedActor, /*bIsOwnedByMass=*/true);
					Rep.bIsActorRepresentation = true;
					Rep.bHasMeshRendering = true;

					UArcVisEntityComponent* VisComp = SpawnedActor->FindComponentByClass<UArcVisEntityComponent>();
					if (VisComp)
					{
						VisComp->NotifyVisActorCreated(Entity);
					}
				}
			}
		});
}
