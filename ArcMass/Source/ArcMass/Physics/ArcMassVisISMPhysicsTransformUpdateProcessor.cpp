// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassVisISMPhysicsTransformUpdateProcessor.h"
#include "ArcMassPhysicsSimulation.h"
#include "ArcMassPhysicsTransformSyncProcessor.h"
#include "ArcMass/Visualization/ArcMassEntityVisualization.h"
#include "MassCommonFragments.h"
#include "Mass/Commands.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "Mesh/ArcRenderMeshFragments.h"
#include "Mesh/ArcRenderMeshUtils.h"
#include "Mesh/ArcRenderStateHelper.h"

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
		FArcRenderISMFragment* ISMFrag;
		FMassEntityHandle HolderEntity;
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

				FArcRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FArcRenderISMFragment>(
					ISMInstance.HolderEntity);

				if (!ISMFrag || !ISMFrag->PerInstanceSMData.IsValidIndex(ISMInstance.InstanceIndex))
				{
					continue;
				}

				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
				FTransform InstanceTransform = CompTransform
					? CompTransform->ComponentRelativeTransform * EntityTransform
					: EntityTransform;

				PendingUpdates.Add({ ISMFrag, ISMInstance.HolderEntity, ISMInstance.InstanceIndex, InstanceTransform.ToMatrixWithScale() });
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

	// Pass 3: expand-only bounds update — O(N_changed) instead of 2xO(N_instances).
	// Walk ISMFrag-sorted PendingUpdates in contiguous runs of the same holder/ISMFrag.
	// For each run, union only the changed instances' boxes into the holder's existing
	// bounds. Bounds grow monotonically while physics bodies are awake; tight recompute
	// happens in UArcMassVisISMPhysicsBoundsRecalcProcessor when PhysicsBodySlept fires.
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArcISMExpandOnlyBounds);

		int32 RunStart = 0;
		const int32 TotalUpdates = PendingUpdates.Num();
		while (RunStart < TotalUpdates)
		{
			// Find the end of this run (all entries with the same ISMFrag pointer)
			int32 RunEnd = RunStart + 1;
			while (RunEnd < TotalUpdates && PendingUpdates[RunEnd].ISMFrag == PendingUpdates[RunStart].ISMFrag)
			{
				++RunEnd;
			}

			const FMassEntityHandle& HolderEntity = PendingUpdates[RunStart].HolderEntity;
			if (!EntityManager.IsEntityValid(HolderEntity))
			{
				RunStart = RunEnd;
				continue;
			}

			FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(HolderEntity);
			FArcRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FArcRenderISMFragment>(HolderEntity);
			FArcRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FArcRenderPrimitiveFragment>(HolderEntity);

			if (!TransformFrag || !ISMFrag || !PrimFrag)
			{
				RunStart = RunEnd;
				continue;
			}

			UStaticMesh* StaticMesh = ISMFrag->GetStaticMesh();

			if (!StaticMesh)
			{
				RunStart = RunEnd;
				continue;
			}

			const FBox InstanceBounds = StaticMesh->GetBounds().GetBox();
			if (!InstanceBounds.IsValid)
			{
				RunStart = RunEnd;
				continue;
			}

			const FMatrix ComponentMatrix = TransformFrag->GetTransform().ToMatrixWithScale();

			// Seed builders with existing bounds so we only expand, never shrink
			FBoxSphereBounds::Builder LocalB;
			LocalB += PrimFrag->LocalBounds;
			FBoxSphereBounds::Builder WorldB;
			WorldB += PrimFrag->WorldBounds;

			for (int32 i = RunStart; i < RunEnd; ++i)
			{
				const FPendingISMTransformUpdate& Update = PendingUpdates[i];
				LocalB += InstanceBounds.TransformBy(Update.TransformMatrix);
				WorldB += InstanceBounds.TransformBy(Update.TransformMatrix * ComponentMatrix);
			}

			PrimFrag->LocalBounds = LocalB;
			PrimFrag->WorldBounds = WorldB;

			RunStart = RunEnd;
		}
	}

	// Pass 4: batch-mark render state dirty on game thread
	TArray<FMassEntityHandle> HoldersToMarkDirty = DirtyHolders.Array();

	EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::None>>(
		[Holders = MoveTemp(HoldersToMarkDirty)](FMassEntityManager& Mgr)
		{
			for (const FMassEntityHandle& Holder : Holders)
			{
				FArcRenderStateFragment* RenderState = Mgr.GetFragmentDataPtr<FArcRenderStateFragment>(Holder);
				if (RenderState)
				{
					RenderState->GetRenderStateHelper().MarkRenderStateDirty();
				}
			}
		});
}
