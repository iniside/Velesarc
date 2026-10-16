// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassLifecycle.h"

#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "Mass/EntityManager.h"
#include "MassEntityTemplateRegistry.h"
#include "Mass/EntityUtils.h"
#include "MassWorldUtils.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
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

void UArcLifecycleSubsystem::RequestForceTransition(const FMassEntityHandle& Entity, uint8 TargetPhase, bool bResetTime)
{
	if (!Entity.IsValid())
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

void UArcLifecycleSubsystem::SetLifecyclePhase(const FMassEntityHandle& Entity, uint8 NewPhase, bool bResetTime)
{
	if (!Entity.IsValid())
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

	const FArcLifecycleConfigFragment* Config = EntityManager.GetConstSharedFragmentDataPtr<FArcLifecycleConfigFragment>(Entity);
	if (Config && !Config->IsValidPhase(NewPhase))
	{
		UE_LOGF(LogMass, Warning,"SetLifecyclePhase: Phase %d out of range (NumPhases=%d)", NewPhase, Config->GetNumPhases());
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

		if (Config && NewPhase == Config->TerminalPhase)
		{
			SignalSubsystem->SignalEntity(UE::ArcMass::Signals::LifecycleDead, Entity);
		}
	}
}

uint8 UArcLifecycleSubsystem::GetEntityLifecyclePhase(const UObject* WorldContextObject, const FMassEntityHandle& Entity)
{
	if (!WorldContextObject || !Entity.IsValid())
	{
		return 0xFF;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return 0xFF;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	if (!EntityManager.IsEntityValid(Entity))
	{
		return 0xFF;
	}

	const FArcLifecycleFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcLifecycleFragment>(Entity);
	return Fragment ? Fragment->CurrentPhase : 0xFF;
}

bool UArcLifecycleSubsystem::HasLifecycle(const UObject* WorldContextObject, const FMassEntityHandle& Entity)
{
	if (!WorldContextObject || !Entity.IsValid()) return false;
	const UWorld* World = WorldContextObject->GetWorld();
	if (!World) return false;
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	if (!EntityManager.IsEntityValid(Entity)) return false;
	return EntityManager.GetFragmentDataPtr<FArcLifecycleFragment>(Entity) != nullptr;
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
	ObservedTypes.Add(FArcLifecycleTag::StaticStruct());
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
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcLifecycleInit);

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
	EntityQuery.AddTagRequirement<FArcLifecycleAutoAdvanceTag>(EMassFragmentPresence::All);
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcLifecycleTick);

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
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcLifecycleForceTransition);

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
			SignalSubsystem->SignalEntity(UE::ArcMass::Signals::LifecyclePhaseChanged, Entity);
		}
		for (const FMassEntityHandle& Entity : DeadEntities)
		{
			SignalSubsystem->SignalEntity(UE::ArcMass::Signals::LifecycleDead, Entity);
		}
	}
}

// ---------------------------------------------------------------------------
// UArcLifecycleActorNotifyProcessor
// ---------------------------------------------------------------------------

UArcLifecycleActorNotifyProcessor::UArcLifecycleActorNotifyProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
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
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcLifecycleActorNotify);

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
	// UMassTraitRepository builds every trait CDO to discover the elements it adds, so this runs
	// with an unconfigured LifecycleConfig — the phase checks belong in ValidateTemplate, not here.
	BuildContext.AddFragment<FArcLifecycleFragment>();
	BuildContext.AddTag<FArcLifecycleTag>();

	if (bAutoAdvance)
	{
		BuildContext.AddTag<FArcLifecycleAutoAdvanceTag>();
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(LifecycleConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}

bool UArcLifecycleTrait::ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const
{
	if (LifecycleConfig.PhaseDurations.IsEmpty())
	{
		UE_LOGF(LogTemp, Error,"UArcLifecycleTrait on '%ls': PhaseDurations is empty — the entity has no lifecycle phases to advance through.", *GetPathName());
		return false;
	}

	if (!LifecycleConfig.IsValidPhase(LifecycleConfig.InitialPhase))
	{
		UE_LOGF(LogTemp, Error,"UArcLifecycleTrait on '%ls': InitialPhase %u is out of range for %d phases.",
			*GetPathName(), LifecycleConfig.InitialPhase, LifecycleConfig.PhaseDurations.Num());
		return false;
	}

	if (!LifecycleConfig.IsValidPhase(LifecycleConfig.TerminalPhase))
	{
		UE_LOGF(LogTemp, Error,"UArcLifecycleTrait on '%ls': TerminalPhase %u is out of range for %d phases.",
			*GetPathName(), LifecycleConfig.TerminalPhase, LifecycleConfig.PhaseDurations.Num());
		return false;
	}

	return true;
}
