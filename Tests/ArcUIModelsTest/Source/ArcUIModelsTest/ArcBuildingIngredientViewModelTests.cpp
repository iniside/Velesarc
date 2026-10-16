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

#include "NativeGameplayTags.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"

#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemId.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"

#include "ArcBuildIngredient.h"
#include "ArcBuildingDefinition.h"

#include "ViewModels/ArcBuildingIngredientViewModel.h"
#include "ViewModels/ArcBuildingViewModel.h"

// ---------------------------------------------------------------------------
// Unique tag prefix: TAG_BuildIngredientTest_* — avoids ODR collisions with
// other test files that declare their own gameplay tags.
// ---------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_BuildIngredientTest_Material, "Test.Build.Material");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_BuildIngredientTest_OtherMaterial, "Test.Build.OtherMaterial");

// ---------------------------------------------------------------------------
// File-scoped helpers
// ---------------------------------------------------------------------------

namespace ArcBuildingIngredientVMHelpers
{
	/**
	 * Write an item definition record with no extra fragments and return its id.
	 */
	FPrimaryAssetId MakeItemDef(FArcItemTestRecordFiles& ItemRecords)
	{
		return ItemRecords.Write(FArcItemDefinitionData());
	}

	/**
	 * Write an item definition record carrying an FArcItemFragment_Tags
	 * with the given AssetTags — the surface FArcBuildIngredient_Tags::DoesItemSatisfy
	 * actually reads (NOT ItemAggregatedTags) — and return its id.
	 */
	FPrimaryAssetId MakeTaggedItemDef(FArcItemTestRecordFiles& ItemRecords, const FGameplayTagContainer& AssetTags)
	{
		FArcItemDefinitionData Def;

		FArcItemFragment_Tags TagsFragment;
		TagsFragment.AssetTags = AssetTags;
		ArcItemTestRecords::AddFragment(Def, TagsFragment);

		return ItemRecords.Write(Def);
	}

	/**
	 * Build an FInstancedStruct holding an FArcBuildIngredient_ItemDef for the
	 * given definition id and required amount.
	 */
	FInstancedStruct MakeItemDefIngredient(const FPrimaryAssetId& DefId, int32 Amount)
	{
		FArcBuildIngredient_ItemDef Ingredient;
		Ingredient.ItemDefinitionId = FArcNamedPrimaryAssetId(DefId);
		Ingredient.Amount = Amount;
		return FInstancedStruct::Make(Ingredient);
	}

	/**
	 * Build an FInstancedStruct holding an FArcBuildIngredient_Tags.
	 */
	FInstancedStruct MakeTagIngredient(const FGameplayTagContainer& RequiredTags, int32 Amount)
	{
		FArcBuildIngredient_Tags Ingredient;
		Ingredient.RequiredTags = RequiredTags;
		Ingredient.Amount = Amount;
		return FInstancedStruct::Make(Ingredient);
	}

