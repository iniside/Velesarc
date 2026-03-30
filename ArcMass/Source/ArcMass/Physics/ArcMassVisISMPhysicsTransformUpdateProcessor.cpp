// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassVisISMPhysicsTransformUpdateProcessor.h"
#include "ArcMassPhysicsSimulation.h"
#include "ArcMassPhysicsTransformSyncProcessor.h"
#include "ArcMass/Visualization/ArcMassEntityVisualization.h"
#include "MassCommonFragments.h"
#include "MassCommands.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mesh/MassEngineMeshFragments.h"
#include "Mesh/MassEngineMeshUtils.h"
#include "MassRenderStateHelper.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassVisISMPhysicsTransformUpdateProcessor)

// ---------------------------------------------------------------------------

UArcMassVisISMPhysicsTransformUpdateProcessor::UArcMassVisISMPhysicsTransformUpdateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = false;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);

	ExecutionOrder.ExecuteAfter.Add(UArcMassPhysicsTransformSyncProcessor::StaticClass()->GetFName());
}

// ---------------------------------------------------------------------------

void UArcMassVisISMPhysicsTransformUpdateProcessor::InitializeInternal(
	UObject& Owner,
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::PhysicsTransformUpdated);
}

// ---------------------------------------------------------------------------

void UArcMassVisISMPhysicsTransformUpdateProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcVisISMInstanceFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcVisComponentTransformFragment>(EMassFragmentPresence::Optional);
}

// ---------------------------------------------------------------------------

void UArcMassVisISMPhysicsTransformUpdateProcessor::SignalEntities(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMassVisISMPhysicsTransformUpdate);

	struct FPendingISMTransformUpdate
	{
		FMassRenderISMFragment* ISMFrag;
		int32 InstanceIndex;
		FMatrix TransformMatrix;
	};

	TArray<FPendingISMTransformUpdate> PendingUpdates;
	PendingUpdates.Reserve(1024);
	
	TSet<FMassEntityHandle> DirtyHolders;

	// Pass 1: gather resolved pointers and pre-computed matrices
	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, &PendingUpdates, &DirtyHolders](FMassExecutionContext& Ctx)
		{
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TConstArrayView<FArcVisISMInstanceFragment> ISMInstances = Ctx.GetFragmentView<FArcVisISMInstanceFragment>();
			const FArcVisComponentTransformFragment* CompTransform =
				Ctx.GetConstSharedFragmentPtr<FArcVisComponentTransformFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FArcVisISMInstanceFragment& ISMInstance = ISMInstances[EntityIt];
				if (ISMInstance.InstanceIndex == INDEX_NONE || !ISMInstance.HolderEntity.IsValid())
				{
					continue;
				}

				if (!EntityManager.IsEntityValid(ISMInstance.HolderEntity))
				{
					continue;
				}

				FMassRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FMassRenderISMFragment>(
					ISMInstance.HolderEntity);

				if (!ISMFrag || !ISMFrag->PerInstanceSMData.IsValidIndex(ISMInstance.InstanceIndex))
				{
					continue;
				}

				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
				FTransform InstanceTransform = CompTransform
					? CompTransform->ComponentRelativeTransform * EntityTransform
					: EntityTransform;

				PendingUpdates.Add({ ISMFrag, ISMInstance.InstanceIndex, InstanceTransform.ToMatrixWithScale() });
				DirtyHolders.Add(ISMInstance.HolderEntity);
			}
		});

	if (PendingUpdates.Num() == 0)
	{
		return;
	}

	// Sort by ISMFrag pointer for cache-contiguous writes to the same holder
	PendingUpdates.Sort([](const FPendingISMTransformUpdate& A, const FPendingISMTransformUpdate& B)
	{
		return A.ISMFrag < B.ISMFrag;
	});

	// Pass 2: apply transforms — tight loop, zero fragment lookups
	for (const FPendingISMTransformUpdate& Update : PendingUpdates)
	{
		Update.ISMFrag->PerInstanceSMData[Update.InstanceIndex].Transform = Update.TransformMatrix;
	}

	// Pass 3: recalculate bounds for dirty holders
	for (const FMassEntityHandle& Holder : DirtyHolders)
	{
		FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Holder);
		FMassRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FMassRenderISMFragment>(Holder);
		FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(Holder);

		if (TransformFrag && ISMFrag && PrimFrag)
		{
			PrimFrag->LocalBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
				*TransformFrag, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::LocalBounds);
			PrimFrag->WorldBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
				*TransformFrag, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::WorldBounds);
		}
	}

	// Pass 4: batch-mark render state dirty on game thread
	TArray<FMassEntityHandle> HoldersToMarkDirty = DirtyHolders.Array();

	EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::None>>(
		[Holders = MoveTemp(HoldersToMarkDirty)](FMassEntityManager& Mgr)
		{
			for (const FMassEntityHandle& Holder : Holders)
			{
				FMassRenderStateFragment* RenderState = Mgr.GetFragmentDataPtr<FMassRenderStateFragment>(Holder);
				if (RenderState)
				{
					RenderState->GetRenderStateHelper().MarkRenderStateDirty();
				}
			}
		});
}
