/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#include "ArcCraft/Shared/ArcCraftModifier.h"

#include "ArcCraft/ArcCraftTags.h"
#include "ArcCraft/Rarity/ArcCraftRarity.h"
#include "ArcCraft/Rarity/ArcCraftRulesData.h"
#include "ArcCraft/Rarity/ArcStatCatalogData.h"
#include "ArcCraft/Recipe/ArcCraftContext.h"
#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinitionData.h"
#include "ArcCraft/Recipe/ArcRandomPoolEvaluation.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcCraftModifier, Log, All);

// -------------------------------------------------------------------
// FArcCraftModifier (base)
// -------------------------------------------------------------------

bool FArcCraftModifier::MatchesTriggerTags(const FGameplayTagContainer& IngredientTags) const
{
	if (TriggerTags.IsEmpty())
	{
		return true; // Empty = always match
	}
	return IngredientTags.HasAll(TriggerTags);
}

// -------------------------------------------------------------------
// ArcCraftModifiers::ConvertModifier
// -------------------------------------------------------------------

const FArcStatCatalogEntry* ArcCraftModifiers::ResolveStat(const FArcCraftRulesData* Rules, FName StatId)
{
	const FArcStatCatalogData* Catalog = Rules != nullptr ? ArcCraftRecords::Find<FArcStatCatalogData>(Rules->StatCatalog) : nullptr;
	if (Catalog == nullptr)
	{
		static bool bWarnedNoCatalog = false;
		if (!bWarnedNoCatalog)
		{
			bWarnedNoCatalog = true;
			UE_LOGF(LogArcCraftModifier, Warning, "ArcCraft: stat modifiers skipped, no craft rules or rules StatCatalog not found");
		}
		return nullptr;
	}

	const FArcStatCatalogEntry* Entry = Catalog->Entries.FindByPredicate(
		[StatId](const FArcStatCatalogEntry& Candidate) -> bool
		{
			return Candidate.StatId == StatId;
		});
	if (Entry == nullptr || !Entry->Attribute.IsValid())
	{
		static TSet<FName> WarnedStatIds;
		if (!WarnedStatIds.Contains(StatId))
		{
			WarnedStatIds.Add(StatId);
			UE_LOGF(LogArcCraftModifier, Warning, "ArcCraft: stat modifier skipped, StatId '%ls' not in the stat catalog or without attribute", *StatId.ToString());
		}
		return nullptr;
	}
	return Entry;
}

namespace ArcCraftModifiersImpl
{
	bool ConvertStat(const FArcCraftModifier_Stats& StatMod, const FArcCraftContext& Context, FArcCraftModifierResult& OutResult)
	{
		const FArcCraftRulesData* Rules = ArcCraftRarity::GetRules();
		const FArcStatCatalogEntry* Stat = ArcCraftModifiers::ResolveStat(Rules, StatMod.StatId);
		if (Stat == nullptr)
		{
			return false;
		}

		const float Value = ArcCraftRarity::ComputeBandValue(
			Stat->Min,
			Stat->Max,
			Rules,
			Context.OutputRarity,
			Context.AverageR,
			Rules->BandBiasPerR,
			Rules->BandBiasPerSkillMargin,
			Context.SkillMargin,
			Context.Stream.FRand());
		const bool bPenalty = StatMod.SlotTags.HasTag(ArcCraftTags::Modifier_Penalty);

		if (Stat->bItemSide)
		{
			OutResult.Type = EArcCraftModifierResultType::Stat;
			OutResult.Stat = FArcItemAttributeStat(Stat->Attribute, bPenalty ? 1.0f - Value : 1.0f + Value);
			OutResult.Stat.Type = EArcModType::Multiply;
			return true;
		}

		FArcDirectModifier& Modifier = OutResult.AttributeModifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Stat->Attribute;
		Modifier.Operation = EArcModifierOp::Add;
		Modifier.Magnitude = (bPenalty ? -Value : Value) * Stat->Sign;
		Modifier.SourceTagReqs = Stat->SourceTagReqs;
		Modifier.TargetTagReqs = Stat->TargetTagReqs;
		OutResult.Type = EArcCraftModifierResultType::AttributeModifier;
		return true;
	}
}

void ArcCraftModifiers::ConvertModifier(
	const FInstancedStruct& Modifier,
	const FGameplayTagContainer& IngredientTags,
	const FArcCraftContext& Context,
	TArray<FArcCraftPendingModifier>& OutPending)
{
	const FArcCraftModifier* BaseMod = Modifier.GetPtr<FArcCraftModifier>();
	if (BaseMod == nullptr)
	{
		return;
	}
	if (BaseMod->MinR > 0.0f && Context.AverageR < BaseMod->MinR)
	{
		return;
	}
	if (!BaseMod->MatchesTriggerTags(IngredientTags))
	{
		return;
	}

	if (const FArcCraftModifier_RandomPool* PoolMod = Modifier.GetPtr<FArcCraftModifier_RandomPool>())
	{
		const FArcRandomPoolDefinitionData* PoolDef = ArcCraftRecords::Find<FArcRandomPoolDefinitionData>(PoolMod->PoolDefinition);
		const FArcRandomPoolSelectionMode* Mode = PoolMod->SelectionMode.GetPtr<FArcRandomPoolSelectionMode>();
		if (PoolDef == nullptr || Mode == nullptr || PoolDef->Entries.Num() == 0)
		{
			return;
		}

		const TArray<int32> SelectedIndices = ArcRandomPoolEval::SelectEntries(*PoolDef, *Mode, Context.AverageR, IngredientTags, Context.Stream);
		for (const int32 SelectedIdx : SelectedIndices)
		{
			if (!PoolDef->Entries.IsValidIndex(SelectedIdx))
			{
				continue;
			}
			for (const FInstancedStruct& EntryModifier : PoolDef->Entries[SelectedIdx].Modifiers)
			{
				ConvertModifier(EntryModifier, IngredientTags, Context, OutPending);
			}
		}
		return;
	}

	FArcCraftModifierResult Result;
	if (const FArcCraftModifier_Stats* StatMod = Modifier.GetPtr<FArcCraftModifier_Stats>())
	{
		if (!ArcCraftModifiersImpl::ConvertStat(*StatMod, Context, Result))
		{
			return;
		}
	}
	else if (const FArcCraftModifier_Abilities* AbilMod = Modifier.GetPtr<FArcCraftModifier_Abilities>())
	{
		if (!AbilMod->AbilityEntry.AbilityDefinition)
		{
			return;
		}
		Result.Type = EArcCraftModifierResultType::Ability;
		Result.Ability = AbilMod->AbilityEntry;
	}
	else if (const FArcCraftModifier_Effects* EffMod = Modifier.GetPtr<FArcCraftModifier_Effects>())
	{
		if (!EffMod->Effect)
		{
			return;
		}
		Result.Type = EArcCraftModifierResultType::Effect;
		Result.Effect = EffMod->Effect;
	}
	else if (const FArcCraftModifier_AttributeModifier* AttrMod = Modifier.GetPtr<FArcCraftModifier_AttributeModifier>())
	{
		if (!AttrMod->Modifier.Attribute.IsValid())
		{
			return;
		}
		Result.Type = EArcCraftModifierResultType::AttributeModifier;
		Result.AttributeModifiers.Add(AttrMod->Modifier);
	}
	else
	{
		return;
	}

	FArcCraftPendingModifier& Pending = OutPending.AddDefaulted_GetRef();
	Pending.SlotTags = BaseMod->SlotTags;
	Pending.Result = MoveTemp(Result);
}
