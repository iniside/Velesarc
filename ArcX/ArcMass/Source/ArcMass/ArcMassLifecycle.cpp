// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassLifecycle.h"

#include "InstancedActorsTypes.h"
#include "InstancedActorsData.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassLifecycle)

// ---------------------------------------------------------------------------
// UArcLifecycleSubsystem
// ---------------------------------------------------------------------------

void UArcLifecycleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UArcLifecycleSubsystem::Deinitialize()
{
	PendingRequests.Empty();
	Super::Deinitialize();
}

void UArcLifecycleSubsystem::RequestForceTransition(const FMassEntityHandle& Entity, EArcLifecyclePhase TargetPhase, bool bResetTime)
{
	if (!Entity.IsValid() || TargetPhase == EArcLifecyclePhase::MAX)
	{
		return;
	}

	PendingRequests.Add({ Entity, TargetPhase, bResetTime });

	UWorld* World = GetWorld();
	if (UMassSignalSubsystem* SignalSubsystem = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr)
	{
		SignalSubsystem->SignalEntity(UE::ArcMass::Signals::LifecycleForceTransition, Entity);
	}
}

void UArcLifecycleSubsystem::SetLifecyclePhase(const FMassEntityHandle& Entity, EArcLifecyclePhase NewPhase, bool bResetTime)
{
	if (!Entity.IsValid() || NewPhase == EArcLifecyclePhase::MAX)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	if (!EntityManager.IsEntityValid(Entity))
	{
		return;
	}

	FArcLifecycleFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcLifecycleFragment>(Entity);
	if (!Fragment)
	{
		return;
	}

	if (Fragment->CurrentPhase == NewPhase)
	{
		return;
	}

	Fragment->PreviousPhase = Fragment->CurrentPhase;
	Fragment->CurrentPhase = NewPhase;
	if (bResetTime)
	{
		Fragment->PhaseTimeElapsed = 0.f;
	}

	// Signal the phase change
	if (UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>())
	{
		SignalSubsystem->SignalEntity(UE::ArcMass::Signals::LifecyclePhaseChanged, Entity);

		if (NewPhase == EArcLifecyclePhase::Dead)
		{
			SignalSubsystem->SignalEntity(UE::ArcMass::Signals::LifecycleDead, Entity);
		}
	}
}

EArcLifecyclePhase UArcLifecycleSubsystem::GetEntityLifecyclePhase(const UObject* WorldContextObject, const FMassEntityHandle& Entity)
{
	if (!WorldContextObject || !Entity.IsValid())
	{
		return EArcLifecyclePhase::MAX;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return EArcLifecyclePhase::MAX;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	if (!EntityManager.IsEntityValid(Entity))
	{
		return EArcLifecyclePhase::MAX;
	}

	const FArcLifecycleFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcLifecycleFragment>(Entity);
	return Fragment ? Fragment->CurrentPhase : EArcLifecyclePhase::MAX;
}

float UArcLifecycleSubsystem::GetEntityPhaseTimeElapsed(const UObject* WorldContextObject, const FMassEntityHandle& Entity)
{
	if (!WorldContextObject || !Entity.IsValid())
	{
		return 0.f;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return 0.f;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	if (!EntityManager.IsEntityValid(Entity))
	{
		return 0.f;
	}

	const FArcLifecycleFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcLifecycleFragment>(Entity);
	return Fragment ? Fragment->PhaseTimeElapsed : 0.f;
}

// ---------------------------------------------------------------------------
// UArcLifecycleInitObserver
// ---------------------------------------------------------------------------

UArcLifecycleInitObserver::UArcLifecycleInitObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcLifecycleTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = false;
}

void UArcLifecycleInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcLifecycleFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcLifecycleConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcLifecycleTag>(EMassFragmentPresence::All);
}

void UArcLifecycleInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ObserverQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcLifecycleFragment> LifecycleFragments = Ctx.GetMutableFragmentView<FArcLifecycleFragment>();
		const FArcLifecycleConfigFragment& Config = Ctx.GetConstSharedFragment<FArcLifecycleConfigFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];
			Lifecycle.CurrentPhase = Config.InitialPhase;
			Lifecycle.PreviousPhase = Config.InitialPhase;
			Lifecycle.PhaseTimeElapsed = 0.f;
		}
	});
}

// ---------------------------------------------------------------------------
// UArcLifecycleTickProcessor
// ---------------------------------------------------------------------------

UArcLifecycleTickProcessor::UArcLifecycleTickProcessor()
	: EntityQuery{*this}
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);

	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::SyncWorldToMass);
}

void UArcLifecycleTickProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcLifecycleFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcLifecycleConfigFragment>(EMassFragmentPresence::All);
}

void UArcLifecycleTickProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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
		TArrayView<FArcLifecycleFragment> LifecycleFragments = Ctx.GetMutableFragmentView<FArcLifecycleFragment>();
		const FArcLifecycleConfigFragment& Config = Ctx.GetConstSharedFragment<FArcLifecycleConfigFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];

			const float PhaseDuration = Config.GetPhaseDuration(Lifecycle.CurrentPhase);

			// Duration <= 0 means stay in this phase until forced (includes Dead with DeadDuration=0)
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
		SignalSubsystem->SignalEntity(UE::ArcMass::Signals::LifecyclePhaseChanged, Entity);
	}
	for (const FMassEntityHandle& Entity : DeadEntities)
	{
		SignalSubsystem->SignalEntity(UE::ArcMass::Signals::LifecycleDead, Entity);
	}
}

// ---------------------------------------------------------------------------
// UArcLifecycleForceTransitionProcessor
// ---------------------------------------------------------------------------

UArcLifecycleForceTransitionProcessor::UArcLifecycleForceTransitionProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcLifecycleForceTransitionProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::LifecycleForceTransition);
}

void UArcLifecycleForceTransitionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcLifecycleFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcLifecycleConfigFragment>(EMassFragmentPresence::All);
}

void UArcLifecycleForceTransitionProcessor::SignalEntities(FMassEntityManager& EntityManager,
	FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	UArcLifecycleSubsystem* Subsystem = World ? World->GetSubsystem<UArcLifecycleSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	// Drain pending requests
	TArray<FArcLifecycleForceTransitionRequest> Requests = MoveTemp(Subsystem->GetPendingRequests());
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
		TArrayView<FArcLifecycleFragment> LifecycleFragments = Ctx.GetMutableFragmentView<FArcLifecycleFragment>();
		const FArcLifecycleConfigFragment& Config = Ctx.GetConstSharedFragment<FArcLifecycleConfigFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			const int32* ReqIdx = RequestByEntity.Find(Entity);
			if (!ReqIdx)
			{
				continue;
			}

			const FArcLifecycleForceTransitionRequest& Req = Requests[*ReqIdx];
			FArcLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];

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
			SignalSubsystem->SignalEntity(UE::ArcMass::Signals::LifecyclePhaseChanged, Entity);
		}
		for (const FMassEntityHandle& Entity : DeadEntities)
		{
			SignalSubsystem->SignalEntity(UE::ArcMass::Signals::LifecycleDead, Entity);
		}
	}
}

// ---------------------------------------------------------------------------
// UArcLifecycleInstancedActorsBridgeProcessor
// ---------------------------------------------------------------------------

UArcLifecycleInstancedActorsBridgeProcessor::UArcLifecycleInstancedActorsBridgeProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcLifecycleInstancedActorsBridgeProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::LifecyclePhaseChanged);
}

void UArcLifecycleInstancedActorsBridgeProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcLifecycleFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FInstancedActorsFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void UArcLifecycleInstancedActorsBridgeProcessor::SignalEntities(FMassEntityManager& EntityManager,
	FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	// Collect delta indices per IAD so we can call ApplyInstanceDeltas after chunk iteration.
	// SetInstanceCurrentLifecyclePhase only writes to the delta list — it does NOT call
	// ApplyInstanceDeltas, so our UArcCoreInstancedActorsData::ApplyInstanceDelta override
	// would never fire on server/standalone without this explicit call.
	TMap<UInstancedActorsData*, TArray<int32>> PendingApplyDeltas;

	EntityQuery.ForEachEntityChunk(Context, [&PendingApplyDeltas](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcLifecycleFragment> LifecycleFragments = Ctx.GetFragmentView<FArcLifecycleFragment>();
		const TConstArrayView<FInstancedActorsFragment> IAFragments = Ctx.GetFragmentView<FInstancedActorsFragment>();

		// Skip chunks that don't have InstancedActors fragment
		if (IAFragments.IsEmpty())
		{
			return;
		}

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FArcLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];
			const FInstancedActorsFragment& IAFragment = IAFragments[EntityIt];

			UInstancedActorsData* InstanceData = IAFragment.InstanceData.Get();
			if (!InstanceData)
			{
				continue;
			}

			InstanceData->SetInstanceCurrentLifecyclePhase(
				IAFragment.InstanceIndex,
				static_cast<uint8>(Lifecycle.CurrentPhase));

			// Find the delta index for this instance. InstanceIndexToDeltaIndex is private,
			// so we scan the (small) delta array. This runs once per signal batch, not per tick.
			const TArray<FInstancedActorsDelta>& Deltas = InstanceData->GetInstanceDeltaList().GetInstanceDeltas();
			for (int32 DeltaIdx = 0; DeltaIdx < Deltas.Num(); ++DeltaIdx)
			{
				if (Deltas[DeltaIdx].GetInstanceIndex() == IAFragment.InstanceIndex)
				{
					PendingApplyDeltas.FindOrAdd(InstanceData).Add(DeltaIdx);
					break;
				}
			}
		}
	});

	// Apply deltas so UArcCoreInstancedActorsData::ApplyInstanceDelta override fires
	// on server/standalone. Clients receive these via replication (OnRep_InstanceDeltas).
	for (auto& [IAD, DeltaIndices] : PendingApplyDeltas)
	{
		IAD->ApplyInstanceDeltas(DeltaIndices);
	}
}

// ---------------------------------------------------------------------------
// UArcLifecycleActorNotifyProcessor
// ---------------------------------------------------------------------------

UArcLifecycleActorNotifyProcessor::UArcLifecycleActorNotifyProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcLifecycleActorNotifyProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::LifecyclePhaseChanged);
}

void UArcLifecycleActorNotifyProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcLifecycleFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
}

void UArcLifecycleActorNotifyProcessor::SignalEntities(FMassEntityManager& EntityManager,
	FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcLifecycleFragment> LifecycleFragments = Ctx.GetFragmentView<FArcLifecycleFragment>();
		const TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();

		// Skip chunks without actor fragments (non-hydrated entities)
		if (ActorFragments.IsEmpty())
		{
			return;
		}

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FMassActorFragment& ActorFragment = ActorFragments[EntityIt];
			AActor* Actor = ActorFragment.GetMutable();
			if (!Actor)
			{
				continue;
			}

			if (Actor->GetClass()->ImplementsInterface(UArcLifecycleObserverInterface::StaticClass()))
			{
				const FArcLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];
				IArcLifecycleObserverInterface::Execute_OnLifecyclePhaseChanged(
					Actor, Actor, Lifecycle.CurrentPhase, Lifecycle.PreviousPhase);
			}
		}
	});
}

// ---------------------------------------------------------------------------
// UArcLifecycleTrait
// ---------------------------------------------------------------------------

void UArcLifecycleTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcLifecycleFragment>();
	BuildContext.AddTag<FArcLifecycleTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(LifecycleConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
