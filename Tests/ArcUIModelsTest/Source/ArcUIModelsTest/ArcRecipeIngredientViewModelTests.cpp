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

#include "NativeGameplayTags.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"


#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemId.h"

#include "Fragments/ArcMassItemStoreFragment.h"

#include "ArcCraft/Recipe/ArcRecipeIngredient.h"

#include "ViewModels/ArcRecipeIngredientViewModel.h"
#include "ViewModels/ArcIngredientCandidateViewModel.h"
#include "ViewModels/ArcCraftingViewModel.h"
#include "ViewModels/ArcItemViewModel.h"
#include "ViewModels/ArcItemStatRowViewModel.h"

#include "Items/Fragments/ArcItemFragment_UIDescription.h"
#include "ItemFragments/ArcItemFragment_ItemStats.h"
#include "Display/ArcStatDisplayRegistry.h"
#include "Attributes/ArcAttribute.h"

#include "Engine/AssetManager.h"
#include "AssetRegistry/AssetBundleData.h"

// ---------------------------------------------------------------------------
// Unique tag prefix: TAG_UIModelsTest_* — avoids ODR collisions with other
// test files that declare their own gameplay tags.
// ---------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UIModelsTest_Material, "Test.Material");

// ---------------------------------------------------------------------------
// File-scoped helpers
// ---------------------------------------------------------------------------

namespace ArcRecipeIngredientVMHelpers
{
	/**
	 * Write an item definition record with no extra fragments and return its id —
	 * sufficient for the ItemDef ingredient match, which only compares PrimaryAssetId.
	 */
	FPrimaryAssetId MakeItemDef(FArcItemTestRecordFiles& ItemRecords)
	{
		return ItemRecords.Write(FArcItemDefinitionData());
	}

	/**
	 * Build a minimal FArcItemSpec for the given definition, with a fresh
	 * item id and the requested stack count.
	 */
	FArcItemSpec MakeSpec(const FPrimaryAssetId& DefId, int32 StackCount = 1)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinition(DefId);
		Spec.SetItemId(FArcItemId::Generate());
		Spec.Amount = static_cast<uint16>(StackCount);
		return Spec;
	}

	/**
	 * Build an FInstancedStruct holding an FArcRecipeIngredient_ItemDef for the
	 * given definition id and required amount.
	 */
	FInstancedStruct MakeItemDefIngredient(const FPrimaryAssetId& DefId, int32 Amount)
	{
		FArcRecipeIngredient_ItemDef Ingredient;
		Ingredient.ItemDefinitionId = FArcNamedPrimaryAssetId(DefId);
		Ingredient.Amount = Amount;
		FInstancedStruct Result;
		Result.InitializeAs<FArcRecipeIngredient_ItemDef>(Ingredient);
		return Result;
	}

	/**
	 * Build an FInstancedStruct holding an FArcRecipeIngredient_Tags.
	 */
	FInstancedStruct MakeTagIngredient(const FGameplayTagContainer& RequiredTags, int32 Amount)
	{
		FArcRecipeIngredient_Tags Ingredient;
		Ingredient.RequiredTags = RequiredTags;
		Ingredient.Amount = Amount;
		FInstancedStruct Result;
		Result.InitializeAs<FArcRecipeIngredient_Tags>(Ingredient);
		return Result;
	}

	/**
	 * Build one loose inventory row of StackCount stacks of DefId in the base item store,
	 * the shape the crafting struct model hands to RefreshFrom.
	 */
	TArray<FArcCraftStoreItemRow> MakeInventoryRows(const FPrimaryAssetId& DefId, int32 StackCount)
	{
		FArcCraftStoreItemRow Row;
		Row.StoreType = FArcMassItemStoreFragment::StaticStruct();
		Row.Spec = MakeSpec(DefId, StackCount);
		Row.Stacks = StackCount;

		TArray<FArcCraftStoreItemRow> Rows;
		Rows.Add(Row);
		return Rows;
	}

	/** One preview row of StackCount stacks of DefId, owned by Owner in the base item store. */
	TArray<FArcCraftStoreItemRow> MakePreviewRows(const FPrimaryAssetId& DefId, int32 StackCount, FMassEntityHandle Owner)
	{
		FArcCraftStoreItemRow Row;
		Row.OwnerEntity = Owner;
		Row.StoreType = FArcMassItemStoreFragment::StaticStruct();
		Row.Spec = MakeSpec(DefId, StackCount);
		Row.Stacks = StackCount;

		TArray<FArcCraftStoreItemRow> Rows;
		Rows.Add(Row);
		return Rows;
	}

	FInstancedStruct MakeOptionalItemDefIngredient(const FPrimaryAssetId& DefId, int32 Amount)
	{
		FArcRecipeIngredient_ItemDef Ingredient;
		Ingredient.ItemDefinitionId = FArcNamedPrimaryAssetId(DefId);
		Ingredient.Amount = Amount;
		Ingredient.bOptional = true;
		return FInstancedStruct::Make(Ingredient);
	}
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

