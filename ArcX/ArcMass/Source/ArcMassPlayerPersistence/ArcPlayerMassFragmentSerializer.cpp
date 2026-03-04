// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcPlayerMassFragmentSerializer.h"

#include "MassAgentComponent.h"
#include "MassEntitySubsystem.h"
#include "ArcMass/Persistence/ArcMassFragmentSerializer.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "Serialization/ArcSaveArchive.h"
#include "Serialization/ArcLoadArchive.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcPlayerMassPersistence, Log, All);

// ── Serializer Registration ─────────────────────────────────────────────

UE_ARC_IMPLEMENT_PERSISTENCE_SERIALIZER(
	FArcPlayerMassFragmentSerializer, "MassAgentComponent");

// ── Provider Registration (leaf under Pawn) ─────────────────────────────

UE_ARC_IMPLEMENT_PLAYER_PROVIDER(
	UMassAgentComponent,
	MassFragments,
	APawn::StaticClass(),
	[](UObject* Parent) -> UObject* {
		AActor* Actor = Cast<AActor>(Parent);
		return Actor ? Actor->FindComponentByClass<UMassAgentComponent>() : nullptr;
	},
	true
);

// ── Save ────────────────────────────────────────────────────────────────

void FArcPlayerMassFragmentSerializer::Save(
	const UMassAgentComponent& Source, FArcSaveArchive& Ar)
{
	const FMassEntityHandle Entity = Source.GetEntityHandle();
	if (!Entity.IsValid())
	{
		UE_LOG(LogArcPlayerMassPersistence, Log,
			TEXT("Save: No valid entity on MassAgentComponent '%s' — skipping"),
			*Source.GetName());
		return;
	}

	UWorld* World = Source.GetWorld();
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityValid(Entity))
	{
		return;
	}

	const FArcMassPersistenceConfigFragment* Config =
		EntityManager.GetConstSharedFragmentDataPtr<
			FArcMassPersistenceConfigFragment>(Entity);

	FArcMassPersistenceConfigFragment DefaultConfig;
	FArcMassFragmentSerializer::SaveEntityFragments(
		EntityManager, Entity,
		Config ? *Config : DefaultConfig,
		Ar, World);
}

// ── Load ────────────────────────────────────────────────────────────────

void FArcPlayerMassFragmentSerializer::Load(
	UMassAgentComponent& Target, FArcLoadArchive& Ar)
{
	const FMassEntityHandle Entity = Target.GetEntityHandle();
	if (!Entity.IsValid())
	{
		UE_LOG(LogArcPlayerMassPersistence, Log,
			TEXT("Load: No valid entity on MassAgentComponent '%s' — "
			     "data stays in cache for deferred apply"),
			*Target.GetName());
		return;
	}

	UWorld* World = Target.GetWorld();
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityValid(Entity))
	{
		return;
	}

	FArcMassFragmentSerializer::LoadEntityFragments(
		EntityManager, Entity, Ar, World);

	UE_LOG(LogArcPlayerMassPersistence, Log,
		TEXT("Load: Applied fragment data to entity on '%s'"),
		*Target.GetName());
}