	/**
	 * Build an instigator entity with a sparse FArcMassItemStoreFragment, seed it
	 * with N stacks of Def via ArcMassItems::AddItem (so GetStacks() reflects
	 * Amount correctly), and return a pointer to the store read back from the entity.
	 * The caller must keep both EM and Entity alive while the pointer is used.
	 */
	const FArcMassItemStoreFragment* BuildSeededInventoryEntity(
		FMassEntityManager& EM,
		FMassEntityHandle& OutEntity,
		const FPrimaryAssetId& DefId,
		int32 StackCount)
	{
		UE::Mass::FEntityBuilder Builder(EM);
		OutEntity = Builder
			.Add<FArcMassItemStoreFragment>()
			.Commit();

		FArcItemSpec Spec;
		Spec.SetItemDefinition(DefId);
		Spec.SetItemId(FArcItemId::Generate());
		Spec.Amount = static_cast<uint16>(StackCount);

		const FArcItemId AddedId = ArcMassItems::AddItem(
			EM, OutEntity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		check(AddedId.IsValid());

		return FMassEntityView(EM, OutEntity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
	}
}

// ---------------------------------------------------------------------------
// Test class: UArcBuildingIngredientViewModel::RefreshFrom
// ---------------------------------------------------------------------------

TEST_CLASS(ArcBuildingIngredientViewModelTests, "ArcUIModels.BuildIngredientVM")
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
	// _ItemDef ingredient — matching definition, inventory covers the requirement.
	// =========================================================================
	TEST_METHOD(ItemDefIngredient_MatchingDefinition_CountsStacks)
	{
		using namespace ArcBuildingIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		FMassEntityHandle Entity;
		const FArcMassItemStoreFragment* Store = BuildSeededInventoryEntity(*EM, Entity, Def, 3);
		ASSERT_THAT(IsNotNull(Store, TEXT("Inventory store entity must be valid")));

		FInstancedStruct IngredientStruct = MakeItemDefIngredient(Def, 2);
		const FArcBuildIngredient* Ingredient = IngredientStruct.GetPtr<FArcBuildIngredient>();
		ASSERT_THAT(IsNotNull(Ingredient));

		UArcBuildingIngredientViewModel* VM = NewObject<UArcBuildingIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		VM->RefreshFrom(*Ingredient, *Store);

		ASSERT_THAT(AreEqual(2, VM->GetRequired(), TEXT("Required must equal ingredient Amount (2)")));
		ASSERT_THAT(AreEqual(3, VM->GetHave(), TEXT("Have must count the 3 matching inventory stacks")));
		ASSERT_THAT(IsTrue(VM->GetbSatisfied(), TEXT("bSatisfied must be true when Have(3) >= Required(2)")));
		ASSERT_THAT(AreEqual(FString(TEXT("3 / 2")), VM->GetHaveRequiredText().ToString(),
			TEXT("HaveRequiredText must read \"<Have> / <Required>\"")));
	}

	// =========================================================================
	// _ItemDef ingredient — non-matching definition => Have == 0, not satisfied.
	// =========================================================================
	TEST_METHOD(ItemDefIngredient_NonMatchingDefinition_HaveIsZero)
	{
		using namespace ArcBuildingIngredientVMHelpers;

		const FPrimaryAssetId InventoryDef = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(InventoryDef.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId RequiredDef = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(RequiredDef.IsValid(), TEXT("Item record must be written and indexed")));

		FMassEntityHandle Entity;
		const FArcMassItemStoreFragment* Store =
			BuildSeededInventoryEntity(*EM, Entity, InventoryDef, 5);
		ASSERT_THAT(IsNotNull(Store, TEXT("Inventory store entity must be valid")));

		// Ingredient requires a different definition than what's in the inventory.
		FInstancedStruct IngredientStruct = MakeItemDefIngredient(RequiredDef, 2);
		const FArcBuildIngredient* Ingredient = IngredientStruct.GetPtr<FArcBuildIngredient>();
		ASSERT_THAT(IsNotNull(Ingredient));

		UArcBuildingIngredientViewModel* VM = NewObject<UArcBuildingIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		VM->RefreshFrom(*Ingredient, *Store);

		ASSERT_THAT(AreEqual(2, VM->GetRequired(), TEXT("Required must equal ingredient Amount (2)")));
		ASSERT_THAT(AreEqual(0, VM->GetHave(), TEXT("Have must be 0 — inventory holds a different definition")));
		ASSERT_THAT(IsFalse(VM->GetbSatisfied(), TEXT("bSatisfied must be false when Have(0) < Required(2)")));
		ASSERT_THAT(AreEqual(FString(TEXT("0 / 2")), VM->GetHaveRequiredText().ToString(),
			TEXT("HaveRequiredText must read \"0 / 2\" when nothing matches")));
	}

	// =========================================================================
	// _Tags ingredient — matches FArcItemFragment_Tags::AssetTags authored on the
	// item definition (NOT ItemAggregatedTags — CountAvailable routes through
	// DoesItemSatisfy, which reads the definition/spec fragment directly).
	// =========================================================================
	TEST_METHOD(TagsIngredient_MatchingAssetTags_CountsStacks)
	{
		using namespace ArcBuildingIngredientVMHelpers;

		FGameplayTagContainer ItemAssetTags;
		ItemAssetTags.AddTag(TAG_BuildIngredientTest_Material);

		const FPrimaryAssetId Def = MakeTaggedItemDef(ItemRecords, ItemAssetTags);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		FMassEntityHandle Entity;
		const FArcMassItemStoreFragment* Store = BuildSeededInventoryEntity(*EM, Entity, Def, 4);
		ASSERT_THAT(IsNotNull(Store, TEXT("Inventory store entity must be valid")));

		FGameplayTagContainer RequiredTags;
		RequiredTags.AddTag(TAG_BuildIngredientTest_Material);
		FInstancedStruct IngredientStruct = MakeTagIngredient(RequiredTags, 3);
		const FArcBuildIngredient* Ingredient = IngredientStruct.GetPtr<FArcBuildIngredient>();
		ASSERT_THAT(IsNotNull(Ingredient));

		UArcBuildingIngredientViewModel* VM = NewObject<UArcBuildingIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		VM->RefreshFrom(*Ingredient, *Store);

		ASSERT_THAT(AreEqual(3, VM->GetRequired(), TEXT("Required must equal ingredient Amount (3)")));
		ASSERT_THAT(AreEqual(4, VM->GetHave(), TEXT("Have must count the 4 stacks carrying the required AssetTag")));
		ASSERT_THAT(IsTrue(VM->GetbSatisfied(), TEXT("bSatisfied must be true when Have(4) >= Required(3)")));
		ASSERT_THAT(AreEqual(FString(TEXT("4 / 3")), VM->GetHaveRequiredText().ToString(),
			TEXT("HaveRequiredText must read \"4 / 3\"")));
	}

	// =========================================================================
	// _Tags ingredient — item's AssetTags don't carry the required tag => Have == 0.
	// =========================================================================
	TEST_METHOD(TagsIngredient_MissingRequiredTag_HaveIsZero)
	{
		using namespace ArcBuildingIngredientVMHelpers;

		FGameplayTagContainer ItemAssetTags;
		ItemAssetTags.AddTag(TAG_BuildIngredientTest_OtherMaterial);

		const FPrimaryAssetId Def = MakeTaggedItemDef(ItemRecords, ItemAssetTags);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		FMassEntityHandle Entity;
		const FArcMassItemStoreFragment* Store = BuildSeededInventoryEntity(*EM, Entity, Def, 5);
		ASSERT_THAT(IsNotNull(Store, TEXT("Inventory store entity must be valid")));

		FGameplayTagContainer RequiredTags;
		RequiredTags.AddTag(TAG_BuildIngredientTest_Material);
		FInstancedStruct IngredientStruct = MakeTagIngredient(RequiredTags, 1);
		const FArcBuildIngredient* Ingredient = IngredientStruct.GetPtr<FArcBuildIngredient>();
		ASSERT_THAT(IsNotNull(Ingredient));

		UArcBuildingIngredientViewModel* VM = NewObject<UArcBuildingIngredientViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		VM->RefreshFrom(*Ingredient, *Store);

		ASSERT_THAT(AreEqual(0, VM->GetHave(),
			TEXT("Have must be 0 — item's AssetTags don't carry the required tag")));
		ASSERT_THAT(IsFalse(VM->GetbSatisfied(), TEXT("bSatisfied must be false when Have(0) < Required(1)")));
	}
};

// ---------------------------------------------------------------------------
// Test class: UArcBuildingViewModel::RefreshFrom (affordability AND-rollup)
// ---------------------------------------------------------------------------

TEST_CLASS(ArcBuildingViewModelTests, "ArcUIModels.BuildingVM")
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
	// All ingredients satisfied => bCanAfford == true; also checks DisplayName /
	// Description passthrough and per-ingredient child VM count.
	// =========================================================================
	TEST_METHOD(AllIngredientsSatisfied_CanAffordTrue)
	{
		using namespace ArcBuildingIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		FMassEntityHandle Entity;
		const FArcMassItemStoreFragment* Store = BuildSeededInventoryEntity(*EM, Entity, Def, 5);
		ASSERT_THAT(IsNotNull(Store, TEXT("Inventory store entity must be valid")));

		UArcBuildingDefinition* Building = NewObject<UArcBuildingDefinition>(
			GetTransientPackage(), NAME_None, RF_Transient);
		Building->DisplayName = FText::FromString(TEXT("Test Building"));
		Building->Description = FText::FromString(TEXT("A building used for testing."));
		Building->Ingredients.Add(MakeItemDefIngredient(Def, 2));
		Building->Ingredients.Add(MakeItemDefIngredient(Def, 3));

		UArcBuildingViewModel* VM = NewObject<UArcBuildingViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		VM->RefreshFrom(Building, *Store);

		ASSERT_THAT(AreEqual(FString(TEXT("Test Building")), VM->GetDisplayName().ToString(),
			TEXT("DisplayName must pass through from the building definition")));
		ASSERT_THAT(AreEqual(FString(TEXT("A building used for testing.")), VM->GetDescription().ToString(),
			TEXT("Description must pass through from the building definition")));
		ASSERT_THAT(AreEqual(2, VM->GetIngredients().Num(),
			TEXT("Ingredients must produce one child VM per authored slot")));
		ASSERT_THAT(IsTrue(VM->GetbCanAfford(),
			TEXT("bCanAfford must be true when every ingredient is satisfied")));
	}