TEST_CLASS(ArcRecipeIngredientViewModelTests, "ArcUIModels.IngredientVM")
{
	FArcItemTestRecordFiles ItemRecords;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
	}

	// =========================================================================
	// Test 1 — Item only in inventory, nothing in station => Have == inventory amount
	//
	// Have = HaveFromInventory + HaveFromStation; with station empty,
	// Have == HaveFromInventory. Only the station input view fills a slot, so an
	// inventory-only item leaves the slot unsatisfied with Missing == Required.
	// =========================================================================
	TEST_METHOD(ItemOnlyInInventory_HaveEqualsInventoryAmount)
	{
		using namespace ArcRecipeIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		// Inventory: 3 stacks of the item.
		const TArray<FArcCraftStoreItemRow> InventoryRows = MakeInventoryRows(Def, 3);

		// Ingredient: needs 2.
		FInstancedStruct IngredientStruct = MakeItemDefIngredient(Def, 2);

		// Station input: empty.
		TArray<FArcItemSpec> StationInput;

		UArcRecipeIngredientViewModel* VM = NewObject<UArcRecipeIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		VM->RefreshFrom(IngredientStruct, InventoryRows, StationInput, TConstArrayView<FArcCraftStoreItemRow>(), nullptr, nullptr);

		ASSERT_THAT(AreEqual(2, VM->GetRequired(), TEXT("Required must equal ingredient Amount (2)")));
		ASSERT_THAT(AreEqual(3, VM->GetHave(),
			TEXT("Have must reflect the 3 inventory stacks when station is empty")));
		ASSERT_THAT(IsFalse(VM->GetbSatisfied(),
			TEXT("bSatisfied must be false when the station input view holds nothing for the slot")));
		ASSERT_THAT(AreEqual(2, VM->GetMissing(),
			TEXT("Missing counts the station input view only, so it equals Required")));
	}

	// =========================================================================
	// Test 2 — Item only in station input => Have == station amount
	//
	// Inventory empty, station has 3 deposited. Have == station supply, bSatisfied.
	// =========================================================================
	TEST_METHOD(ItemOnlyInStationInput_HaveEqualsStationAmount)
	{
		using namespace ArcRecipeIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		// Ingredient: needs 2.
		FInstancedStruct IngredientStruct = MakeItemDefIngredient(Def, 2);

		// Station input: 3 stacks of the same definition.
		TArray<FArcItemSpec> StationInput;
		StationInput.Add(MakeSpec(Def, 3));

		UArcRecipeIngredientViewModel* VM = NewObject<UArcRecipeIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		VM->RefreshFrom(IngredientStruct, TConstArrayView<FArcCraftStoreItemRow>(), StationInput, TConstArrayView<FArcCraftStoreItemRow>(), nullptr, nullptr);

		ASSERT_THAT(AreEqual(2, VM->GetRequired(), TEXT("Required must equal ingredient Amount (2)")));
		ASSERT_THAT(AreEqual(3, VM->GetHave(),
			TEXT("Have must reflect the 3 station stacks when inventory is empty")));
		ASSERT_THAT(IsTrue(VM->GetbSatisfied(),
			TEXT("bSatisfied must be true when the station holds 3 >= Required(2)")));
	}

	// =========================================================================
	// Test 3 — Item in both inventory and station => Have == sum
	//
	// Inv=2, Station=2, Required=3 => Have=4, but only the station's 2 fill the
	// slot: bSatisfied==false, Missing==1.
	// =========================================================================
	TEST_METHOD(ItemInBothInventoryAndStation_HaveIsSum)
	{
		using namespace ArcRecipeIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		// Inventory: 2 stacks.
		const TArray<FArcCraftStoreItemRow> InventoryRows = MakeInventoryRows(Def, 2);

		// Ingredient: needs 3.
		FInstancedStruct IngredientStruct = MakeItemDefIngredient(Def, 3);

		// Station input: 2 stacks of the same item definition (VM sums Spec.Amount for station).
		TArray<FArcItemSpec> StationInput;
		StationInput.Add(MakeSpec(Def, 2));

		UArcRecipeIngredientViewModel* VM = NewObject<UArcRecipeIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		VM->RefreshFrom(IngredientStruct, InventoryRows, StationInput, TConstArrayView<FArcCraftStoreItemRow>(), nullptr, nullptr);

		ASSERT_THAT(AreEqual(3, VM->GetRequired(), TEXT("Required must equal ingredient Amount (3)")));
		ASSERT_THAT(AreEqual(4, VM->GetHave(),
			TEXT("Have must be the sum of inventory + station amounts")));
		ASSERT_THAT(IsFalse(VM->GetbSatisfied(),
			TEXT("Must not be satisfied when the station holds 2 < Required(3), whatever the inventory holds")));
		ASSERT_THAT(AreEqual(1, VM->GetMissing(),
			TEXT("Missing must be Required(3) - station(2)")));
	}

	// =========================================================================
	// Test 4 — Nothing in inventory or station => Have == 0, not satisfied
	//
	// Regression: empty state => bSatisfied false, Missing == Required.
	// =========================================================================
	TEST_METHOD(NothingInInventoryOrStation_NotSatisfied)
	{
		using namespace ArcRecipeIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		FInstancedStruct IngredientStruct = MakeItemDefIngredient(Def, 2);
		TArray<FArcItemSpec> StationInput;

		UArcRecipeIngredientViewModel* VM = NewObject<UArcRecipeIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		VM->RefreshFrom(IngredientStruct, TConstArrayView<FArcCraftStoreItemRow>(), StationInput, TConstArrayView<FArcCraftStoreItemRow>(), nullptr, nullptr);

		ASSERT_THAT(AreEqual(2, VM->GetRequired(), TEXT("Required must be 2")));
		ASSERT_THAT(AreEqual(0, VM->GetHave(), TEXT("Have must be 0 when nothing is available")));
		ASSERT_THAT(IsFalse(VM->GetbSatisfied(), TEXT("bSatisfied must be false when Have == 0")));
		ASSERT_THAT(AreEqual(VM->GetRequired(), VM->GetMissing(),
			TEXT("Missing must equal Required when not satisfied")));
	}

	TEST_METHOD(PreviewInput_CountsTowardStation)
	{
		using namespace ArcRecipeIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		FInstancedStruct IngredientStruct = MakeItemDefIngredient(Def, 3);
		TArray<FArcItemSpec> ServerInput;
		ServerInput.Add(MakeSpec(Def, 1));
		const TArray<FArcCraftStoreItemRow> PreviewRows = MakePreviewRows(Def, 2, FMassEntityHandle(7, 1));

		UArcRecipeIngredientViewModel* VM = NewObject<UArcRecipeIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);
		VM->RefreshFrom(IngredientStruct, TConstArrayView<FArcCraftStoreItemRow>(), ServerInput, PreviewRows, nullptr, nullptr);

		ASSERT_THAT(AreEqual(3, VM->GetHaveFromStation(), TEXT("HaveFromStation = server input 1 + preview 2")));
		ASSERT_THAT(AreEqual(0, VM->GetMissing(), TEXT("The preview covers the missing stacks")));
		ASSERT_THAT(IsTrue(VM->GetbSatisfied(), TEXT("Server input plus preview satisfy the slot")));
		ASSERT_THAT(IsTrue(VM->GetbFilled()));
	}

	TEST_METHOD(OptionalSlotEmpty_SatisfiedAndMarkedEmpty)
	{
		using namespace ArcRecipeIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		FInstancedStruct IngredientStruct = MakeOptionalItemDefIngredient(Def, 1);
		const TArray<FArcCraftStoreItemRow> InventoryRows = MakeInventoryRows(Def, 4);

		UArcRecipeIngredientViewModel* VM = NewObject<UArcRecipeIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);
		VM->RefreshFrom(IngredientStruct, InventoryRows, TConstArrayView<FArcItemSpec>(), TConstArrayView<FArcCraftStoreItemRow>(), nullptr, nullptr);

		ASSERT_THAT(IsTrue(VM->GetbOptional()));
		ASSERT_THAT(IsTrue(VM->GetbSatisfied(), TEXT("An optional slot never blocks, so it is always satisfied")));
		ASSERT_THAT(IsTrue(VM->GetbOptionalEmpty(), TEXT("Nothing fitting in the input view: the slot shows as empty")));
		ASSERT_THAT(IsFalse(VM->GetbFilled()));
		ASSERT_THAT(AreEqual(0, VM->GetHaveFromStation(), TEXT("Inventory stacks do not fill the slot")));
	}

	TEST_METHOD(OptionalSlotFilledByPreview_NotEmpty)
	{
		using namespace ArcRecipeIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		FInstancedStruct IngredientStruct = MakeOptionalItemDefIngredient(Def, 1);
		const TArray<FArcCraftStoreItemRow> PreviewRows = MakePreviewRows(Def, 1, FMassEntityHandle(7, 1));

		UArcRecipeIngredientViewModel* VM = NewObject<UArcRecipeIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);
		VM->RefreshFrom(IngredientStruct, TConstArrayView<FArcCraftStoreItemRow>(), TConstArrayView<FArcItemSpec>(), PreviewRows, nullptr, nullptr);

		ASSERT_THAT(IsTrue(VM->GetbSatisfied()));
		ASSERT_THAT(IsFalse(VM->GetbOptionalEmpty(), TEXT("A fitting preview row fills the optional slot")));
		ASSERT_THAT(IsTrue(VM->GetbFilled()));
	}

	TEST_METHOD(RequiredSlot_NotOptional_NeverMarkedEmpty)
	{
		using namespace ArcRecipeIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		FInstancedStruct IngredientStruct = MakeItemDefIngredient(Def, 1);

		UArcRecipeIngredientViewModel* VM = NewObject<UArcRecipeIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);
		VM->RefreshFrom(IngredientStruct, TConstArrayView<FArcCraftStoreItemRow>(), TConstArrayView<FArcItemSpec>(), TConstArrayView<FArcCraftStoreItemRow>(), nullptr, nullptr);

		ASSERT_THAT(IsFalse(VM->GetbOptional()));
		ASSERT_THAT(IsFalse(VM->GetbSatisfied(), TEXT("An empty required slot is not satisfied")));
		ASSERT_THAT(IsFalse(VM->GetbOptionalEmpty(), TEXT("The empty flag is for optional slots only")));
	}

	// =========================================================================
	// Test 5 — _ItemDef slot payload: RequiredItemId valid + matches definition
	//
	// After RefreshFrom with an FArcRecipeIngredient_ItemDef ingredient,
	// GetRequiredItemId() must be valid and its AssetId must match the definition.
	// GetRequiredTags() must be empty (ByItemDef slot).
	// =========================================================================
	TEST_METHOD(ItemDefSlot_RequiredItemId_MatchesDefinition)
	{
		using namespace ArcRecipeIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		FInstancedStruct IngredientStruct = MakeItemDefIngredient(Def, 1);
		TArray<FArcItemSpec> StationInput;

		UArcRecipeIngredientViewModel* VM = NewObject<UArcRecipeIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		VM->RefreshFrom(IngredientStruct, TConstArrayView<FArcCraftStoreItemRow>(), StationInput, TConstArrayView<FArcCraftStoreItemRow>(), nullptr, nullptr);

		ASSERT_THAT(IsTrue(VM->GetRequiredItemId().AssetId.IsValid(),
			TEXT("RequiredItemId.AssetId must be valid for an _ItemDef ingredient")));
		ASSERT_THAT(IsTrue(VM->GetRequiredItemId().AssetId == Def,
			TEXT("RequiredItemId.AssetId must match the ingredient's definition id")));
		ASSERT_THAT(IsTrue(VM->GetRequiredTags().IsEmpty(),
			TEXT("RequiredTags must be empty for an _ItemDef slot")));
	}

	// =========================================================================
	// Test 6 — _Tags slot payload: RequiredTags == ingredient tags, RequiredItemId invalid
	//
	// After RefreshFrom with an FArcRecipeIngredient_Tags ingredient carrying
	// TAG_UIModelsTest_Material, GetRequiredTags() must contain that tag and
	// GetRequiredItemId() must be invalid (ByTag slot has no specific definition).
	// =========================================================================
	TEST_METHOD(TagsSlot_RequiredTags_MatchIngredientTags)
	{
		using namespace ArcRecipeIngredientVMHelpers;

		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_UIModelsTest_Material);

		FInstancedStruct IngredientStruct = MakeTagIngredient(Tags, 1);
		TArray<FArcItemSpec> StationInput;

		UArcRecipeIngredientViewModel* VM = NewObject<UArcRecipeIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		VM->RefreshFrom(IngredientStruct, TConstArrayView<FArcCraftStoreItemRow>(), StationInput, TConstArrayView<FArcCraftStoreItemRow>(), nullptr, nullptr);

		ASSERT_THAT(IsTrue(VM->GetRequiredTags().HasTag(TAG_UIModelsTest_Material),
			TEXT("RequiredTags must contain the tag declared on the _Tags ingredient")));
		ASSERT_THAT(IsFalse(VM->GetRequiredItemId().AssetId.IsValid(),
			TEXT("RequiredItemId must be invalid for a _Tags slot")));
	}
};

