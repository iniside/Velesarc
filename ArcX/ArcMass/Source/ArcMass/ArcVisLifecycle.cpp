// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisLifecycle.h"

#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcVisLifecycle)

// ---------------------------------------------------------------------------
// FArcVisLifecycleConfigFragment
// ---------------------------------------------------------------------------

float FArcVisLifecycleConfigFragment::GetPhaseDuration(EArcLifecyclePhase Phase) const
{
	switch (Phase)
	{
	case EArcLifecyclePhase::Start:   return StartDuration;
	case EArcLifecyclePhase::Growing: return GrowingDuration;
	case EArcLifecyclePhase::Grown:   return GrownDuration;
	case EArcLifecyclePhase::Dying:   return DyingDuration;
	case EArcLifecyclePhase::Dead:    return DeadDuration;
	default:                          return 0.f;
	}
}

const FArcVisLifecyclePhaseVisuals& FArcVisLifecycleConfigFragment::GetPhaseVisuals(EArcLifecyclePhase Phase) const
{
	switch (Phase)
	{
	case EArcLifecyclePhase::Start:   return StartVisuals;
	case EArcLifecyclePhase::Growing: return GrowingVisuals;
	case EArcLifecyclePhase::Grown:   return GrownVisuals;
	case EArcLifecyclePhase::Dying:   return DyingVisuals;
	case EArcLifecyclePhase::Dead:    return DeadVisuals;
	default:                          return StartVisuals;
	}
}

UStaticMesh* FArcVisLifecycleConfigFragment::ResolveISMMesh(EArcLifecyclePhase Phase, const FArcVisConfigFragment& BaseConfig) const
{
	const FArcVisLifecyclePhaseVisuals& Visuals = GetPhaseVisuals(Phase);
	return Visuals.HasMeshOverride() ? Visuals.StaticMesh.Get() : BaseConfig.StaticMesh.Get();
}

const TArray<TObjectPtr<UMaterialInterface>>& FArcVisLifecycleConfigFragment::ResolveISMMaterials(
	EArcLifecyclePhase Phase, const FArcVisConfigFragment& BaseConfig) const
{
	const FArcVisLifecyclePhaseVisuals& Visuals = GetPhaseVisuals(Phase);
	return Visuals.MaterialOverrides.Num() > 0 ? Visuals.MaterialOverrides : BaseConfig.MaterialOverrides;
}

TSubclassOf<AActor> FArcVisLifecycleConfigFragment::ResolveActorClass(
	EArcLifecyclePhase Phase, const FArcVisConfigFragment& BaseConfig) const
{
	const FArcVisLifecyclePhaseVisuals& Visuals = GetPhaseVisuals(Phase);
	return Visuals.HasActorOverride() ? Visuals.ActorClassOverride : BaseConfig.ActorClass;
}

// ---------------------------------------------------------------------------
// UArcVisLifecycleSubsystem
// ---------------------------------------------------------------------------

void UArcVisLifecycleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UArcVisLifecycleSubsystem::Deinitialize()
{
	PendingRequests.Empty();
	Super::Deinitialize();
}

void UArcVisLifecycleSubsystem::RequestForceTransition(const FMassEntityHandle& Entity, EArcLifecyclePhase TargetPhase, bool bResetTime)
{
	if (!Entity.IsValid() || TargetPhase == EArcLifecyclePhase::MAX)
	{
		return;
	}

	PendingRequests.Add({ Entity, TargetPhase, bResetTime });

	UWorld* World = GetWorld();
	if (UMassSignalSubsystem* SignalSubsystem = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr)
	{
		SignalSubsystem->SignalEntity(UE::ArcMass::Signals::VisLifecycleForceTransition, Entity);
	}
}

void UArcVisLifecycleSubsystem::SetLifecyclePhase(const FMassEntityHandle& Entity, EArcLifecyclePhase NewPhase, bool bResetTime)
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

	FArcVisLifecycleFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcVisLifecycleFragment>(Entity);
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

	if (UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>())
	{
		SignalSubsystem->SignalEntity(UE::ArcMass::Signals::VisLifecyclePhaseChanged, Entity);

		if (NewPhase == EArcLifecyclePhase::Dead)
		{
			const FArcVisLifecycleConfigFragment* Config =
				EntityManager.GetConstSharedFragmentDataPtr<FArcVisLifecycleConfigFragment>(Entity);
			if (Config && Config->bSignalDeadForDestruction)
			{
				SignalSubsystem->SignalEntity(UE::ArcMass::Signals::VisLifecycleDead, Entity);
			}
		}
	}
}

EArcLifecyclePhase UArcVisLifecycleSubsystem::GetEntityVisLifecyclePhase(const UObject* WorldContextObject, const FMassEntityHandle& Entity)
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

	const FArcVisLifecycleFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcVisLifecycleFragment>(Entity);
	return Fragment ? Fragment->CurrentPhase : EArcLifecyclePhase::MAX;
}

float UArcVisLifecycleSubsystem::GetEntityVisPhaseTimeElapsed(const UObject* WorldContextObject, const FMassEntityHandle& Entity)
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

	const FArcVisLifecycleFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcVisLifecycleFragment>(Entity);
	return Fragment ? Fragment->PhaseTimeElapsed : 0.f;
}
