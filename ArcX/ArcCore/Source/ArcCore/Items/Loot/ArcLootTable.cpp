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

#include "ArcCore/Items/Loot/ArcLootTable.h"
#include "Items/ArcItemDefinition.h"
#include "Engine/AssetManager.h"


DEFINE_LOG_CATEGORY_STATIC(LogArcLoot, Log, All);

// --- FArcLootTableEntry ---

void FArcLootTableEntry::CalculateDrops(FConstStructView Context, TArray<FArcLootDropResult>& OutDrops) const
{
	if (!ItemDefinitionId.IsValid())
	{
		return;
	}

	// Probability gate
	const float Probability = GetDropProbability();
	if (Probability < 1.0f)
	{
		const float Roll = FMath::FRand();
		if (Roll >= Probability)
		{
			return;
		}
	}

	const int32 Amount = FMath::RandRange(MinAmount, MaxAmount);

	FArcLootDropResult Result;
	Result.ItemSpec.SetItemDefinition(ItemDefinitionId);
	Result.ItemSpec.SetAmount(Amount);
	Result.LootEntityConfig = LootEntityConfig;
	OutDrops.Add(MoveTemp(Result));
}

// --- FArcLootRollStrategy ---

void FArcLootRollStrategy::RollEntries(
	const TArray<FInstancedStruct>& Entries,
	FConstStructView Context,
	TArray<FArcLootDropResult>& OutDrops) const
{
	for (const FInstancedStruct& EntryStruct : Entries)
	{
		if (const FArcLootTableEntry* Entry = EntryStruct.GetPtr<FArcLootTableEntry>())
		{
			Entry->CalculateDrops(Context, OutDrops);
		}
	}
}

// --- FArcLootRollStrategy_WeightedSelection ---

void FArcLootRollStrategy_WeightedSelection::RollEntries(
	const TArray<FInstancedStruct>& Entries,
	FConstStructView Context,
	TArray<FArcLootDropResult>& OutDrops) const
{
	struct FWeightedEntry
	{
		int32 EntryIndex;
		float Weight;
	};

	TArray<FWeightedEntry> WeightedEntries;
	float TotalWeight = 0.0f;

	for (int32 Idx = 0; Idx < Entries.Num(); ++Idx)
	{
		if (const FArcLootTableEntry* Entry = Entries[Idx].GetPtr<FArcLootTableEntry>())
		{
			const float W = Entry->GetWeight();
			if (W > 0.0f)
			{
				WeightedEntries.Add({Idx, W});
				TotalWeight += W;
			}
		}
	}

	if (WeightedEntries.IsEmpty() || TotalWeight <= 0.0f)
	{
		return;
	}

	// Select up to MaxDrops entries via weighted random without replacement
	const int32 NumToSelect = FMath::Min(MaxDrops, WeightedEntries.Num());

	for (int32 SelectIdx = 0; SelectIdx < NumToSelect && !WeightedEntries.IsEmpty(); ++SelectIdx)
	{
		float Roll = FMath::FRand() * TotalWeight;
		int32 PickedIdx = WeightedEntries.Num() - 1; // fallback to last for float precision

		for (int32 WIdx = 0; WIdx < WeightedEntries.Num(); ++WIdx)
		{
			Roll -= WeightedEntries[WIdx].Weight;
			if (Roll <= 0.0f)
			{
				PickedIdx = WIdx;
				break;
			}
		}

		const int32 EntryIdx = WeightedEntries[PickedIdx].EntryIndex;

		if (const FArcLootTableEntry* Entry = Entries[EntryIdx].GetPtr<FArcLootTableEntry>())
		{
			Entry->CalculateDrops(Context, OutDrops);
		}

		// Remove selected (without replacement)
		TotalWeight -= WeightedEntries[PickedIdx].Weight;
		WeightedEntries.RemoveAtSwap(PickedIdx);
	}
}

// --- UArcLootTable ---

UArcLootTable::UArcLootTable()
{
	LootTableType = FPrimaryAssetType(TEXT("ArcLootTable"));
}

