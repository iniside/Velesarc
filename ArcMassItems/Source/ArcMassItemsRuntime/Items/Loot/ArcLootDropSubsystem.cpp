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

#include "Items/Loot/ArcLootDropSubsystem.h"

#include "Items/Loot/ArcLootLogs.h"
#include "Items/Loot/ArcLootTable.h"
#include "Items/Loot/ArcLootFragments.h"
#include "MassEntityConfigAsset.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "StructUtils/InstancedStruct.h"
#include "Engine/World.h"
#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "MassWorldUtils.h"

DEFINE_LOG_CATEGORY(LogArcLootDrop);

void UArcLootDropSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

// TODO: DropLoot could reuse SpawnLootContainer for its spawn+populate tail.
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

	ARCLOOT_VLOG_LOCATION(World, Log, SpawnTransform.GetLocation(), 50.0f, FColor::Yellow,
		TEXT("Loot rolled at %s, %d drops"), *SpawnTransform.GetLocation().ToCompactString(), Drops.Num());

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
			UE_LOGF(LogArcLootDrop, Warning,"DropLoot: Drop has no LootEntityConfig and will be skipped.");
		}
	}

	if (GroupedDrops.IsEmpty())
	{
		return SpawnedEntities;
	}

	// 3. Get spawning infrastructure
	UArcMassSpawnerSubsystem* ArcSpawner = World->GetSubsystem<UArcMassSpawnerSubsystem>();
	if (!ArcSpawner)
	{
		return SpawnedEntities;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	MaxItemsPerEntity = FMath::Max(1, MaxItemsPerEntity);

	const FVector BaseLocation = SpawnTransform.GetLocation();
	const FRotator BaseRotation = SpawnTransform.GetRotation().Rotator();

	// Count total entities for scatter calculation
	int32 TotalEntities = 0;
	for (const TPair<const UMassEntityConfigAsset*, TArray<FArcLootDropResult*>>& Pair : GroupedDrops)
	{
		TotalEntities += FMath::DivideAndRoundUp(Pair.Value.Num(), MaxItemsPerEntity);
	}

	int32 GlobalEntityIdx = 0;

	// 4. For each config group, spawn entities and populate
	for (const TPair<const UMassEntityConfigAsset*, TArray<FArcLootDropResult*>>& Pair : GroupedDrops)
	{
		UArcMassEntityConfigAsset* ArcCfg = Cast<UArcMassEntityConfigAsset>(const_cast<UMassEntityConfigAsset*>(Pair.Key));
		if (ArcCfg == nullptr)
		{
			UE_LOGF(LogArcLootDrop, Warning,"DropLoot: EntityConfig is not UArcMassEntityConfigAsset; skipping group.");
			continue;
		}

		const TArray<FArcLootDropResult*>& GroupDrops = Pair.Value;
		const int32 NumEntities = FMath::DivideAndRoundUp(GroupDrops.Num(), MaxItemsPerEntity);

		for (int32 EntityIdx = 0; EntityIdx < NumEntities; ++EntityIdx)
		{
			FVector SpawnLocation = BaseLocation;
			if (TotalEntities > 1)
			{
				const float Angle = (static_cast<float>(GlobalEntityIdx) / static_cast<float>(TotalEntities)) * 2.0f * UE_PI;
				const float Radius = 50.0f + FMath::FRand() * 30.0f;
				SpawnLocation.X += FMath::Cos(Angle) * Radius;
				SpawnLocation.Y += FMath::Sin(Angle) * Radius;
			}

			const FMassEntityHandle Entity = ArcSpawner->SpawnEntity(ArcCfg, FTransform(BaseRotation, SpawnLocation));
			if (!Entity.IsValid())
			{
				continue;
			}

			// Populate loot container fragment.
			// Note: MoveTemp consumes ItemSpec from the Drops array — Drops must not be read after this loop.
			FMassEntityView LootEntityView(EntityManager, Entity);
			if (FArcLootContainerFragment* LootFragment = LootEntityView.GetSparseFragmentDataPtr<FArcLootContainerFragment>())
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

FMassEntityHandle UArcLootDropSubsystem::SpawnLootContainer(
	const UMassEntityConfigAsset* Config,
	const FTransform& Xform,
	TArray<FArcItemSpec>&& Specs)
{
	UArcMassEntityConfigAsset* ArcCfg = Cast<UArcMassEntityConfigAsset>(const_cast<UMassEntityConfigAsset*>(Config));
	if (ArcCfg == nullptr)
	{
		UE_LOGF(LogArcLootDrop, Warning,"SpawnLootContainer: Config is null or not a UArcMassEntityConfigAsset; returning invalid handle.");
		return FMassEntityHandle();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOGF(LogArcLootDrop, Warning,"SpawnLootContainer: no World; returning invalid handle.");
		return FMassEntityHandle();
	}

	UArcMassSpawnerSubsystem* ArcSpawner = World->GetSubsystem<UArcMassSpawnerSubsystem>();
	if (!ArcSpawner)
	{
		UE_LOGF(LogArcLootDrop, Warning,"SpawnLootContainer: no UArcMassSpawnerSubsystem; returning invalid handle.");
		return FMassEntityHandle();
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	const FMassEntityHandle Entity = ArcSpawner->SpawnEntity(ArcCfg, Xform);
	if (!Entity.IsValid())
	{
		UE_LOGF(LogArcLootDrop, Warning,"SpawnLootContainer: SpawnEntity returned an invalid handle.");
		return Entity;
	}

	FMassEntityView View(EntityManager, Entity);
	if (FArcLootContainerFragment* LootFragment = View.GetSparseFragmentDataPtr<FArcLootContainerFragment>())
	{
		LootFragment->Items.Reserve(Specs.Num());
		for (FArcItemSpec& Spec : Specs)
		{
			LootFragment->Items.Add(MoveTemp(Spec));
		}
	}
	else
	{
		UE_LOGF(LogArcLootDrop, Warning,"SpawnLootContainer: spill config has no FArcLootContainerFragment; specs not consumed.");
	}

	return Entity;
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
		UE_LOGF(LogArcLootDrop, Warning,"RollLootTable: No roll strategy assigned to loot table.");
		return Results;
	}

	const FArcLootRollStrategy* Strategy = LootTable->RollStrategy.GetPtr<FArcLootRollStrategy>();
	if (!Strategy)
	{
		UE_LOGF(LogArcLootDrop, Warning,"RollLootTable: Invalid roll strategy type.");
		return Results;
	}

	// Stamp the loot context with the entity subsystem + an evaluation outer so per-entry
	// FWorldConditionQuery evaluation has what it needs. Copy whatever the caller provided
	// (or default-construct a FArcLootContext if none), then mutate the stamped fields in place.
	FInstancedStruct LocalCtx;
	if (Context.IsValid() && Context.GetScriptStruct() != nullptr
		&& Context.GetScriptStruct()->IsChildOf(FArcLootContext::StaticStruct()))
	{
		LocalCtx.InitializeAs(Context.GetScriptStruct(), Context.GetMemory());
	}
	else
	{
		LocalCtx.InitializeAs<FArcLootContext>();
	}

	if (FArcLootContext* MutableCtx = LocalCtx.GetMutablePtr<FArcLootContext>())
	{
		if (const UWorld* World = GetWorld())
		{
			MutableCtx->MassEntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		}
		MutableCtx->EvaluationOuter = this;

		UE_LOGF(LogArcLootDrop, Verbose,"RollLootTable: table=%ls ctx=%ls Self=%ls Instigator=%ls SelfActor=%ls InstigatorActor=%ls subsystem=%ls",
			*GetNameSafe(LootTable),
			*GetNameSafe(LocalCtx.GetScriptStruct()),
			*MutableCtx->SelfEntity.DebugGetDescription(),
			*MutableCtx->InstigatorEntity.DebugGetDescription(),
			*GetNameSafe(MutableCtx->SelfActor.Get()),
			*GetNameSafe(MutableCtx->InstigatorActor.Get()),
			MutableCtx->MassEntitySubsystem.IsValid() ? TEXT("ok") : TEXT("NULL"));
	}
	else
	{
		UE_LOGF(LogArcLootDrop, Warning,"RollLootTable: table=%ls — LocalCtx is not an FArcLootContext (struct=%ls); conditions will PASS THROUGH",
			*GetNameSafe(LootTable), *GetNameSafe(LocalCtx.GetScriptStruct()));
	}

	Strategy->RollEntries(LootTable->Entries, FConstStructView(LocalCtx), Results);

	UE_LOGF(LogArcLootDrop, Verbose,"RollLootTable: table=%ls produced %d drop(s)",
		*GetNameSafe(LootTable), Results.Num());
	for (const FArcLootDropResult& Result : Results)
	{
		UE_LOGF(LogArcLootDrop, Verbose,"  drop: item=%ls x%d",
			*Result.ItemSpec.GetItemDefinitionId().ToString(), Result.ItemSpec.Amount);
	}

	return Results;
}
