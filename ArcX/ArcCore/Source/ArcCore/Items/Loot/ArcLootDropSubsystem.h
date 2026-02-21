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

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "MassEntityHandle.h"
#include "StructUtils/StructView.h"
#include "ArcLootDropSubsystem.generated.h"

class UArcLootTable;
struct FArcLootDropResult;

/**
 * World subsystem managing loot table rolling and Mass entity spawning.
 * Does not hardcode when loot is dropped — user code calls this manually.
 */
UCLASS()
class ARCCORE_API UArcLootDropSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Roll a loot table and spawn loot entities at the given transform.
	 * Entity config is read per-drop from each FArcLootTableEntry.
	 * Drops are grouped by entity config; each group spawns its own entities.
	 *
	 * @param LootTable         The loot table to roll.
	 * @param Context           Context for loot calculation (FConstStructView of FArcLootContext or derived).
	 * @param SpawnTransform    Base transform for spawned loot entities.
	 * @param MaxItemsPerEntity Max items per loot entity. Overflow spawns additional entities.
	 * @return Array of spawned entity handles.
	 */
	TArray<FMassEntityHandle> DropLoot(
		const UArcLootTable* LootTable,
		FConstStructView Context,
		const FTransform& SpawnTransform,
		int32 MaxItemsPerEntity = 1);

	/**
	 * Roll a loot table without spawning entities. Useful for preview/debug.
	 * Delegates to the table's roll strategy.
	 * @param LootTable The loot table to roll.
	 * @param Context   Context for loot calculation.
	 * @return Array of drop results.
	 */
	TArray<FArcLootDropResult> RollLootTable(
		const UArcLootTable* LootTable,
		FConstStructView Context) const;
};
