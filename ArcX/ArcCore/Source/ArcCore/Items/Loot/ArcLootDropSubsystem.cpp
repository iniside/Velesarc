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

#include "ArcCore/Items/Loot/ArcLootDropSubsystem.h"

#include "ArcCore/Items/Loot/ArcLootTable.h"
#include "ArcCore/Items/Loot/ArcLootFragments.h"
#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityManager.h"
#include "MassSpawnerSubsystem.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcLootDrop, Log, All);

void UArcLootDropSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

TArray<FMassEntityHandle> UArcLootDropSubsystem::DropLoot(
	const UArcLootTable* LootTable,
	FConstStructView Context,
	const FTransform& SpawnTransform,
	int32 MaxItemsPerEntity)
{
	TArray<FMassEntityHandle> SpawnedEntities;

	if (!LootTable)
	{
		return SpawnedEntities;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return SpawnedEntities;
	}

	// 1. Roll the table
	TArray<FArcLootDropResult> Drops = RollLootTable(LootTable, Context);
	if (Drops.IsEmpty())
	{
		return SpawnedEntities;
	}

	// 2. Group drops by entity config
	TMap<const UMassEntityConfigAsset*, TArray<FArcLootDropResult*>> GroupedDrops;
	for (FArcLootDropResult& Drop : Drops)
	{
		if (Drop.LootEntityConfig != nullptr)
		{
			GroupedDrops.FindOrAdd(Drop.LootEntityConfig.Get()).Add(&Drop);
		}
		else
		{
			UE_LOG(LogArcLootDrop, Warning,
				TEXT("DropLoot: Drop has no LootEntityConfig and will be skipped."));
		}
	}

	if (GroupedDrops.IsEmpty())
	{
		return SpawnedEntities;
	}

	// 3. Get spawning infrastructure
	UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
	if (!SpawnerSubsystem)
	{
		return SpawnedEntities;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	MaxItemsPerEntity = FMath::Max(1, MaxItemsPerEntity);

	const FVector BaseLocation = SpawnTransform.GetLocation();
	const FRotator BaseRotation = SpawnTransform.GetRotation().Rotator();

	// Count total entities for scatter calculation
	int32 TotalEntities = 0;
	for (const auto& Pair : GroupedDrops)
	{
		TotalEntities += FMath::DivideAndRoundUp(Pair.Value.Num(), MaxItemsPerEntity);
	}

	int32 GlobalEntityIdx = 0;

	// 4. For each config group, spawn entities and populate
	for (const auto& Pair : GroupedDrops)
	{
		const UMassEntityConfigAsset* EntityConfig = Pair.Key;
		const TArray<FArcLootDropResult*>& GroupDrops = Pair.Value;

		const FMassEntityTemplate& EntityTemplate = EntityConfig->GetOrCreateEntityTemplate(*World);
		if (!EntityTemplate.IsValid())
		{
			UE_LOG(LogArcLootDrop, Warning, TEXT("DropLoot: Invalid entity template from config asset."));
			continue;
		}

		const int32 NumEntities = FMath::DivideAndRoundUp(GroupDrops.Num(), MaxItemsPerEntity);

		for (int32 EntityIdx = 0; EntityIdx < NumEntities; ++EntityIdx)
		{
			TArray<FMassEntityHandle> BatchEntities;
			TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
				SpawnerSubsystem->SpawnEntities(EntityTemplate, 1, BatchEntities);

			if (BatchEntities.IsEmpty())
			{
				continue;
			}

			const FMassEntityHandle Entity = BatchEntities[0];

			// Set transform with scatter offset for multiple entities
			FVector SpawnLocation = BaseLocation;
			if (TotalEntities > 1)
			{
				const float Angle = (static_cast<float>(GlobalEntityIdx) / static_cast<float>(TotalEntities)) * 2.0f * UE_PI;
				const float Radius = 50.0f + FMath::FRand() * 30.0f;
				SpawnLocation.X += FMath::Cos(Angle) * Radius;
				SpawnLocation.Y += FMath::Sin(Angle) * Radius;
			}

			if (FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity))
			{
				TransformFragment->SetTransform(FTransform(BaseRotation, SpawnLocation));
			}

			// Populate loot container fragment.
			// Note: MoveTemp consumes ItemSpec from the Drops array — Drops must not be read after this loop.
			if (FArcLootContainerFragment* LootFragment = EntityManager.GetFragmentDataPtr<FArcLootContainerFragment>(Entity))
			{
				const int32 StartIdx = EntityIdx * MaxItemsPerEntity;
				const int32 EndIdx = FMath::Min(StartIdx + MaxItemsPerEntity, GroupDrops.Num());

				LootFragment->Items.Reserve(EndIdx - StartIdx);
				for (int32 DropIdx = StartIdx; DropIdx < EndIdx; ++DropIdx)
				{
					LootFragment->Items.Add(MoveTemp(GroupDrops[DropIdx]->ItemSpec));
				}
			}

			SpawnedEntities.Add(Entity);
			++GlobalEntityIdx;
		}
	}

	return SpawnedEntities;
}

TArray<FArcLootDropResult> UArcLootDropSubsystem::RollLootTable(
	const UArcLootTable* LootTable,
	FConstStructView Context) const
{
	TArray<FArcLootDropResult> Results;

	if (!LootTable || LootTable->Entries.IsEmpty())
	{
		return Results;
	}

	if (!LootTable->RollStrategy.IsValid())
	{
		UE_LOG(LogArcLootDrop, Warning, TEXT("RollLootTable: No roll strategy assigned to loot table."));
		return Results;
	}

	const FArcLootRollStrategy* Strategy = LootTable->RollStrategy.GetPtr<FArcLootRollStrategy>();
	if (!Strategy)
	{
		UE_LOG(LogArcLootDrop, Warning, TEXT("RollLootTable: Invalid roll strategy type."));
		return Results;
	}

	Strategy->RollEntries(LootTable->Entries, Context, Results);

	return Results;
}
