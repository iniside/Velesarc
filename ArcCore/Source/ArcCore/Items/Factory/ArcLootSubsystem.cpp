/**
 * This file is part of ArcX.
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

#include "ArcCore/Items/Factory/ArcLootSubsystem.h"

#include "ArcItemSpecGeneratorDefinition.h"
#include "ArcCore/Items/ArcItemDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

FArcItemSpec FArcItemDataRowEntry::GetItem(AActor* From
	, APlayerController* For) const
{
	FArcItemSpec Spec;
	Spec.SetItemDefinition(ItemDefinitionId);
	return Spec;
}

FArcItemSpec FArcItemSpecGeneratorRowEntry::GetItem(AActor* From
	, APlayerController* For) const
{
	return FArcItemSpec() ;//ItemGenerator.Get<FArcItemSpecGenerator>().GenerateItemSpec(Entry, *this, From, For);
}

FArcSelectedItemSpecContainer FArcGetItemsFromTable_Random::GenerateItems(UArcItemListTable* InLootTable
																		  , AActor* From
																		  , APlayerController* For) const
{
	FArcSelectedItemSpecContainer Values;

	const int32 NumEntries = InLootTable->ItemSpawnDataRows.Num();
	if (NumEntries == 0)
	{
		return Values;
	}

	TArray<int32> SelectedEntries;
	SelectedEntries.Reserve(MaxItems + 1);
	
	for (int32 ItemIdx = 0; ItemIdx < MaxItems; ItemIdx++)
	{
		SelectedEntries.Add(FMath::RandRange(0, NumEntries - 1));
	}

	// That might be even to much ?
	Values.SelectedItems.Reserve(16);
	for (int32 EntryIdx : SelectedEntries)
	{
		const FArcItemSpawnDataRow& ItemRow = InLootTable->ItemSpawnDataRows[EntryIdx];
		
		TArray<FArcItemSpec> ItemSpecs;
		ItemRow.ItemGenerator->GetItems(ItemSpecs, From, For);
		Values.SelectedItems.Append(MoveTemp(ItemSpecs));
	}
	
	return Values;
}

void FArcItemGenerator_SingleItem::GenerateItems(TArray<FArcItemSpec>& OutItems
	, const UArcItemGeneratorDefinition* InDef
	, AActor* From
	, APlayerController* For) const
{
	const int32 NumDef = InDef->ItemDefinitions.Num();
	if (NumDef == 0)
	{
		return;
	}
	const int32 DefIdx = FMath::RandRange(0, NumDef - 1);
	OutItems.Reset(1);

	FArcItemSpec Spec;
	Spec.SetItemDefinition(InDef->ItemDefinitions[DefIdx]);
	
	OutItems.Add(MoveTemp(Spec));
}

void UArcItemGeneratorDefinition::GetItems(TArray<FArcItemSpec>& OutItems
										   , AActor* From
										   , APlayerController* For) const
{
	ItemGenerator.Get<FArcItemGenerator>().GenerateItems(OutItems, this, From, For);
}

void UArcLootSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

FArcSelectedItemSpecContainer UArcLootSubsystem::GenerateItemsFor(UArcItemListTable* LootTable
								  , AActor* From
								  , APlayerController* For)
{
	FArcSelectedItemSpecContainer GenItems = LootTable->ItemGetter.GetPtr<FArcGetItemsFromTable>()->GenerateItems(
		LootTable
		, From
		, For);

	return GenItems;
}

