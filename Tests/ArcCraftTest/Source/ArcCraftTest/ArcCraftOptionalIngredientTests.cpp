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
#include "ArcItemTestRecordFiles.h"
#include "ArcCraftTestRecordFiles.h"
#include "ArcCraftTestContextProbe.h"
#include "Components/ActorTestSpawner.h"

#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"

#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Rarity.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcCraftTickProcessor.h"
#include "ArcCraft/Mass/ArcMassAvailableRecipesSharedFragment.h"
#include "ArcCraft/Rarity/ArcCraftRulesData.h"
#include "ArcCraft/Rarity/ArcCraftSettings.h"
#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Station/ArcCraftItemSource.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OptIng_Leather, "Test.OptIng.Leather");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OptIng_Thread, "Test.OptIng.Thread");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OptIng_Gem, "Test.OptIng.Gem");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OptIng_Stone, "Test.OptIng.Stone");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OptIng_Buckle, "Test.OptIng.Buckle");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OptIng_RarityCommon, "Test.OptIng.Rarity.Common");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OptIng_RarityFine, "Test.OptIng.Rarity.Fine");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OptIng_RarityRare, "Test.OptIng.Rarity.Rare");

namespace ArcCraftOptionalIngredientTestsLocal
{
	constexpr float Epsilon = 0.0001f;

	FPrimaryAssetId WriteTaggedDef(FArcItemTestRecordFiles& ItemRecords, const FGameplayTag& Tag)
	{
		FArcItemDefinitionData Def;
		FArcItemFragment_Tags TagsFragment;
		TagsFragment.AssetTags.AddTag(Tag);
		ArcItemTestRecords::AddFragment(Def, TagsFragment);
		return ItemRecords.Write(Def);
	}

