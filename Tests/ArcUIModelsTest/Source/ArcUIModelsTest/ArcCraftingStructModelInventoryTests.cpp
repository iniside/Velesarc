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
#include "ArcMassItemTestHelpers.h"
#include "ArcUIModelsTestAgentComponent.h"
#include "Components/ActorTestSpawner.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "NativeGameplayTags.h"

#include "Mass/EntityBuilder.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"

#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"

#include "ArcFrontendMVVMModelComponent.h"
#include "Models/ArcCraftingStructModel.h"
#include "ViewModels/ArcCraftingViewModel.h"
#include "ViewModels/ArcIngredientCandidateViewModel.h"
#include "ViewModels/ArcRecipeViewModel.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CraftSM_Slot, "Test.CraftSM.Slot");

namespace ArcCraftingStructModelInventoryTestsLocal
{
	FArcItemId AddItem(FMassEntityManager& EM, FMassEntityHandle Owner, const UScriptStruct* StoreType, const FPrimaryAssetId& Def, int32 Stacks)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinition(Def);
		Spec.SetItemId(FArcItemId::Generate());
		Spec.Amount = static_cast<uint16>(Stacks);
		return ArcMassItems::AddItem(EM, Owner, StoreType, Spec);
	}

	const FArcCraftStoreItemRow* FindRow(TConstArrayView<FArcCraftStoreItemRow> Rows, const FArcItemId& ItemId)
	{
		for (const FArcCraftStoreItemRow& Row : Rows)
		{
			if (Row.Spec.ItemId == ItemId)
			{
				return &Row;
			}
		}
		return nullptr;
	}
}

// ---------------------------------------------------------------------------
// FArcCraftingStructModel reads every InventoryStoreTypes store on the player entity and stamps each row
// with the owner entity and the store it came from.
// ---------------------------------------------------------------------------

TEST_CLASS(ArcCraftingStructModelInventory, "ArcUIModels.CraftingStructModel.Inventory")
{
	FArcItemTestRecordFiles ItemRecords;
	FActorTestSpawner Spawner;
	FMassEntityManager* EM = nullptr;
	FArcCraftingStructModel Model;
	UArcFrontendMVVMModelComponent* Component = nullptr;
	bool bModelInitialized = false;

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

	AFTER_EACH()
	{
		if (bModelInitialized && Component != nullptr)
		{
			Model.Deinitialize(*Component);
		}
	}

	TEST_METHOD(TwoInventoryStores_RowsCarryOwnerAndTheirOwnStore)
	{
		using namespace ArcCraftingStructModelInventoryTestsLocal;

		const FPrimaryAssetId Def = ItemRecords.Write(FArcItemDefinitionData());
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		UE::Mass::FEntityBuilder Builder(*EM);
		const FMassEntityHandle Player = Builder
			.Add<FTransformFragment>()
			.Add<FArcMassItemStoreFragment>()
			.Add<FArcTestSecondaryItemStoreFragment>()
			.Commit();

		const FArcItemId BaseItem = AddItem(*EM, Player, FArcMassItemStoreFragment::StaticStruct(), Def, 3);
		const FArcItemId BagItem = AddItem(*EM, Player, FArcTestSecondaryItemStoreFragment::StaticStruct(), Def, 2);
		const FArcItemId EquippedItem = AddItem(*EM, Player, FArcMassItemStoreFragment::StaticStruct(), Def, 1);
		ASSERT_THAT(IsTrue(ArcMassItems::AddItemToSlot(*EM, Player, FArcMassItemStoreFragment::StaticStruct(), EquippedItem, TAG_CraftSM_Slot)));

		APlayerState& PlayerState = Spawner.SpawnActor<APlayerState>();
		APawn& Pawn = Spawner.SpawnActor<APawn>();
		UArcUIModelsTestAgentComponent* Agent = NewObject<UArcUIModelsTestAgentComponent>(&Pawn);
		Pawn.SetPlayerState(&PlayerState);
		Agent->AssociateEntityForTest(Player);
		ASSERT_THAT(IsTrue(Pawn.FindComponentByClass<UMassAgentComponent>() == Agent));

		Component = NewObject<UArcFrontendMVVMModelComponent>(&PlayerState);
		Model.InventoryStoreTypes.Reset();
		Model.InventoryStoreTypes.Add(FArcMassItemStoreFragment::StaticStruct());
		Model.InventoryStoreTypes.Add(FArcTestSecondaryItemStoreFragment::StaticStruct());
		Model.CreateViewModels(*Component);
		Model.Initialize(*Component);
		bModelInitialized = true;

		UArcCraftingViewModel* CraftVM = Cast<UArcCraftingViewModel>(
			Component->FindViewModelByContext(Model.ContextName, UArcCraftingViewModel::StaticClass()));
		ASSERT_THAT(IsNotNull(CraftVM, TEXT("The struct model registers its crafting view model")));
		ASSERT_THAT(IsTrue(CraftVM->GetInstigatorEntity() == Player, TEXT("The struct model binds the pawn's Mass entity")));

		CraftVM->SelectRecipe(NewObject<UArcRecipeViewModel>(Component));

		const TArray<FArcCraftStoreItemRow> Rows = CraftVM->GetInventoryRowsAfterPreview();
		ASSERT_THAT(AreEqual(2, Rows.Num(), TEXT("Two loose items across two stores; the equipped item is left out")));

		const FArcCraftStoreItemRow* BaseRow = FindRow(Rows, BaseItem);
		const FArcCraftStoreItemRow* BagRow = FindRow(Rows, BagItem);
		ASSERT_THAT(IsNotNull(BaseRow));
		ASSERT_THAT(IsNotNull(BagRow, TEXT("The secondary store is read")));
		ASSERT_THAT(IsNull(FindRow(Rows, EquippedItem), TEXT("A slotted item is not a preview source")));

		ASSERT_THAT(IsTrue(BaseRow->StoreType == FArcMassItemStoreFragment::StaticStruct()));
		ASSERT_THAT(IsTrue(BagRow->StoreType == FArcTestSecondaryItemStoreFragment::StaticStruct(),
			TEXT("A secondary-store item carries its own store type")));
		ASSERT_THAT(IsTrue(BaseRow->OwnerEntity == Player && BagRow->OwnerEntity == Player));
		ASSERT_THAT(AreEqual(3, BaseRow->Stacks));
		ASSERT_THAT(AreEqual(2, BagRow->Stacks));
	}
};
