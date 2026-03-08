/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcAbilityAction_SpawnMassProjectile.h"

#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "ArcMass/Projectile/ArcProjectileFragments.h"

#include "MassEntityConfigAsset.h"
#include "MassSpawnerSubsystem.h"
#include "MassCommonFragments.h"
#include "Engine/World.h"

void FArcAbilityAction_SpawnMassProjectile::Execute(FArcAbilityActionContext& Context)
{
	if (!EntityConfig)
	{
		return;
	}

	UArcCoreAbilitySystemComponent* ArcASC = Context.Ability->GetArcASC();
	if (!ArcASC)
	{
		return;
	}

	if (!OriginTargetingTag.IsValid())
	{
		return;
	}

	UWorld* World = Context.Ability->GetWorld();
	if (!World)
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

	// Get spawn origin from global targeting
	bool bOriginSuccess = false;
	FHitResult OriginHit = UArcCoreAbilitySystemComponent::GetGlobalHitResult(ArcASC, OriginTargetingTag, bOriginSuccess);
	if (!bOriginSuccess)
	{
		return;
	}

	// Get target location from global targeting (optional — if not provided, use fragment default velocity)
	TOptional<FVector> TargetLocation;
	if (TargetTargetingTag.IsValid())
	{
		bool bTargetSuccess = false;
		FVector TargetLoc = UArcCoreAbilitySystemComponent::GetGlobalHitLocation(ArcASC, TargetTargetingTag, bTargetSuccess);
		if (bTargetSuccess)
		{
			TargetLocation = TargetLoc;
		}
	}

	const FVector SpawnLocation = OriginHit.ImpactPoint;

	// Spawn entity
	TArray<FMassEntityHandle> SpawnedEntities;
	TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
		SpawnerSubsystem->SpawnEntities(EntityTemplate, 1, SpawnedEntities);

	if (SpawnedEntities.IsEmpty())
	{
		return;
	}

	const FMassEntityHandle Entity = SpawnedEntities[0];
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	// Set spawn transform
	FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
	if (TransformFrag)
	{
		FTransform SpawnTransform = FTransform::Identity;
		SpawnTransform.SetLocation(SpawnLocation);
		TransformFrag->SetTransform(SpawnTransform);
	}

	// Set velocity direction and speed
	FArcProjectileFragment* ProjectileFrag = EntityManager.GetFragmentDataPtr<FArcProjectileFragment>(Entity);
	if (ProjectileFrag)
	{
		FVector Direction = FVector::ZeroVector;
		if (TargetLocation.IsSet())
		{
			Direction = (TargetLocation.GetValue() - SpawnLocation).GetSafeNormal();
		}

		if (Direction.IsNearlyZero())
		{
			// No valid target — use instigator forward direction
			if (const FGameplayAbilityActorInfo* ActorInfo = Context.ActorInfo)
			{
				if (AActor* Avatar = ActorInfo->AvatarActor.Get())
				{
					Direction = Avatar->GetActorForwardVector();
				}
			}
		}

		if (!Direction.IsNearlyZero())
		{
			ProjectileFrag->Velocity = Direction * InitialSpeed;

			if (TransformFrag)
			{
				TransformFrag->GetMutableTransform().SetRotation(Direction.ToOrientationQuat());
			}
		}

		ProjectileFrag->Instigator = Context.Ability;
	}

	// Add instigator to collision ignore list
	if (bIgnoreInstigator)
	{
		FArcProjectileCollisionFilterFragment* FilterFrag = EntityManager.GetFragmentDataPtr<FArcProjectileCollisionFilterFragment>(Entity);
		if (FilterFrag)
		{
			if (const FGameplayAbilityActorInfo* ActorInfo = Context.ActorInfo)
			{
				if (AActor* InstigatorActor = ActorInfo->AvatarActor.Get())
				{
					FilterFrag->IgnoredActors.Add(InstigatorActor);
				}
			}
		}
	}
}
