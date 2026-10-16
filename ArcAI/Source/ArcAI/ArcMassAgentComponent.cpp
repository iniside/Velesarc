// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAgentComponent.h"

#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Engine/World.h"

#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"

UArcMassAgentComponent::UArcMassAgentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UArcMassAgentComponent::OnRegister()
{
	// Bypass UMassAgentComponent::OnRegister — UArcMassSpawnerSubsystem owns the lifecycle from
	// BeginPlay. The engine path (UMassAgentSubsystem::RegisterAgentComponent) would call
	// EntityCreationPending() before BeginPlay, then double-spawn via HandlePendingInitialization
	// on the next PrePhysics phase. Server-driven non-puppet only: TemplateID and replication-puppet
	// wiring intentionally skipped — never place this component on a net-simulating client puppet.
	UActorComponent::OnRegister();
}

void UArcMassAgentComponent::OnUnregister()
{
	// Mirror of OnRegister bypass. EndPlay clears the entity handle (state → None) before the base
	// OnUnregister would run, so UnregisterWithAgentSubsystem would no-op anyway; override for symmetry.
	UActorComponent::OnUnregister();
}

void UArcMassAgentComponent::BeginPlay()
{
	Super::BeginPlay();

	SpawnedHandle = UArcMassSpawnerSubsystem::SpawnForAgentComponent(*this);
	if (SpawnedHandle.IsValid())
	{
		SetEntityHandle(SpawnedHandle);
	}
}

void UArcMassAgentComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	UArcMassSpawnerSubsystem::DestroySpawnedForAgentComponent(*this, SpawnedHandle);
	SpawnedHandle = FMassEntityHandle();
	Super::EndPlay(Reason);
}

void UArcMassAgentComponent::SetEntityHandle(const FMassEntityHandle NewHandle)
{
	Super::SetEntityHandle(NewHandle);

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(GetWorld());
	if (EntitySubsystem == nullptr)
	{
		return;
	}

	const FMassEntityView EntityView(EntitySubsystem->GetEntityManager(), AgentHandle);
	FTransformFragment* TransformFragment = EntityView.GetFragmentDataPtr<FTransformFragment>();
	if (TransformFragment)
	{
		TransformFragment->SetTransform(GetOwner()->GetActorTransform());
	}
}
