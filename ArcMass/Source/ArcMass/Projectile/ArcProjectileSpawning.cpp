// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcProjectileSpawning.h"

#include "ArcProjectileFragments.h"

#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassSpawnerSubsystem.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcProjectileSpawning)

// ---------------------------------------------------------------------------
// Spawn processor
// ---------------------------------------------------------------------------

UArcProjectileSpawnProcessor::UArcProjectileSpawnProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = false;
}

void UArcProjectileSpawnProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcProjectileFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcProjectileCollisionFilterFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FArcProjectileTag>(EMassFragmentPresence::All);
}

void UArcProjectileSpawnProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!ensure(Context.ValidateAuxDataType<FArcProjectileSpawnData>()))
	{
		return;
	}

	FArcProjectileSpawnData& SpawnData = Context.GetMutableAuxData().GetMutable<FArcProjectileSpawnData>();

	const int32 NumTransforms = SpawnData.Transforms.Num();
	const int32 NumVelocities = SpawnData.Velocities.Num();
	const int32 NumInstigators = SpawnData.Instigators.Num();
	const int32 NumIgnoredActors = SpawnData.IgnoredActors.Num();

	int32 NextIndex = 0;

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		TArrayView<FTransformFragment> Transforms = Ctx.GetMutableFragmentView<FTransformFragment>();
		TArrayView<FArcProjectileFragment> Projectiles = Ctx.GetMutableFragmentView<FArcProjectileFragment>();
		TArrayView<FArcProjectileCollisionFilterFragment> Filters = Ctx.GetMutableFragmentView<FArcProjectileCollisionFilterFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const int32 Idx = NextIndex++;

			if (Idx < NumTransforms)
			{
				Transforms[EntityIt].GetMutableTransform() = SpawnData.Transforms[Idx];
			}

			if (Idx < NumVelocities)
			{
				Projectiles[EntityIt].Velocity = SpawnData.Velocities[Idx];
			}

			if (Idx < NumInstigators)
			{
				Projectiles[EntityIt].Instigator = SpawnData.Instigators[Idx];
			}

			if (Idx < NumIgnoredActors)
			{
				Filters[EntityIt].IgnoredActors = SpawnData.IgnoredActors[Idx];
			}
		}
	});
}

// ---------------------------------------------------------------------------
// Static helper
// ---------------------------------------------------------------------------

void UE::ArcMass::Projectile::BatchSpawnProjectiles(
	UWorld* World,
	UMassEntityConfigAsset* EntityConfig,
	const FArcProjectileSpawnData& SpawnData,
	TArray<FMassEntityHandle>& OutEntities)
{
	if (!World || !EntityConfig)
	{
		return;
	}

	UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
	if (!SpawnerSubsystem)
	{
		return;
	}

	const FMassEntityTemplate& EntityTemplate = EntityConfig->GetOrCreateEntityTemplate(*World);
	if (!EntityTemplate.IsValid())
	{
		return;
	}

	const int32 Count = SpawnData.Transforms.Num();
	if (Count <= 0)
	{
		return;
	}

	SpawnerSubsystem->SpawnEntities(
		EntityTemplate.GetTemplateID(),
		static_cast<uint32>(Count),
		FConstStructView::Make(SpawnData),
		UArcProjectileSpawnProcessor::StaticClass(),
		OutEntities
	);
}
