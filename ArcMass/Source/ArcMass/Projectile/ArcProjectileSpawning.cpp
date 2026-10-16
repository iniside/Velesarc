// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcProjectileSpawning.h"

#include "ArcProjectileFragments.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"

#include "Engine/World.h"
#include "MassEntityTemplate.h"
#include "StructUtils/SharedStruct.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcProjectileSpawning)

namespace ArcProjectileSpawningPrivate
{
	template<typename T>
	T GetInitialFragmentValue(const FMassEntityTemplate& EntityTemplate)
	{
		const FInstancedStruct* InitialValue = EntityTemplate.GetInitialFragmentValues().FindByPredicate(
			FStructTypeEqualOperator(T::StaticStruct()));
		return InitialValue ? InitialValue->Get<T>() : T();
	}

	const FArcTargetedProjectileConfigFragment* GetTargetedConfig(const FMassEntityTemplate& EntityTemplate)
	{
		const FConstSharedStruct* SharedValue = EntityTemplate.GetSharedFragmentValues().GetConstSharedFragments().FindByPredicate(
			FStructTypeEqualOperator(FArcTargetedProjectileConfigFragment::StaticStruct()));
		return SharedValue && SharedValue->IsValid()
			? &SharedValue->Get<const FArcTargetedProjectileConfigFragment>()
			: nullptr;
	}
}

// ---------------------------------------------------------------------------
// Static helper
// ---------------------------------------------------------------------------

void UE::ArcMass::Projectile::BatchSpawnProjectiles(
	UWorld* World,
	UArcMassEntityConfigAsset* EntityConfig,
	const FArcProjectileSpawnData& SpawnData,
	TArray<FMassEntityHandle>& OutEntities)
{
	if (!World || !EntityConfig)
	{
		return;
	}

	UArcMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UArcMassSpawnerSubsystem>();
	if (!SpawnerSubsystem)
	{
		return;
	}

	const FMassEntityTemplate& EntityTemplate = EntityConfig->GetConfig().GetOrCreateEntityTemplate(*World);
	if (!EntityTemplate.IsValid())
	{
		return;
	}

	const int32 Count = SpawnData.Transforms.Num();
	if (Count <= 0)
	{
		return;
	}

	const FArcTargetedProjectileConfigFragment* TargetedConfig = ArcProjectileSpawningPrivate::GetTargetedConfig(EntityTemplate);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		TArray<FInstancedStruct, TInlineAllocator<4>> Overrides;

		if (SpawnData.Velocities.IsValidIndex(Index) || SpawnData.Instigators.IsValidIndex(Index))
		{
			FArcProjectileFragment Projectile = ArcProjectileSpawningPrivate::GetInitialFragmentValue<FArcProjectileFragment>(EntityTemplate);
			if (SpawnData.Velocities.IsValidIndex(Index))
			{
				Projectile.Velocity = SpawnData.Velocities[Index];
			}
			if (SpawnData.Instigators.IsValidIndex(Index))
			{
				Projectile.Instigator = SpawnData.Instigators[Index];
			}
			Overrides.Add(FInstancedStruct::Make(Projectile));
		}

		if (SpawnData.IgnoredActors.IsValidIndex(Index))
		{
			FArcProjectileCollisionFilterFragment Filter =
				ArcProjectileSpawningPrivate::GetInitialFragmentValue<FArcProjectileCollisionFilterFragment>(EntityTemplate);
			Filter.IgnoredActors = SpawnData.IgnoredActors[Index];
			Overrides.Add(FInstancedStruct::Make(Filter));
		}

		if (SpawnData.TargetEntities.IsValidIndex(Index))
		{
			FArcTargetedProjectileFragment TargetedProjectile =
				ArcProjectileSpawningPrivate::GetInitialFragmentValue<FArcTargetedProjectileFragment>(EntityTemplate);
			TargetedProjectile.TargetEntity = SpawnData.TargetEntities[Index];
			TargetedProjectile.RemainingDuration = TargetedConfig
				&& TargetedConfig->MovementMode == EArcTargetedProjectileMovementMode::FixedDuration
				? TargetedConfig->FixedDuration
				: 0.f;
			Overrides.Add(FInstancedStruct::Make(TargetedProjectile));
		}

		if (SpawnData.ActionFragments.IsValidIndex(Index))
		{
			Overrides.Add(FInstancedStruct::Make(SpawnData.ActionFragments[Index]));
		}

		const FMassEntityHandle Entity = SpawnerSubsystem->SpawnEntityWithOverrides(
			EntityConfig,
			SpawnData.Transforms[Index],
			Overrides);
		if (Entity.IsValid())
		{
			OutEntities.Add(Entity);
		}
	}
}
