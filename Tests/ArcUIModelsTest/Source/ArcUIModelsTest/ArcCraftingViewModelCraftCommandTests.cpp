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

#include "ArcStandalonePIEComponent.h"

#if ENABLE_ARC_STANDALONE_PIE_TEST

#include "CQTest.h"
#include "ArcItemTestRecordFiles.h"
#include "ArcCraftTestRecordFiles.h"
#include "ArcMassItemTestHelpers.h"
#include "ArcMassItemsTestGameMode.h"

#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/StrongObjectPtr.h"

#include "Mass/EntityBuilder.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"

#include "ArcCraft/Mass/ArcCraftDeliveryConfigFragment.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcMassAvailableRecipesSharedFragment.h"
#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Station/ArcCraftItemSource.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"

#include "ViewModels/ArcCraftingViewModel.h"
#include "ViewModels/ArcIngredientCandidateViewModel.h"
#include "ViewModels/ArcItemViewModel.h"
#include "ViewModels/ArcRecipeViewModel.h"

namespace ArcCraftingVMCraftCommandTestsLocal
{
	FMassEntityHandle BuildDepositStation(FMassEntityManager& EM, const FPrimaryAssetId& PinnedRecipe)
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcCraftInputFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcCraftQueueFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcCraftOutputFragment()));

		const FMassEntityHandle Entity = EM.CreateEntity(Fragments);
		EM.AddTagToEntity(Entity, FArcCraftStationTag::StaticStruct());

		FArcCraftStationConfigFragment Config;
		Config.ConcurrentCraftSlots = 1;
		EM.AddConstSharedFragmentToEntity(Entity, EM.GetOrCreateConstSharedFragment(Config));

		FArcMassAvailableRecipesSharedFragment Available;
		Available.Recipes.Add(PinnedRecipe);
		EM.AddConstSharedFragmentToEntity(Entity, EM.GetOrCreateConstSharedFragment(Available));

		FArcCraftDeliveryConfigFragment Delivery;
		Delivery.IngredientSource.InitializeAs<FArcCraftItemSource_EntityStore>();
		EM.AddConstSharedFragmentToEntity(Entity, EM.GetOrCreateConstSharedFragment(Delivery));
		return Entity;
	}

	FArcCraftStoreItemRow AddInventoryItem(FMassEntityManager& EM, FMassEntityHandle Owner, UScriptStruct* StoreType, const FPrimaryAssetId& Def, int32 Stacks)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinition(Def);
		Spec.SetItemId(FArcItemId::Generate());
		Spec.Amount = static_cast<uint16>(Stacks);

		FArcCraftStoreItemRow Row;
		Row.OwnerEntity = Owner;
		Row.StoreType = StoreType;
		Row.Spec = Spec;
		Row.Spec.SetItemId(ArcMassItems::AddItem(EM, Owner, StoreType, Spec));
		Row.Stacks = Stacks;
		return Row;
	}

	int32 HeldStacks(FMassEntityManager& EM, const FArcCraftStoreItemRow& Row)
	{
		const FArcItemData* Data = ArcMassItems::FindItemById(EM, Row.OwnerEntity, Row.StoreType, Row.Spec.ItemId);
		return Data != nullptr ? static_cast<int32>(Data->GetStacks()) : 0;
	}

	int32 InputStacksOf(FMassEntityManager& EM, FMassEntityHandle Station, const FArcItemId& ItemId)
	{
		int32 Stacks = 0;
		for (const FArcItemSpec& Spec : EM.GetFragmentDataPtr<FArcCraftInputFragment>(Station)->InputItems)
		{
			Stacks += Spec.ItemId == ItemId ? static_cast<int32>(Spec.Amount) : 0;
		}
		return Stacks;
	}

	UArcItemViewModel* MakeItemVM(const FArcCraftStoreItemRow& Row)
	{
		UArcItemViewModel* VM = NewObject<UArcItemViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
		VM->RefreshFrom(Row.Spec, Row.Stacks, nullptr);
		return VM;
	}

	FMassEntityHandle BuildPlayer(FMassEntityManager& EM)
	{
		UE::Mass::FEntityBuilder Builder(EM);
		return Builder
			.Add<FTransformFragment>()
			.Add<FArcMassItemStoreFragment>()
			.Commit();
	}

	FArcRecipeDefinitionData MakeOreRecipe(const FPrimaryAssetId& OreDef, int32 Amount)
	{
		FArcRecipeDefinitionData Recipe;
		Recipe.CraftTime = 100.0f;
		FArcRecipeIngredient_ItemDef OreSlot;
		OreSlot.ItemDefinitionId = FArcNamedPrimaryAssetId(OreDef);
		OreSlot.Amount = Amount;
		Recipe.Ingredients.Add(FInstancedStruct::Make(OreSlot));
		return Recipe;
	}
}

