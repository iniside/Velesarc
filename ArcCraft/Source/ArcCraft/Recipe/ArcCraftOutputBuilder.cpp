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

#include "ArcCraft/Recipe/ArcCraftOutputBuilder.h"

#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "ItemFragments/ArcItemFragment_ItemStats.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "ItemFragments/ArcItemFragment_GrantedMassAbilities.h"
#include "ItemFragments/ArcItemFragment_GrantedMassEffects.h"
#include "ItemFragments/ArcItemFragment_MassAttributeModifier.h"
#include "Items/Fragments/ArcItemFragment_InstanceTags.h"
#include "Items/Fragments/ArcItemFragment_Rarity.h"
#include "ArcCraft/Rarity/ArcCraftRarity.h"
#include "ArcCraft/Rarity/ArcCraftRulesData.h"
#include "ArcCraft/Recipe/ArcCraftContext.h"
#include "ArcCraft/Recipe/ArcCraftModifierSlot.h"
#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"
#include "ArcCraft/Recipe/ArcCraftSlotSelector.h"
#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Recipe/ArcRecipeModifierSource.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "ArcCraft/Station/ArcRecipeLookup.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcCraftOutputBuilder, Log, All);

static void ApplyResult(FArcItemSpec& OutSpec, const FArcCraftModifierResult& Result)
{
	switch (Result.Type)
	{
	case EArcCraftModifierResultType::Stat:
		{
			FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			if (!StatsFragment)
			{
				StatsFragment = new FArcItemFragment_ItemStats();
				StatsFragment->DefaultStats.Add(Result.Stat);
				OutSpec.AddInstanceData(StatsFragment);
			}
			else
			{
				StatsFragment->DefaultStats.Add(Result.Stat);
			}
		}
		break;

	case EArcCraftModifierResultType::Ability:
		{
			FArcItemFragment_GrantedMassAbilities* Fragment = OutSpec.FindFragmentMutable<FArcItemFragment_GrantedMassAbilities>();
			if (!Fragment)
			{
				Fragment = new FArcItemFragment_GrantedMassAbilities();
				OutSpec.AddInstanceData(Fragment);
			}
			Fragment->Abilities.Add(Result.Ability);
		}
		break;

	case EArcCraftModifierResultType::Effect:
		{
			FArcItemFragment_GrantedMassEffects* Fragment = OutSpec.FindFragmentMutable<FArcItemFragment_GrantedMassEffects>();
			if (!Fragment)
			{
				Fragment = new FArcItemFragment_GrantedMassEffects();
				OutSpec.AddInstanceData(Fragment);
			}
			Fragment->Effects.Add(Result.Effect);
		}
		break;

	case EArcCraftModifierResultType::AttributeModifier:
		{
			FArcItemFragment_MassAttributeModifier* Fragment = OutSpec.FindFragmentMutable<FArcItemFragment_MassAttributeModifier>();
			if (!Fragment)
			{
				Fragment = new FArcItemFragment_MassAttributeModifier();
				OutSpec.AddInstanceData(Fragment);
			}
			Fragment->Modifiers.Append(Result.AttributeModifiers);
		}
		break;

	default:
		break;
	}
}

namespace ArcCraftOutputBuilderInternal
{
	const FArcRecipeDefinitionData* FindChairRecipe(
		const FArcCraftRulesData* Rules,
		const FPrimaryAssetId& RecipeId,
		const FArcRecipeDefinitionData& Recipe,
		const FGameplayTag& OutputRarity)
	{
		if (Rules == nullptr || !OutputRarity.IsValid())
		{
			return &Recipe;
		}

		const FGameplayTag StartRarity = Recipe.GuaranteedRarity.IsValid() ? Recipe.GuaranteedRarity : Rules->DefaultRarity;
		if (OutputRarity == StartRarity)
		{
			return &Recipe;
		}

		const FPrimaryAssetId BaseId = Recipe.BaseRecipe.AssetId.IsValid() ? Recipe.BaseRecipe.AssetId : RecipeId;
		const FPrimaryAssetId VariantId = FArcRecipeLookup::FindRarityVariant(BaseId, OutputRarity, Rules->DefaultRarity);
		if (VariantId.IsValid())
		{
			return ArcCraftRecords::Find<FArcRecipeDefinitionData>(VariantId);
		}

		static TSet<TPair<FPrimaryAssetId, FGameplayTag>> LoggedRecipeRarities;
		bool bAlreadyLogged = false;
		LoggedRecipeRarities.Add(TPair<FPrimaryAssetId, FGameplayTag>(RecipeId, OutputRarity), &bAlreadyLogged);
		if (!bAlreadyLogged)
		{
			const bool bPoor = !Rules->Ladder.IsEmpty() && OutputRarity == Rules->Ladder[0].Rarity;
			if (bPoor)
			{
				UE_LOGF(LogArcCraftOutputBuilder, Verbose, "ArcCraft: recipe %ls has no %ls variant, no chairs",
					*RecipeId.ToString(), *OutputRarity.ToString());
			}
			else
			{
				UE_LOGF(LogArcCraftOutputBuilder, Warning, "ArcCraft: recipe %ls has no %ls variant, no chairs",
					*RecipeId.ToString(), *OutputRarity.ToString());
			}
		}
		return nullptr;
	}

