// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityHandle.h"

#include "ArcItemTestRecordFiles.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"

#include "ArcBuildIngredient.h"
#include "ArcBuildResourceConsumption.h"
#include "ArcBuildingDefinition.h"

#include "ArcBuilderTestHelpers.h"

// ---------------------------------------------------------------------------
// Unique tag prefix: TAG_BldRC_* — build resource consumption tests
// ---------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_BldRC_Wood, "Test.Material.BldRCWood");

namespace ArcBuildResourceConsumptionTestHelpers
{
	using namespace ArcBuilderTest::TestHelpers;

	/** Building definition requiring Amount stacks of TAG_BldRC_Wood via FArcBuildIngredient_Tags. */
	UArcBuildingDefinition* MakeTagIngredientBuildingDef(const FName& InName, int32 Amount)
	{
		UArcBuildingDefinition* Def = MakeTransientBuildingDef(InName);

		FArcBuildIngredient_Tags Ingredient;
		Ingredient.Amount = Amount;
		Ingredient.RequiredTags.AddTag(TAG_BldRC_Wood);
		Def->Ingredients.Add(FInstancedStruct::Make(Ingredient));
		Def->ResourceConsumption = TInstancedStruct<FArcBuildResourceConsumption>::Make<FArcBuildResourceConsumption_MassStore>();

		return Def;
	}

	/** Building definition requiring Amount stacks of a specific item definition. */
	UArcBuildingDefinition* MakeExactDefIngredientBuildingDef(const FName& InName, const FPrimaryAssetId& RequiredDefId, int32 Amount)
	{
		UArcBuildingDefinition* Def = MakeTransientBuildingDef(InName);

		FArcBuildIngredient_ItemDef Ingredient;
		Ingredient.Amount = Amount;
		Ingredient.ItemDefinitionId = RequiredDefId;
		Def->Ingredients.Add(FInstancedStruct::Make(Ingredient));

		Def->ResourceConsumption = TInstancedStruct<FArcBuildResourceConsumption>::Make<FArcBuildResourceConsumption_MassStore>();

		return Def;
	}

	int32 StoreItemCount(FMassEntityManager& EM, FMassEntityHandle Entity)
	{
		const FArcMassItemStoreFragment* Store = EM.GetSparseElementDataForEntity(
			FArcMassItemStoreFragment::StaticStruct(), Entity).GetPtr<FArcMassItemStoreFragment>();
		return Store != nullptr ? Store->ReplicatedItems.Items.Num() : -1;
	}

	int32 TotalStacksForDefinition(FMassEntityManager& EM, FMassEntityHandle Entity, const FPrimaryAssetId& DefId)
	{
		const FArcMassItemStoreFragment* Store = EM.GetSparseElementDataForEntity(
			FArcMassItemStoreFragment::StaticStruct(), Entity).GetPtr<FArcMassItemStoreFragment>();
		if (Store == nullptr)
		{
			return 0;
		}
		int32 Total = 0;
		for (const FArcMassReplicatedItem& ReplicatedItem : Store->ReplicatedItems.Items)
		{
			const FArcItemData* Item = ReplicatedItem.ToItem();
			if (Item != nullptr && Item->Spec.GetItemDefinitionId() == DefId)
			{
				Total += Item->Stacks;
			}
		}
		return Total;
	}
}

