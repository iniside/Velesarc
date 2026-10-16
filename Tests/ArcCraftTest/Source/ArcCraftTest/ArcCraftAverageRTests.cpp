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

#include "CQTest.h"

#include "StructUtils/InstancedStruct.h"
#include "ArcCraft/Rarity/ArcCraftRarity.h"
#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"

/** Tolerance for float comparisons across all tests in this translation unit. */
static constexpr float AverageRTestEpsilon = 0.0001f;

namespace ArcCraftAverageRTestsLocal
{
	void AddSlot(FArcRecipeDefinitionData& Recipe, int32 Amount, bool bOptional)
	{
		FArcRecipeIngredient_ItemDef Ingredient;
		Ingredient.Amount = Amount;
		Ingredient.bOptional = bOptional;
		Recipe.Ingredients.Add(FInstancedStruct::Make(Ingredient));
	}

	/** Boots: leather 4, thread 2, optional gem 1. */
	FArcRecipeDefinitionData MakeBootsRecipe()
	{
		FArcRecipeDefinitionData Recipe;
		AddSlot(Recipe, 4, false);
		AddSlot(Recipe, 2, false);
		AddSlot(Recipe, 1, true);
		return Recipe;
	}
}

TEST_CLASS(ArcCraftAverageR, "ArcCraft.Rarity.AverageR")
{
	/**
	 * Single-ingredient recipe with Amount=1, slot step 1.5f.
	 * ComputeAverageR must return 1.5f.
	 */
	TEST_METHOD(SingleIngredient_Amount1_ReturnsStepDirectly)
	{
		FArcRecipeDefinitionData Recipe;

		FArcRecipeIngredient_ItemDef Ing;
		Ing.Amount = 1;
		FInstancedStruct Struct;
		Struct.InitializeAs<FArcRecipeIngredient_ItemDef>(Ing);
		Recipe.Ingredients.Add(MoveTemp(Struct));

		TArray<float> Steps;
		Steps.Add(1.5f);

		const float Result = ArcCraftRarity::ComputeAverageR(Recipe, Steps, TConstArrayView<bool>());

		ASSERT_THAT(IsNear(1.5f, Result, AverageRTestEpsilon,
			TEXT("Single ingredient with step 1.5 and amount 1 should yield average 1.5")));
	}

	/**
	 * Two-ingredient recipe, amounts (1, 1), steps (1.0, 2.0).
	 * Weighted average = (1*1.0 + 1*2.0) / 2 = 1.5f.
	 */
	TEST_METHOD(TwoIngredients_EqualWeights_ReturnsSimpleAverage)
	{
		FArcRecipeDefinitionData Recipe;

		FArcRecipeIngredient_ItemDef Ing1;
		Ing1.Amount = 1;
		FInstancedStruct Struct1;
		Struct1.InitializeAs<FArcRecipeIngredient_ItemDef>(Ing1);
		Recipe.Ingredients.Add(MoveTemp(Struct1));

		FArcRecipeIngredient_ItemDef Ing2;
		Ing2.Amount = 1;
		FInstancedStruct Struct2;
		Struct2.InitializeAs<FArcRecipeIngredient_ItemDef>(Ing2);
		Recipe.Ingredients.Add(MoveTemp(Struct2));

		TArray<float> Steps;
		Steps.Add(1.0f);
		Steps.Add(2.0f);

		const float Result = ArcCraftRarity::ComputeAverageR(Recipe, Steps, TConstArrayView<bool>());

		ASSERT_THAT(IsNear(1.5f, Result, AverageRTestEpsilon,
			TEXT("Two ingredients amount=1 each, steps 1.0 and 2.0 must average to 1.5")));
	}

	/**
	 * Two-ingredient recipe, amounts (3, 1), steps (1.0, 2.0).
	 * Weighted average = (3*1.0 + 1*2.0) / 4 = 1.25f.
	 */
	TEST_METHOD(TwoIngredients_DifferentWeights_ReturnsWeightedAverage)
	{
		FArcRecipeDefinitionData Recipe;

		FArcRecipeIngredient_ItemDef Ing1;
		Ing1.Amount = 3;
		FInstancedStruct Struct1;
		Struct1.InitializeAs<FArcRecipeIngredient_ItemDef>(Ing1);
		Recipe.Ingredients.Add(MoveTemp(Struct1));

		FArcRecipeIngredient_ItemDef Ing2;
		Ing2.Amount = 1;
		FInstancedStruct Struct2;
		Struct2.InitializeAs<FArcRecipeIngredient_ItemDef>(Ing2);
		Recipe.Ingredients.Add(MoveTemp(Struct2));

		TArray<float> Steps;
		Steps.Add(1.0f);
		Steps.Add(2.0f);

		const float Result = ArcCraftRarity::ComputeAverageR(Recipe, Steps, TConstArrayView<bool>());

		ASSERT_THAT(IsNear(1.25f, Result, AverageRTestEpsilon,
			TEXT("Weights (3,1) with steps (1.0,2.0) must yield 1.25f")));
	}

	TEST_METHOD(Boots_GemSlotEmpty_WeighsZero)
	{
		const FArcRecipeDefinitionData Recipe = ArcCraftAverageRTestsLocal::MakeBootsRecipe();
		const TArray<float> Steps = { 1.2f, 1.0f, 1.0f };
		const TArray<bool> Filled = { true, true, false };

		const float Result = ArcCraftRarity::ComputeAverageR(Recipe, Steps, Filled);

		ASSERT_THAT(IsNear((4.0f * 1.2f + 2.0f * 1.0f) / 6.0f, Result, AverageRTestEpsilon,
			TEXT("An empty optional slot has weight 0: (4*1.2 + 2*1.0) / 6")));
		ASSERT_THAT(IsFalse(FMath::IsNearlyEqual((4.0f * 1.2f + 2.0f * 1.0f + 1.0f * 1.0f) / 7.0f, Result, AverageRTestEpsilon),
			TEXT("The empty slot's neutral step must not be averaged in over 7")));
	}

	TEST_METHOD(Boots_GemSlotFilled_WeighsItsAmount)
	{
		const FArcRecipeDefinitionData Recipe = ArcCraftAverageRTestsLocal::MakeBootsRecipe();
		const TArray<float> Steps = { 1.2f, 1.0f, 1.5f };
		const TArray<bool> Filled = { true, true, true };

		const float Result = ArcCraftRarity::ComputeAverageR(Recipe, Steps, Filled);

		ASSERT_THAT(IsNear((4.0f * 1.2f + 2.0f * 1.0f + 1.0f * 1.5f) / 7.0f, Result, AverageRTestEpsilon,
			TEXT("A filled optional slot weighs its Amount: (4*1.2 + 2*1.0 + 1*1.5) / 7")));
	}

	/**
	 * Empty slot steps — should return 1.0f.
	 */
	TEST_METHOD(EmptySteps_Returns1)
	{
		FArcRecipeDefinitionData Recipe;

		FArcRecipeIngredient_ItemDef Ing;
		Ing.Amount = 1;
		FInstancedStruct Struct;
		Struct.InitializeAs<FArcRecipeIngredient_ItemDef>(Ing);
		Recipe.Ingredients.Add(MoveTemp(Struct));

		TArray<float> EmptySteps;

		const float Result = ArcCraftRarity::ComputeAverageR(Recipe, EmptySteps, TConstArrayView<bool>());

		ASSERT_THAT(IsNear(1.0f, Result, AverageRTestEpsilon,
			TEXT("Empty slot steps must return 1.0f fallback")));
	}
};
