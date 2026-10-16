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

#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

#include "Fragments/ArcMassItemStoreFragment.h"

#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"

#include "ViewModels/ArcCraftingViewModel.h"
#include "ViewModels/ArcIngredientCandidateViewModel.h"
#include "ViewModels/ArcItemViewModel.h"
#include "ViewModels/ArcRecipeIngredientViewModel.h"
#include "ViewModels/ArcRecipeViewModel.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InPrev_Gem, "Test.InPrev.Gem");

namespace ArcCraftingVMInputPreviewTestsLocal
{
	const FMassEntityHandle PlayerEntity(7, 1);

	FArcItemSpec MakeSpec(const FPrimaryAssetId& Def, int32 Stacks)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinition(Def);
		Spec.SetItemId(FArcItemId::Generate());
		Spec.Amount = static_cast<uint16>(Stacks);
		return Spec;
	}

	FArcCraftStoreItemRow MakeRow(const FPrimaryAssetId& Def, int32 Stacks, const UScriptStruct* StoreType = FArcMassItemStoreFragment::StaticStruct())
	{
		FArcCraftStoreItemRow Row;
		Row.OwnerEntity = PlayerEntity;
		Row.StoreType = const_cast<UScriptStruct*>(StoreType);
		Row.Spec = MakeSpec(Def, Stacks);
		Row.Stacks = Stacks;
		return Row;
	}

	UArcItemViewModel* MakeItemVM(const FArcCraftStoreItemRow& Row)
	{
		UArcItemViewModel* VM = NewObject<UArcItemViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
		VM->RefreshFrom(Row.Spec, Row.Stacks, nullptr);
		return VM;
	}

	int32 PreviewStacksOf(const UArcCraftingViewModel& CraftVM, const FArcItemId& ItemId)
	{
		int32 Stacks = 0;
		for (const FArcCraftStoreItemRow& Row : CraftVM.GetPreviewRows())
		{
			Stacks += Row.Spec.ItemId == ItemId ? Row.Stacks : 0;
		}
		return Stacks;
	}

	UArcIngredientCandidateViewModel* FindPreviewRow(const UArcCraftingViewModel& CraftVM, const FArcItemId& ItemId)
	{
		for (const TObjectPtr<UArcIngredientCandidateViewModel>& Row : CraftVM.GetStationInput())
		{
			if (Row != nullptr && Row->GetbPreview() && Row->GetItemId() == ItemId)
			{
				return Row.Get();
			}
		}
		return nullptr;
	}

	struct FBootsFixture
	{
		FPrimaryAssetId Leather;
		FPrimaryAssetId Thread;
		FPrimaryAssetId Gem;
		FArcRecipeDefinitionData Recipe;

		bool Write(FArcItemTestRecordFiles& ItemRecords)
		{
			Leather = ItemRecords.Write(FArcItemDefinitionData());
			Thread = ItemRecords.Write(FArcItemDefinitionData());
			FArcItemDefinitionData GemDef;
			FArcItemFragment_Tags Tags;
			Tags.AssetTags.AddTag(TAG_InPrev_Gem);
			ArcItemTestRecords::AddFragment(GemDef, Tags);
			Gem = ItemRecords.Write(GemDef);

			FArcRecipeIngredient_ItemDef LeatherSlot;
			LeatherSlot.ItemDefinitionId = FArcNamedPrimaryAssetId(Leather);
			LeatherSlot.Amount = 4;
			FArcRecipeIngredient_ItemDef ThreadSlot;
			ThreadSlot.ItemDefinitionId = FArcNamedPrimaryAssetId(Thread);
			ThreadSlot.Amount = 2;
			FArcRecipeIngredient_Tags GemSlot;
			GemSlot.RequiredTags.AddTag(TAG_InPrev_Gem);
			GemSlot.Amount = 1;
			GemSlot.bOptional = true;

			Recipe.CraftTime = 1.0f;
			Recipe.Ingredients.Add(FInstancedStruct::Make(LeatherSlot));
			Recipe.Ingredients.Add(FInstancedStruct::Make(ThreadSlot));
			Recipe.Ingredients.Add(FInstancedStruct::Make(GemSlot));
			return Leather.IsValid() && Thread.IsValid() && Gem.IsValid();
		}
	};

	UArcRecipeViewModel* MakeRecipeVM(
		const FArcRecipeDefinitionData& Recipe,
		TConstArrayView<FArcCraftStoreItemRow> Inventory,
		TConstArrayView<FArcItemSpec> ServerInput,
		TConstArrayView<FArcCraftStoreItemRow> Preview)
	{
		UArcRecipeViewModel* VM = NewObject<UArcRecipeViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
		const FPrimaryAssetId RecipeId(FArcRecipeDefinitionData::JsonPrimaryAssetType(), FName(TEXT("InPrevBoots")));
		VM->RefreshFrom(RecipeId, Recipe, Inventory, ServerInput, Preview, nullptr, nullptr, 10);
		return VM;
	}
}

