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
#include "Components/ActorTestSpawner.h"

#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"

#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

#include "ArcCraft/Commands/ArcCraftFromStationInputCommand.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"

// ---------------------------------------------------------------------------
// Unique tag prefix: TAG_StnInQ_* — station-input queue command tests
// ---------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StnInQ_Ore, "Test.Material.StnInQOre");

// ---------------------------------------------------------------------------
// File-scoped helpers
// ---------------------------------------------------------------------------

namespace ArcCraftFromStationInputCommandHelpers
{
	FPrimaryAssetId MakeOreItemDef(FArcItemTestRecordFiles& ItemRecords)
	{
		FArcItemDefinitionData Def;

		FArcItemFragment_Tags TagsFrag;
		TagsFrag.AssetTags.AddTag(TAG_StnInQ_Ore);
		ArcItemTestRecords::AddFragment(Def, TagsFrag);

		return ItemRecords.Write(Def);
	}

	/** Minimal FArcItemSpec for a definition with a fresh item id and the given stack count. */
	FArcItemSpec MakeSpec(const FPrimaryAssetId& Def, int32 Amount)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinition(Def);
		Spec.SetItemId(FArcItemId::Generate());
		Spec.Amount = static_cast<uint16>(Amount);
		return Spec;
	}

	/** Fresh recipe id; the in-memory record behind it is passed to ApplyQueueFromInput directly. */
	FPrimaryAssetId MakeRecipeId()
	{
		return FPrimaryAssetId(FArcRecipeDefinitionData::JsonPrimaryAssetType(), FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	/**
	 * Recipe with a single FArcRecipeIngredient_Tags slot requiring TAG_StnInQ_Ore,
	 * Amount = 1, bConsumeOnCraft = true. Consume-true is required for the fundability
	 * clamp: each fundability pass decrements the scratch stack, so a deposited spec of
	 * Amount == N funds exactly N crafts (mirrors activation consume).
	 */
	FArcRecipeDefinitionData MakeConsumingTagRecipe()
	{
		FArcRecipeDefinitionData Recipe;
		Recipe.CraftTime = 1.0f;

		FArcRecipeIngredient_Tags Ingredient;
		Ingredient.Amount = 1;
		Ingredient.bConsumeOnCraft = true;
		Ingredient.RequiredTags.AddTag(TAG_StnInQ_Ore);

		FInstancedStruct IngredientStruct;
		IngredientStruct.InitializeAs<FArcRecipeIngredient_Tags>(Ingredient);
		Recipe.Ingredients.Add(MoveTemp(IngredientStruct));

		return Recipe;
	}

	/** Recipe with zero ingredients — funds the full requested amount (loop-bound check). */
	FArcRecipeDefinitionData MakeFreeRecipe()
	{
		FArcRecipeDefinitionData Recipe;
		Recipe.CraftTime = 1.0f;
		return Recipe;
	}

	/**
	 * Station entity carrying both regular fragments ApplyQueueFromInput resolves:
	 *   - FArcCraftInputFragment (seed InputItems after creation)
	 *   - FArcCraftQueueFragment (the appended/coalesced queue under test)
	 * CreateEntity commits synchronously, so the handle is immediately active.
	 */
	FMassEntityHandle BuildQueueStationEntity(FMassEntityManager& EM)
	{
		TArray<FInstancedStruct> Frags;
		Frags.Add(FInstancedStruct::Make(FTransformFragment()));
		Frags.Add(FInstancedStruct::Make(FArcCraftInputFragment()));
		Frags.Add(FInstancedStruct::Make(FArcCraftQueueFragment()));

		FMassEntityHandle Entity = EM.CreateEntity(Frags);
		EM.AddTagToEntity(Entity, FArcCraftStationTag::StaticStruct());
		return Entity;
	}

	/** Instigator entity — only a stable, valid handle is needed (it is stamped on the
	 *  queue entry, never consumed by ApplyQueueFromInput). */
	FMassEntityHandle BuildInstigatorEntity(FMassEntityManager& EM)
	{
		TArray<FInstancedStruct> Frags;
		Frags.Add(FInstancedStruct::Make(FTransformFragment()));
		return EM.CreateEntity(Frags);
	}
}

// ---------------------------------------------------------------------------
// Test class: ApplyQueueFromInput fundability clamp + coalesce
//
// Asserts against the ArcCraftFromStationInput::ApplyQueueFromInput free function
// directly (NOT FArcCraftFromStationInputCommand::Execute) — the in-memory recipe
// record is not indexed, so Execute would not resolve it. The free function is the
// matching test seam.
// ---------------------------------------------------------------------------

TEST_CLASS(ArcCraftFromStationInputCommand, "ArcCraft.FromStationInput")
{
	FArcItemTestRecordFiles ItemRecords;
	FActorTestSpawner Spawner;
	FMassEntityManager* EM = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");

		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EM = &MES->GetMutableEntityManager();
	}

	// =========================================================================
	// Queue_FundsN_AppendsPendingEntry
	//
	// Input funds N. ApplyQueueFromInput(Amount=N) returns N and appends exactly
	// one Pending entry {Amount=N, RecipeId=fixture, InstigatorEntity=instigator}.
	// =========================================================================
	TEST_METHOD(Queue_FundsN_AppendsPendingEntry)
	{
		using namespace ArcCraftFromStationInputCommandHelpers;

		const int32 N = 3;

		const FPrimaryAssetId Def = MakeOreItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId RecipeId = MakeRecipeId();
		const FArcRecipeDefinitionData Recipe = MakeConsumingTagRecipe();

		FMassEntityHandle Station = BuildQueueStationEntity(*EM);
		FMassEntityHandle Instigator = BuildInstigatorEntity(*EM);

		FArcCraftInputFragment* InputFrag = EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station);
		ASSERT_THAT(IsNotNull(InputFrag));
		InputFrag->InputItems.Add(MakeSpec(Def, N));

		const int32 Queued = ArcCraftFromStationInput::ApplyQueueFromInput(*EM, Station, Instigator, RecipeId, Recipe, N);

		ASSERT_THAT(AreEqual(N, Queued,
			TEXT("ApplyQueueFromInput must queue all N when input funds exactly N")));

		const FArcCraftQueueFragment* QueueFrag = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		ASSERT_THAT(IsNotNull(QueueFrag));
		ASSERT_THAT(AreEqual(1, QueueFrag->Entries.Num(),
			TEXT("Exactly one queue entry must be appended")));

		const FArcCraftQueueEntry& Entry = QueueFrag->Entries[0];
		ASSERT_THAT(IsTrue(Entry.State == EArcCraftQueueEntryState::Pending,
			TEXT("Appended entry must be Pending")));
		ASSERT_THAT(AreEqual(N, Entry.Amount,
			TEXT("Entry Amount must equal the funded count N")));
		ASSERT_THAT(IsTrue(Entry.RecipeId == RecipeId,
			TEXT("Entry RecipeId must equal the fixture recipe id")));
		ASSERT_THAT(IsTrue(Entry.InstigatorEntity == Instigator,
			TEXT("Entry InstigatorEntity must equal the passed instigator handle")));
	}

	// =========================================================================
	// Queue_ClampsToFundable
	//
	// Request N+2 but input only funds N. Returns N, entry Amount == N.
	// =========================================================================
	TEST_METHOD(Queue_ClampsToFundable)
	{
		using namespace ArcCraftFromStationInputCommandHelpers;

		const int32 N = 2;

		const FPrimaryAssetId Def = MakeOreItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId RecipeId = MakeRecipeId();
		const FArcRecipeDefinitionData Recipe = MakeConsumingTagRecipe();

		FMassEntityHandle Station = BuildQueueStationEntity(*EM);
		FMassEntityHandle Instigator = BuildInstigatorEntity(*EM);

		FArcCraftInputFragment* InputFrag = EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station);
		ASSERT_THAT(IsNotNull(InputFrag));
		InputFrag->InputItems.Add(MakeSpec(Def, N)); // funds only N

		const int32 Queued = ArcCraftFromStationInput::ApplyQueueFromInput(*EM, Station, Instigator, RecipeId, Recipe, N + 2);

		ASSERT_THAT(AreEqual(N, Queued,
			TEXT("Requested N+2 must clamp to the fundable count N")));

		const FArcCraftQueueFragment* QueueFrag = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		ASSERT_THAT(IsNotNull(QueueFrag));
		ASSERT_THAT(AreEqual(1, QueueFrag->Entries.Num(),
			TEXT("Exactly one clamped entry must be appended")));
		ASSERT_THAT(AreEqual(N, QueueFrag->Entries[0].Amount,
			TEXT("Clamped entry Amount must equal the fundable count N")));
	}

	// =========================================================================
	// Queue_ZeroFundable_NoEntry
	//
	// Empty input cannot fund the ingredient recipe. Returns 0, Entries stays empty.
	// =========================================================================
	TEST_METHOD(Queue_ZeroFundable_NoEntry)
	{
		using namespace ArcCraftFromStationInputCommandHelpers;

		const FPrimaryAssetId RecipeId = MakeRecipeId();
		const FArcRecipeDefinitionData Recipe = MakeConsumingTagRecipe();

		FMassEntityHandle Station = BuildQueueStationEntity(*EM);
		FMassEntityHandle Instigator = BuildInstigatorEntity(*EM);

		// No input deposited — recipe requires an ore ingredient that is absent.
		const int32 Queued = ArcCraftFromStationInput::ApplyQueueFromInput(*EM, Station, Instigator, RecipeId, Recipe, 3);

		ASSERT_THAT(AreEqual(0, Queued,
			TEXT("ApplyQueueFromInput must return 0 when input cannot fund a single craft")));

		const FArcCraftQueueFragment* QueueFrag = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		ASSERT_THAT(IsNotNull(QueueFrag));
		ASSERT_THAT(AreEqual(0, QueueFrag->Entries.Num(),
			TEXT("No queue entry must be appended when nothing is fundable")));
	}

	// =========================================================================
	// Queue_FreeRecipe_QueuesRequested
	//
	// 0-ingredient recipe consumes nothing per pass → funds the full requested Amount.
	// =========================================================================
	TEST_METHOD(Queue_FreeRecipe_QueuesRequested)
	{
		using namespace ArcCraftFromStationInputCommandHelpers;

		const int32 Requested = 4;

		const FPrimaryAssetId RecipeId = MakeRecipeId();
		const FArcRecipeDefinitionData Recipe = MakeFreeRecipe();

		FMassEntityHandle Station = BuildQueueStationEntity(*EM);
		FMassEntityHandle Instigator = BuildInstigatorEntity(*EM);

		// No input needed for a free recipe.
		const int32 Queued = ArcCraftFromStationInput::ApplyQueueFromInput(*EM, Station, Instigator, RecipeId, Recipe, Requested);

		ASSERT_THAT(AreEqual(Requested, Queued,
			TEXT("A 0-ingredient recipe must queue the full requested amount")));

		const FArcCraftQueueFragment* QueueFrag = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		ASSERT_THAT(IsNotNull(QueueFrag));
		ASSERT_THAT(AreEqual(1, QueueFrag->Entries.Num(),
			TEXT("Exactly one entry must be appended for the free recipe")));
		ASSERT_THAT(AreEqual(Requested, QueueFrag->Entries[0].Amount,
			TEXT("Free-recipe entry Amount must equal the requested amount")));
	}

	// =========================================================================
	// Queue_CoalescesIntoPendingTail
	//
	// Two calls with the same recipe + instigator coalesce into a single Pending
	// entry whose Amount is the sum. ApplyQueueFromInput does not consume station
	// input, so both calls see the same fundable input.
	// =========================================================================
	TEST_METHOD(Queue_CoalescesIntoPendingTail)
	{
		using namespace ArcCraftFromStationInputCommandHelpers;

		const FPrimaryAssetId Def = MakeOreItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId RecipeId = MakeRecipeId();
		const FArcRecipeDefinitionData Recipe = MakeConsumingTagRecipe();

		FMassEntityHandle Station = BuildQueueStationEntity(*EM);
		FMassEntityHandle Instigator = BuildInstigatorEntity(*EM);

		FArcCraftInputFragment* InputFrag = EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station);
		ASSERT_THAT(IsNotNull(InputFrag));
		InputFrag->InputItems.Add(MakeSpec(Def, 2)); // funds at least 1 per call

		const int32 Queued1 = ArcCraftFromStationInput::ApplyQueueFromInput(*EM, Station, Instigator, RecipeId, Recipe, 1);
		const int32 Queued2 = ArcCraftFromStationInput::ApplyQueueFromInput(*EM, Station, Instigator, RecipeId, Recipe, 1);

		ASSERT_THAT(AreEqual(1, Queued1,
			TEXT("First call must queue 1")));
		ASSERT_THAT(AreEqual(1, Queued2,
			TEXT("Second call must queue 1")));

		const FArcCraftQueueFragment* QueueFrag = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		ASSERT_THAT(IsNotNull(QueueFrag));
		ASSERT_THAT(AreEqual(1, QueueFrag->Entries.Num(),
			TEXT("Two matching calls must coalesce into a single queue entry")));
		ASSERT_THAT(AreEqual(2, QueueFrag->Entries[0].Amount,
			TEXT("Coalesced entry Amount must be the sum of both calls")));
		ASSERT_THAT(IsTrue(QueueFrag->Entries[0].State == EArcCraftQueueEntryState::Pending,
			TEXT("Coalesced entry must remain Pending")));
	}
};
