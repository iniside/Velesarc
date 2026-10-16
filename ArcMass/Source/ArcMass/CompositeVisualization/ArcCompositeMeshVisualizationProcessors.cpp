// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCompositeMeshVisualizationProcessors.h"

#include "ArcCompositeMeshVisualizationFragments.h"
#include "MassCommonFragments.h"
#include "Mass/Commands.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "ArcMass/Visualization/ArcMassEntityVisualization.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "Mesh/ArcRenderMeshFragments.h"

// Physics
#include "ArcMass/Physics/ArcMassPhysicsBodyConfig.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "ArcMass/Visualization/ArcVisProcessorUtils.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace ArcCompositeMeshVisProcessors_Private
{
	/** Per-entity data captured by value before the chunk views go stale. */
	struct FPendingCompositeActivation
	{
		FMassEntityHandle SourceEntity;
		FIntVector Cell;
		FTransform EntityWorldTransform;
		TArray<FArcCompositeMeshEntryConfig> Entries;
	};
} // namespace ArcCompositeMeshVisProcessors_Private

// ---------------------------------------------------------------------------
// UArcCompositeMeshVisEntityInitObserver
// ---------------------------------------------------------------------------

UArcCompositeMeshVisEntityInitObserver::UArcCompositeMeshVisEntityInitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcCompositeMeshVisEntityTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcCompositeMeshVisEntityInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddTagRequirement<FArcCompositeMeshVisEntityTag>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcMassPhysicsBodyConfigFragment>(EMassFragmentPresence::Optional);
}

void UArcCompositeMeshVisEntityInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* Subsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcCompositeMeshVisEntityInit);

	TArray<FMassEntityHandle> ActivateEntities;
	TArray<FMassEntityHandle> PhysicsActivateEntities;

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, &ActivateEntities, &PhysicsActivateEntities](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();

			const FArcMassPhysicsBodyConfigFragment* PhysicsConfigPtr = Ctx.GetConstSharedFragmentPtr<FArcMassPhysicsBodyConfigFragment>();
			const bool bHasPhysicsConfig = PhysicsConfigPtr && PhysicsConfigPtr->BodySetup;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];
				const FTransformFragment& TransformFrag = Transforms[EntityIt];
				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				const FVector WorldPos = TransformFrag.GetTransform().GetLocation();
				ArcMass::Visualization::RegisterEntityInGrids(*Subsystem, Entity, WorldPos, Rep, bHasPhysicsConfig);

				ArcMass::Visualization::CollectActivationSignals(*Subsystem, Rep, Entity, bHasPhysicsConfig, ActivateEntities, PhysicsActivateEntities);
			}
		});

	if (SignalSubsystem && ActivateEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::VisMeshActivated, ActivateEntities);
	}

	if (SignalSubsystem && PhysicsActivateEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyRequested, PhysicsActivateEntities);
	}
}

// ---------------------------------------------------------------------------
// UArcCompositeMeshVisActivateProcessor
// ---------------------------------------------------------------------------

UArcCompositeMeshVisActivateProcessor::UArcCompositeMeshVisActivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bRequiresGameThreadExecution = true;
}

void UArcCompositeMeshVisActivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisMeshActivated);
}

void UArcCompositeMeshVisActivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcCompositeMeshVisInstanceLinkFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcCompositeMeshConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcCompositeMeshVisEntityTag>(EMassFragmentPresence::All);
}

void UArcCompositeMeshVisActivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* Subsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcCompositeMeshVisActivate);

	TArray<ArcCompositeMeshVisProcessors_Private::FPendingCompositeActivation> PendingActivations;

	EntityQuery.ForEachEntityChunk(Context,
		[&PendingActivations](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcCompositeMeshConfigFragment& CompositeCfg = Ctx.GetConstSharedFragment<FArcCompositeMeshConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];

				if (Rep.bHasMeshRendering)
				{
					continue;
				}

				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FTransformFragment& TransformFrag = Transforms[EntityIt];

				// Capture all data by value — chunk views are invalid inside the deferred command.
				ArcCompositeMeshVisProcessors_Private::FPendingCompositeActivation Pending;
				Pending.SourceEntity = Entity;
				Pending.Cell = Rep.MeshGridCoords;
				Pending.EntityWorldTransform = TransformFrag.GetTransform();
				Pending.Entries = CompositeCfg.Entries;

				PendingActivations.Add(MoveTemp(Pending));

				Rep.bHasMeshRendering = true;
			}
		});

	if (PendingActivations.IsEmpty())
	{
		return;
	}

	TWeakObjectPtr<UArcEntityVisualizationSubsystem> WeakSubsystem(Subsystem);
	Context.Defer().PushCommand<FMassDeferredCreateCommand>(
		[WeakSubsystem, Activations = MoveTemp(PendingActivations)]
		(FMassEntityManager& EM) mutable
		{
			UArcEntityVisualizationSubsystem* SubsystemPtr = WeakSubsystem.Get();
			if (!SubsystemPtr)
			{
				return;
			}

			for (ArcCompositeMeshVisProcessors_Private::FPendingCompositeActivation& Activation : Activations)
			{
				if (!EM.IsEntityValid(Activation.SourceEntity))
				{
					continue;
				}

				// Guard against same-frame activate->deactivate: if the flag was cleared between the
				// synchronous signal pass and this deferred flush, deinit already ran and will not
				// reclaim any instances we add here -- skip to avoid orphaned ISM instances.
				const FArcVisRepresentationFragment* RepPtr =
					EM.GetFragmentDataPtr<FArcVisRepresentationFragment>(Activation.SourceEntity);
				if (!RepPtr || !RepPtr->bHasMeshRendering)
				{
					continue;
				}

				FArcCompositeMeshVisInstanceLinkFragment* LinkFrag =
					EM.GetFragmentDataPtr<FArcCompositeMeshVisInstanceLinkFragment>(Activation.SourceEntity);
				if (!LinkFrag)
				{
					continue;
				}

				// Ensure the Instances array starts clean for this activation pass.
				LinkFrag->Instances.Reset();
				LinkFrag->Instances.Reserve(Activation.Entries.Num());

				for (int32 EntryIdx = 0; EntryIdx < Activation.Entries.Num(); ++EntryIdx)
				{
					const FArcCompositeMeshEntryConfig& Entry = Activation.Entries[EntryIdx];
					const UStaticMesh* Mesh = Entry.Mesh.Get();
					USkinnedAsset* SkinnedAsset = Entry.SkinnedAsset.Get();
					if (!Mesh && !SkinnedAsset)
					{
						// Keep slot in sync — push an invalid entry so indices stay aligned.
						FArcCompositeMeshSubInstance InvalidSlot;
						LinkFrag->Instances.Add(InvalidSlot);
						continue;
					}

					// Build override materials fragment if the entry has overrides.
					FArcOverrideMaterialsFragment OverrideMats;
					bool bHasOverrides = false;
					if (Entry.MaterialOverrides.Num() > 0)
					{
						OverrideMats.OverrideMaterials = Entry.MaterialOverrides;
						bHasOverrides = true;
					}
					const FArcOverrideMaterialsFragment* OverrideMatsPtr = bHasOverrides ? &OverrideMats : nullptr;

					// Compose instance transform: sub-mesh relative offset applied to entity world transform.
					FTransform InstanceTransform = Entry.RelativeTransform * Activation.EntityWorldTransform;

					FArcCompositeMeshSubInstance SubInstance;

					if (SkinnedAsset)
					{
						FArcMassSkinnedMeshFragment SkinnedFrag;
						SkinnedFrag.SkinnedAsset = SkinnedAsset;
						SkinnedFrag.TransformProvider = Entry.TransformProvider;

						FMassEntityHandle HolderEntity = SubsystemPtr->FindOrCreateSkinnedISMHolder(
							Activation.Cell,
							SkinnedFrag,
							Entry.MeshConfig,
							OverrideMatsPtr,
							EM);

						if (!HolderEntity.IsValid())
						{
							LinkFrag->Instances.Add(SubInstance);
							continue;
						}

						// AnimationIndex 0 matches BatchActivateSkinnedISMEntities. EntryIdx is the SubIndex
						// — uniquely identifies this sub-mesh within the source entity's instances on a
						// given holder, so {SourceEntity, EntryIdx} is the removal key. The returned dense
						// index is informational (removal is key-based, mirroring the static path).
						SubInstance.HolderEntity = HolderEntity;
						SubInstance.InstanceIndex = SubsystemPtr->AddSkinnedISMInstance(
							HolderEntity, InstanceTransform, /*AnimationIndex*/0, EM, Activation.SourceEntity, EntryIdx);
						SubInstance.bIsSkinned = true;
					}
					else
					{
						FArcMassStaticMeshConfigFragment MeshCfg(Mesh);

						FMassEntityHandle HolderEntity = SubsystemPtr->FindOrCreateISMHolder(
							Activation.Cell,
							MeshCfg,
							Entry.MeshConfig,
							OverrideMatsPtr,
							EM);

						if (!HolderEntity.IsValid())
						{
							LinkFrag->Instances.Add(SubInstance);
							continue;
						}

						// EntryIdx is the SubIndex — uniquely identifies this sub-mesh within the source entity's
						// set of instances on a given holder, preventing ghost instances when two sub-meshes
						// resolve to the same holder.
						SubInstance.HolderEntity = HolderEntity;
						SubInstance.InstanceIndex = SubsystemPtr->AddISMInstance(
							HolderEntity, Activation.SourceEntity, EntryIdx, InstanceTransform, EM);
					}

					LinkFrag->Instances.Add(SubInstance);
				}
			}
		});
}

