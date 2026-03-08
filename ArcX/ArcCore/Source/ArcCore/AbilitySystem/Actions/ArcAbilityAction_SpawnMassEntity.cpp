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

#include "ArcAbilityAction_SpawnMassEntity.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "MassEntityConfigAsset.h"
#include "MassSpawnerSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "Engine/World.h"

void FArcAbilityAction_SpawnMassEntity::Execute(FArcAbilityActionContext& Context)
{
	if (!EntityConfig)
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

	// Resolve spawn transform from TargetData
	FTransform SpawnTransform = FTransform::Identity;

	if (Context.TargetData.IsValid(0))
	{
		const FGameplayAbilityTargetData* TargetData = Context.TargetData.Get(0);

		switch (SpawnOrigin)
		{
		case EArcAbilityActorSpawnOrigin::ImpactPoint:
		{
			if (const FHitResult* Hit = TargetData->GetHitResult())
			{
				SpawnTransform.SetLocation(Hit->ImpactPoint);
				//SpawnTransform.SetRotation(Hit->ImpactNormal.Rotation().Quaternion());
			}
			else
			{
				SpawnTransform.SetLocation(TargetData->GetEndPoint());
			}
			break;
		}
		case EArcAbilityActorSpawnOrigin::ActorLocation:
		{
			AActor* Actor = nullptr;
			if (TargetData->HasHitResult())
			{
				Actor = TargetData->GetHitResult()->GetActor();
			}
			if (!Actor && TargetData->GetActors().Num() > 0)
			{
				Actor = TargetData->GetActors()[0].Get();
			}
			if (Actor)
			{
				SpawnTransform = Actor->GetActorTransform();
			}
			break;
		}
		case EArcAbilityActorSpawnOrigin::Origin:
		{
			if (TargetData->HasOrigin())
			{
				SpawnTransform = TargetData->GetOrigin();
			}
			break;
		}
		case EArcAbilityActorSpawnOrigin::Custom:
			break;
		}
	}

	// Spawn entity
	TArray<FMassEntityHandle> SpawnedEntities;
	TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
		SpawnerSubsystem->SpawnEntities(EntityTemplate, 1, SpawnedEntities);

	if (SpawnedEntities.IsEmpty())
	{
		return;
	}

	// Set transform on the spawned entity if it has a FTransformFragment
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(SpawnedEntities[0]);
	if (TransformFrag)
	{
		TransformFrag->SetTransform(SpawnTransform);
	}
}