// ===========================================================================
// Test class: candidates and the inventory / station have split
// ===========================================================================

TEST_CLASS(ArcIngredientVMCandidateTests, "ArcUIModels.IngredientVM.Candidates")
{
	FArcItemTestRecordFiles ItemRecords;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
	}

	// =========================================================================
	// InventoryCandidates_AppearWithFromStationFalse
	//
	// An item only in inventory produces a candidate with bFromStation==false.
	// A station-deposited item produces a candidate with bFromStation==true.
	// =========================================================================
	TEST_METHOD(InventoryCandidates_AppearWithFromStationFalse)
	{
		using namespace ArcRecipeIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		// Inventory: 1 stack.
		const TArray<FArcCraftStoreItemRow> InventoryRows = MakeInventoryRows(Def, 1);

		// Station: 1 stack of the same def (different item id).
		FArcItemSpec StationSpec = MakeSpec(Def, 1);
		TArray<FArcItemSpec> StationInput;
		StationInput.Add(StationSpec);

		FInstancedStruct IngredientStruct = MakeItemDefIngredient(Def, 1);

		UArcRecipeIngredientViewModel* VM = NewObject<UArcRecipeIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);
		VM->RefreshFrom(IngredientStruct, InventoryRows, StationInput, TConstArrayView<FArcCraftStoreItemRow>(), nullptr, nullptr);

		ASSERT_THAT(AreEqual(1, VM->GetHaveFromInventory(),
			TEXT("HaveFromInventory must be 1 from the inventory row")));

		const TArray<TObjectPtr<UArcIngredientCandidateViewModel>>& Candidates = VM->GetCandidates();
		const UArcIngredientCandidateViewModel* InvCandidate = nullptr;
		for (const TObjectPtr<UArcIngredientCandidateViewModel>& C : Candidates)
		{
			if (C != nullptr && !C->GetbFromStation())
			{
				InvCandidate = C.Get();
				break;
			}
		}
		ASSERT_THAT(IsNotNull(InvCandidate,
			TEXT("Must find at least one inventory candidate with bFromStation==false")));

		const UArcIngredientCandidateViewModel* StationCandidate = nullptr;
		for (const TObjectPtr<UArcIngredientCandidateViewModel>& C : Candidates)
		{
			if (C != nullptr && C->GetItemId() == StationSpec.ItemId)
			{
				StationCandidate = C.Get();
				break;
			}
		}
		ASSERT_THAT(IsNotNull(StationCandidate, TEXT("The station input spec must produce a candidate")));
		ASSERT_THAT(IsTrue(StationCandidate->GetbFromStation(),
			TEXT("Candidate with station-spec ItemId must have bFromStation==true")));
		ASSERT_THAT(IsFalse(StationCandidate->GetbPreview(), TEXT("A server input candidate is not a preview row")));
	}

	TEST_METHOD(PreviewCandidates_CarryPreviewOwnerAndStore)
	{
		using namespace ArcRecipeIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FMassEntityHandle Owner(7, 1);
		const TArray<FArcCraftStoreItemRow> PreviewRows = MakePreviewRows(Def, 2, Owner);
		FInstancedStruct IngredientStruct = MakeItemDefIngredient(Def, 1);

		UArcRecipeIngredientViewModel* VM = NewObject<UArcRecipeIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);
		VM->RefreshFrom(IngredientStruct, TConstArrayView<FArcCraftStoreItemRow>(), TConstArrayView<FArcItemSpec>(), PreviewRows, nullptr, nullptr);

		const UArcIngredientCandidateViewModel* PreviewCandidate = nullptr;
		for (const TObjectPtr<UArcIngredientCandidateViewModel>& C : VM->GetCandidates())
		{
			if (C != nullptr && C->GetItemId() == PreviewRows[0].Spec.ItemId)
			{
				PreviewCandidate = C.Get();
				break;
			}
		}
		ASSERT_THAT(IsNotNull(PreviewCandidate, TEXT("A preview row must produce a candidate")));
		ASSERT_THAT(IsTrue(PreviewCandidate->GetbPreview(), TEXT("The candidate is marked as preview")));
		ASSERT_THAT(IsTrue(PreviewCandidate->GetbFromStation(), TEXT("A preview row is part of the station input view")));
		ASSERT_THAT(IsTrue(PreviewCandidate->GetOwnerEntity() == Owner, TEXT("The candidate carries the owner entity of its row")));
		ASSERT_THAT(IsTrue(PreviewCandidate->GetStoreType() == FArcMassItemStoreFragment::StaticStruct(),
			TEXT("The candidate carries the store type of its row")));
		ASSERT_THAT(AreEqual(2, PreviewCandidate->GetStacks()));
	}

	// =========================================================================
	// HaveSplit_Fields
	//
	// Inventory 3 + station 2 => HaveFromInventory==3, HaveFromStation==2, Have==5.
	// =========================================================================
	TEST_METHOD(HaveSplit_Fields)
	{
		using namespace ArcRecipeIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		// Inventory: 3 stacks.
		const TArray<FArcCraftStoreItemRow> InventoryRows = MakeInventoryRows(Def, 3);

		// Station: 2 stacks (spec with a fresh item id so it counts separately).
		TArray<FArcItemSpec> StationInput;
		StationInput.Add(MakeSpec(Def, 2));

		FInstancedStruct IngredientStruct = MakeItemDefIngredient(Def, 1);

		UArcRecipeIngredientViewModel* VM = NewObject<UArcRecipeIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);
		VM->RefreshFrom(IngredientStruct, InventoryRows, StationInput, TConstArrayView<FArcCraftStoreItemRow>(), nullptr, nullptr);

		ASSERT_THAT(AreEqual(3, VM->GetHaveFromInventory(),
			TEXT("HaveFromInventory must reflect the 3 inventory stacks")));
		ASSERT_THAT(AreEqual(2, VM->GetHaveFromStation(),
			TEXT("HaveFromStation must reflect the 2 station stacks")));
		ASSERT_THAT(AreEqual(5, VM->GetHave(),
			TEXT("Have must equal HaveFromInventory + HaveFromStation")));
	}

	// =========================================================================
	// SlottedAvailableText — "Slotted / Required"
	//   Slotted  = HaveFromStation (matching items in station input)
	//   Required = the slot's per-craft required amount
	// Station 2, required 1 => "2 / 1" (inventory does not affect this field).
	// =========================================================================
	TEST_METHOD(SlottedAvailableText_Format)
	{
		using namespace ArcRecipeIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const TArray<FArcCraftStoreItemRow> InventoryRows = MakeInventoryRows(Def, 3);

		TArray<FArcItemSpec> StationInput;
		StationInput.Add(MakeSpec(Def, 2));

		FInstancedStruct IngredientStruct = MakeItemDefIngredient(Def, 1);

		UArcRecipeIngredientViewModel* VM = NewObject<UArcRecipeIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);
		VM->RefreshFrom(IngredientStruct, InventoryRows, StationInput, TConstArrayView<FArcCraftStoreItemRow>(), nullptr, nullptr);

		// Slotted = HaveFromStation (2), Required = 1.
		ASSERT_THAT(AreEqual(FString(TEXT("2 / 1")), VM->GetSlottedAvailableText().ToString(),
			TEXT("SlottedAvailableText must read \"<HaveFromStation> / <Required>\"")));
	}
};