TEST_CLASS(ArcBuildResourceConsumptionMassStore, "ArcBuilder.ResourceConsumption.MassStore")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EM = nullptr;
	FArcItemTestRecordFiles Records;

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
	// TagMatch_Satisfied — a tag-matched ingredient is satisfied by a store item
	// carrying the required tag with enough stacks; OutMatched snapshot's Amount
	// equals the stacks that would be taken.
	// =========================================================================
	TEST_METHOD(TagMatch_Satisfied)
	{
		using namespace ArcBuildResourceConsumptionTestHelpers;
		using namespace ArcBuilderTest::TestHelpers;

		const FPrimaryAssetId WoodDef = MakeTaggedItemDef(Records, TAG_BldRC_Wood);
		ASSERT_THAT(IsTrue(WoodDef.IsValid(), TEXT("Item record must be written and indexed")));
		UArcBuildingDefinition* BuildingDef = MakeTagIngredientBuildingDef(TEXT("BldRC1_Building"), 3);

		FMassEntityHandle Instigator = CreateStoreEntity(*EM);
		ArcMassItems::AddItem(*EM, Instigator, FArcMassItemStoreFragment::StaticStruct(), MakeSpec(WoodDef, 5));

		const FArcBuildResourceConsumption* Consumption = BuildingDef->ResourceConsumption.GetPtr();
		ASSERT_THAT(IsNotNull(Consumption));

		TArray<FArcItemSpec> Matched;
		const bool bOk = Consumption->CheckResources(*EM, Instigator, BuildingDef, &Matched);

		ASSERT_THAT(IsTrue(bOk, TEXT("Tag-matched ingredient with sufficient stacks must be satisfied")));
		ASSERT_THAT(AreEqual(1, Matched.Num(), TEXT("Exactly one item stack should match the single ingredient slot")));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(Matched[0].Amount), TEXT("Matched spec Amount must equal stacks taken (3), not stacks available (5)")));
	}

	// =========================================================================
	// ExactDefMatch_Satisfied — FArcBuildIngredient_ItemDef matches by definition id.
	// =========================================================================
	TEST_METHOD(ExactDefMatch_Satisfied)
	{
		using namespace ArcBuildResourceConsumptionTestHelpers;
		using namespace ArcBuilderTest::TestHelpers;

		const FPrimaryAssetId WoodDef = MakeTaggedItemDef(Records, FGameplayTag());
		ASSERT_THAT(IsTrue(WoodDef.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId StoneDef = MakeTaggedItemDef(Records, FGameplayTag());
		ASSERT_THAT(IsTrue(StoneDef.IsValid(), TEXT("Item record must be written and indexed")));
		UArcBuildingDefinition* BuildingDef = MakeExactDefIngredientBuildingDef(TEXT("BldRC2_Building"), WoodDef, 2);

		FMassEntityHandle Instigator = CreateStoreEntity(*EM);
		// A non-matching item present alongside the required one — must not be counted.
		ArcMassItems::AddItem(*EM, Instigator, FArcMassItemStoreFragment::StaticStruct(), MakeSpec(StoneDef, 10));
		ArcMassItems::AddItem(*EM, Instigator, FArcMassItemStoreFragment::StaticStruct(), MakeSpec(WoodDef, 2));

		const FArcBuildResourceConsumption* Consumption = BuildingDef->ResourceConsumption.GetPtr();
		ASSERT_THAT(IsNotNull(Consumption));

		TArray<FArcItemSpec> Matched;
		const bool bOk = Consumption->CheckResources(*EM, Instigator, BuildingDef, &Matched);

		ASSERT_THAT(IsTrue(bOk, TEXT("Exact-definition ingredient must be satisfied by the matching item")));
		ASSERT_THAT(AreEqual(1, Matched.Num()));
		ASSERT_THAT(IsTrue(Matched[0].GetItemDefinitionId() == WoodDef,
			TEXT("Matched spec must reference the required definition, not the non-matching stone item")));
	}

	// =========================================================================
	// InsufficientStacks_CheckFalse — fewer stacks than required across all
	// candidates must fail the check.
	// =========================================================================
	TEST_METHOD(InsufficientStacks_CheckFalse)
	{
		using namespace ArcBuildResourceConsumptionTestHelpers;
		using namespace ArcBuilderTest::TestHelpers;

		const FPrimaryAssetId WoodDef = MakeTaggedItemDef(Records, TAG_BldRC_Wood);
		ASSERT_THAT(IsTrue(WoodDef.IsValid(), TEXT("Item record must be written and indexed")));
		UArcBuildingDefinition* BuildingDef = MakeTagIngredientBuildingDef(TEXT("BldRC3_Building"), 5);

		FMassEntityHandle Instigator = CreateStoreEntity(*EM);
		ArcMassItems::AddItem(*EM, Instigator, FArcMassItemStoreFragment::StaticStruct(), MakeSpec(WoodDef, 2));

		const FArcBuildResourceConsumption* Consumption = BuildingDef->ResourceConsumption.GetPtr();
		ASSERT_THAT(IsNotNull(Consumption));

		TArray<FArcItemSpec> Matched;
		const bool bOk = Consumption->CheckResources(*EM, Instigator, BuildingDef, &Matched);

		ASSERT_THAT(IsFalse(bOk, TEXT("Requiring 5 with only 2 available must fail")));
		ASSERT_THAT(AreEqual(0, Matched.Num(), TEXT("OutMatched must be empty on failure")));
	}

	// =========================================================================
	// MultiItemFill_OneSlot — one ingredient slot filled from two separate item
	// stacks whose amounts sum to the required Amount.
	// =========================================================================
	TEST_METHOD(MultiItemFill_OneSlot)
	{
		using namespace ArcBuildResourceConsumptionTestHelpers;
		using namespace ArcBuilderTest::TestHelpers;

		const FPrimaryAssetId WoodDefA = MakeTaggedItemDef(Records, TAG_BldRC_Wood);
		ASSERT_THAT(IsTrue(WoodDefA.IsValid(), TEXT("Item record must be written and indexed")));
		const FPrimaryAssetId WoodDefB = MakeTaggedItemDef(Records, TAG_BldRC_Wood);
		ASSERT_THAT(IsTrue(WoodDefB.IsValid(), TEXT("Item record must be written and indexed")));
		UArcBuildingDefinition* BuildingDef = MakeTagIngredientBuildingDef(TEXT("BldRC4_Building"), 5);

		FMassEntityHandle Instigator = CreateStoreEntity(*EM);
		ArcMassItems::AddItem(*EM, Instigator, FArcMassItemStoreFragment::StaticStruct(), MakeSpec(WoodDefA, 2));
		ArcMassItems::AddItem(*EM, Instigator, FArcMassItemStoreFragment::StaticStruct(), MakeSpec(WoodDefB, 3));

		const FArcBuildResourceConsumption* Consumption = BuildingDef->ResourceConsumption.GetPtr();
		ASSERT_THAT(IsNotNull(Consumption));

		TArray<FArcItemSpec> Matched;
		const bool bOk = Consumption->CheckResources(*EM, Instigator, BuildingDef, &Matched);

		ASSERT_THAT(IsTrue(bOk, TEXT("Two stacks summing to the required amount must satisfy one slot")));
		ASSERT_THAT(AreEqual(2, Matched.Num(), TEXT("Both stacks must be reported as matched")));

		int32 TotalTaken = 0;
		for (const FArcItemSpec& Spec : Matched)
		{
			TotalTaken += Spec.Amount;
		}
		ASSERT_THAT(AreEqual(5, TotalTaken, TEXT("Sum of taken stacks must equal the required amount")));
	}

	// =========================================================================
	// Consume_RemovesCorrectStacksAndReturnsSpecs — ConsumeResources removes
	// exactly the matched stacks from the store and reports matching specs.
	// =========================================================================
	TEST_METHOD(Consume_RemovesCorrectStacksAndReturnsSpecs)
	{
		using namespace ArcBuildResourceConsumptionTestHelpers;
		using namespace ArcBuilderTest::TestHelpers;

		const FPrimaryAssetId WoodDef = MakeTaggedItemDef(Records, TAG_BldRC_Wood);
		ASSERT_THAT(IsTrue(WoodDef.IsValid(), TEXT("Item record must be written and indexed")));
		UArcBuildingDefinition* BuildingDef = MakeTagIngredientBuildingDef(TEXT("BldRC5_Building"), 3);

		FMassEntityHandle Instigator = CreateStoreEntity(*EM);
		ArcMassItems::AddItem(*EM, Instigator, FArcMassItemStoreFragment::StaticStruct(), MakeSpec(WoodDef, 5));

		const FArcBuildResourceConsumption* Consumption = BuildingDef->ResourceConsumption.GetPtr();
		ASSERT_THAT(IsNotNull(Consumption));

		TArray<FArcItemSpec> Consumed;
		const bool bOk = Consumption->ConsumeResources(*EM, Instigator, BuildingDef, Consumed);

		ASSERT_THAT(IsTrue(bOk, TEXT("Consume must succeed when the store funds the ingredient")));
		ASSERT_THAT(AreEqual(1, Consumed.Num()));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(Consumed[0].Amount), TEXT("Consumed spec Amount must equal stacks removed")));

		const int32 Remaining = TotalStacksForDefinition(*EM, Instigator, WoodDef);
		ASSERT_THAT(AreEqual(2, Remaining, TEXT("Store must retain the unconsumed 2 stacks (5 - 3)")));
	}

	// =========================================================================
	// EmptyIngredients_FreePlacement — a building with no ingredients is always
	// satisfied and reports/consumes nothing.
	// =========================================================================
	TEST_METHOD(EmptyIngredients_FreePlacement)
	{
		using namespace ArcBuilderTest::TestHelpers;

		UArcBuildingDefinition* BuildingDef = MakeTransientBuildingDef(TEXT("BldRC6_Building"));
		BuildingDef->ResourceConsumption = TInstancedStruct<FArcBuildResourceConsumption>::Make<FArcBuildResourceConsumption_MassStore>();

		FMassEntityHandle Instigator = CreateStoreEntity(*EM);

		const FArcBuildResourceConsumption* Consumption = BuildingDef->ResourceConsumption.GetPtr();
		ASSERT_THAT(IsNotNull(Consumption));

		TArray<FArcItemSpec> Matched;
		const bool bCheckOk = Consumption->CheckResources(*EM, Instigator, BuildingDef, &Matched);
		ASSERT_THAT(IsTrue(bCheckOk, TEXT("Empty ingredients must always check true (free placement)")));
		ASSERT_THAT(AreEqual(0, Matched.Num()));

		TArray<FArcItemSpec> Consumed;
		FArcBuildResourceConsumption* MutableConsumption =
			const_cast<FArcBuildResourceConsumption*>(Consumption);
		const bool bConsumeOk = MutableConsumption->ConsumeResources(*EM, Instigator, BuildingDef, Consumed);
		ASSERT_THAT(IsTrue(bConsumeOk, TEXT("Empty ingredients must always consume true")));
		ASSERT_THAT(AreEqual(0, Consumed.Num()));
	}

	// =========================================================================
	// Check_IsReadOnly — CheckResources never mutates the store, even though it
	// reports a satisfiable match.
	// =========================================================================
	TEST_METHOD(Check_IsReadOnly)
	{
		using namespace ArcBuildResourceConsumptionTestHelpers;
		using namespace ArcBuilderTest::TestHelpers;

		const FPrimaryAssetId WoodDef = MakeTaggedItemDef(Records, TAG_BldRC_Wood);
		ASSERT_THAT(IsTrue(WoodDef.IsValid(), TEXT("Item record must be written and indexed")));
		UArcBuildingDefinition* BuildingDef = MakeTagIngredientBuildingDef(TEXT("BldRC7_Building"), 3);

		FMassEntityHandle Instigator = CreateStoreEntity(*EM);
		ArcMassItems::AddItem(*EM, Instigator, FArcMassItemStoreFragment::StaticStruct(), MakeSpec(WoodDef, 5));

		const int32 CountBefore = StoreItemCount(*EM, Instigator);
		const int32 StacksBefore = TotalStacksForDefinition(*EM, Instigator, WoodDef);

		const FArcBuildResourceConsumption* Consumption = BuildingDef->ResourceConsumption.GetPtr();
		ASSERT_THAT(IsNotNull(Consumption));

		TArray<FArcItemSpec> Matched;
		Consumption->CheckResources(*EM, Instigator, BuildingDef, &Matched);

		ASSERT_THAT(AreEqual(CountBefore, StoreItemCount(*EM, Instigator), TEXT("Item count in store must be unchanged after Check")));
		ASSERT_THAT(AreEqual(StacksBefore, TotalStacksForDefinition(*EM, Instigator, WoodDef), TEXT("Stacks must be unchanged after Check")));
	}
};
