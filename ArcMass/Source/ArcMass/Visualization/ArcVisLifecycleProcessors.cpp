// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisLifecycleProcessors.h"

#include "ArcVisLifecycle.h"
#include "ArcMassEntityVisualization.h"
#include "ArcMass/Lifecycle/ArcMassLifecycle.h"
#include "ArcVisEntityComponent.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mesh/MassEngineMeshFragments.h"
#include "ArcMassVisualizationConfigFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcVisLifecycleProcessors)

// ---------------------------------------------------------------------------
// UArcVisLifecycleInitObserver
// ---------------------------------------------------------------------------

UArcVisLifecycleInitObserver::UArcVisLifecycleInitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcVisLifecycleTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = false;
}

void UArcVisLifecycleInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcVisLifecycleFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcVisLifecycleConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcVisLifecycleTag>(EMassFragmentPresence::All);
}

void UArcVisLifecycleInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisLifecycleInit);

	ObserverQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcVisLifecycleFragment> LifecycleFragments = Ctx.GetMutableFragmentView<FArcVisLifecycleFragment>();
		const FArcVisLifecycleConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisLifecycleConfigFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcVisLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];
			Lifecycle.CurrentPhase = Config.InitialPhase;
			Lifecycle.PreviousPhase = Config.InitialPhase;
			Lifecycle.PhaseTimeElapsed = 0.f;
		}
	});
}

// ---------------------------------------------------------------------------
// UArcVisLifecycleTickProcessor
// ---------------------------------------------------------------------------

UArcVisLifecycleTickProcessor::UArcVisLifecycleTickProcessor()
	: EntityQuery{*this}
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);

	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::SyncWorldToMass);
}

void UArcVisLifecycleTickProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisLifecycleFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcVisLifecycleConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcVisLifecycleAutoAdvanceTag>(EMassFragmentPresence::All);
}

void UArcVisLifecycleTickProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	if (DeltaTime <= 0.f)
	{
		return;
	}

	UWorld* World = EntityManager.GetWorld();
	UMassSignalSubsystem* SignalSubsystem = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr;
	if (!SignalSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisLifecycleTick);

	TArray<FMassEntityHandle> PhaseChangedEntities;
	TArray<FMassEntityHandle> DeadEntities;

	EntityQuery.ForEachEntityChunk(Context,
		[DeltaTime, &PhaseChangedEntities, &DeadEntities](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcVisLifecycleFragment> LifecycleFragments = Ctx.GetMutableFragmentView<FArcVisLifecycleFragment>();
		const FArcVisLifecycleConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisLifecycleConfigFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcVisLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];

			if (Lifecycle.CurrentPhase == Config.TerminalPhase)
			{
				continue;
			}

			const float PhaseDuration = Config.GetPhaseDuration(Lifecycle.CurrentPhase);

			// Duration <= 0 means stay in this phase until forced
			if (PhaseDuration <= 0.f)
			{
				continue;
			}

			Lifecycle.PhaseTimeElapsed += DeltaTime;

			if (Lifecycle.PhaseTimeElapsed >= PhaseDuration)
			{
				const uint8 NextPhase = Config.GetNextPhase(Lifecycle.CurrentPhase);

				Lifecycle.PreviousPhase = Lifecycle.CurrentPhase;
				Lifecycle.CurrentPhase = NextPhase;
				Lifecycle.PhaseTimeElapsed = 0.f;

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				PhaseChangedEntities.Add(Entity);

				if (NextPhase == Config.TerminalPhase)
				{
					DeadEntities.Add(Entity);
				}
			}
		}
	});

	// Raise signals outside chunk iteration
	for (const FMassEntityHandle& Entity : PhaseChangedEntities)
	{
		SignalSubsystem->SignalEntity(UE::ArcMass::Signals::VisLifecyclePhaseChanged, Entity);
	}
	for (const FMassEntityHandle& Entity : DeadEntities)
	{
		SignalSubsystem->SignalEntity(UE::ArcMass::Signals::VisLifecycleDead, Entity);
	}
}

