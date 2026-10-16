// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisLifecycleProcessors.h"

#include "ArcMass/Lifecycle/ArcMassLifecycle.h"
#include "ArcMass/Visualization/ArcLifecyclePhaseVisualsFragment.h"
#include "ArcMassEntityVisualization.h"
#include "ArcMassVisualizationConfigFragments.h"
#include "MassCommonFragments.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "Mesh/ArcRenderMeshFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcVisLifecycleProcessors)

// ---------------------------------------------------------------------------
// UArcVisLifecycleVisSwitchProcessor
// ---------------------------------------------------------------------------

UArcVisLifecycleVisSwitchProcessor::UArcVisLifecycleVisSwitchProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bRequiresGameThreadExecution = true;
}

void UArcVisLifecycleVisSwitchProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::LifecyclePhaseChanged);
}

void UArcVisLifecycleVisSwitchProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcLifecycleFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcLifecycleConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassVisualizationMeshConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcOverrideMaterialsFragment>(EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcVisISMInstanceFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcVisComponentTransformFragment>(EMassFragmentPresence::Optional);
	// New: per-phase visuals (optional — entities without vis data just skip the swap)
	EntityQuery.AddConstSharedRequirement<FArcLifecyclePhaseVisualsFragment>(EMassFragmentPresence::Optional);
}

void UArcVisLifecycleVisSwitchProcessor::SignalEntities(FMassEntityManager& EntityManager,
	FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	UArcEntityVisualizationSubsystem* Subsystem = World ? World->GetSubsystem<UArcEntityVisualizationSubsystem>() : nullptr;

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisLifecycleVisSwitch);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Ctx)
	{
		TConstArrayView<FArcLifecycleFragment> LifecycleFragments = Ctx.GetFragmentView<FArcLifecycleFragment>();
		TConstArrayView<FArcVisRepresentationFragment> RepFragments = Ctx.GetFragmentView<FArcVisRepresentationFragment>();
		TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FArcVisISMInstanceFragment> ISMInstanceFragments = Ctx.GetMutableFragmentView<FArcVisISMInstanceFragment>();

		const FArcVisConfigFragment& BaseConfig = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();
		const FArcMassStaticMeshConfigFragment& StaticMeshConfigFrag = Ctx.GetConstSharedFragment<FArcMassStaticMeshConfigFragment>();
		const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag = Ctx.GetConstSharedFragment<FArcMassVisualizationMeshConfigFragment>();
		const FArcOverrideMaterialsFragment* OverrideMats = Ctx.GetConstSharedFragmentPtr<FArcOverrideMaterialsFragment>();
		const FArcVisComponentTransformFragment* CompTransformPtr = Ctx.GetConstSharedFragmentPtr<FArcVisComponentTransformFragment>();

		// Per-phase visuals are optional — if absent, no mesh/material swap is possible
		const FArcLifecyclePhaseVisualsFragment* VisualsConfig = Ctx.GetConstSharedFragmentPtr<FArcLifecyclePhaseVisualsFragment>();
		if (!VisualsConfig)
		{
			return;
		}

		const bool bHasISMInstance = ISMInstanceFragments.Num() > 0;

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FArcLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];
			const FArcVisRepresentationFragment& Rep = RepFragments[EntityIt];

			if (Rep.bHasMeshRendering && bHasISMInstance && Subsystem)
			{
				// Mesh-mode: swap ISM instance to different holder if mesh changed.
				UStaticMesh* NewMesh = VisualsConfig->ResolveMesh(Lifecycle.CurrentPhase, StaticMeshConfigFrag);
				if (NewMesh && NewMesh != StaticMeshConfigFrag.Mesh.Get())
				{
					FArcVisISMInstanceFragment& ISMInstance = ISMInstanceFragments[EntityIt];
					FMassEntityHandle SourceEntity = Ctx.GetEntity(EntityIt);

					// Remove from old holder — SubIndex=0 (lifecycle swap is single-mesh path only).
					if (ISMInstance.HolderEntity.IsValid())
					{
						Subsystem->RemoveISMInstance(
							ISMInstance.HolderEntity, SourceEntity, 0, Rep.MeshGridCoords, EntityManager);
					}

					// Add to new holder with resolved mesh — SubIndex=0, single-mesh path.
					FArcMassStaticMeshConfigFragment ResolvedMeshConfigFrag(NewMesh);
					FMassEntityHandle NewHolder = Subsystem->FindOrCreateISMHolder(
						Rep.MeshGridCoords, ResolvedMeshConfigFrag, VisMeshConfigFrag, OverrideMats, EntityManager);

					const FTransformFragment& TransformFrag = TransformFragments[EntityIt];
					const FTransform InstanceTransform = CompTransformPtr
						? CompTransformPtr->ComponentRelativeTransform * TransformFrag.GetTransform()
						: TransformFrag.GetTransform();
					int32 NewIndex = Subsystem->AddISMInstance(NewHolder, SourceEntity, 0, InstanceTransform, EntityManager);

					// Update cached fragment — authoritative index is in HolderEntityToDenseIndex.
					ISMInstance.HolderEntity = NewHolder;
					ISMInstance.InstanceIndex = NewIndex;
				}
			}
		}
	});
}