	FArcItemSpec MakeSpec(const FPrimaryAssetId& Def, int32 Amount, const FGameplayTag& Rarity = FGameplayTag())
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinition(Def);
		Spec.SetItemId(FArcItemId::Generate());
		Spec.Amount = static_cast<uint16>(Amount);
		if (Rarity.IsValid())
		{
			FArcItemFragment_Rarity* RarityFragment = new FArcItemFragment_Rarity();
			RarityFragment->Rarity = Rarity;
			Spec.AddInstanceData(RarityFragment);
		}
		return Spec;
	}

	void AddDefSlot(FArcRecipeDefinitionData& Recipe, const FPrimaryAssetId& Def, int32 Amount, bool bOptional)
	{
		FArcRecipeIngredient_ItemDef Ingredient;
		Ingredient.ItemDefinitionId = FArcNamedPrimaryAssetId(Def);
		Ingredient.Amount = Amount;
		Ingredient.bOptional = bOptional;
		Recipe.Ingredients.Add(FInstancedStruct::Make(Ingredient));
	}

	void AddTagSlot(FArcRecipeDefinitionData& Recipe, const FGameplayTag& Tag, int32 Amount, bool bOptional)
	{
		FArcRecipeIngredient_Tags Ingredient;
		Ingredient.RequiredTags.AddTag(Tag);
		Ingredient.Amount = Amount;
		Ingredient.bOptional = bOptional;
		Recipe.Ingredients.Add(FInstancedStruct::Make(Ingredient));
	}

	struct FBootsDefs
	{
		FPrimaryAssetId Leather;
		FPrimaryAssetId Thread;
		FPrimaryAssetId Gem;

		bool IsValid() const
		{
			return Leather.IsValid() && Thread.IsValid() && Gem.IsValid();
		}
	};

	FBootsDefs WriteBootsDefs(FArcItemTestRecordFiles& ItemRecords)
	{
		FBootsDefs Defs;
		Defs.Leather = WriteTaggedDef(ItemRecords, TAG_OptIng_Leather);
		Defs.Thread = WriteTaggedDef(ItemRecords, TAG_OptIng_Thread);
		Defs.Gem = WriteTaggedDef(ItemRecords, TAG_OptIng_Gem);
		return Defs;
	}

	/** Boots: leather 4, thread 2, optional gem 1 (tag slot). */
	FArcRecipeDefinitionData MakeBootsRecipe(const FBootsDefs& Defs)
	{
		FArcRecipeDefinitionData Recipe;
		Recipe.CraftTime = 10.0f;
		AddDefSlot(Recipe, Defs.Leather, 4, false);
		AddDefSlot(Recipe, Defs.Thread, 2, false);
		AddTagSlot(Recipe, TAG_OptIng_Gem, 1, true);
		return Recipe;
	}

	/** Points UArcCraftSettings::Rules at a ladder Common 1.0 (default) / Fine 1.2 / Rare 1.5; restores on destruction. */
	class FRarityLadderScope
	{
	public:
		~FRarityLadderScope()
		{
			if (bInstalled)
			{
				GetMutableDefault<UArcCraftSettings>()->Rules = PreviousRules;
			}
		}

		bool Install(FArcCraftTestRecordFiles& Records)
		{
			FArcCraftRulesData Rules;
			Rules.DefaultRarity = TAG_OptIng_RarityCommon;
			FArcCraftRarityStep& Common = Rules.Ladder.AddDefaulted_GetRef();
			Common.Rarity = TAG_OptIng_RarityCommon;
			Common.Step = 1.0f;
			FArcCraftRarityStep& Fine = Rules.Ladder.AddDefaulted_GetRef();
			Fine.Rarity = TAG_OptIng_RarityFine;
			Fine.Step = 1.2f;
			FArcCraftRarityStep& Rare = Rules.Ladder.AddDefaulted_GetRef();
			Rare.Rarity = TAG_OptIng_RarityRare;
			Rare.Step = 1.5f;

			const FPrimaryAssetId RulesId = Records.Write(Rules);
			if (!RulesId.IsValid())
			{
				return false;
			}
			UArcCraftSettings* Settings = GetMutableDefault<UArcCraftSettings>();
			PreviousRules = Settings->Rules;
			Settings->Rules = FArcNamedPrimaryAssetId(RulesId);
			bInstalled = true;
			return true;
		}

	private:
		FArcNamedPrimaryAssetId PreviousRules;
		bool bInstalled = false;
	};

	FMassEntityHandle BuildTickStation(FMassEntityManager& EM, const FPrimaryAssetId& PinnedRecipe)
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcCraftInputFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcCraftQueueFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcCraftOutputFragment()));

		FMassEntityHandle Entity = EM.CreateEntity(Fragments);
		EM.AddTagToEntity(Entity, FArcCraftStationTag::StaticStruct());
		EM.AddTagToEntity(Entity, FArcCraftAutoTickTag::StaticStruct());

		FArcCraftStationConfigFragment Config;
		Config.ConcurrentCraftSlots = 1;
		EM.AddConstSharedFragmentToEntity(Entity, EM.GetOrCreateConstSharedFragment(Config));

		FArcMassAvailableRecipesSharedFragment Available;
		Available.Recipes.Add(PinnedRecipe);
		EM.AddConstSharedFragmentToEntity(Entity, EM.GetOrCreateConstSharedFragment(Available));
		return Entity;
	}
}

// ---------------------------------------------------------------------------
// MatchAndConsumeFromSpecs: required slots first, optional slots never fail the match
// ---------------------------------------------------------------------------

