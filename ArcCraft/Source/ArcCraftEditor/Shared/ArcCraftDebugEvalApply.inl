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
 * distributed under the LICENSE is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

// ArcCraftDebugEvalApply.inl
//
// Included directly by ArcMaterialPropertyTable_DebugEval.cpp,
// ArcRandomPool_DebugEval.cpp, and ArcStaticModifierSet_DebugEval.cpp.
//
// Each of those .cpp files must include the required headers BEFORE this .inl:
//   #include "Abilities/ArcAbilityDefinition.h"
//   #include "Effects/ArcEffectDefinition.h"
//   #include "ArcCraft/Shared/ArcCraftModifier.h"
//
// The function is defined as static so each including TU gets its own copy
// with no ODR conflict. Identical code will be ICF-folded by the linker.
//
// Guard against double-inclusion in unity builds (multiple DebugEval .cpp files
// may end up in the same Module.*.cpp unity translation unit).
#ifndef ARC_CRAFT_DEBUG_EVAL_APPLY_INL_INCLUDED
#define ARC_CRAFT_DEBUG_EVAL_APPLY_INL_INCLUDED

#include "ArcCraft/Rarity/ArcCraftRarity.h"
#include "ArcCraft/Rarity/ArcCraftRulesData.h"
#include "ArcCraft/Rarity/ArcStatCatalogData.h"
#include "ArcCraft/Recipe/ArcCraftContext.h"
#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "Math/RandomStream.h"

namespace ArcCraftDebugEval
{

/**
 * Fill the band edges of a stat modifier for the debug output rarity: ArcCraftRarity::ComputeBandValue at
 * U = 0 and U = 1. Leaves bHasBand false for other modifier types or a stat ArcCraftModifiers::ResolveStat rejects.
 */
static void FillStatBand(
	const FInstancedStruct& ModStruct,
	const FArcModifierDebugContext& DebugCtx,
	float AverageR,
	FArcModifierRowEvaluation& OutRow)
{
	const FArcCraftModifier_Stats* StatMod = ModStruct.GetPtr<FArcCraftModifier_Stats>();
	if (StatMod == nullptr)
	{
		return;
	}
	const FArcCraftRulesData* Rules = ArcCraftRarity::GetRules();
	const FArcStatCatalogEntry* Stat = ArcCraftModifiers::ResolveStat(Rules, StatMod->StatId);
	if (Stat == nullptr)
	{
		return;
	}

	OutRow.bHasBand = true;
	OutRow.BandAtU0 = ArcCraftRarity::ComputeBandValue(Stat->Min, Stat->Max, Rules, DebugCtx.OutputRarity,
		AverageR, Rules->BandBiasPerR, Rules->BandBiasPerSkillMargin, DebugCtx.SkillMargin, 0.0f);
	OutRow.BandAtU1 = ArcCraftRarity::ComputeBandValue(Stat->Min, Stat->Max, Rules, DebugCtx.OutputRarity,
		AverageR, Rules->BandBiasPerR, Rules->BandBiasPerSkillMargin, DebugCtx.SkillMargin, 1.0f);
}

/**
 * Run the runtime conversion (ArcCraftModifiers::ConvertModifier) on one modifier with a fresh
 * random stream, the debug output rarity and skill margin, and describe each result.
 */
static TArray<FText> CollectApplyOutputs(
	const FInstancedStruct& ModStruct,
	const FGameplayTagContainer& AggregatedTags,
	const FArcModifierDebugContext& DebugCtx,
	float AverageR)
{
	TArray<FText> Lines;

	FArcCraftContext Context;
	Context.AverageR = AverageR;
	Context.SkillMargin = DebugCtx.SkillMargin;
	Context.OutputRarity = DebugCtx.OutputRarity;
	Context.Stream = FRandomStream(FMath::Rand32());

	TArray<FArcCraftPendingModifier> Pending;
	ArcCraftModifiers::ConvertModifier(ModStruct, AggregatedTags, Context, Pending);

	for (const FArcCraftPendingModifier& Entry : Pending)
	{
		const FArcCraftModifierResult& Result = Entry.Result;
		switch (Result.Type)
		{
		case EArcCraftModifierResultType::Stat:
			{
				FString OpStr;
				switch (Result.Stat.Type)
				{
				case EArcModType::Additive:    OpStr = TEXT("Additive"); break;
				case EArcModType::Multiply:    OpStr = TEXT("Multiply"); break;
				case EArcModType::Division:    OpStr = TEXT("Division"); break;
				default:                       OpStr = TEXT("?"); break;
				}
				Lines.Add(FText::FromString(FString::Printf(TEXT("Stat: %s %+.3f (%s)"),
					*Result.Stat.Stat.PropertyName.ToString(), Result.Stat.Value.GetValue(), *OpStr)));
			}
			break;

		case EArcCraftModifierResultType::Ability:
			{
				const UObject* AbilObj = Result.Ability.AbilityDefinition.Get();
				const FString AbilName = AbilObj ? AbilObj->GetName() : TEXT("(null)");
				Lines.Add(FText::FromString(FString::Printf(TEXT("Ability: %s"), *AbilName)));
			}
			break;

		case EArcCraftModifierResultType::Effect:
			{
				const UObject* EffectObj = Result.Effect.Get();
				const FString EffectName = EffectObj ? EffectObj->GetName() : TEXT("(null)");
				Lines.Add(FText::FromString(FString::Printf(TEXT("Effect: %s"), *EffectName)));
			}
			break;

		case EArcCraftModifierResultType::AttributeModifier:
			for (const FArcDirectModifier& DM : Result.AttributeModifiers)
			{
				FString OpStr;
				switch (DM.Operation)
				{
				case EArcModifierOp::Add:              OpStr = TEXT("Add"); break;
				case EArcModifierOp::MultiplyAdditive:  OpStr = TEXT("MulAdd"); break;
				case EArcModifierOp::DivideAdditive:    OpStr = TEXT("DivAdd"); break;
				case EArcModifierOp::MultiplyCompound:  OpStr = TEXT("MulComp"); break;
				case EArcModifierOp::AddFinal:          OpStr = TEXT("AddFinal"); break;
				case EArcModifierOp::Override:          OpStr = TEXT("Override"); break;
				default:                                OpStr = TEXT("?"); break;
				}
				Lines.Add(FText::FromString(FString::Printf(TEXT("Attribute: %s %+.3f (%s)"),
					*DM.Attribute.PropertyName.ToString(), DM.Magnitude, *OpStr)));
			}
			break;

		default:
			break;
		}
	}

	if (Lines.IsEmpty())
	{
		Lines.Add(FText::FromString(TEXT("(no output — base modifier or unrecognized type)")));
	}

	return Lines;
}

} // namespace ArcCraftDebugEval

#endif // ARC_CRAFT_DEBUG_EVAL_APPLY_INL_INCLUDED