// ---------------------------------------------------------------------------
// Local preview edits: transfer, withdraw, clear. None of them sends a command.
// ---------------------------------------------------------------------------

TEST_CLASS(ArcCraftingVMInputPreviewEdits, "ArcUIModels.CraftingVM.InputPreview.Edits")
{
	FArcItemTestRecordFiles ItemRecords;
	UArcCraftingViewModel* CraftVM = nullptr;
	FPrimaryAssetId Def;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		CraftVM = NewObject<UArcCraftingViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
		CraftVM->SetbSupportsManualDeposit(true);
		Def = ItemRecords.Write(FArcItemDefinitionData());
	}

	TEST_METHOD(Transfer_AddsToPreview_CappedBySourceStacks)
	{
		using namespace ArcCraftingVMInputPreviewTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FArcCraftStoreItemRow Row = MakeRow(Def, 3);
		CraftVM->SetInventoryRows({ Row });
		UArcItemViewModel* Item = MakeItemVM(Row);

		ASSERT_THAT(IsTrue(CraftVM->TransferItemToStation(Item, 2)));
		ASSERT_THAT(AreEqual(2, PreviewStacksOf(*CraftVM, Row.Spec.ItemId)));
		ASSERT_THAT(IsTrue(CraftVM->GetbHasInputPreview()));

		ASSERT_THAT(IsTrue(CraftVM->TransferItemToStation(Item, 5), TEXT("A larger request adds what is left")));
		ASSERT_THAT(AreEqual(3, PreviewStacksOf(*CraftVM, Row.Spec.ItemId), TEXT("The preview is capped by the 3 stacks held")));
		ASSERT_THAT(IsFalse(CraftVM->TransferItemToStation(Item, 1), TEXT("Nothing is left to add")));

		ASSERT_THAT(AreEqual(1, CraftVM->GetPreviewRows().Num(), TEXT("Repeated transfers of one item share a preview row")));
		ASSERT_THAT(AreEqual(0, CraftVM->GetInventoryRowsAfterPreview().Num(), TEXT("No inventory stacks are left after the preview")));

		const UArcIngredientCandidateViewModel* ViewRow = FindPreviewRow(*CraftVM, Row.Spec.ItemId);
		ASSERT_THAT(IsNotNull(ViewRow, TEXT("The input view lists the preview row")));
		ASSERT_THAT(AreEqual(3, ViewRow->GetStacks()));
		ASSERT_THAT(IsTrue(ViewRow->GetOwnerEntity() == PlayerEntity));
		ASSERT_THAT(IsTrue(ViewRow->GetStoreType() == FArcMassItemStoreFragment::StaticStruct()));
	}

	TEST_METHOD(Transfer_WithoutManualDeposit_Refused)
	{
		using namespace ArcCraftingVMInputPreviewTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		CraftVM->SetbSupportsManualDeposit(false);
		const FArcCraftStoreItemRow Row = MakeRow(Def, 3);
		CraftVM->SetInventoryRows({ Row });

		ASSERT_THAT(IsFalse(CraftVM->TransferItemToStation(MakeItemVM(Row), 1)));
		ASSERT_THAT(AreEqual(0, CraftVM->GetPreviewRows().Num()));
	}

	TEST_METHOD(Transfer_ItemNotInInventory_Refused)
	{
		using namespace ArcCraftingVMInputPreviewTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FArcCraftStoreItemRow Held = MakeRow(Def, 3);
		CraftVM->SetInventoryRows({ Held });
		const FArcCraftStoreItemRow NotHeld = MakeRow(Def, 3);

		ASSERT_THAT(IsFalse(CraftVM->TransferItemToStation(MakeItemVM(NotHeld), 1), TEXT("An item the inventory rows do not hold cannot be previewed")));
		ASSERT_THAT(AreEqual(0, CraftVM->GetPreviewRows().Num()));
	}

	TEST_METHOD(WithdrawPreviewRow_EditsPreviewWithoutWorld)
	{
		using namespace ArcCraftingVMInputPreviewTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FArcCraftStoreItemRow Row = MakeRow(Def, 3);
		CraftVM->SetInventoryRows({ Row });
		ASSERT_THAT(IsTrue(CraftVM->TransferItemToStation(MakeItemVM(Row), 3)));

		UArcIngredientCandidateViewModel* ViewRow = FindPreviewRow(*CraftVM, Row.Spec.ItemId);
		ASSERT_THAT(IsNotNull(ViewRow));

		ASSERT_THAT(IsTrue(CraftVM->WithdrawInputItem(nullptr, ViewRow, 1),
			TEXT("A preview row is edited locally, so no world or command context is needed")));
		ASSERT_THAT(AreEqual(2, PreviewStacksOf(*CraftVM, Row.Spec.ItemId)));
		ASSERT_THAT(AreEqual(1, CraftVM->GetInventoryRowsAfterPreview().Num(), TEXT("The withdrawn stack is available again")));

		ViewRow = FindPreviewRow(*CraftVM, Row.Spec.ItemId);
		ASSERT_THAT(IsTrue(CraftVM->WithdrawInputItem(nullptr, ViewRow, 5)));
		ASSERT_THAT(AreEqual(0, CraftVM->GetPreviewRows().Num(), TEXT("Withdrawing at least the row's stacks removes it")));
		ASSERT_THAT(AreEqual(0, CraftVM->GetStationInput().Num()));
		ASSERT_THAT(IsFalse(CraftVM->GetbHasInputPreview()));
	}

	TEST_METHOD(WithdrawServerRow_WithoutWorld_IsNotAPreviewEdit)
	{
		using namespace ArcCraftingVMInputPreviewTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FArcCraftStoreItemRow Row = MakeRow(Def, 3);
		CraftVM->SetInventoryRows({ Row });
		ASSERT_THAT(IsTrue(CraftVM->TransferItemToStation(MakeItemVM(Row), 2)));
		CraftVM->SetServerInput({ Row.Spec }, nullptr);

		UArcIngredientCandidateViewModel* ServerRow = nullptr;
		for (const TObjectPtr<UArcIngredientCandidateViewModel>& ViewRow : CraftVM->GetStationInput())
		{
			if (ViewRow != nullptr && !ViewRow->GetbPreview())
			{
				ServerRow = ViewRow.Get();
			}
		}
		ASSERT_THAT(IsNotNull(ServerRow));

		ASSERT_THAT(IsFalse(CraftVM->WithdrawInputItem(nullptr, ServerRow, 1),
			TEXT("A server row goes to the withdraw command, which needs a command context")));
		ASSERT_THAT(AreEqual(2, PreviewStacksOf(*CraftVM, Row.Spec.ItemId), TEXT("The preview row with the same ItemId is not touched")));
	}

	TEST_METHOD(ClearInputPreview_DropsPreviewKeepsServerRows)
	{
		using namespace ArcCraftingVMInputPreviewTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FArcCraftStoreItemRow Row = MakeRow(Def, 3);
		CraftVM->SetInventoryRows({ Row });
		ASSERT_THAT(IsTrue(CraftVM->TransferItemToStation(MakeItemVM(Row), 2)));
		CraftVM->SetServerInput({ MakeSpec(Def, 1) }, nullptr);
		ASSERT_THAT(AreEqual(2, CraftVM->GetStationInput().Num()));

		CraftVM->ClearInputPreview();

		ASSERT_THAT(AreEqual(0, CraftVM->GetPreviewRows().Num()));
		ASSERT_THAT(IsFalse(CraftVM->GetbHasInputPreview()));
		ASSERT_THAT(AreEqual(1, CraftVM->GetStationInput().Num(), TEXT("Only the server row is left in the input view")));
		ASSERT_THAT(IsFalse(CraftVM->GetStationInput()[0]->GetbPreview()));
	}

	TEST_METHOD(StationChange_DropsPreview)
	{
		using namespace ArcCraftingVMInputPreviewTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		CraftVM->SetBoundStation(FMassEntityHandle(11, 1));
		const FArcCraftStoreItemRow Row = MakeRow(Def, 3);
		CraftVM->SetInventoryRows({ Row });
		ASSERT_THAT(IsTrue(CraftVM->TransferItemToStation(MakeItemVM(Row), 2)));

		CraftVM->SetBoundStation(FMassEntityHandle(12, 1));

		ASSERT_THAT(AreEqual(0, CraftVM->GetPreviewRows().Num(), TEXT("A station change drops the preview")));
		ASSERT_THAT(IsFalse(CraftVM->GetbHasInputPreview()));
	}

	TEST_METHOD(InventoryShrinks_PreviewClampedToHeld)
	{
		using namespace ArcCraftingVMInputPreviewTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		FArcCraftStoreItemRow Row = MakeRow(Def, 3);
		CraftVM->SetInventoryRows({ Row });
		ASSERT_THAT(IsTrue(CraftVM->TransferItemToStation(MakeItemVM(Row), 3)));

		Row.Stacks = 1;
		CraftVM->SetInventoryRows({ Row });

		ASSERT_THAT(AreEqual(1, PreviewStacksOf(*CraftVM, Row.Spec.ItemId), TEXT("The preview never exceeds what the source holds")));
	}
};