TEST_CLASS(ArcCraftOptionalIngredientMatch, "ArcCraft.OptionalIngredients.Match")
{
	FArcItemTestRecordFiles ItemRecords;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
	}

	TEST_METHOD(EmptyOptionalSlot_MatchSucceeds_SlotUnfilledWithDefaultSpec)
	{
		using namespace ArcCraftOptionalIngredientTestsLocal;

		const FBootsDefs Defs = WriteBootsDefs(ItemRecords);
		ASSERT_THAT(IsTrue(Defs.IsValid(), TEXT("Item records must be written and indexed")));
		const FArcRecipeDefinitionData Recipe = MakeBootsRecipe(Defs);

		TArray<FArcItemSpec> Items = { MakeSpec(Defs.Leather, 4), MakeSpec(Defs.Thread, 2) };
		TArray<FArcItemSpec> Matched;
		TArray<float> Steps;
		TArray<bool> Filled;

		const bool bMatched = FArcCraftItemSource::MatchAndConsumeFromSpecs(Items, Recipe, true, &Matched, &Steps, &Filled);

		ASSERT_THAT(IsTrue(bMatched, TEXT("An empty optional slot must not fail the match")));
		ASSERT_THAT(AreEqual(3, Filled.Num()));
		ASSERT_THAT(IsTrue(Filled[0] && Filled[1], TEXT("Both required slots are filled")));
		ASSERT_THAT(IsFalse(Filled[2], TEXT("The gem slot stays empty")));
		ASSERT_THAT(AreEqual(3, Matched.Num()));
		ASSERT_THAT(IsFalse(Matched[2].GetItemDefinitionId().IsValid(), TEXT("An empty slot reports a default spec")));
		ASSERT_THAT(IsTrue(Matched[0].GetItemDefinitionId() == Defs.Leather, TEXT("Results stay in slot order")));
		ASSERT_THAT(IsTrue(Matched[1].GetItemDefinitionId() == Defs.Thread, TEXT("Results stay in slot order")));
		ASSERT_THAT(IsNear(1.0f, Steps[2], Epsilon, TEXT("An empty slot reports the neutral step")));
		ASSERT_THAT(AreEqual(0, Items.Num(), TEXT("Leather and thread are consumed")));
	}

	TEST_METHOD(FilledOptionalSlot_IsConsumed)
	{
		using namespace ArcCraftOptionalIngredientTestsLocal;

		const FBootsDefs Defs = WriteBootsDefs(ItemRecords);
		ASSERT_THAT(IsTrue(Defs.IsValid(), TEXT("Item records must be written and indexed")));
		const FArcRecipeDefinitionData Recipe = MakeBootsRecipe(Defs);

		TArray<FArcItemSpec> Items = { MakeSpec(Defs.Gem, 1), MakeSpec(Defs.Leather, 4), MakeSpec(Defs.Thread, 2) };
		TArray<FArcItemSpec> Matched;
		TArray<bool> Filled;

		const bool bMatched = FArcCraftItemSource::MatchAndConsumeFromSpecs(Items, Recipe, true, &Matched, nullptr, &Filled);

		ASSERT_THAT(IsTrue(bMatched));
		ASSERT_THAT(IsTrue(Filled[0] && Filled[1] && Filled[2], TEXT("All three slots are filled")));
		ASSERT_THAT(IsTrue(Matched[2].GetItemDefinitionId() == Defs.Gem, TEXT("The gem fills the optional slot")));
		ASSERT_THAT(AreEqual(0, Items.Num(), TEXT("The gem is consumed with the required ingredients")));
	}

	TEST_METHOD(RequiredSlotMissing_MatchFails_InputUntouched)
	{
		using namespace ArcCraftOptionalIngredientTestsLocal;

		const FBootsDefs Defs = WriteBootsDefs(ItemRecords);
		ASSERT_THAT(IsTrue(Defs.IsValid(), TEXT("Item records must be written and indexed")));
		const FPrimaryAssetId Buckle = WriteTaggedDef(ItemRecords, TAG_OptIng_Buckle);
		ASSERT_THAT(IsTrue(Buckle.IsValid()));

		FArcRecipeDefinitionData Recipe = MakeBootsRecipe(Defs);
		AddDefSlot(Recipe, Buckle, 1, false);

		TArray<FArcItemSpec> Items = { MakeSpec(Defs.Leather, 4), MakeSpec(Defs.Thread, 2) };

		ASSERT_THAT(IsFalse(FArcCraftItemSource::MatchAndConsumeFromSpecs(Items, Recipe, true),
			TEXT("A missing required buckle fails the match")));
		ASSERT_THAT(AreEqual(2, Items.Num(), TEXT("A failed match consumes nothing")));
	}

	TEST_METHOD(StoneFittingNoSlot_StaysInInput)
	{
		using namespace ArcCraftOptionalIngredientTestsLocal;

		const FBootsDefs Defs = WriteBootsDefs(ItemRecords);
		ASSERT_THAT(IsTrue(Defs.IsValid(), TEXT("Item records must be written and indexed")));
		const FPrimaryAssetId Stone = WriteTaggedDef(ItemRecords, TAG_OptIng_Stone);
		ASSERT_THAT(IsTrue(Stone.IsValid()));
		const FArcRecipeDefinitionData Recipe = MakeBootsRecipe(Defs);

		TArray<FArcItemSpec> Items = { MakeSpec(Stone, 1), MakeSpec(Defs.Leather, 4), MakeSpec(Defs.Thread, 2) };
		TArray<bool> Filled;

		ASSERT_THAT(IsTrue(FArcCraftItemSource::MatchAndConsumeFromSpecs(Items, Recipe, true, nullptr, nullptr, &Filled)));
		ASSERT_THAT(IsFalse(Filled[2], TEXT("A stone does not fill the gem slot")));
		ASSERT_THAT(AreEqual(1, Items.Num(), TEXT("Only the stone is left")));
		ASSERT_THAT(IsTrue(Items[0].GetItemDefinitionId() == Stone, TEXT("The stone stays in the input, unused")));
	}

	TEST_METHOD(GemFittingRequiredTagSlot_RequiredSlotTakesItFirst)
	{
		using namespace ArcCraftOptionalIngredientTestsLocal;

		const FPrimaryAssetId Gem = WriteTaggedDef(ItemRecords, TAG_OptIng_Gem);
		ASSERT_THAT(IsTrue(Gem.IsValid()));

		// The optional slot is listed first, so slot order alone would hand it the only gem.
		FArcRecipeDefinitionData Recipe;
		AddTagSlot(Recipe, TAG_OptIng_Gem, 1, true);
		AddTagSlot(Recipe, TAG_OptIng_Gem, 1, false);

		TArray<FArcItemSpec> Items = { MakeSpec(Gem, 1) };
		TArray<FArcItemSpec> Matched;
		TArray<bool> Filled;

		ASSERT_THAT(IsTrue(FArcCraftItemSource::MatchAndConsumeFromSpecs(Items, Recipe, true, &Matched, nullptr, &Filled),
			TEXT("The required slot is matched before the optional one, so one gem funds the recipe")));
		ASSERT_THAT(IsFalse(Filled[0], TEXT("The optional slot stays empty")));
		ASSERT_THAT(IsTrue(Filled[1], TEXT("The required slot takes the gem")));
		ASSERT_THAT(IsTrue(Matched[1].GetItemDefinitionId() == Gem, TEXT("Results stay in slot order")));
		ASSERT_THAT(AreEqual(0, Items.Num()));
	}

	TEST_METHOD(GemFittingRequiredTagSlot_SecondGemFillsOptionalSlot)
	{
		using namespace ArcCraftOptionalIngredientTestsLocal;

		const FPrimaryAssetId Gem = WriteTaggedDef(ItemRecords, TAG_OptIng_Gem);
		ASSERT_THAT(IsTrue(Gem.IsValid()));

		FArcRecipeDefinitionData Recipe;
		AddTagSlot(Recipe, TAG_OptIng_Gem, 1, true);
		AddTagSlot(Recipe, TAG_OptIng_Gem, 1, false);

		TArray<FArcItemSpec> Items = { MakeSpec(Gem, 1), MakeSpec(Gem, 1) };
		TArray<bool> Filled;

		ASSERT_THAT(IsTrue(FArcCraftItemSource::MatchAndConsumeFromSpecs(Items, Recipe, true, nullptr, nullptr, &Filled)));
		ASSERT_THAT(IsTrue(Filled[0] && Filled[1], TEXT("The optional slot takes the second gem")));
		ASSERT_THAT(AreEqual(0, Items.Num(), TEXT("Both gems are consumed")));
	}
};