// ---------------------------------------------------------------------------
// UArcCompositeMeshVisDeactivateProcessor
// ---------------------------------------------------------------------------

UArcCompositeMeshVisDeactivateProcessor::UArcCompositeMeshVisDeactivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bRequiresGameThreadExecution = true;
}

void UArcCompositeMeshVisDeactivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisMeshDeactivated);
}

void UArcCompositeMeshVisDeactivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcCompositeMeshVisInstanceLinkFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FArcCompositeMeshVisEntityTag>(EMassFragmentPresence::All);
}

void UArcCompositeMeshVisDeactivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* Subsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcCompositeMeshVisDeactivate);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TArrayView<FArcCompositeMeshVisInstanceLinkFragment> Links = Ctx.GetMutableFragmentView<FArcCompositeMeshVisInstanceLinkFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];

				if (!Rep.bHasMeshRendering)
				{
					continue;
				}

				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				FArcCompositeMeshVisInstanceLinkFragment& Link = Links[EntityIt];

				for (int32 i = 0; i < Link.Instances.Num(); ++i)
				{
					FArcCompositeMeshSubInstance& SubInstance = Link.Instances[i];
					if (SubInstance.HolderEntity.IsValid())
					{
						// Both pools are now keyed by {SourceEntity, SubIndex}: the loop index i IS the
						// per-sub-mesh ordinal (EntryIdx at add time), so {Entity, i} resolves the live
						// dense slot regardless of sibling swap-removes. Wrong API = leaked slot.
						if (SubInstance.bIsSkinned)
						{
							Subsystem->RemoveSkinnedISMInstance(
								SubInstance.HolderEntity, FArcHolderInstanceKey{Entity, i}, Rep.MeshGridCoords, EntityManager);
						}
						else
						{
							Subsystem->RemoveISMInstance(
								SubInstance.HolderEntity, Entity, i, Rep.MeshGridCoords, EntityManager);
						}
					}
				}

				// Reset in place — fragment stays in the archetype; no runtime recomposition.
				Link.Instances.Reset();
				Rep.bHasMeshRendering = false;
			}
		});
}

// ---------------------------------------------------------------------------
// UArcCompositeMeshVisEntityDeinitObserver
// ---------------------------------------------------------------------------

UArcCompositeMeshVisEntityDeinitObserver::UArcCompositeMeshVisEntityDeinitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcCompositeMeshVisEntityTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcCompositeMeshVisEntityDeinitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FArcCompositeMeshVisInstanceLinkFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddTagRequirement<FArcCompositeMeshVisEntityTag>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcMassPhysicsBodyConfigFragment>(EMassFragmentPresence::Optional);
	ObserverQuery.AddRequirement<FArcMassPhysicsBodyFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
}

void UArcCompositeMeshVisEntityDeinitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* Subsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcCompositeMeshVisEntityDeinit);

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TArrayView<FArcCompositeMeshVisInstanceLinkFragment> Links = Ctx.GetMutableFragmentView<FArcCompositeMeshVisInstanceLinkFragment>();

			const FArcMassPhysicsBodyConfigFragment* PhysicsConfigPtr = Ctx.GetConstSharedFragmentPtr<FArcMassPhysicsBodyConfigFragment>();
			const bool bHasPhysicsConfig = PhysicsConfigPtr && PhysicsConfigPtr->BodySetup;
			TArrayView<FArcMassPhysicsBodyFragment> PhysicsBodyFrags = Ctx.GetMutableFragmentView<FArcMassPhysicsBodyFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];
				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				FArcCompositeMeshVisInstanceLinkFragment& Link = Links[EntityIt];

				if (Rep.bHasMeshRendering)
				{
					for (int32 i = 0; i < Link.Instances.Num(); ++i)
					{
						FArcCompositeMeshSubInstance& SubInstance = Link.Instances[i];
						if (SubInstance.HolderEntity.IsValid())
						{
							// Same dispatch as the deactivate processor — see comment there.
							if (SubInstance.bIsSkinned)
							{
								Subsystem->RemoveSkinnedISMInstance(
									SubInstance.HolderEntity, FArcHolderInstanceKey{Entity, i}, Rep.MeshGridCoords, EntityManager);
							}
							else
							{
								Subsystem->RemoveISMInstance(
									SubInstance.HolderEntity, Entity, i, Rep.MeshGridCoords, EntityManager);
							}
						}
					}
					Link.Instances.Reset();
				}

				ArcMass::Visualization::UnregisterEntityFromGrids(*Subsystem, Entity, Rep, bHasPhysicsConfig);

				if (PhysicsBodyFrags.Num() > 0)
				{
					PhysicsBodyFrags[EntityIt].TerminateBody();
				}
			}
		});
}