	void ApplyModifiers(
		FArcItemSpec& OutputSpec,
		const FArcRecipeDefinitionData& Recipe,
		const FArcRecipeDefinitionData* ChairRecipe,
		const TArray<FArcItemSpec>& MatchedIngredients,
		const FArcCraftContext& Context)
	{
		TArray<FArcCraftPendingModifier> PendingModifiers;
		PendingModifiers.Reserve(Recipe.ModifierSources.Num());
		for (int32 Idx = 0; Idx < Recipe.ModifierSources.Num(); ++Idx)
		{
			const FArcRecipeModifierSource* Source =
				Recipe.ModifierSources[Idx].GetPtr<FArcRecipeModifierSource>();
			if (Source)
			{
				TArray<FArcCraftPendingModifier> Results = Source->Evaluate(MatchedIngredients, Context);
				for (FArcCraftPendingModifier& Pending : Results)
				{
					Pending.ModifierIndex = Idx;
				}
				PendingModifiers.Append(MoveTemp(Results));
			}
		}

		const TArray<int32> ResolvedIndices = ChairRecipe != nullptr
			? FArcCraftSlotResolver::Resolve(
				PendingModifiers, ChairRecipe->ModifierSlots,
				ChairRecipe->SlotSelector.GetPtr<FArcCraftSlotSelector>(), Context.Stream)
			: FArcCraftSlotResolver::Resolve(PendingModifiers, {}, nullptr, Context.Stream);

		for (int32 Idx : ResolvedIndices)
		{
			if (PendingModifiers[Idx].Result.Type != EArcCraftModifierResultType::None)
			{
				ApplyResult(OutputSpec, PendingModifiers[Idx].Result);
			}
		}
	}

	FGameplayTag FindIngredientRarity(
		const FArcCraftRulesData& Rules,
		const FArcRecipeDefinitionData& Recipe,
		const FArcCraftContext& Context)
	{
		const int32 SlotIdx = Recipe.Ingredients.IndexOfByPredicate(
			[](const FInstancedStruct& Ingredient) -> bool
			{
				const FArcRecipeIngredient* Base = Ingredient.GetPtr<FArcRecipeIngredient>();
				return Base != nullptr && !Base->bOptional;
			});
		if (SlotIdx == INDEX_NONE || Rules.Ladder.IsEmpty())
		{
			return Rules.DefaultRarity;
		}

		const float SlotStep = Context.SlotRaritySteps.IsValidIndex(SlotIdx) ? Context.SlotRaritySteps[SlotIdx] : 1.0f;
		FGameplayTag Rarity = Rules.Ladder[0].Rarity;
		for (const FArcCraftRarityStep& LadderStep : Rules.Ladder)
		{
			if (LadderStep.Step <= SlotStep + KINDA_SMALL_NUMBER)
			{
				Rarity = LadderStep.Rarity;
			}
		}
		return Rarity;
	}

	FGameplayTag DecideRarity(
		const FArcCraftRulesData* Rules,
		const FArcRecipeDefinitionData& Recipe,
		const FArcItemSpec& MainOutputSpec,
		const FArcCraftContext& Context)
	{
		if (Rules == nullptr || !ArcCraftRarity::DoesOutputRollRarity(*Rules, MainOutputSpec.GetItemDefinition()))
		{
			return FGameplayTag();
		}

		if (Recipe.Outputs[0].bKeepIngredientRarity)
		{
			return FindIngredientRarity(*Rules, Recipe, Context);
		}

		return ArcCraftRarity::DecideOutputRarity(
			*Rules, Recipe.GuaranteedRarity, Context.AverageR, Context.SkillMargin,
			[&Context]() -> float
			{
				return Context.Stream.FRand();
			});
	}
}

TArray<FArcItemSpec> FArcCraftOutputBuilder::Build(
	const FPrimaryAssetId& RecipeId,
	const FArcRecipeDefinitionData& Recipe,
	const TArray<FArcItemSpec>& MatchedIngredients,
	FArcCraftContext& Context)
{
	TArray<FArcItemSpec> OutputSpecs;
	OutputSpecs.Reserve(Recipe.Outputs.Num());
	for (int32 OutputIdx = 0; OutputIdx < Recipe.Outputs.Num(); ++OutputIdx)
	{
		const FArcRecipeOutput& Output = Recipe.Outputs[OutputIdx];
		if (!Output.ItemDefinition.IsValid())
		{
			continue;
		}

		FArcItemSpec OutputSpec = FArcItemSpec::NewItem(Output.ItemDefinition, Output.Level, Output.Amount);
		if (OutputIdx == 0)
		{
			const FArcCraftRulesData* Rules = ArcCraftRarity::GetRules();
			Context.OutputRarity = ArcCraftOutputBuilderInternal::DecideRarity(Rules, Recipe, OutputSpec, Context);

			const FArcRecipeDefinitionData* ChairRecipe = Output.bKeepIngredientRarity
				? &Recipe
				: ArcCraftOutputBuilderInternal::FindChairRecipe(Rules, RecipeId, Recipe, Context.OutputRarity);
			ArcCraftOutputBuilderInternal::ApplyModifiers(OutputSpec, Recipe, ChairRecipe, MatchedIngredients, Context);

			// Absent fragment means DefaultRarity, so default-rarity outputs keep stacking with plain items.
			if (Context.OutputRarity.IsValid() && Context.OutputRarity != Rules->DefaultRarity)
			{
				FArcItemFragment_Rarity* RarityFragment = new FArcItemFragment_Rarity();
				RarityFragment->Rarity = Context.OutputRarity;
				OutputSpec.AddInstanceData(RarityFragment);
			}
		}

		if (!Output.InstanceTags.IsEmpty())
		{
			FArcItemFragment_InstanceTags* InstanceTagsFragment = new FArcItemFragment_InstanceTags();
			InstanceTagsFragment->Tags = Output.InstanceTags;
			OutputSpec.AddInstanceData(InstanceTagsFragment);
		}
		OutputSpecs.Add(MoveTemp(OutputSpec));
	}

	return OutputSpecs;
}
