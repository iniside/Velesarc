// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisLifecycleProcessors.h"

#include "ArcVisLifecycle.h"
#include "ArcMassEntityVisualization.h"
#include "ArcMassEntityVisualizationProcessors.h"
#include "ArcMassLifecycle.h"
#include "ArcVisEntityComponent.h"
#include "MassActorSubsystem.h"
#include "MassCommands.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcVisLifecycleProcessors)

// ---------------------------------------------------------------------------
// UArcVisLifecycleInitObserver
// ---------------------------------------------------------------------------

UArcVisLifecycleInitObserver::UArcVisLifecycleInitObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcVisLifecycleTag::StaticStruct();
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

	TArray<FMassEntityHandle> PhaseChangedEntities;
	TArray<FMassEntityHandle> DeadEntities;

	EntityQuery.ForEachEntityChunk(Context,
		[DeltaTime, &PhaseChangedEntities, &DeadEntities](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcVisLifecycleFragment> LifecycleFragments = Ctx.GetMutableFragmentView<FArcVisLifecycleFragment>();
		const FArcVisLifecycleConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisLifecycleConfigFragment>();

		if (Config.bDisableAutoTick)
		{
			return;
		}

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcVisLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];

			const float PhaseDuration = Config.GetPhaseDuration(Lifecycle.CurrentPhase);

			// Duration <= 0 means stay in this phase until forced
			if (PhaseDuration <= 0.f)
			{
				continue;
			}

			Lifecycle.PhaseTimeElapsed += DeltaTime;

			if (Lifecycle.PhaseTimeElapsed >= PhaseDuration)
			{
				EArcLifecyclePhase NextPhase;

				if (Lifecycle.CurrentPhase == EArcLifecyclePhase::Dead)
				{
					// Respawn: Dead wraps around to InitialPhase
					NextPhase = Config.InitialPhase;
				}
				else
				{
					NextPhase = FArcLifecycleConfigFragment::GetNextPhase(Lifecycle.CurrentPhase);
					if (NextPhase == EArcLifecyclePhase::MAX)
					{
						continue;
					}
				}

				Lifecycle.PreviousPhase = Lifecycle.CurrentPhase;
				Lifecycle.CurrentPhase = NextPhase;
				Lifecycle.PhaseTimeElapsed = 0.f;

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				PhaseChangedEntities.Add(Entity);

				if (NextPhase == EArcLifecyclePhase::Dead && Config.bSignalDeadForDestruction)
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

			if (Req.TargetPhase == EArcLifecyclePhase::Dead && Config.bSignalDeadForDestruction)
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
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcVisPrePlacedActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void UArcVisLifecycleVisSwitchProcessor::SignalEntities(FMassEntityManager& EntityManager,
	FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, World](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcVisLifecycleFragment> LifecycleFragments = Ctx.GetFragmentView<FArcVisLifecycleFragment>();
		const TConstArrayView<FArcVisRepresentationFragment> RepFragments = Ctx.GetFragmentView<FArcVisRepresentationFragment>();
		TArrayView<FMassActorFragment> MassActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();
		const FArcVisConfigFragment& BaseConfig = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();
		const FArcVisLifecycleConfigFragment& LCConfig = Ctx.GetConstSharedFragment<FArcVisLifecycleConfigFragment>();

		// Optional pre-placed actor fragment
		const TConstArrayView<FArcVisPrePlacedActorFragment> PrePlacedFragments = Ctx.GetFragmentView<FArcVisPrePlacedActorFragment>();
		const bool bHasPrePlaced = !PrePlacedFragments.IsEmpty();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FArcVisLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];
			const FArcVisRepresentationFragment& Rep = RepFragments[EntityIt];
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

			if (!Rep.bIsActorRepresentation)
			{
				// --- ISM mode: check if mesh needs switching ---
				UStaticMesh* NewMesh = LCConfig.ResolveISMMesh(Lifecycle.CurrentPhase, BaseConfig);
				UStaticMesh* OldMesh = Rep.CurrentISMMesh ? Rep.CurrentISMMesh.Get() : BaseConfig.StaticMesh.Get();

				if (NewMesh != OldMesh)
				{
					FArcVisLifecycleMeshSwitchFragment SwitchFragment;
					SwitchFragment.TargetPhase = Lifecycle.CurrentPhase;
					Ctx.Defer().PushCommand<FMassCommandAddFragmentInstances>(Entity, SwitchFragment);
				}
			}
			else
			{
				// --- Actor mode ---
				FMassActorFragment& MassActorFragment = MassActorFragments[EntityIt];
				const FArcVisLifecyclePhaseVisuals& PhaseVisuals = LCConfig.GetPhaseVisuals(Lifecycle.CurrentPhase);
				const bool bIsPrePlaced = bHasPrePlaced && PrePlacedFragments[EntityIt].PrePlacedActor.IsValid();

				AActor* CurrentActor = MassActorFragment.GetMutable();
				if (!CurrentActor)
				{
					continue;
				}

				// Pre-placed actors cannot be swapped to a different class — just notify via interface
				if (bIsPrePlaced)
				{
					if (CurrentActor->GetClass()->ImplementsInterface(UArcLifecycleObserverInterface::StaticClass()))
					{
						IArcLifecycleObserverInterface::Execute_OnLifecyclePhaseChanged(
							CurrentActor, CurrentActor, Lifecycle.CurrentPhase, Lifecycle.PreviousPhase);
					}
					continue;
				}

				if (PhaseVisuals.HasActorOverride())
				{
					TSubclassOf<AActor> NewActorClass = PhaseVisuals.ActorClassOverride;

					// Only swap if class actually differs
					if (CurrentActor->GetClass() != NewActorClass.Get())
					{
						const FTransform& EntityTransform = TransformFragments[EntityIt].GetTransform();

						// Notify before destroy
						if (UArcVisEntityComponent* VisComp = CurrentActor->FindComponentByClass<UArcVisEntityComponent>())
						{
							VisComp->NotifyVisActorPreDestroy();
						}
						CurrentActor->Destroy();
						MassActorFragment.ResetAndUpdateHandleMap();

						// Spawn new actor
						FActorSpawnParameters SpawnParams;
						SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						SpawnParams.bDeferConstruction = true;

						AActor* NewActor = World->SpawnActor<AActor>(NewActorClass, EntityTransform, SpawnParams);
						MassActorFragment.SetAndUpdateHandleMap(Entity, NewActor, true);

						if (UArcVisEntityComponent* VisComp = NewActor ? NewActor->FindComponentByClass<UArcVisEntityComponent>() : nullptr)
						{
							VisComp->NotifyVisActorCreated(Entity);
						}

						NewActor->FinishSpawning(EntityTransform);
					}
					else
					{
						// Same class — notify via interface
						if (CurrentActor->GetClass()->ImplementsInterface(UArcLifecycleObserverInterface::StaticClass()))
						{
							IArcLifecycleObserverInterface::Execute_OnLifecyclePhaseChanged(
								CurrentActor, CurrentActor, Lifecycle.CurrentPhase, Lifecycle.PreviousPhase);
						}
					}
				}
				else
				{
					// No actor override — notify via interface
					if (CurrentActor->GetClass()->ImplementsInterface(UArcLifecycleObserverInterface::StaticClass()))
					{
						IArcLifecycleObserverInterface::Execute_OnLifecyclePhaseChanged(
							CurrentActor, CurrentActor, Lifecycle.CurrentPhase, Lifecycle.PreviousPhase);
					}
				}
			}
		}
	});
}

