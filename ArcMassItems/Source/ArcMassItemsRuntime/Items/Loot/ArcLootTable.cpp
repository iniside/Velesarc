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

#include "Items/Loot/ArcLootTable.h"
#include "Items/Loot/ArcLootLogs.h"
#include "Items/Loot/ArcLootWorldConditionSchema.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Actor.h"
#include "MassEntitySubsystem.h"
#include "UObject/UnrealType.h"
#include "WorldConditionContext.h"
#include "WorldConditionSchema.h"


DEFINE_LOG_CATEGORY(LogArcLoot);

// ---------------------------------------------------------------------------
// Schema stamping helper
// ---------------------------------------------------------------------------

namespace ArcLootTableSchemaInternal
{
	void StampEntrySchemas(UArcLootTable& Table)
	{
		for (FInstancedStruct& EntryStruct : Table.Entries)
		{
			FArcLootTableEntry* Entry = EntryStruct.GetMutablePtr<FArcLootTableEntry>();
			if (Entry == nullptr)
			{
				continue;
			}

			if (Entry->Conditions.GetSchemaClass() != UArcLootWorldConditionSchema::StaticClass())
			{
				Entry->Conditions.SetSchemaClass(UArcLootWorldConditionSchema::StaticClass());
			}

			// Always recompile: SharedDefinition is a snapshot of EditableConditions taken at
			// Initialize() time, so skipping when the schema is already stamped would leave edits
			// to existing conditions evaluating against the stale compiled query. In cooked builds
			// Initialize's default mode preserves the serialized SharedDefinition when
			// EditableConditions is empty (stripped on cook).
			Entry->Conditions.Initialize(&Table);
		}
	}
}

namespace ArcLootConditionEval
{
	bool Passes(const FWorldConditionQueryDefinition& Conditions, const FArcLootContext& LootCtx)
	{
		if (!Conditions.IsValid())
		{
			// IsValid() false only means there is no compiled SharedDefinition — legitimate when
			// the designer authored no conditions, but a stale/never-compiled query when
			// EditableConditions is non-empty. Fail closed in the latter case so a broken filter
			// excludes the drop instead of leaking it. EditableConditions has no public accessor,
			// so read it via reflection (the property exists in all builds; cook strips its content).
			static const FArrayProperty* EditableConditionsProperty = CastField<FArrayProperty>(
				FWorldConditionQueryDefinition::StaticStruct()->FindPropertyByName(TEXT("EditableConditions")));
			if (EditableConditionsProperty != nullptr)
			{
				FScriptArrayHelper ArrayHelper(EditableConditionsProperty,
					EditableConditionsProperty->ContainerPtrToValuePtr<void>(&Conditions));
				if (ArrayHelper.Num() > 0)
				{
					UE_LOGF(LogArcLoot, Warning,"Entry has %d authored condition(s) but no compiled SharedDefinition — FAIL closed (re-save the loot table asset)",
						ArrayHelper.Num());
					return false;
				}
			}
			UE_LOGF(LogArcLoot, Verbose,"No conditions on entry — pass through");
			return true;
		}
		const TSubclassOf<UWorldConditionSchema> SchemaClass = Conditions.GetSchemaClass();
		if (!SchemaClass)
		{
			UE_LOGF(LogArcLoot, Warning,"Conditions present but no schema stamped — pass through (should not happen after schema-stamp fix)");
			return true;
		}
		const UWorldConditionSchema* Schema = GetDefault<UWorldConditionSchema>(SchemaClass);
		if (Schema == nullptr)
		{
			UE_LOGF(LogArcLoot, Warning,"Schema CDO null for class %ls — pass through", *GetNameSafe(SchemaClass));
			return true;
		}
		const UObject* Outer = LootCtx.EvaluationOuter.Get();
		if (Outer == nullptr)
		{
			UE_LOGF(LogArcLoot, Warning,"EvaluationOuter not set on FArcLootContext — pass through");
			return true;
		}

		FWorldConditionContextData ContextData(*Schema);
		ContextData.SetContextData(TEXT("SelfEntity"),          &LootCtx.SelfEntity);
		ContextData.SetContextData(TEXT("InstigatorEntity"),    &LootCtx.InstigatorEntity);
		ContextData.SetContextData(TEXT("SelfActor"),           LootCtx.SelfActor.Get());
		ContextData.SetContextData(TEXT("InstigatorActor"),     LootCtx.InstigatorActor.Get());
		ContextData.SetContextData(TEXT("MassEntitySubsystem"), LootCtx.MassEntitySubsystem.Get());

		FWorldConditionQueryState QueryState;
		QueryState.Initialize(*Outer, Conditions);
		FWorldConditionContext Context(QueryState, ContextData);
		if (!Context.Activate())
		{
			UE_LOGF(LogArcLoot, Warning,"FWorldConditionContext::Activate failed — pass through");
			return true;
		}
		const bool bResult = Context.IsTrue();
		UE_LOGF(LogArcLoot, Verbose,"Condition eval: %ls", bResult ? TEXT("PASS") : TEXT("FAIL"));
		Context.Deactivate();
		return bResult;
	}
}

// --- FArcLootTableEntry ---