// ---------------------------------------------------------------------------
// Fill, slot click and bCanCraft against a recipe with an optional gem slot
// ---------------------------------------------------------------------------

TEST_CLASS(ArcCraftingVMInputPreviewRecipe, "ArcUIModels.CraftingVM.InputPreview.Recipe")
{
	FArcItemTestRecordFiles ItemRecords;
	ArcCraftingVMInputPreviewTestsLocal::FBootsFixture Boots;
	UArcCraftingViewModel* CraftVM = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		CraftVM = NewObject<UArcCraftingViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
		CraftVM->SetbSupportsManualDeposit(true);
	}

	TEST_METHOD(Fill_AddsRequiredDeficits_SkipsOptionalSlot)
	{
		using namespace ArcCraftingVMInputPreviewTestsLocal;
		ASSERT_THAT(IsTrue(Boots.Write(ItemRecords), TEXT("Item records must be written and indexed")));

		const FArcCraftStoreItemRow Leather = MakeRow(Boots.Leather, 10);
		const FArcCraftStoreItemRow Thread = MakeRow(Boots.Thread, 1);
		const FArcCraftStoreItemRow Gem = MakeRow(Boots.Gem, 5);
		const TArray<FArcCraftStoreItemRow> Inventory = { Leather, Thread, Gem };
		const TArray<FArcItemSpec> Server = { MakeSpec(Boots.Leather, 1) };

		CraftVM->SetInventoryRows(TArray<FArcCraftStoreItemRow>(Inventory));
		CraftVM->SetServerInput(Server, nullptr);
		CraftVM->SelectRecipe(MakeRecipeVM(Boots.Recipe, Inventory, Server, {}));

		ASSERT_THAT(IsTrue(CraftVM->FillStationInput(1)));

		ASSERT_THAT(AreEqual(3, PreviewStacksOf(*CraftVM, Leather.Spec.ItemId), TEXT("Leather deficit = 4 needed - 1 in the server input")));
		ASSERT_THAT(AreEqual(1, PreviewStacksOf(*CraftVM, Thread.Spec.ItemId), TEXT("Thread adds all it holds toward the deficit of 2")));
		ASSERT_THAT(AreEqual(0, PreviewStacksOf(*CraftVM, Gem.Spec.ItemId), TEXT("Fill does not fill optional slots")));
	}

	TEST_METHOD(Fill_TwoCrafts_DoublesTheNeed)
	{
		using namespace ArcCraftingVMInputPreviewTestsLocal;
		ASSERT_THAT(IsTrue(Boots.Write(ItemRecords), TEXT("Item records must be written and indexed")));

		const FArcCraftStoreItemRow Leather = MakeRow(Boots.Leather, 10);
		const FArcCraftStoreItemRow Thread = MakeRow(Boots.Thread, 10);
		const TArray<FArcCraftStoreItemRow> Inventory = { Leather, Thread };

		CraftVM->SetInventoryRows(TArray<FArcCraftStoreItemRow>(Inventory));
		CraftVM->SelectRecipe(MakeRecipeVM(Boots.Recipe, Inventory, {}, {}));

		ASSERT_THAT(IsTrue(CraftVM->FillStationInput(2)));
		ASSERT_THAT(AreEqual(8, PreviewStacksOf(*CraftVM, Leather.Spec.ItemId)));
		ASSERT_THAT(AreEqual(4, PreviewStacksOf(*CraftVM, Thread.Spec.ItemId)));
		ASSERT_THAT(IsFalse(CraftVM->FillStationInput(2), TEXT("A second fill finds no deficit left")));
	}

	TEST_METHOD(AddFirstMatch_OptionalSlot_AddsUpToNeed)
	{
		using namespace ArcCraftingVMInputPreviewTestsLocal;
		ASSERT_THAT(IsTrue(Boots.Write(ItemRecords), TEXT("Item records must be written and indexed")));

		const FArcCraftStoreItemRow Leather = MakeRow(Boots.Leather, 10);
		const FArcCraftStoreItemRow Gem = MakeRow(Boots.Gem, 5);
		const TArray<FArcCraftStoreItemRow> Inventory = { Leather, Gem };
		CraftVM->SetInventoryRows(TArray<FArcCraftStoreItemRow>(Inventory));

		UArcRecipeViewModel* RecipeVM = MakeRecipeVM(Boots.Recipe, Inventory, {}, {});
		ASSERT_THAT(AreEqual(1, RecipeVM->GetOptionalIngredients().Num()));
		UArcRecipeIngredientViewModel* GemSlot = RecipeVM->GetOptionalIngredients()[0];

		ASSERT_THAT(IsTrue(CraftVM->AddFirstMatchToInput(GemSlot), TEXT("A click on the optional slot adds a fitting item")));
		ASSERT_THAT(AreEqual(1, PreviewStacksOf(*CraftVM, Gem.Spec.ItemId), TEXT("Only the slot's need of 1 is added")));
		ASSERT_THAT(AreEqual(0, PreviewStacksOf(*CraftVM, Leather.Spec.ItemId), TEXT("Leather does not fit the gem slot")));
		ASSERT_THAT(IsFalse(CraftVM->AddFirstMatchToInput(GemSlot), TEXT("A filled slot adds nothing more")));
	}

	TEST_METHOD(AddFirstMatch_RequiredSlot_CountsServerInput)
	{
		using namespace ArcCraftingVMInputPreviewTestsLocal;
		ASSERT_THAT(IsTrue(Boots.Write(ItemRecords), TEXT("Item records must be written and indexed")));

		const FArcCraftStoreItemRow Leather = MakeRow(Boots.Leather, 10);
		const TArray<FArcCraftStoreItemRow> Inventory = { Leather };
		const TArray<FArcItemSpec> Server = { MakeSpec(Boots.Leather, 3) };
		CraftVM->SetInventoryRows(TArray<FArcCraftStoreItemRow>(Inventory));
		CraftVM->SetServerInput(Server, nullptr);

		UArcRecipeViewModel* RecipeVM = MakeRecipeVM(Boots.Recipe, Inventory, Server, {});
		UArcRecipeIngredientViewModel* LeatherSlot = RecipeVM->GetRequiredIngredients()[0];

		ASSERT_THAT(IsTrue(CraftVM->AddFirstMatchToInput(LeatherSlot)));
		ASSERT_THAT(AreEqual(1, PreviewStacksOf(*CraftVM, Leather.Spec.ItemId), TEXT("Need = 4 - 3 already in the server input")));
	}

	TEST_METHOD(CanCraft_IgnoresEmptyOptionalSlot)
	{
		using namespace ArcCraftingVMInputPreviewTestsLocal;
		ASSERT_THAT(IsTrue(Boots.Write(ItemRecords), TEXT("Item records must be written and indexed")));

		const TArray<FArcItemSpec> Server = { MakeSpec(Boots.Leather, 4) };
		const TArray<FArcCraftStoreItemRow> Preview = { MakeRow(Boots.Thread, 2) };

		UArcRecipeViewModel* RecipeVM = MakeRecipeVM(Boots.Recipe, {}, Server, Preview);

		ASSERT_THAT(IsTrue(RecipeVM->GetbCanCraft(), TEXT("Required leather (server) and thread (preview) are there; the empty gem slot does not block")));
		ASSERT_THAT(AreEqual(1, RecipeVM->GetCraftableFromInput(), TEXT("The input view funds one craft")));
		ASSERT_THAT(AreEqual(2, RecipeVM->GetRequiredIngredients().Num()));
		ASSERT_THAT(AreEqual(1, RecipeVM->GetOptionalIngredients().Num()));
		ASSERT_THAT(IsTrue(RecipeVM->GetOptionalIngredients()[0]->GetbOptionalEmpty()));
	}

	TEST_METHOD(CanCraft_RequiredSlotShort_False)
	{
		using namespace ArcCraftingVMInputPreviewTestsLocal;
		ASSERT_THAT(IsTrue(Boots.Write(ItemRecords), TEXT("Item records must be written and indexed")));

		const TArray<FArcItemSpec> Server = { MakeSpec(Boots.Leather, 4), MakeSpec(Boots.Gem, 1) };
		const TArray<FArcCraftStoreItemRow> Preview = { MakeRow(Boots.Thread, 1) };

		UArcRecipeViewModel* RecipeVM = MakeRecipeVM(Boots.Recipe, {}, Server, Preview);

		ASSERT_THAT(IsFalse(RecipeVM->GetbCanCraft(), TEXT("Thread 1 of 2 blocks the craft; the filled gem does not help")));
		ASSERT_THAT(AreEqual(0, RecipeVM->GetCraftableFromInput()));
	}
};