// ---------------------------------------------------------------------------
// UArcVisLifecycleISMMeshSwitchProcessor
// ---------------------------------------------------------------------------

UArcVisLifecycleISMMeshSwitchProcessor::UArcVisLifecycleISMMeshSwitchProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;

	// Run before cell tracking so activation sees the correct mesh
	ExecutionOrder.ExecuteBefore.Add(UArcVisPlayerCellTrackingProcessor::StaticClass()->GetFName());
}

void UArcVisLifecycleISMMeshSwitchProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisLifecycleMeshSwitchFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcVisLifecycleFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcVisLifecycleConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
}

void UArcVisLifecycleISMMeshSwitchProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* VisSubsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!VisSubsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, VisSubsystem](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcVisLifecycleMeshSwitchFragment> SwitchFragments = Ctx.GetFragmentView<FArcVisLifecycleMeshSwitchFragment>();
		const TConstArrayView<FArcVisLifecycleFragment> LifecycleFragments = Ctx.GetFragmentView<FArcVisLifecycleFragment>();
		TArrayView<FArcVisRepresentationFragment> RepFragments = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
		const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();
		const FArcVisConfigFragment& BaseConfig = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();
		const FArcVisLifecycleConfigFragment& LCConfig = Ctx.GetConstSharedFragment<FArcVisLifecycleConfigFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcVisRepresentationFragment& Rep = RepFragments[EntityIt];
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

			// If entity became actor since the switch was queued, skip
			if (Rep.bIsActorRepresentation)
			{
				Ctx.Defer().RemoveFragment<FArcVisLifecycleMeshSwitchFragment>(Entity);
				continue;
			}

			const FArcVisLifecycleMeshSwitchFragment& Switch = SwitchFragments[EntityIt];
			const FTransform& EntityTransform = TransformFragments[EntityIt].GetTransform();

			UStaticMesh* OldMesh = Rep.CurrentISMMesh ? Rep.CurrentISMMesh.Get() : BaseConfig.StaticMesh.Get();
			UStaticMesh* NewMesh = LCConfig.ResolveISMMesh(Switch.TargetPhase, BaseConfig);
			const TArray<TObjectPtr<UMaterialInterface>>& NewMaterials = LCConfig.ResolveISMMaterials(Switch.TargetPhase, BaseConfig);

			// Remove old ISM instance
			if (OldMesh && Rep.ISMInstanceId != INDEX_NONE)
			{
				VisSubsystem->RemoveISMInstance(
					Rep.GridCoords, BaseConfig.ISMManagerClass, OldMesh, Rep.ISMInstanceId, EntityManager);
				Rep.ISMInstanceId = INDEX_NONE;
			}

			// Add new ISM instance
			if (NewMesh)
			{
				Rep.ISMInstanceId = VisSubsystem->AddISMInstance(
					Rep.GridCoords,
					BaseConfig.ISMManagerClass,
					NewMesh,
					NewMaterials,
					BaseConfig.bCastShadows,
					EntityTransform,
					Entity);
			}

			Rep.CurrentISMMesh = NewMesh;

			// Remove the transient switch fragment
			Ctx.Defer().RemoveFragment<FArcVisLifecycleMeshSwitchFragment>(Entity);
		}
	});
}
