// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedWorld/Processors/ArcIWMeshActivateProcessor.h"

#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
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
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIWMeshActivate);

	TArray<FArcIWMeshActivationRequest> Requests;
	AArcIWMassISMPartitionActor* PartitionActor = nullptr;

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, &Requests, &PartitionActor](FMassExecutionContext& Ctx)
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

				PartitionActor = MassISMPartition;
				const FTransform& WorldTransform = Transforms[EntityIt].GetTransform();
				const int32 MeshCount = Config.MeshDescriptors.Num();
				Instance.ISMInstanceIds.SetNum(MeshCount);

				for (int32 MeshIdx = 0; MeshIdx < MeshCount; ++MeshIdx)
				{
					Instance.ISMInstanceIds[MeshIdx] = INDEX_NONE;
					const FArcIWMeshEntry& MeshEntry = Config.MeshDescriptors[MeshIdx];
					if (!MeshEntry.Mesh)
					{
						continue;
					}

					int32 HolderSlot = Instance.MeshSlotBase + MeshIdx;
					if (!MassISMPartition->IsValidHolderSlot(HolderSlot))
					{
						continue;
					}

					bool bHasOverrideMats = false;
					for (const TObjectPtr<UMaterialInterface>& Mat : MeshEntry.Materials)
					{
						if (Mat)
						{
							bHasOverrideMats = true;
							break;
						}
					}

					FArcIWMeshActivationRequest Req;
					Req.SourceEntity = Ctx.GetEntity(EntityIt);
					Req.FinalTransform = MeshEntry.RelativeTransform * WorldTransform;
					Req.MeshEntry = MeshEntry;
					Req.MeshIdx = MeshIdx;
					Req.HolderSlot = HolderSlot;
					Req.bHasOverrideMats = bHasOverrideMats;
					Requests.Add(MoveTemp(Req));
				}
			}
		});

	if (PartitionActor && Requests.Num() > 0)
	{
		PartitionActor->ActivateMesh(Requests, EntityManager, Context);
	}
}