// ===========================================================================
// Test class: UArcCraftingViewModel hovered/pinned/detail ingredient state
//
// STRATEGY: SetHoveredIngredient/PinIngredient/SelectRecipe are pure pointer
// bookkeeping over UArcRecipeIngredientViewModel instances — no Mass world or
// AssetManager involved. Ingredient VMs are plain NewObject instances used only
// as distinct identities; their RefreshFrom state is irrelevant to this state
// machine.
// ===========================================================================

TEST_CLASS(ArcCraftingViewModelHoverPinTests, "ArcUIModels.CraftingVM.HoverPin")
{
	FArcItemTestRecordFiles ItemRecords;
	UArcCraftingViewModel* CraftVM = nullptr;
	UArcRecipeIngredientViewModel* IngredientA = nullptr;
	UArcRecipeIngredientViewModel* IngredientB = nullptr;
	UArcRecipeIngredientViewModel* IngredientC = nullptr;

	BEFORE_EACH()
	{
		CraftVM = NewObject<UArcCraftingViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
		check(CraftVM);

		IngredientA = NewObject<UArcRecipeIngredientViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
		IngredientB = NewObject<UArcRecipeIngredientViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
		IngredientC = NewObject<UArcRecipeIngredientViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
	}

	// =========================================================================
	// Test 1 — Hover with nothing pinned: DetailIngredient follows the hover.
	// =========================================================================
	TEST_METHOD(Hover_NothingPinned_DetailFollowsHover)
	{
		CraftVM->SetHoveredIngredient(IngredientA);

		ASSERT_THAT(IsTrue(CraftVM->GetHoveredIngredient() == IngredientA,
			TEXT("HoveredIngredient must be set to A")));
		ASSERT_THAT(IsNull(CraftVM->GetPinnedIngredient(),
			TEXT("PinnedIngredient must remain null when only hovering")));
		ASSERT_THAT(IsTrue(CraftVM->GetDetailIngredient() == IngredientA,
			TEXT("DetailIngredient must equal the hovered ingredient when nothing is pinned")));
	}

	// =========================================================================
	// Test 2 — Pin wins over a subsequent hover change.
	// =========================================================================
	TEST_METHOD(Pin_WinsOverSubsequentHover)
	{
		CraftVM->PinIngredient(IngredientB);
		ASSERT_THAT(IsTrue(CraftVM->GetPinnedIngredient() == IngredientB,
			TEXT("PinnedIngredient must be B after pinning")));
		ASSERT_THAT(IsTrue(CraftVM->GetDetailIngredient() == IngredientB,
			TEXT("DetailIngredient must equal the pinned ingredient B")));

		CraftVM->SetHoveredIngredient(IngredientC);

		ASSERT_THAT(IsTrue(CraftVM->GetHoveredIngredient() == IngredientC,
			TEXT("HoveredIngredient must update to C")));
		ASSERT_THAT(IsTrue(CraftVM->GetPinnedIngredient() == IngredientB,
			TEXT("PinnedIngredient must remain B — hover does not clear a pin")));
		ASSERT_THAT(IsTrue(CraftVM->GetDetailIngredient() == IngredientB,
			TEXT("DetailIngredient must still equal the pinned ingredient B, not the newly hovered C")));
	}

	// =========================================================================
	// Test 3 — Re-pinning the same ingredient toggles the pin off; DetailIngredient
	// falls back to whatever is currently hovered.
	// =========================================================================
	TEST_METHOD(RepinSameIngredient_TogglesPinOff_FallsBackToHover)
	{
		CraftVM->SetHoveredIngredient(IngredientC);
		CraftVM->PinIngredient(IngredientB);
		ASSERT_THAT(IsTrue(CraftVM->GetPinnedIngredient() == IngredientB));

		// Pin the same ingredient again — must toggle off.
		CraftVM->PinIngredient(IngredientB);

		ASSERT_THAT(IsNull(CraftVM->GetPinnedIngredient(),
			TEXT("Re-pinning the same ingredient must clear PinnedIngredient")));
		ASSERT_THAT(IsTrue(CraftVM->GetDetailIngredient() == IngredientC,
			TEXT("DetailIngredient must fall back to the currently hovered ingredient (C) once unpinned")));
	}

	// =========================================================================
	// Test 4 — SelectRecipe(nullptr) clears Hovered/Pinned/Detail ingredient state.
	// =========================================================================
	TEST_METHOD(SelectRecipeNull_ClearsHoveredPinnedDetail)
	{
		// Establish a selected recipe first — hover/pin only exist while a recipe is
		// selected, and the real station-change path is a non-null -> null transition.
		// (SelectRecipe's no-op guard would otherwise early-out on nullptr -> nullptr.)
		UArcRecipeViewModel* Recipe = NewObject<UArcRecipeViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
		CraftVM->SelectRecipe(Recipe);

		CraftVM->SetHoveredIngredient(IngredientA);
		CraftVM->PinIngredient(IngredientB);
		ASSERT_THAT(IsTrue(CraftVM->GetPinnedIngredient() == IngredientB));

		CraftVM->SelectRecipe(nullptr);

		ASSERT_THAT(IsNull(CraftVM->GetHoveredIngredient(),
			TEXT("SelectRecipe(nullptr) must clear HoveredIngredient")));
		ASSERT_THAT(IsNull(CraftVM->GetPinnedIngredient(),
			TEXT("SelectRecipe(nullptr) must clear PinnedIngredient")));
		ASSERT_THAT(IsNull(CraftVM->GetDetailIngredient(),
			TEXT("SelectRecipe(nullptr) must clear DetailIngredient")));
	}
};

