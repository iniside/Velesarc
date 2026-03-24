// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWMeshActivateProcessor.h"

#include "ArcIWTypes.h"
#include "ArcIWMassISMPartitionActor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWMeshActivateProcessor)

// ---------------------------------------------------------------------------
// UArcIWMeshActivateProcessor
// ---------------------------------------------------------------------------

UArcIWMeshActivateProcessor::UArcIWMeshActivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcIWMeshActivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcIW::Signals::MeshCellActivated);
}

void UArcIWMeshActivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcIWEntityTag>(EMassFragmentPresence::All);
}

void UArcIWMeshActivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];

				if (Instance.bIsActorRepresentation)
				{
					continue;
				}

				// Skip if already has ISM instances (init observer may have added them in the same frame).
				bool bAlreadyInstanced = false;
				for (int32 Id : Instance.ISMInstanceIds)
				{
					if (Id != INDEX_NONE)
					{
						bAlreadyInstanced = true;
						break;
					}
				}
				if (bAlreadyInstanced)
				{
					continue;
				}

				AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
				if (!MassISMPartition)
				{
					continue;
				}

				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
				MassISMPartition->ActivateMesh(
					Ctx.GetEntity(EntityIt), Instance, Config, EntityTransform, EntityManager, Ctx);
			}
		});
}