FPrimaryAssetId UArcLootTable::GetPrimaryAssetId() const
{
	if (!LootTableId.IsValid())
	{
		return FPrimaryAssetId();
	}
	return FPrimaryAssetId(LootTableType, *LootTableId.ToString(EGuidFormats::DigitsWithHyphens));
}

void UArcLootTable::PostInitProperties()
{
	Super::PostInitProperties();

	if (!LootTableId.IsValid())
	{
		LootTableId = FGuid::NewGuid();
	}
}

void UArcLootTable::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	if (DuplicateMode == EDuplicateMode::Normal)
	{
		LootTableId = FGuid::NewGuid();
	}
}

void UArcLootTable::RegenerateLootTableId()
{
	LootTableId = FGuid::NewGuid();
	MarkPackageDirty();
}

#if WITH_EDITORONLY_DATA
void UArcLootTable::DebugRollLootTable()
{
	if (Entries.IsEmpty())
	{
		UE_LOG(LogArcLoot, Warning, TEXT("DebugRollLootTable: No entries in loot table."));
		return;
	}

	if (!RollStrategy.IsValid())
	{
		UE_LOG(LogArcLoot, Warning, TEXT("DebugRollLootTable: No roll strategy assigned."));
		return;
	}

	const FArcLootRollStrategy* Strategy = RollStrategy.GetPtr<FArcLootRollStrategy>();
	if (!Strategy)
	{
		UE_LOG(LogArcLoot, Warning, TEXT("DebugRollLootTable: Invalid roll strategy type."));
		return;
	}

	FConstStructView ContextView;
	if (DebugContext.IsValid())
	{
		ContextView = DebugContext;
	}

	// Track per-entry stats
	struct FEntryStats
	{
		int32 TimesDropped = 0;
		int32 TotalAmount = 0;
		FString ItemName;
	};

	TArray<FEntryStats> Stats;
	Stats.SetNum(Entries.Num());

	for (int32 EntryIdx = 0; EntryIdx < Entries.Num(); ++EntryIdx)
	{
		if (const FArcLootTableEntry* Entry = Entries[EntryIdx].GetPtr<FArcLootTableEntry>())
		{
			Stats[EntryIdx].ItemName = Entry->ItemDefinitionId.ToString();
		}
	}

	// Simulate rolls
	for (int32 RollIdx = 0; RollIdx < DebugRollCount; ++RollIdx)
	{
		TArray<FArcLootDropResult> Drops;
		Strategy->RollEntries(Entries, ContextView, Drops);

		// Map drops back to entry indices for stats
		for (const FArcLootDropResult& Drop : Drops)
		{
			for (int32 EntryIdx = 0; EntryIdx < Entries.Num(); ++EntryIdx)
			{
				if (const FArcLootTableEntry* Entry = Entries[EntryIdx].GetPtr<FArcLootTableEntry>())
				{
					if (Entry->ItemDefinitionId == Drop.ItemSpec.GetItemDefinitionId())
					{
						Stats[EntryIdx].TimesDropped++;
						Stats[EntryIdx].TotalAmount += Drop.ItemSpec.Amount;
						break;
					}
				}
			}
		}
	}

	// Log results
	UE_LOG(LogArcLoot, Log, TEXT("=== Loot Table Debug Roll (%d rolls) ==="), DebugRollCount);
	for (int32 Idx = 0; Idx < Stats.Num(); ++Idx)
	{
		const FEntryStats& S = Stats[Idx];
		const float DropRate = (static_cast<float>(S.TimesDropped) / static_cast<float>(DebugRollCount)) * 100.0f;
		const float AvgAmount = S.TimesDropped > 0 ? static_cast<float>(S.TotalAmount) / static_cast<float>(S.TimesDropped) : 0.0f;
		UE_LOG(LogArcLoot, Log, TEXT("  [%d] %s - Drop Rate: %.1f%% | Avg Amount: %.1f | Times Dropped: %d"),
			Idx, *S.ItemName, DropRate, AvgAmount, S.TimesDropped);
	}
	UE_LOG(LogArcLoot, Log, TEXT("=== End Debug Roll ==="));
}
#endif