// ===========================================================================
// Test class: UArcItemViewModel::RefreshFromDefinition
//
// STRATEGY: RefreshFromDefinition reads FArcItemDefinitionData::FindFragment<T>()
// directly off the passed-in record. Records are written and indexed through
// FArcItemTestRecordFiles and resolved with FArcItemDefinitionData::Find. FArcAttributeRef only needs a
// valid UScriptStruct* + FName identity for TMap keying/humanization — it does
// not require a real FMassSparseFragment-derived attribute set, so no test-only
// attribute-set header is needed here.
// ===========================================================================

namespace ArcItemVMRefreshFromDefinitionHelpers
{
	/** Build an FArcAttributeRef with a stable identity: any reflected UScriptStruct as
	 *  FragmentType (only used for TMap keying here, never actually read as a fragment)
	 *  plus the given PropertyName (surfaces via the humanized-fallback path on a registry miss). */
	FArcAttributeRef MakeAttributeRef(const FName& PropertyName)
	{
		FArcAttributeRef Ref;
		Ref.FragmentType = FArcItemFragment_ItemStats::StaticStruct();
		Ref.PropertyName = PropertyName;
		return Ref;
	}

	/** Item definition carrying UIDescription (name/description) and one ItemStats
	 *  DefaultStats entry keyed by the given PropertyName. */
	const FArcItemDefinitionData* MakeStatItemDef(FArcItemTestRecordFiles& ItemRecords, const FText& DisplayName, const FText& Description, const FName& StatPropertyName, float StatValue)
	{
		FArcItemDefinitionData Def;

		FArcItemFragment_UIDescription UIFrag;
		UIFrag.ItemName = DisplayName;
		UIFrag.ItemDescription = Description;
		ArcItemTestRecords::AddFragment(Def, UIFrag);

		FArcItemFragment_ItemStats StatsFrag;
		FArcItemAttributeStat Stat;
		Stat.Type = EArcModType::Additive;
		Stat.Value = FArcScalableFloat(StatValue);
		Stat.Stat = MakeAttributeRef(StatPropertyName);
		StatsFrag.DefaultStats.Add(Stat);
		ArcItemTestRecords::AddFragment(Def, StatsFrag);

		return FArcItemDefinitionData::Find(ItemRecords.Write(Def));
	}
}

