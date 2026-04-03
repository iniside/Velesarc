// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisActorDeactivateProcessor.h"

#include "ArcMassEntityVisualization.h"
#include "ArcMassEntityVisualizationProcessors.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcVisActorDeactivateProcessor)

UArcVisActorDeactivateProcessor::UArcVisActorDeactivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteBefore.Add(UArcVisMeshDeactivateProcessor::StaticClass()->GetFName());
}

void UArcVisActorDeactivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisMeshDeactivated);
}

void UArcVisActorDeactivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcVisActorConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcVisPrePlacedActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void UArcVisActorDeactivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisActorDeactivate);

	EntityQuery.ForEachEntityChunk(Context,
		[](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TArrayView<FMassActorFragment> ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
			TConstArrayView<FArcVisPrePlacedActorFragment> PrePlacedFrags = Ctx.GetFragmentView<FArcVisPrePlacedActorFragment>();
			const bool bHasPrePlaced = PrePlacedFrags.Num() > 0;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];
				if (!Rep.bIsActorRepresentation)
				{
					continue;
				}

				FMassActorFragment& ActorFrag = ActorFrags[EntityIt];
				AActor* Actor = ActorFrag.GetMutable();
				if (Actor)
				{
					bool bIsPrePlaced = false;
					if (bHasPrePlaced)
					{
						const FArcVisPrePlacedActorFragment& PrePlaced = PrePlacedFrags[EntityIt];
						bIsPrePlaced = (PrePlaced.PrePlacedActor.Get() == Actor);
					}

					if (bIsPrePlaced)
					{
						Actor->SetActorHiddenInGame(true);
						Actor->SetActorEnableCollision(false);
					}
					else if (ActorFrag.IsOwnedByMass())
					{
						Actor->Destroy();
					}
					ActorFrag.ResetAndUpdateHandleMap();
				}
				Rep.bIsActorRepresentation = false;
				Rep.bHasMeshRendering = false;
			}
		});
}