	// =========================================================================
	// One unsatisfied ingredient => bCanAfford == false.
	// =========================================================================
	TEST_METHOD(OneIngredientUnsatisfied_CanAffordFalse)
	{
		using namespace ArcBuildingIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		FMassEntityHandle Entity;
		const FArcMassItemStoreFragment* Store = BuildSeededInventoryEntity(*EM, Entity, Def, 1);
		ASSERT_THAT(IsNotNull(Store, TEXT("Inventory store entity must be valid")));

		UArcBuildingDefinition* Building = NewObject<UArcBuildingDefinition>(
			GetTransientPackage(), NAME_None, RF_Transient);
		// Satisfied slot: needs 1, inventory has 1.
		Building->Ingredients.Add(MakeItemDefIngredient(Def, 1));
		// Unsatisfied slot: needs 5, inventory only has 1.
		Building->Ingredients.Add(MakeItemDefIngredient(Def, 5));

		UArcBuildingViewModel* VM = NewObject<UArcBuildingViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		VM->RefreshFrom(Building, *Store);

		ASSERT_THAT(AreEqual(2, VM->GetIngredients().Num()));
		ASSERT_THAT(IsFalse(VM->GetbCanAfford(),
			TEXT("bCanAfford must be false when any ingredient is unsatisfied")));
	}