// ---------------------------------------------------------------------------
// CraftFromStationInput sends the preview grouped by (owner entity, store type) through the real
// player controller; the server command moves every row out of the store it names.
// ---------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(ArcCraftingVMCraftCommand, "ArcUIModels.CraftingVM.CraftCommand", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing, AArcMassItemsTestGameMode::StaticClass() };
	FArcItemTestRecordFiles ItemRecords;
	FArcCraftTestRecordFiles Records;
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	TStrongObjectPtr<UArcCraftingViewModel> CraftVM;
	FMassEntityHandle Station;
	FArcCraftStoreItemRow Ore;
	FArcCraftStoreItemRow Thread;
	FArcCraftStoreItemRow OtherOre;

	int32 FramesWaited = 0;

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(TEXT("FMassEntityQuery::CacheArchetypes"), EAutomationExpectedErrorFlags::Contains, -1);
		// The test game mode loads no experience asset; the craft path does not need one.
		TestRunner->AddExpectedError(TEXT("Failed to identify experience"), EAutomationExpectedErrorFlags::Contains, -1);
		PIE.Then(TEXT("Setup"), [this]()
		{
			FAutomationTestBase::bSuppressLogWarnings = true;
			World = PIE.GetState().World;
			UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
			check(MES);
			EM = &MES->GetMutableEntityManager();
		});
	}

	TEST_METHOD(PreviewFromTwoStores_SentGrouped_MovedAndQueued)
	{
		PIE.Then(TEXT("Preview two stores and craft"), [this]()
		{
			using namespace ArcCraftingVMCraftCommandTestsLocal;

			const FPrimaryAssetId OreDef = ItemRecords.Write(FArcItemDefinitionData());
			const FPrimaryAssetId ThreadDef = ItemRecords.Write(FArcItemDefinitionData());
			ASSERT_THAT(IsTrue(OreDef.IsValid() && ThreadDef.IsValid(), TEXT("Item records must be written and indexed")));

			FArcRecipeDefinitionData Recipe;
			Recipe.CraftTime = 100.0f;
			FArcRecipeIngredient_ItemDef OreSlot;
			OreSlot.ItemDefinitionId = FArcNamedPrimaryAssetId(OreDef);
			OreSlot.Amount = 3;
			FArcRecipeIngredient_ItemDef ThreadSlot;
			ThreadSlot.ItemDefinitionId = FArcNamedPrimaryAssetId(ThreadDef);
			ThreadSlot.Amount = 1;
			Recipe.Ingredients.Add(FInstancedStruct::Make(OreSlot));
			Recipe.Ingredients.Add(FInstancedStruct::Make(ThreadSlot));
			const FPrimaryAssetId RecipeId = Records.Write(Recipe);
			ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));

			Station = BuildDepositStation(*EM, RecipeId);
			UE::Mass::FEntityBuilder Builder(*EM);
			const FMassEntityHandle Player = Builder
				.Add<FTransformFragment>()
				.Add<FArcMassItemStoreFragment>()
				.Add<FArcTestSecondaryItemStoreFragment>()
				.Commit();

			Ore = AddInventoryItem(*EM, Player, FArcMassItemStoreFragment::StaticStruct(), OreDef, 2);
			OtherOre = AddInventoryItem(*EM, Player, FArcMassItemStoreFragment::StaticStruct(), OreDef, 4);
			Thread = AddInventoryItem(*EM, Player, FArcTestSecondaryItemStoreFragment::StaticStruct(), ThreadDef, 2);
			ASSERT_THAT(IsTrue(Ore.Spec.ItemId.IsValid() && OtherOre.Spec.ItemId.IsValid() && Thread.Spec.ItemId.IsValid()));

			CraftVM.Reset(NewObject<UArcCraftingViewModel>(GetTransientPackage(), NAME_None, RF_Transient));
			CraftVM->SetBoundStation(Station);
			CraftVM->SetInstigatorEntity(Player);
			CraftVM->SetbSupportsManualDeposit(true);
			CraftVM->SetInventoryRows({ Ore, OtherOre, Thread });

			ASSERT_THAT(IsTrue(CraftVM->TransferItemToStation(MakeItemVM(Ore), 2)));
			ASSERT_THAT(IsTrue(CraftVM->TransferItemToStation(MakeItemVM(OtherOre), 1)));
			ASSERT_THAT(IsTrue(CraftVM->TransferItemToStation(MakeItemVM(Thread), 1)));

			UArcRecipeViewModel* RecipeVM = NewObject<UArcRecipeViewModel>(CraftVM.Get());
			RecipeVM->RefreshFrom(RecipeId, Recipe, CraftVM->GetInventoryRowsAfterPreview(), {}, CraftVM->GetPreviewRows(), nullptr, nullptr, 10);
			ASSERT_THAT(IsTrue(RecipeVM->GetbCanCraft(), TEXT("The preview alone satisfies the recipe")));
			CraftVM->SelectRecipe(RecipeVM);

			ASSERT_THAT(IsTrue(CraftVM->CraftFromStationInput(World, 1), TEXT("The grouped craft command is dispatched")));
		})
		.Until(TEXT("Items reach the station input"), [this]() -> bool
		{
			return ArcCraftingVMCraftCommandTestsLocal::InputStacksOf(*EM, Station, Thread.Spec.ItemId) > 0;
		}, FTimespan::FromSeconds(5))
		.Then(TEXT("Every row moved out of the store it came from"), [this]()
		{
			using namespace ArcCraftingVMCraftCommandTestsLocal;

			ASSERT_THAT(AreEqual(2, InputStacksOf(*EM, Station, Ore.Spec.ItemId)));
			ASSERT_THAT(AreEqual(1, InputStacksOf(*EM, Station, OtherOre.Spec.ItemId)));
			ASSERT_THAT(AreEqual(1, InputStacksOf(*EM, Station, Thread.Spec.ItemId), TEXT("The secondary-store row was sent with its own store type")));
			ASSERT_THAT(AreEqual(0, HeldStacks(*EM, Ore)));
			ASSERT_THAT(AreEqual(3, HeldStacks(*EM, OtherOre)));
			ASSERT_THAT(AreEqual(1, HeldStacks(*EM, Thread)));

			const FArcCraftQueueFragment* Queue = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
			ASSERT_THAT(AreEqual(1, Queue->Entries.Num(), TEXT("The craft is queued")));
			ASSERT_THAT(AreEqual(0, CraftVM->GetPreviewRows().Num(), TEXT("A confirmed send removes the sent rows from the preview")));
		});
	}

	TEST_METHOD(WithdrawServerRow_SendsCommand_StacksReturnToPlayer)
	{
		PIE.Then(TEXT("Withdraw a server input row"), [this]()
		{
			using namespace ArcCraftingVMCraftCommandTestsLocal;

			const FPrimaryAssetId OreDef = ItemRecords.Write(FArcItemDefinitionData());
			ASSERT_THAT(IsTrue(OreDef.IsValid(), TEXT("Item record must be written and indexed")));
			const FPrimaryAssetId RecipeId = Records.Write(MakeOreRecipe(OreDef, 1));
			ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));

			Station = BuildDepositStation(*EM, RecipeId);
			const FMassEntityHandle Player = BuildPlayer(*EM);

			FArcItemSpec Deposited;
			Deposited.SetItemDefinition(OreDef);
			Deposited.SetItemId(FArcItemId::Generate());
			Deposited.Amount = 3;
			EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station)->InputItems.Add(Deposited);
			Ore.OwnerEntity = Player;
			Ore.StoreType = FArcMassItemStoreFragment::StaticStruct();
			Ore.Spec = Deposited;

			CraftVM.Reset(NewObject<UArcCraftingViewModel>(GetTransientPackage(), NAME_None, RF_Transient));
			CraftVM->SetBoundStation(Station);
			CraftVM->SetInstigatorEntity(Player);
			CraftVM->SetbSupportsManualDeposit(true);
			CraftVM->SetServerInput(EM->GetFragmentDataPtr<FArcCraftInputFragment>(Station)->InputItems, nullptr);

			ASSERT_THAT(AreEqual(1, CraftVM->GetStationInput().Num()));
			UArcIngredientCandidateViewModel* ServerRow = CraftVM->GetStationInput()[0];
			ASSERT_THAT(IsFalse(ServerRow->GetbPreview()));
			ASSERT_THAT(IsTrue(CraftVM->WithdrawInputItem(World, ServerRow, 2), TEXT("A server row withdraw is sent as a command")));
		})
		.Until(TEXT("Stacks reach the player store"), [this]() -> bool
		{
			return ArcMassItems::CountItemsByDefinition(*EM, Ore.OwnerEntity, FArcMassItemStoreFragment::StaticStruct(), Ore.Spec.GetItemDefinitionId()) >= 2;
		}, FTimespan::FromSeconds(5))
		.Then(TEXT("The station keeps the rest"), [this]()
		{
			using namespace ArcCraftingVMCraftCommandTestsLocal;

			ASSERT_THAT(AreEqual(2, ArcMassItems::CountItemsByDefinition(*EM, Ore.OwnerEntity, FArcMassItemStoreFragment::StaticStruct(), Ore.Spec.GetItemDefinitionId()),
				TEXT("Exactly the 2 withdrawn stacks reach the player")));
			ASSERT_THAT(AreEqual(1, InputStacksOf(*EM, Station, Ore.Spec.ItemId), TEXT("1 of 3 stacks stays in the station input")));
		});
	}

	TEST_METHOD(RejectedCraft_KeepsPreview)
	{
		PIE.Then(TEXT("Preview one stack more than held and craft"), [this]()
		{
			using namespace ArcCraftingVMCraftCommandTestsLocal;

			TestRunner->AddExpectedMessage(TEXT("sources rejected"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);

			const FPrimaryAssetId OreDef = ItemRecords.Write(FArcItemDefinitionData());
			ASSERT_THAT(IsTrue(OreDef.IsValid(), TEXT("Item record must be written and indexed")));
			const FArcRecipeDefinitionData Recipe = MakeOreRecipe(OreDef, 1);
			const FPrimaryAssetId RecipeId = Records.Write(Recipe);
			ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));

			Station = BuildDepositStation(*EM, RecipeId);
			const FMassEntityHandle Player = BuildPlayer(*EM);
			Ore = AddInventoryItem(*EM, Player, FArcMassItemStoreFragment::StaticStruct(), OreDef, 2);

			// The client's inventory rows are stale: they claim 3 stacks while the store holds 2.
			FArcCraftStoreItemRow StaleRow = Ore;
			StaleRow.Stacks = 3;

			CraftVM.Reset(NewObject<UArcCraftingViewModel>(GetTransientPackage(), NAME_None, RF_Transient));
			CraftVM->SetBoundStation(Station);
			CraftVM->SetInstigatorEntity(Player);
			CraftVM->SetbSupportsManualDeposit(true);
			CraftVM->SetInventoryRows({ StaleRow });
			ASSERT_THAT(IsTrue(CraftVM->TransferItemToStation(MakeItemVM(StaleRow), 3)));

			UArcRecipeViewModel* RecipeVM = NewObject<UArcRecipeViewModel>(CraftVM.Get());
			RecipeVM->RefreshFrom(RecipeId, Recipe, CraftVM->GetInventoryRowsAfterPreview(), {}, CraftVM->GetPreviewRows(), nullptr, nullptr, 10);
			CraftVM->SelectRecipe(RecipeVM);

			FramesWaited = 0;
			ASSERT_THAT(IsTrue(CraftVM->CraftFromStationInput(World, 1), TEXT("The client still sends the command")));
		})
		.Until(TEXT("Let the confirm and deferred commands run"), [this]() -> bool
		{
			return ++FramesWaited >= 10;
		}, FTimespan::FromSeconds(5))
		.Then(TEXT("The rejected rows stay in the preview"), [this]()
		{
			using namespace ArcCraftingVMCraftCommandTestsLocal;

			ASSERT_THAT(AreEqual(1, CraftVM->GetPreviewRows().Num(), TEXT("A failed confirm keeps the preview")));
			ASSERT_THAT(AreEqual(3, CraftVM->GetPreviewRows()[0].Stacks));
			ASSERT_THAT(AreEqual(2, HeldStacks(*EM, Ore), TEXT("The player keeps the item")));
			ASSERT_THAT(AreEqual(0, InputStacksOf(*EM, Station, Ore.Spec.ItemId), TEXT("Nothing reaches the station input")));
			ASSERT_THAT(AreEqual(0, EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station)->Entries.Num(), TEXT("Nothing is queued")));
		});
	}
};

#endif // ENABLE_ARC_STANDALONE_PIE_TEST
