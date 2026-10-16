// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassVisISMPhysicsBoundsRecalcProcessor.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassVisISMPhysicsBoundsRecalcProcessor)

// ---------------------------------------------------------------------------

UArcMassVisISMPhysicsBoundsRecalcProcessor::UArcMassVisISMPhysicsBoundsRecalcProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = false;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);

	ExecutionOrder.ExecuteAfter.Add(UArcMassPhysicsTransformSyncProcessor::StaticClass()->GetFName());
}

// ---------------------------------------------------------------------------

void UArcMassVisISMPhysicsBoundsRecalcProcessor::InitializeInternal(
	UObject& Owner,
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::PhysicsBodySlept);
}

// ---------------------------------------------------------------------------

void UArcMassVisISMPhysicsBoundsRecalcProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcVisISMInstanceFragment>(EMassFragmentAccess::ReadOnly);
}

// ---------------------------------------------------------------------------

void UArcMassVisISMPhysicsBoundsRecalcProcessor::SignalEntities(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMassVisISMPhysicsBoundsRecalc);

	TSet<FMassEntityHandle> DirtyHolders;

	// Pass 1: gather unique holder entities (multithreaded-safe, no game-thread deps)
	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, &DirtyHolders](FMassExecutionContext& Ctx)
		{
			TConstArrayView<FArcVisISMInstanceFragment> ISMInstances = Ctx.GetFragmentView<FArcVisISMInstanceFragment>();

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

				DirtyHolders.Add(ISMInstance.HolderEntity);
			}
		});

	if (DirtyHolders.Num() == 0)
	{
		return;
	}

	// Pass 2: recalculate tight bounds from final resting transforms
	for (const FMassEntityHandle& Holder : DirtyHolders)
	{
		FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Holder);
		FArcRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FArcRenderISMFragment>(Holder);
		FArcRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FArcRenderPrimitiveFragment>(Holder);

		if (TransformFrag && ISMFrag && PrimFrag)
		{
			PrimFrag->LocalBounds = UE::ArcMassRender::Mesh::CalculateInstancedStaticMeshBounds(
				*TransformFrag, *ISMFrag, UE::ArcMassRender::Mesh::EBoundsType::LocalBounds);
			PrimFrag->WorldBounds = UE::ArcMassRender::Mesh::CalculateInstancedStaticMeshBounds(
				*TransformFrag, *ISMFrag, UE::ArcMassRender::Mesh::EBoundsType::WorldBounds);
		}
	}

	// Pass 3: batch-mark render state dirty on game thread via deferred command
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