	// =========================================================================
	// Zero ingredients => bCanAfford == true (trivially satisfied — free placement).
	// =========================================================================
	TEST_METHOD(ZeroIngredients_CanAffordTrue)
	{
		using namespace ArcBuildingIngredientVMHelpers;

		FArcMassItemStoreFragment EmptyStore;

		UArcBuildingDefinition* Building = NewObject<UArcBuildingDefinition>(
			GetTransientPackage(), NAME_None, RF_Transient);

		UArcBuildingViewModel* VM = NewObject<UArcBuildingViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		VM->RefreshFrom(Building, EmptyStore);

		ASSERT_THAT(AreEqual(0, VM->GetIngredients().Num(),
			TEXT("Ingredients must be empty when the definition has none")));
		ASSERT_THAT(IsTrue(VM->GetbCanAfford(),
			TEXT("bCanAfford must be trivially true with zero ingredients")));
	}

	// =========================================================================
	// Ingredient child VMs are recycled by slot index across RefreshFrom calls —
	// same VM instances, not new objects each time.
	// =========================================================================
	TEST_METHOD(RefreshFromTwice_RecyclesIngredientChildVMs)
	{
		using namespace ArcBuildingIngredientVMHelpers;

		const FPrimaryAssetId Def = MakeItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		FMassEntityHandle Entity;
		const FArcMassItemStoreFragment* Store = BuildSeededInventoryEntity(*EM, Entity, Def, 2);
		ASSERT_THAT(IsNotNull(Store, TEXT("Inventory store entity must be valid")));

		UArcBuildingDefinition* Building = NewObject<UArcBuildingDefinition>(
			GetTransientPackage(), NAME_None, RF_Transient);
		Building->Ingredients.Add(MakeItemDefIngredient(Def, 1));
		Building->Ingredients.Add(MakeItemDefIngredient(Def, 2));

		UArcBuildingViewModel* VM = NewObject<UArcBuildingViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		VM->RefreshFrom(Building, *Store);
		ASSERT_THAT(AreEqual(2, VM->GetIngredients().Num()));
		UArcBuildingIngredientViewModel* FirstSlot0 = VM->GetIngredients()[0].Get();
		UArcBuildingIngredientViewModel* FirstSlot1 = VM->GetIngredients()[1].Get();
		ASSERT_THAT(IsNotNull(FirstSlot0));
		ASSERT_THAT(IsNotNull(FirstSlot1));

		VM->RefreshFrom(Building, *Store);
		ASSERT_THAT(AreEqual(2, VM->GetIngredients().Num()));

		ASSERT_THAT(IsTrue(VM->GetIngredients()[0].Get() == FirstSlot0,
			TEXT("Slot 0 child VM must be recycled across RefreshFrom calls")));
		ASSERT_THAT(IsTrue(VM->GetIngredients()[1].Get() == FirstSlot1,
			TEXT("Slot 1 child VM must be recycled across RefreshFrom calls")));
	}
};