// ---------------------------------------------------------------------------
// Tick processor activation: the filled mask feeds CachedAverageR and ConsumedIngredientDefs
// ---------------------------------------------------------------------------

TEST_CLASS(ArcCraftOptionalIngredientActivation, "ArcCraft.OptionalIngredients.Activation")
{
	FArcItemTestRecordFiles ItemRecords;
	FArcCraftTestRecordFiles Records;
	ArcCraftOptionalIngredientTestsLocal::FRarityLadderScope Ladder;
	FActorTestSpawner Spawner;
	FMassEntityManager* EM = nullptr;
	UArcCraftTickProcessor* Processor = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");

		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EM = &MES->GetMutableEntityManager();

		TSharedRef<FMassEntityManager> SharedEM = EM->AsShared();
		Processor = NewObject<UArcCraftTickProcessor>();
		Processor->CallInitialize(EM->GetOwner(), SharedEM);
	}

	void RunProcessor()
	{
		RunProcessor(0.0f);
	}

	void RunProcessor(float DeltaSeconds)
	{
		UE::Mass::FProcessingContext ProcessingContext(*EM, DeltaSeconds, /*bFlushCommandBuffer=*/true);
		UE::Mass::Executor::Run(*Processor, ProcessingContext);
	}

	/** Boots with a main output and the context probe, so completion runs the output builder. */
	FPrimaryAssetId WriteProbedBootsRecipe(const ArcCraftOptionalIngredientTestsLocal::FBootsDefs& Defs)
	{
		FArcRecipeDefinitionData Recipe = ArcCraftOptionalIngredientTestsLocal::MakeBootsRecipe(Defs);
		FArcRecipeOutput& Output = Recipe.Outputs.AddDefaulted_GetRef();
		Output.ItemDefinition = FArcNamedPrimaryAssetId(ItemRecords.Write(FArcItemDefinitionData()));
		Output.Amount = 1;
		Recipe.ModifierSources.Add(FInstancedStruct::Make(FArcRecipeModifierSource_TestContextProbe()));
		return Output.ItemDefinition.IsValid() ? Records.Write(Recipe) : FPrimaryAssetId();
	}

	FArcCraftQueueEntry* QueueBoots(const FPrimaryAssetId& RecipeId, FMassEntityHandle Station)
	{
		FArcCraftQueueEntry Entry;
		Entry.EntryId = FGuid::NewGuid();
		Entry.RecipeId = RecipeId;
		Entry.Amount = 1;
		FArcCraftQueueFragment* Queue = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		Queue->Entries.Add(Entry);
		return &Queue->Entries[0];
	}

	TEST_METHOD(BootsWithoutGem_PromotedWithEmptySlotAtWeightZero)
	{
		using namespace ArcCraftOptionalIngredientTestsLocal;

		ASSERT_THAT(IsTrue(Ladder.Install(Records), TEXT("Rules record must be written and indexed")));
		const FBootsDefs Defs = WriteBootsDefs(ItemRecords);
		ASSERT_THAT(IsTrue(Defs.IsValid(), TEXT("Item records must be written and indexed")));
		const FPrimaryAssetId RecipeId = Records.Write(MakeBootsRecipe(Defs));
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));

		const FMassEntityHandle Station = BuildTickStation(*EM, RecipeId);
		FArcCraftInputFragment* Input = EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station);
		Input->InputItems.Add(MakeSpec(Defs.Leather, 4, TAG_OptIng_RarityFine));
		Input->InputItems.Add(MakeSpec(Defs.Thread, 2, TAG_OptIng_RarityCommon));
		FArcCraftQueueEntry* Entry = QueueBoots(RecipeId, Station);

		RunProcessor();

		ASSERT_THAT(IsTrue(Entry->State == EArcCraftQueueEntryState::Active,
			TEXT("Boots without gem are fundable, so the entry is promoted")));
		ASSERT_THAT(AreEqual(3, Entry->ConsumedIngredientDefs.Num()));
		ASSERT_THAT(IsTrue(Entry->ConsumedIngredientDefs[0] == Defs.Leather));
		ASSERT_THAT(IsTrue(Entry->ConsumedIngredientDefs[1] == Defs.Thread));
		ASSERT_THAT(IsFalse(Entry->ConsumedIngredientDefs[2].IsValid(), TEXT("The empty gem slot records no definition")));
		ASSERT_THAT(IsNear((4.0f * 1.2f + 2.0f * 1.0f) / 6.0f, Entry->CachedAverageR, Epsilon,
			TEXT("CachedAverageR = (4*1.2 + 2*1.0) / 6; the empty slot weighs 0")));
		ASSERT_THAT(AreEqual(0, Input->InputItems.Num(), TEXT("Leather and thread are consumed")));
	}

	TEST_METHOD(BootsWithoutGem_Completion_SlotAmountsZeroForEmptySlot)
	{
		using namespace ArcCraftOptionalIngredientTestsLocal;

		const FBootsDefs Defs = WriteBootsDefs(ItemRecords);
		ASSERT_THAT(IsTrue(Defs.IsValid(), TEXT("Item records must be written and indexed")));
		const FPrimaryAssetId RecipeId = WriteProbedBootsRecipe(Defs);
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));

		const FMassEntityHandle Station = BuildTickStation(*EM, RecipeId);
		FArcCraftInputFragment* Input = EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station);
		Input->InputItems.Add(MakeSpec(Defs.Leather, 4));
		Input->InputItems.Add(MakeSpec(Defs.Thread, 2));
		QueueBoots(RecipeId, Station);
		FArcRecipeModifierSource_TestContextProbe::Reset();

		RunProcessor(0.0f);
		RunProcessor(10.0f);

		ASSERT_THAT(AreEqual(1, FArcRecipeModifierSource_TestContextProbe::EvaluateCount(),
			TEXT("The craft completes and the output builder evaluates the recipe's sources once")));
		const TArray<int32>& SlotAmounts = FArcRecipeModifierSource_TestContextProbe::CapturedSlotAmounts();
		ASSERT_THAT(AreEqual(3, SlotAmounts.Num()));
		ASSERT_THAT(AreEqual(4, SlotAmounts[0], TEXT("Leather consumes its Amount")));
		ASSERT_THAT(AreEqual(2, SlotAmounts[1], TEXT("Thread consumes its Amount")));
		ASSERT_THAT(AreEqual(0, SlotAmounts[2], TEXT("The empty gem slot has SlotAmount 0, so shares are 4/6 and 2/6")));
	}

	TEST_METHOD(BootsWithGem_Completion_SlotAmountsCountTheGem)
	{
		using namespace ArcCraftOptionalIngredientTestsLocal;

		const FBootsDefs Defs = WriteBootsDefs(ItemRecords);
		ASSERT_THAT(IsTrue(Defs.IsValid(), TEXT("Item records must be written and indexed")));
		const FPrimaryAssetId RecipeId = WriteProbedBootsRecipe(Defs);
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));

		const FMassEntityHandle Station = BuildTickStation(*EM, RecipeId);
		FArcCraftInputFragment* Input = EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station);
		Input->InputItems.Add(MakeSpec(Defs.Leather, 4));
		Input->InputItems.Add(MakeSpec(Defs.Thread, 2));
		Input->InputItems.Add(MakeSpec(Defs.Gem, 1));
		QueueBoots(RecipeId, Station);
		FArcRecipeModifierSource_TestContextProbe::Reset();

		RunProcessor(0.0f);
		RunProcessor(10.0f);

		ASSERT_THAT(AreEqual(1, FArcRecipeModifierSource_TestContextProbe::EvaluateCount()));
		const TArray<int32>& SlotAmounts = FArcRecipeModifierSource_TestContextProbe::CapturedSlotAmounts();
		ASSERT_THAT(AreEqual(3, SlotAmounts.Num()));
		ASSERT_THAT(IsTrue(SlotAmounts[0] == 4 && SlotAmounts[1] == 2 && SlotAmounts[2] == 1,
			TEXT("A filled gem slot consumes its Amount: shares 4/7, 2/7, 1/7")));
	}

	TEST_METHOD(BootsWithGem_PromotedWithGemWeighted)
	{
		using namespace ArcCraftOptionalIngredientTestsLocal;

		ASSERT_THAT(IsTrue(Ladder.Install(Records), TEXT("Rules record must be written and indexed")));
		const FBootsDefs Defs = WriteBootsDefs(ItemRecords);
		ASSERT_THAT(IsTrue(Defs.IsValid(), TEXT("Item records must be written and indexed")));
		const FPrimaryAssetId RecipeId = Records.Write(MakeBootsRecipe(Defs));
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));

		const FMassEntityHandle Station = BuildTickStation(*EM, RecipeId);
		FArcCraftInputFragment* Input = EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station);
		Input->InputItems.Add(MakeSpec(Defs.Leather, 4, TAG_OptIng_RarityFine));
		Input->InputItems.Add(MakeSpec(Defs.Thread, 2, TAG_OptIng_RarityCommon));
		Input->InputItems.Add(MakeSpec(Defs.Gem, 1, TAG_OptIng_RarityRare));
		FArcCraftQueueEntry* Entry = QueueBoots(RecipeId, Station);

		RunProcessor();

		ASSERT_THAT(IsTrue(Entry->State == EArcCraftQueueEntryState::Active));
		ASSERT_THAT(AreEqual(3, Entry->ConsumedIngredientDefs.Num()));
		ASSERT_THAT(IsTrue(Entry->ConsumedIngredientDefs[2] == Defs.Gem, TEXT("The gem fills the optional slot")));
		ASSERT_THAT(IsNear(1.5f, Entry->SlotRaritySteps[2], Epsilon));
		ASSERT_THAT(IsNear((4.0f * 1.2f + 2.0f * 1.0f + 1.0f * 1.5f) / 7.0f, Entry->CachedAverageR, Epsilon,
			TEXT("CachedAverageR = (4*1.2 + 2*1.0 + 1*1.5) / 7")));
		ASSERT_THAT(AreEqual(0, Input->InputItems.Num(), TEXT("The gem is consumed too")));
	}
};
