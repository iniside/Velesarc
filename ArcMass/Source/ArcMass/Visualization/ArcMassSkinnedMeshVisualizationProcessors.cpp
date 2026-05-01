// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassSkinnedMeshVisualizationProcessors.h"

#include "ArcMassEntityVisualization.h"
#include "ArcVisProcessorUtils.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "ArcMassVisualizationConfigFragments.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyConfig.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "Mesh/MassEngineMeshFragments.h"

// ---------------------------------------------------------------------------
// UArcVisSkinnedMeshEntityInitObserver
// ---------------------------------------------------------------------------

UArcVisSkinnedMeshEntityInitObserver::UArcVisSkinnedMeshEntityInitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcVisEntityTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcVisSkinnedMeshEntityInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcMassSkinnedMeshFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcMassVisualizationMeshConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FMassOverrideMaterialsFragment>(EMassFragmentPresence::Optional);
	ObserverQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcMassPhysicsBodyConfigFragment>(EMassFragmentPresence::Optional);
}

void UArcVisSkinnedMeshEntityInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisSkinnedMeshEntityInit);

	TArray<FMassEntityHandle> ActivateEntities;
	TArray<FMassEntityHandle> PhysicsActivateEntities;

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, World, &ActivateEntities, &PhysicsActivateEntities](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();

			const FArcVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();

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
// UArcVisSkinnedMeshActivateProcessor
// ---------------------------------------------------------------------------

UArcVisSkinnedMeshActivateProcessor::UArcVisSkinnedMeshActivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcVisSkinnedMeshActivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisMeshActivated);
}

void UArcVisSkinnedMeshActivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassSkinnedMeshFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassVisualizationMeshConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMassOverrideMaterialsFragment>(EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcVisISMInstanceFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::None);
	EntityQuery.AddConstSharedRequirement<FArcVisComponentTransformFragment>(EMassFragmentPresence::Optional);
}

void UArcVisSkinnedMeshActivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisSkinnedMeshActivate);

	TArray<FArcPendingSkinnedISMActivation> PendingActivations;

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, World, &PendingActivations](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();

			const FArcVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();
			const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = Ctx.GetConstSharedFragment<FArcMassSkinnedMeshFragment>();
			const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag = Ctx.GetConstSharedFragment<FArcMassVisualizationMeshConfigFragment>();
			const FMassOverrideMaterialsFragment* OverrideMatsPtr = Ctx.GetConstSharedFragmentPtr<FMassOverrideMaterialsFragment>();

			const FArcVisComponentTransformFragment* CompTransformPtr = Ctx.GetConstSharedFragmentPtr<FArcVisComponentTransformFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];

				if (Rep.bHasMeshRendering)
				{
					continue;
				}

				const FTransformFragment& TransformFrag = Transforms[EntityIt];
				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				FArcPendingSkinnedISMActivation Activation;
				Activation.SourceEntity = Entity;
				Activation.Cell = Rep.MeshGridCoords;
				Activation.WorldTransform = CompTransformPtr
					? CompTransformPtr->ComponentRelativeTransform * TransformFrag.GetTransform()
					: TransformFrag.GetTransform();
				Activation.SkinnedMeshFrag = SkinnedMeshFrag;
				Activation.VisMeshConfigFrag = VisMeshConfigFrag;
				if (OverrideMatsPtr)
				{
					Activation.OverrideMats = *OverrideMatsPtr;
					Activation.bHasOverrideMats = true;
				}
				PendingActivations.Add(MoveTemp(Activation));

				Rep.bHasMeshRendering = true;
			}
		});

	Subsystem->BatchActivateSkinnedISMEntities(MoveTemp(PendingActivations), Context);
}

// ---------------------------------------------------------------------------
// UArcVisSkinnedMeshDeactivateProcessor
// ---------------------------------------------------------------------------

UArcVisSkinnedMeshDeactivateProcessor::UArcVisSkinnedMeshDeactivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcVisSkinnedMeshDeactivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisMeshDeactivated);
}

void UArcVisSkinnedMeshDeactivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcVisISMInstanceFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcMassSkinnedMeshFragment>(EMassFragmentPresence::All);
}

void UArcVisSkinnedMeshDeactivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisSkinnedMeshDeactivate);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, &Context](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TConstArrayView<FArcVisISMInstanceFragment> ISMInstanceFrags = Ctx.GetFragmentView<FArcVisISMInstanceFragment>();

			const bool bHasISMInstance = ISMInstanceFrags.Num() > 0;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];

				if (!Rep.bHasMeshRendering)
				{
					continue;
				}

				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				if (bHasISMInstance)
				{
					const FArcVisISMInstanceFragment& ISMInstance = ISMInstanceFrags[EntityIt];
					if (ISMInstance.InstanceIndex != INDEX_NONE)
					{
						Subsystem->RemoveSkinnedISMInstance(
							ISMInstance.HolderEntity, ISMInstance.InstanceIndex, Rep.MeshGridCoords, EntityManager);
					}
					Context.Defer().RemoveFragment<FArcVisISMInstanceFragment>(Entity);
				}

				Rep.bHasMeshRendering = false;
			}
		});
}

// ---------------------------------------------------------------------------
// UArcVisSkinnedMeshEntityDeinitObserver
// ---------------------------------------------------------------------------

UArcVisSkinnedMeshEntityDeinitObserver::UArcVisSkinnedMeshEntityDeinitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcVisEntityTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcVisSkinnedMeshEntityDeinitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcMassSkinnedMeshFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	ObserverQuery.AddRequirement<FArcVisISMInstanceFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	ObserverQuery.AddConstSharedRequirement<FArcMassPhysicsBodyConfigFragment>(EMassFragmentPresence::Optional);
	ObserverQuery.AddRequirement<FArcMassPhysicsBodyFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
}

void UArcVisSkinnedMeshEntityDeinitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisSkinnedMeshEntityDeinit);

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TConstArrayView<FArcVisISMInstanceFragment> ISMInstanceFrags = Ctx.GetFragmentView<FArcVisISMInstanceFragment>();

			const FArcMassPhysicsBodyConfigFragment* PhysicsConfigPtr = Ctx.GetConstSharedFragmentPtr<FArcMassPhysicsBodyConfigFragment>();
			const bool bHasPhysicsConfig = PhysicsConfigPtr && PhysicsConfigPtr->BodySetup;
			TArrayView<FArcMassPhysicsBodyFragment> PhysicsBodyFrags = Ctx.GetMutableFragmentView<FArcMassPhysicsBodyFragment>();
			const bool bHasPhysicsBody = PhysicsBodyFrags.Num() > 0;

			const bool bHasISMInstance = ISMInstanceFrags.Num() > 0;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];
				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				if (Rep.bHasMeshRendering && bHasISMInstance)
				{
					const FArcVisISMInstanceFragment& ISMInstance = ISMInstanceFrags[EntityIt];
					if (ISMInstance.InstanceIndex != INDEX_NONE)
					{
						Subsystem->RemoveSkinnedISMInstance(
							ISMInstance.HolderEntity, ISMInstance.InstanceIndex, Rep.MeshGridCoords, EntityManager);
					}
				}

				ArcMass::Visualization::UnregisterEntityFromGrids(*Subsystem, Entity, Rep, bHasPhysicsConfig);

				if (bHasPhysicsBody)
				{
					FArcMassPhysicsBodyFragment& PhysicsBody = PhysicsBodyFrags[EntityIt];
					PhysicsBody.TerminateBody();
				}
			}
		});
}