TEST_CLASS(ArcItemViewModelRefreshFromDefinitionTests, "ArcUIModels.ItemVM.RefreshFromDefinition")
{
	FArcItemTestRecordFiles ItemRecords;
	// =========================================================================
	// Test 1 — Name/Description populated from UIDescription; one StatRow built
	// from DefaultStats with the registry HIT giving the authored Label.
	// =========================================================================
	TEST_METHOD(RegistryHit_UsesAuthoredLabel)
	{
		using namespace ArcItemVMRefreshFromDefinitionHelpers;

		const FArcItemDefinitionData* Def = MakeStatItemDef(
			ItemRecords, FText::FromString(TEXT("Test Widget")), FText::FromString(TEXT("A widget for testing.")),
			TEXT("FireRate"), 4.f);
		ASSERT_THAT(IsNotNull(Def, TEXT("Item record must be written and indexed")));

		UArcStatDisplayRegistry* StatReg = NewObject<UArcStatDisplayRegistry>(GetTransientPackage(), NAME_None, RF_Transient);
		FArcStatDisplayData DisplayData;
		DisplayData.DisplayName = FText::FromString(TEXT("Fire Rate"));
		StatReg->Entries.Add(MakeAttributeRef(TEXT("FireRate")), DisplayData);

		UArcItemViewModel* VM = NewObject<UArcItemViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
		VM->RefreshFromDefinition(Def, StatReg);

		ASSERT_THAT(AreEqual(FString(TEXT("Test Widget")), VM->GetDisplayName().ToString()));
		ASSERT_THAT(AreEqual(FString(TEXT("A widget for testing.")), VM->GetDescription().ToString()));
		ASSERT_THAT(AreEqual(1, VM->GetStatRows().Num(), TEXT("One DefaultStats entry must produce one StatRow")));
		ASSERT_THAT(IsNotNull(VM->GetStatRows()[0].Get()));
		ASSERT_THAT(AreEqual(FString(TEXT("Fire Rate")), VM->GetStatRows()[0]->GetLabel().ToString(),
			TEXT("A registry hit must use the authored DisplayName")));
	}

	// =========================================================================
	// Test 2 — Registry MISS falls back to a humanized PropertyName label.
	// =========================================================================
	TEST_METHOD(RegistryMiss_FallsBackToHumanizedPropertyName)
	{
		using namespace ArcItemVMRefreshFromDefinitionHelpers;

		const FArcItemDefinitionData* Def = MakeStatItemDef(
			ItemRecords, FText::FromString(TEXT("Plain Item")), FText::GetEmpty(),
			TEXT("ReloadSpeed"), 2.f);
		ASSERT_THAT(IsNotNull(Def, TEXT("Item record must be written and indexed")));

		// Empty registry — FindDisplay always misses.
		UArcStatDisplayRegistry* StatReg = NewObject<UArcStatDisplayRegistry>(GetTransientPackage(), NAME_None, RF_Transient);

		UArcItemViewModel* VM = NewObject<UArcItemViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
		VM->RefreshFromDefinition(Def, StatReg);

		ASSERT_THAT(AreEqual(1, VM->GetStatRows().Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("Reload Speed")), VM->GetStatRows()[0]->GetLabel().ToString(),
			TEXT("A registry miss must humanize the raw PropertyName (\"ReloadSpeed\" -> \"Reload Speed\")")));
	}

	// =========================================================================
	// Test 3 — Passing a null Def clears name/description/icon/stat rows.
	// =========================================================================
	TEST_METHOD(NullDef_ClearsFields)
	{
		using namespace ArcItemVMRefreshFromDefinitionHelpers;

		const FArcItemDefinitionData* Def = MakeStatItemDef(
			ItemRecords, FText::FromString(TEXT("Has Data")), FText::FromString(TEXT("Some description")),
			TEXT("Damage"), 10.f);
		ASSERT_THAT(IsNotNull(Def, TEXT("Item record must be written and indexed")));

		UArcItemViewModel* VM = NewObject<UArcItemViewModel>(GetTransientPackage(), NAME_None, RF_Transient);

		// First populate from a real def so we can observe the clear.
		VM->RefreshFromDefinition(Def, nullptr);
		ASSERT_THAT(AreEqual(FString(TEXT("Has Data")), VM->GetDisplayName().ToString()));
		ASSERT_THAT(AreEqual(1, VM->GetStatRows().Num()));

		// Now refresh with a null def — everything definition-derived must clear.
		VM->RefreshFromDefinition(nullptr, nullptr);

		ASSERT_THAT(IsFalse(VM->GetItemDefinition().IsValid(), TEXT("ItemDefinition must clear to invalid")));
		ASSERT_THAT(IsTrue(VM->GetDisplayName().IsEmpty(), TEXT("DisplayName must clear")));
		ASSERT_THAT(IsTrue(VM->GetDescription().IsEmpty(), TEXT("Description must clear")));
		ASSERT_THAT(AreEqual(0, VM->GetStatRows().Num(), TEXT("StatRows must clear to empty on a null def")));
	}
};