bool FArcLootTableEntry::PassesConditions(FConstStructView Context) const
{
	const FArcLootContext* LootCtx = Context.GetPtr<const FArcLootContext>();
	if (LootCtx == nullptr)
	{
		// This is the only branch that admits an entry without evaluating its conditions
		// silently. Warn loudly: a null/wrong-typed context at runtime means every entry
		// passes regardless of its tag query.
		UE_LOGF(LogArcLoot, Warning,"PassesConditions[%ls]: context view is null or not an FArcLootContext (struct=%ls) — PASS THROUGH without evaluating conditions",
			*ItemDefinitionId.ToString(),
			*GetNameSafe(Context.GetScriptStruct()));
		return true;
	}
	return ArcLootConditionEval::Passes(Conditions, *LootCtx);
}

void FArcLootTableEntry::CalculateDrops(FConstStructView Context, TArray<FArcLootDropResult>& OutDrops) const
{
	if (!PassesConditions(Context))
	{
		UE_LOGF(LogArcLoot, Verbose,"CalculateDrops[%ls]: conditions FAILED — no drop", *ItemDefinitionId.ToString());
		return;
	}

	if (!ItemDefinitionId.IsValid())
	{
		UE_LOGF(LogArcLoot, Verbose,"CalculateDrops: entry has invalid ItemDefinitionId — no drop");
		return;
	}

	// Probability gate
	const float Probability = GetDropProbability();
	if (Probability < 1.0f)
	{
		const float Roll = FMath::FRand();
		if (Roll >= Probability)
		{
			UE_LOGF(LogArcLoot, Verbose,"CalculateDrops[%ls]: probability gate failed (roll %.3f >= %.3f) — no drop",
				*ItemDefinitionId.ToString(), Roll, Probability);
			return;
		}
	}

	const int32 Amount = FMath::RandRange(MinAmount, MaxAmount);
	UE_LOGF(LogArcLoot, Verbose,"CalculateDrops[%ls]: DROP x%d", *ItemDefinitionId.ToString(), Amount);

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
			const bool bPasses = W > 0.0f && Entry->PassesConditions(Context);
			UE_LOGF(LogArcLoot, Verbose,"WeightedSelection: entry %d [%ls] weight=%.2f eligible=%ls",
				Idx, *Entry->ItemDefinitionId.ToString(), W, bPasses ? TEXT("YES") : TEXT("no"));
			if (bPasses)
			{
				WeightedEntries.Add({Idx, W});
				TotalWeight += W;
			}
		}
	}

	UE_LOGF(LogArcLoot, Verbose,"WeightedSelection: %d/%d entries eligible, total weight %.2f, MaxDrops=%d",
		WeightedEntries.Num(), Entries.Num(), TotalWeight, MaxDrops);

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
			UE_LOGF(LogArcLoot, Verbose,"WeightedSelection: picked entry %d [%ls]",
				EntryIdx, *Entry->ItemDefinitionId.ToString());
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

void UArcLootTable::PostLoad()
{
	Super::PostLoad();
	ArcLootTableSchemaInternal::StampEntrySchemas(*this);
}

void UArcLootTable::PostInitProperties()
{
	Super::PostInitProperties();

	if (!LootTableId.IsValid())
	{
		LootTableId = FGuid::NewGuid();
	}

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		ArcLootTableSchemaInternal::StampEntrySchemas(*this);
	}
}

#if WITH_EDITOR
void UArcLootTable::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	// Re-stamp unconditionally: the helper is idempotent (no-op when schema already correct),
	// and nested edits on FInstancedStruct arrays surface as inner-property changes, not "Entries"
	// changes, so a name-based filter on PropertyChangedEvent.Property would miss authoring events.
	ArcLootTableSchemaInternal::StampEntrySchemas(*this);
}
#endif

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
		UE_LOGF(LogArcLoot, Warning,"DebugRollLootTable: No entries in loot table.");
		return;
	}

	if (!RollStrategy.IsValid())
	{
		UE_LOGF(LogArcLoot, Warning,"DebugRollLootTable: No roll strategy assigned.");
		return;
	}

	const FArcLootRollStrategy* Strategy = RollStrategy.GetPtr<FArcLootRollStrategy>();
	if (!Strategy)
	{
		UE_LOGF(LogArcLoot, Warning,"DebugRollLootTable: Invalid roll strategy type.");
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
	UE_LOGF(LogArcLoot, Log,"=== Loot Table Debug Roll (%d rolls) ===", DebugRollCount);
	for (int32 Idx = 0; Idx < Stats.Num(); ++Idx)
	{
		const FEntryStats& S = Stats[Idx];
		const float DropRate = (static_cast<float>(S.TimesDropped) / static_cast<float>(DebugRollCount)) * 100.0f;
		const float AvgAmount = S.TimesDropped > 0 ? static_cast<float>(S.TotalAmount) / static_cast<float>(S.TimesDropped) : 0.0f;
		UE_LOGF(LogArcLoot, Log,"  [%d] %ls - Drop Rate: %.1f%% | Avg Amount: %.1f | Times Dropped: %d",
			Idx, *S.ItemName, DropRate, AvgAmount, S.TimesDropped);
	}
	UE_LOGF(LogArcLoot, Log,"=== End Debug Roll ===");
}
#endif
