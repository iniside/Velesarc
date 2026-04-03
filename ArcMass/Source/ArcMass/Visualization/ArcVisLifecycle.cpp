// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisLifecycle.h"
#include "ArcMassVisualizationConfigFragments.h"

#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcVisLifecycle)

// ---------------------------------------------------------------------------
// FArcVisLifecycleConfigFragment
// ---------------------------------------------------------------------------

float FArcVisLifecycleConfigFragment::GetPhaseDuration(uint8 Phase) const
{
	return PhaseDurations.IsValidIndex(Phase) ? PhaseDurations[Phase] : 0.f;
}

static const FArcVisLifecyclePhaseVisuals GEmptyPhaseVisuals;

const FArcVisLifecyclePhaseVisuals& FArcVisLifecycleConfigFragment::GetPhaseVisuals(uint8 Phase) const
{
	return PhaseVisuals.IsValidIndex(Phase) ? PhaseVisuals[Phase] : GEmptyPhaseVisuals;
}

UStaticMesh* FArcVisLifecycleConfigFragment::ResolveMesh(uint8 Phase, const FArcMassStaticMeshConfigFragment& BaseMeshConfigFrag) const
{
	const FArcVisLifecyclePhaseVisuals& Visuals = GetPhaseVisuals(Phase);
	return Visuals.HasMeshOverride() ? Visuals.StaticMesh.Get() : const_cast<UStaticMesh*>(BaseMeshConfigFrag.Mesh.Get());
}

const TArray<TObjectPtr<UMaterialInterface>>& FArcVisLifecycleConfigFragment::ResolveMaterials(
	uint8 Phase, const FArcVisConfigFragment& BaseConfig) const
{
	const FArcVisLifecyclePhaseVisuals& Visuals = GetPhaseVisuals(Phase);
	return Visuals.MaterialOverrides.Num() > 0 ? Visuals.MaterialOverrides : BaseConfig.MaterialOverrides;
}

TSubclassOf<AActor> FArcVisLifecycleConfigFragment::ResolveActorClass(
	uint8 Phase, const FArcVisActorConfigFragment& BaseActorConfig) const
{
	const FArcVisLifecyclePhaseVisuals& Visuals = GetPhaseVisuals(Phase);
	return Visuals.HasActorOverride() ? Visuals.ActorClassOverride : BaseActorConfig.ActorClass;
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

void UArcVisLifecycleSubsystem::RequestForceTransition(const FMassEntityHandle& Entity, uint8 TargetPhase, bool bResetTime)
{
	if (!Entity.IsValid())
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

void UArcVisLifecycleSubsystem::SetLifecyclePhase(const FMassEntityHandle& Entity, uint8 NewPhase, bool bResetTime)
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

	FArcVisLifecycleFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcVisLifecycleFragment>(Entity);
	if (!Fragment)
	{
		return;
	}

	if (Fragment->CurrentPhase == NewPhase)
	{
		return;
	}

	const FArcVisLifecycleConfigFragment* Config =
		EntityManager.GetConstSharedFragmentDataPtr<FArcVisLifecycleConfigFragment>(Entity);

	if (Config && !Config->IsValidPhase(NewPhase))
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

		if (Config && NewPhase == Config->TerminalPhase)
		{
			SignalSubsystem->SignalEntity(UE::ArcMass::Signals::VisLifecycleDead, Entity);
		}
	}
}

uint8 UArcVisLifecycleSubsystem::GetEntityVisLifecyclePhase(const UObject* WorldContextObject, const FMassEntityHandle& Entity)
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

	const FArcVisLifecycleFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcVisLifecycleFragment>(Entity);
	return Fragment ? Fragment->CurrentPhase : 0xFF;
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