// ---------------------------------------------------------------------------
// UArcVisLifecycleForceTransitionProcessor
// ---------------------------------------------------------------------------

UArcVisLifecycleForceTransitionProcessor::UArcVisLifecycleForceTransitionProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcVisLifecycleForceTransitionProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisLifecycleForceTransition);
}

void UArcVisLifecycleForceTransitionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisLifecycleFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcVisLifecycleConfigFragment>(EMassFragmentPresence::All);
}

void UArcVisLifecycleForceTransitionProcessor::SignalEntities(FMassEntityManager& EntityManager,
	FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	UArcVisLifecycleSubsystem* Subsystem = World ? World->GetSubsystem<UArcVisLifecycleSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisLifecycleForceTransition);

	// Drain pending requests
	TArray<FArcVisLifecycleForceTransitionRequest> Requests = MoveTemp(Subsystem->GetPendingRequests());
	Subsystem->GetPendingRequests().Reset();

	if (Requests.IsEmpty())
	{
		return;
	}

	// Build entity -> request lookup (last request wins)
	TMap<FMassEntityHandle, int32> RequestByEntity;
	for (int32 i = 0; i < Requests.Num(); ++i)
	{
		RequestByEntity.Add(Requests[i].Entity, i);
	}

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();

	TArray<FMassEntityHandle> PhaseChangedEntities;
	TArray<FMassEntityHandle> DeadEntities;

	EntityQuery.ForEachEntityChunk(Context,
		[&Requests, &RequestByEntity, &PhaseChangedEntities, &DeadEntities](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcVisLifecycleFragment> LifecycleFragments = Ctx.GetMutableFragmentView<FArcVisLifecycleFragment>();
		const FArcVisLifecycleConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisLifecycleConfigFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			const int32* ReqIdx = RequestByEntity.Find(Entity);
			if (!ReqIdx)
			{
				continue;
			}

			const FArcVisLifecycleForceTransitionRequest& Req = Requests[*ReqIdx];
			FArcVisLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];

			if (Lifecycle.CurrentPhase == Req.TargetPhase)
			{
				continue;
			}

			Lifecycle.PreviousPhase = Lifecycle.CurrentPhase;
			Lifecycle.CurrentPhase = Req.TargetPhase;
			if (Req.bResetTime)
			{
				Lifecycle.PhaseTimeElapsed = 0.f;
			}

			PhaseChangedEntities.Add(Entity);

			if (Req.TargetPhase == Config.TerminalPhase)
			{
				DeadEntities.Add(Entity);
			}
		}
	});

	// Raise signals outside chunk iteration
	if (SignalSubsystem)
	{
		for (const FMassEntityHandle& Entity : PhaseChangedEntities)
		{
			SignalSubsystem->SignalEntity(UE::ArcMass::Signals::VisLifecyclePhaseChanged, Entity);
		}
		for (const FMassEntityHandle& Entity : DeadEntities)
		{
			SignalSubsystem->SignalEntity(UE::ArcMass::Signals::VisLifecycleDead, Entity);
		}
	}
}

// ---------------------------------------------------------------------------
// UArcVisLifecycleVisSwitchProcessor
// ---------------------------------------------------------------------------

UArcVisLifecycleVisSwitchProcessor::UArcVisLifecycleVisSwitchProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcVisLifecycleVisSwitchProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisLifecyclePhaseChanged);
}

void UArcVisLifecycleVisSwitchProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisLifecycleFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcVisLifecycleConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassVisualizationMeshConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMassOverrideMaterialsFragment>(EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcVisISMInstanceFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcVisComponentTransformFragment>(EMassFragmentPresence::Optional);
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
		TConstArrayView<FArcVisLifecycleFragment> LifecycleFragments = Ctx.GetFragmentView<FArcVisLifecycleFragment>();
		TConstArrayView<FArcVisRepresentationFragment> RepFragments = Ctx.GetFragmentView<FArcVisRepresentationFragment>();
		TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
		TArrayView<FArcVisISMInstanceFragment> ISMInstanceFragments = Ctx.GetMutableFragmentView<FArcVisISMInstanceFragment>();

		const FArcVisConfigFragment& BaseConfig = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();
		const FArcVisLifecycleConfigFragment& LCConfig = Ctx.GetConstSharedFragment<FArcVisLifecycleConfigFragment>();
		const FArcMassStaticMeshConfigFragment& StaticMeshConfigFrag = Ctx.GetConstSharedFragment<FArcMassStaticMeshConfigFragment>();
		const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag = Ctx.GetConstSharedFragment<FArcMassVisualizationMeshConfigFragment>();
		const FMassOverrideMaterialsFragment* OverrideMats = Ctx.GetConstSharedFragmentPtr<FMassOverrideMaterialsFragment>();
		const FArcVisComponentTransformFragment* CompTransformPtr = Ctx.GetConstSharedFragmentPtr<FArcVisComponentTransformFragment>();

		const bool bHasISMInstance = ISMInstanceFragments.Num() > 0;

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FArcVisLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];
			const FArcVisRepresentationFragment& Rep = RepFragments[EntityIt];

			if (Rep.bIsActorRepresentation)
			{
				// Actor-mode: swap actor class if the phase has an override (unchanged)
				const FArcVisLifecyclePhaseVisuals& PhaseVisuals = LCConfig.GetPhaseVisuals(Lifecycle.CurrentPhase);
				if (PhaseVisuals.HasActorOverride())
				{
					const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
					FMassActorFragment& ActorFrag = ActorFragments[EntityIt];
					AActor* CurrentActor = ActorFrag.GetMutable();

					if (CurrentActor && ActorFrag.IsOwnedByMass())
					{
						CurrentActor->Destroy();
						ActorFrag.ResetAndUpdateHandleMap();
					}

					UWorld* ActorWorld = EntityManager.GetWorld();
					if (ActorWorld)
					{
						const FTransformFragment& TransformFrag = TransformFragments[EntityIt];
						FActorSpawnParameters SpawnParams;
						SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						AActor* NewActor = ActorWorld->SpawnActor<AActor>(PhaseVisuals.ActorClassOverride, TransformFrag.GetTransform(), SpawnParams);
						if (NewActor)
						{
							ActorFrag.SetAndUpdateHandleMap(Entity, NewActor, /*bIsOwnedByMass=*/true);
						}
					}
				}
				// If no actor override, the existing actor stays and UArcLifecycleActorNotifyProcessor
				// handles notification via IArcLifecycleObserverInterface.
			}
			else if (Rep.bHasMeshRendering && bHasISMInstance && Subsystem)
			{
				// Mesh-mode: swap ISM instance to different holder if mesh changed
				UStaticMesh* NewMesh = LCConfig.ResolveMesh(Lifecycle.CurrentPhase, StaticMeshConfigFrag);
				if (NewMesh && NewMesh != StaticMeshConfigFrag.Mesh.Get())
				{
					FArcVisISMInstanceFragment& ISMInstance = ISMInstanceFragments[EntityIt];

					// Remove from old holder
					if (ISMInstance.InstanceIndex != INDEX_NONE)
					{
						Subsystem->RemoveISMInstance(
							ISMInstance.HolderEntity, ISMInstance.InstanceIndex, Rep.MeshGridCoords, EntityManager);
					}

					// Add to new holder with resolved mesh
					FArcMassStaticMeshConfigFragment ResolvedMeshConfigFrag(NewMesh);
					FMassEntityHandle NewHolder = Subsystem->FindOrCreateISMHolder(
						Rep.MeshGridCoords, ResolvedMeshConfigFrag, VisMeshConfigFrag, OverrideMats, EntityManager);

					const FTransformFragment& TransformFrag = TransformFragments[EntityIt];
					const FTransform InstanceTransform = CompTransformPtr
						? CompTransformPtr->ComponentRelativeTransform * TransformFrag.GetTransform()
						: TransformFrag.GetTransform();
					int32 NewIndex = Subsystem->AddISMInstance(NewHolder, InstanceTransform, EntityManager);

					ISMInstance.HolderEntity = NewHolder;
					ISMInstance.InstanceIndex = NewIndex;
				}
			}
		}
	});
}
