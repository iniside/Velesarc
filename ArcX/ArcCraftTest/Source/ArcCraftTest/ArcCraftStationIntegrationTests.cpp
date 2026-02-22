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

#include "ArcCraftStationIntegrationTests.h"

#include "CQTest.h"

#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"
#include "Components/ActorTestSpawner.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"
#include "ArcCraft/Station/ArcCraftItemSource.h"
#include "ArcCraft/Station/ArcCraftOutputDelivery.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcCraftStationTest, Log, All);

// ---- Tags unique to this translation unit ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StationTest_Station_Forge, "Station.Test.Forge");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StationTest_Station_Alchemy, "Station.Test.Alchemy");

// ===================================================================
// Helpers
// ===================================================================

namespace CraftStationTestHelpers
{
	/**
	 * Create a transient UArcItemDefinition with a unique ItemId.
	 * The definition has ItemType="ArcItem" (from constructor) and a generated GUID.
	 */
	UArcItemDefinition* CreateTransientItemDef(const FName& Name)
	{
		UArcItemDefinition* Def = NewObject<UArcItemDefinition>(
			GetTransientPackage(), Name, RF_Transient);
		Def->RegenerateItemId();
		return Def;
	}

	/**
	 * Create a transient recipe definition with ItemDef-based ingredients.
	 * Each ingredient requires exactly 1 of the corresponding item definition.
	 * Uses FPrimaryAssetId from actual UArcItemDefinition objects for matching.
	 */
	UArcRecipeDefinition* CreateTransientRecipe(
		const FName& Name,
		const TArray<const UArcItemDefinition*>& IngredientItemDefs,
		const UArcItemDefinition* OutputItemDef,
		float CraftTime = 1.0f,
		const FGameplayTagContainer& RequiredStationTags = FGameplayTagContainer())
	{
		UArcRecipeDefinition* Recipe = NewObject<UArcRecipeDefinition>(
			GetTransientPackage(), Name, RF_Transient);
		Recipe->RecipeName = FText::FromName(Name);
		Recipe->CraftTime = CraftTime;
		Recipe->RequiredStationTags = RequiredStationTags;
		Recipe->OutputAmount = 1;
		Recipe->OutputLevel = 1;
		Recipe->OutputItemDefinition = OutputItemDef->GetPrimaryAssetId();

		for (const UArcItemDefinition* IngDef : IngredientItemDefs)
		{
			FArcRecipeIngredient_ItemDef Ingredient;
			Ingredient.ItemDefinitionId = IngDef->GetPrimaryAssetId();
			Ingredient.Amount = 1;
			Ingredient.bConsumeOnCraft = true;

			FInstancedStruct IngStruct;
			IngStruct.InitializeAs<FArcRecipeIngredient_ItemDef>(Ingredient);
			Recipe->Ingredients.Add(MoveTemp(IngStruct));
		}

		return Recipe;
	}

	/**
	 * Create an FArcItemSpec for a transient item definition.
	 * Sets both the FPrimaryAssetId and the UArcItemDefinition* pointer.
	 */
	FArcItemSpec MakeItemSpec(
		const UArcItemDefinition* ItemDef,
		int32 Amount = 1,
		uint8 Level = 1)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinition(ItemDef->GetPrimaryAssetId())
			.SetItemDefinitionAsset(ItemDef)
			.SetAmount(Amount)
			.SetItemLevel(Level);
		return Spec;
	}

	/** Add an item to a store using a transient definition. Returns its item ID. */
	FArcItemId AddItemToStore(
		UArcItemsStoreComponent* Store,
		const UArcItemDefinition* ItemDef,
		int32 Amount = 1)
	{
		FArcItemSpec Spec = MakeItemSpec(ItemDef, Amount);
		return Store->AddItem(Spec, FArcItemId::InvalidId);
	}

	/** Configure a station with StationStore input and StoreOnStation output. */
	void ConfigureStationStore(
		UArcCraftStationTestComponent* Station,
		const FGameplayTagContainer& StationTags)
	{
		Station->SetStationTags(StationTags);

		FArcCraftItemSource_StationStore SourceConfig;
		SourceConfig.ItemsStoreClass = UArcCraftStationTestInputStore::StaticClass();
		TInstancedStruct<FArcCraftItemSource> SourceStruct;
		SourceStruct.InitializeAs<FArcCraftItemSource_StationStore>(SourceConfig);
		Station->SetItemSource(SourceStruct);

		FArcCraftOutputDelivery_StoreOnStation DeliveryConfig;
		DeliveryConfig.OutputStoreClass = UArcCraftStationTestOutputStore::StaticClass();
		TInstancedStruct<FArcCraftOutputDelivery> DeliveryStruct;
		DeliveryStruct.InitializeAs<FArcCraftOutputDelivery_StoreOnStation>(DeliveryConfig);
		Station->SetOutputDelivery(DeliveryStruct);
	}

	/** Configure a station with InstigatorStore input and ToInstigator output. */
	void ConfigureInstigatorStore(
		UArcCraftStationTestComponent* Station,
		const FGameplayTagContainer& StationTags)
	{
		Station->SetStationTags(StationTags);

		FArcCraftItemSource_InstigatorStore SourceConfig;
		SourceConfig.ItemsStoreClass = UArcCraftStationTestInstigatorStore::StaticClass();
		TInstancedStruct<FArcCraftItemSource> SourceStruct;
		SourceStruct.InitializeAs<FArcCraftItemSource_InstigatorStore>(SourceConfig);
		Station->SetItemSource(SourceStruct);

		FArcCraftOutputDelivery_ToInstigator DeliveryConfig;
		DeliveryConfig.ItemsStoreClass = UArcCraftStationTestInstigatorStore::StaticClass();
		TInstancedStruct<FArcCraftOutputDelivery> DeliveryStruct;
		DeliveryStruct.InitializeAs<FArcCraftOutputDelivery_ToInstigator>(DeliveryConfig);
		Station->SetOutputDelivery(DeliveryStruct);
	}
}

// ===================================================================
// TEST: Station Store source — deposit items into station, then craft
// ===================================================================

TEST_CLASS(ArcCraftStation_StationStoreTests, "ArcCraft.Station.StationStore")
{
	FActorTestSpawner Spawner;

	TObjectPtr<UArcItemDefinition> IronOreDef;
	TObjectPtr<UArcItemDefinition> WoodDef;
	TObjectPtr<UArcItemDefinition> SwordDef;
	TObjectPtr<UArcRecipeDefinition> SwordRecipe;

	AArcCraftStationTestableActor* StationActor = nullptr;
	AArcCraftStationTestInstigator* Instigator = nullptr;

	BEFORE_EACH()
	{
		Spawner.GetWorld();

		// Create transient item definitions
		IronOreDef = CraftStationTestHelpers::CreateTransientItemDef(TEXT("TestDef_IronOre"));
		WoodDef = CraftStationTestHelpers::CreateTransientItemDef(TEXT("TestDef_Wood"));
		SwordDef = CraftStationTestHelpers::CreateTransientItemDef(TEXT("TestDef_Sword"));

		FGameplayTagContainer ForgeTags;
		ForgeTags.AddTag(TAG_StationTest_Station_Forge);

		SwordRecipe = CraftStationTestHelpers::CreateTransientRecipe(
			TEXT("TestRecipe_Sword"),
			{ IronOreDef, WoodDef },
			SwordDef,
			1.0f,
			ForgeTags);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = *FGuid::NewGuid().ToString();
		StationActor = &Spawner.SpawnActor<AArcCraftStationTestableActor>(SpawnParams);

		CraftStationTestHelpers::ConfigureStationStore(
			StationActor->StationComponent, ForgeTags);
		StationActor->StationComponent->SetTimeMode(EArcCraftStationTimeMode::AutoTick);

		// Register all transient definitions so output specs can resolve them
		StationActor->StationComponent->RegisterTransientDefinition(IronOreDef);
		StationActor->StationComponent->RegisterTransientDefinition(WoodDef);
		StationActor->StationComponent->RegisterTransientDefinition(SwordDef);

		FActorSpawnParameters InstigatorParams;
		InstigatorParams.Name = *FGuid::NewGuid().ToString();
		Instigator = &Spawner.SpawnActor<AArcCraftStationTestInstigator>(InstigatorParams);
	}

	// ---------------------------------------------------------------
	// Deposit items first, then select recipe — standard flow
	// ---------------------------------------------------------------

	TEST_METHOD(DepositItemsThenQueueRecipe_Succeeds)
	{
		// Deposit iron ore into station
		FArcItemSpec IronSpec = CraftStationTestHelpers::MakeItemSpec(IronOreDef);
		bool bIronDeposited = StationActor->StationComponent->DepositItem(IronSpec, Instigator);
		ASSERT_THAT(IsTrue(bIronDeposited, TEXT("Iron ore deposit should succeed")));

		// Deposit wood into station
		FArcItemSpec WoodSpec = CraftStationTestHelpers::MakeItemSpec(WoodDef);
		bool bWoodDeposited = StationActor->StationComponent->DepositItem(WoodSpec, Instigator);
		ASSERT_THAT(IsTrue(bWoodDeposited, TEXT("Wood deposit should succeed")));

		// Verify items are in the input store
		TArray<const FArcItemData*> StoreItems = StationActor->InputStore->GetItems();
		ASSERT_THAT(AreEqual(2, StoreItems.Num(), TEXT("Input store should have 2 items")));

		// Select recipe — should consume deposited items
		bool bQueued = StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator);
		ASSERT_THAT(IsTrue(bQueued, TEXT("QueueRecipe should succeed after depositing items")));

		// Queue should have one entry
		const TArray<FArcCraftQueueEntry>& Queue = StationActor->StationComponent->GetQueue();
		ASSERT_THAT(AreEqual(1, Queue.Num(), TEXT("Queue should have 1 entry")));
		ASSERT_THAT(IsTrue(Queue[0].Recipe == SwordRecipe, TEXT("Queue entry recipe mismatch")));
		ASSERT_THAT(AreEqual(1, Queue[0].Amount, TEXT("Queue entry amount should be 1")));
		ASSERT_THAT(AreEqual(0, Queue[0].CompletedAmount, TEXT("No items completed yet")));

		// Items should be consumed from the input store
		TArray<const FArcItemData*> PostQueueItems = StationActor->InputStore->GetItems();
		ASSERT_THAT(AreEqual(0, PostQueueItems.Num(), TEXT("Input store should be empty")));
	}

	// ---------------------------------------------------------------
	// Select recipe without ingredients — fails
	// ---------------------------------------------------------------

	TEST_METHOD(QueueRecipeWithoutIngredients_Fails)
	{
		bool bQueued = StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator);
		ASSERT_THAT(IsFalse(bQueued, TEXT("QueueRecipe should fail without ingredients")));

		const TArray<FArcCraftQueueEntry>& Queue = StationActor->StationComponent->GetQueue();
		ASSERT_THAT(AreEqual(0, Queue.Num(), TEXT("Queue should be empty")));
	}

	// ---------------------------------------------------------------
	// Deposit partial items, queue fails, deposit remainder, queue succeeds
	// ---------------------------------------------------------------

	TEST_METHOD(DepositPartialThenRemainder_QueueSucceeds)
	{
		// Deposit only iron
		FArcItemSpec IronSpec = CraftStationTestHelpers::MakeItemSpec(IronOreDef);
		StationActor->StationComponent->DepositItem(IronSpec, Instigator);

		// Try to queue — should fail (missing wood)
		bool bQueued1 = StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator);
		ASSERT_THAT(IsFalse(bQueued1, TEXT("Should fail with partial ingredients")));

		// Iron should still be in the store (not consumed on failed queue)
		TArray<const FArcItemData*> Items = StationActor->InputStore->GetItems();
		ASSERT_THAT(AreEqual(1, Items.Num(), TEXT("Iron should remain in store")));

		// Now deposit wood
		FArcItemSpec WoodSpec = CraftStationTestHelpers::MakeItemSpec(WoodDef);
		StationActor->StationComponent->DepositItem(WoodSpec, Instigator);

		// Try again — should succeed
		bool bQueued2 = StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator);
		ASSERT_THAT(IsTrue(bQueued2, TEXT("Should succeed with all ingredients")));
	}

	// ---------------------------------------------------------------
	// CanCraftRecipe reflects ingredient availability
	// ---------------------------------------------------------------

	TEST_METHOD(CanCraftRecipe_ReflectsIngredientState)
	{
		// Before depositing
		ASSERT_THAT(IsFalse(
			StationActor->StationComponent->CanCraftRecipe(SwordRecipe, Instigator),
			TEXT("Should not be craftable without ingredients")));

		// Deposit one ingredient
		FArcItemSpec IronSpec = CraftStationTestHelpers::MakeItemSpec(IronOreDef);
		StationActor->StationComponent->DepositItem(IronSpec, Instigator);

		ASSERT_THAT(IsFalse(
			StationActor->StationComponent->CanCraftRecipe(SwordRecipe, Instigator),
			TEXT("Should not be craftable with partial ingredients")));

		// Deposit second ingredient
		FArcItemSpec WoodSpec = CraftStationTestHelpers::MakeItemSpec(WoodDef);
		StationActor->StationComponent->DepositItem(WoodSpec, Instigator);

		ASSERT_THAT(IsTrue(
			StationActor->StationComponent->CanCraftRecipe(SwordRecipe, Instigator),
			TEXT("Should be craftable with all ingredients")));
	}

	// ---------------------------------------------------------------
	// AutoTick: craft completes after sufficient time
	// ---------------------------------------------------------------

	TEST_METHOD(AutoTick_CraftCompletesAfterTime)
	{
		// Deposit both ingredients
		FArcItemSpec IronSpec = CraftStationTestHelpers::MakeItemSpec(IronOreDef);
		StationActor->StationComponent->DepositItem(IronSpec, Instigator);
		FArcItemSpec WoodSpec = CraftStationTestHelpers::MakeItemSpec(WoodDef);
		StationActor->StationComponent->DepositItem(WoodSpec, Instigator);

		// Queue recipe (CraftTime = 1.0s)
		ASSERT_THAT(IsTrue(
			StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator)));

		// Simulate tick — not enough time yet
		StationActor->StationComponent->SimulateTick(0.5f);
		ASSERT_THAT(AreEqual(1, StationActor->StationComponent->GetQueue().Num(),
			TEXT("Entry should still be in queue")));
		ASSERT_THAT(AreEqual(0, StationActor->StationComponent->GetQueue()[0].CompletedAmount,
			TEXT("Nothing completed yet")));

		// Simulate remaining time
		StationActor->StationComponent->SimulateTick(0.6f);

		// Entry should be removed after completion
		ASSERT_THAT(AreEqual(0, StationActor->StationComponent->GetQueue().Num(),
			TEXT("Queue should be empty after craft completes")));

		// Output should be in the output store
		TArray<const FArcItemData*> OutputItems = StationActor->OutputStore->GetItems();
		ASSERT_THAT(AreEqual(1, OutputItems.Num(), TEXT("Output store should have 1 crafted item")));
		ASSERT_THAT(IsTrue(OutputItems[0]->GetItemDefinitionId() == SwordDef->GetPrimaryAssetId(),
			TEXT("Output should be a sword")));
	}

	// ---------------------------------------------------------------
	// Cancel queue entry
	// ---------------------------------------------------------------

	TEST_METHOD(CancelQueueEntry_RemovesFromQueue)
	{
		FArcItemSpec IronSpec = CraftStationTestHelpers::MakeItemSpec(IronOreDef);
		StationActor->StationComponent->DepositItem(IronSpec, Instigator);
		FArcItemSpec WoodSpec = CraftStationTestHelpers::MakeItemSpec(WoodDef);
		StationActor->StationComponent->DepositItem(WoodSpec, Instigator);

		StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator);

		const TArray<FArcCraftQueueEntry>& Queue = StationActor->StationComponent->GetQueue();
		ASSERT_THAT(AreEqual(1, Queue.Num()));
		FGuid EntryId = Queue[0].EntryId;

		bool bCanceled = StationActor->StationComponent->CancelQueueEntry(EntryId);
		ASSERT_THAT(IsTrue(bCanceled, TEXT("Cancel should succeed")));
		ASSERT_THAT(AreEqual(0, StationActor->StationComponent->GetQueue().Num(),
			TEXT("Queue should be empty after cancel")));
	}

	// ---------------------------------------------------------------
	// Wrong station tags — recipe rejected
	// ---------------------------------------------------------------

	TEST_METHOD(WrongStationTags_QueueFails)
	{
		// Change station tags to alchemy (recipe requires forge)
		FGameplayTagContainer AlchemyTags;
		AlchemyTags.AddTag(TAG_StationTest_Station_Alchemy);
		StationActor->StationComponent->SetStationTags(AlchemyTags);

		// Deposit items
		FArcItemSpec IronSpec = CraftStationTestHelpers::MakeItemSpec(IronOreDef);
		StationActor->StationComponent->DepositItem(IronSpec, Instigator);
		FArcItemSpec WoodSpec = CraftStationTestHelpers::MakeItemSpec(WoodDef);
		StationActor->StationComponent->DepositItem(WoodSpec, Instigator);

		bool bQueued = StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator);
		ASSERT_THAT(IsFalse(bQueued, TEXT("Should fail with wrong station tags")));
	}

	// ---------------------------------------------------------------
	// Queue multiple recipes — processes in priority order
	// ---------------------------------------------------------------

	TEST_METHOD(QueueMultipleRecipes_ProcessesInPriorityOrder)
	{
		TObjectPtr<UArcItemDefinition> ShieldDef =
			CraftStationTestHelpers::CreateTransientItemDef(TEXT("TestDef_Shield"));
		StationActor->StationComponent->RegisterTransientDefinition(ShieldDef);

		FGameplayTagContainer ForgeTags;
		ForgeTags.AddTag(TAG_StationTest_Station_Forge);

		TObjectPtr<UArcRecipeDefinition> ShieldRecipe = CraftStationTestHelpers::CreateTransientRecipe(
			TEXT("TestRecipe_Shield"),
			{ IronOreDef, WoodDef },
			ShieldDef,
			1.0f, ForgeTags);

		// Deposit + queue sword
		FArcItemSpec IronSpec = CraftStationTestHelpers::MakeItemSpec(IronOreDef);
		FArcItemSpec WoodSpec = CraftStationTestHelpers::MakeItemSpec(WoodDef);

		StationActor->StationComponent->DepositItem(IronSpec, Instigator);
		StationActor->StationComponent->DepositItem(WoodSpec, Instigator);
		ASSERT_THAT(IsTrue(StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator)));

		// Deposit + queue shield
		StationActor->StationComponent->DepositItem(IronSpec, Instigator);
		StationActor->StationComponent->DepositItem(WoodSpec, Instigator);
		ASSERT_THAT(IsTrue(StationActor->StationComponent->QueueRecipe(ShieldRecipe, Instigator)));

		ASSERT_THAT(AreEqual(2, StationActor->StationComponent->GetQueue().Num(),
			TEXT("Should have 2 queue entries")));

		// Complete first
		StationActor->StationComponent->SimulateTick(1.1f);
		ASSERT_THAT(AreEqual(1, StationActor->StationComponent->GetQueue().Num(),
			TEXT("One entry should remain")));

		// Complete second
		StationActor->StationComponent->SimulateTick(1.1f);
		ASSERT_THAT(AreEqual(0, StationActor->StationComponent->GetQueue().Num(),
			TEXT("Queue should be empty")));

		TArray<const FArcItemData*> OutputItems = StationActor->OutputStore->GetItems();
		ASSERT_THAT(AreEqual(2, OutputItems.Num(), TEXT("Should have 2 crafted items")));
	}

	// ---------------------------------------------------------------
	// Queue capacity limit
	// ---------------------------------------------------------------

	TEST_METHOD(QueueFull_RejectsNewEntry)
	{
		StationActor->StationComponent->SetMaxQueueSize(1);

		FArcItemSpec IronSpec = CraftStationTestHelpers::MakeItemSpec(IronOreDef);
		FArcItemSpec WoodSpec = CraftStationTestHelpers::MakeItemSpec(WoodDef);

		// Fill queue
		StationActor->StationComponent->DepositItem(IronSpec, Instigator);
		StationActor->StationComponent->DepositItem(WoodSpec, Instigator);
		ASSERT_THAT(IsTrue(StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator)));

		// Try second — rejected
		StationActor->StationComponent->DepositItem(IronSpec, Instigator);
		StationActor->StationComponent->DepositItem(WoodSpec, Instigator);
		ASSERT_THAT(IsFalse(StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator),
			TEXT("Should reject when queue is full")));
	}

	// ---------------------------------------------------------------
	// Queue multiple amount (craft N of same recipe)
	// ---------------------------------------------------------------

	TEST_METHOD(QueueMultipleAmount_CompletesAll)
	{
		FArcItemSpec IronSpec = CraftStationTestHelpers::MakeItemSpec(IronOreDef);
		FArcItemSpec WoodSpec = CraftStationTestHelpers::MakeItemSpec(WoodDef);

		StationActor->StationComponent->DepositItem(IronSpec, Instigator);
		StationActor->StationComponent->DepositItem(WoodSpec, Instigator);

		// Queue 3 of the same recipe
		ASSERT_THAT(IsTrue(StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator, 3)));

		const TArray<FArcCraftQueueEntry>& Queue = StationActor->StationComponent->GetQueue();
		ASSERT_THAT(AreEqual(1, Queue.Num()));
		ASSERT_THAT(AreEqual(3, Queue[0].Amount));

		// Complete all 3 items (CraftTime = 1.0s each)
		StationActor->StationComponent->SimulateTick(1.1f);
		ASSERT_THAT(AreEqual(1, StationActor->StationComponent->GetQueue().Num()));

		StationActor->StationComponent->SimulateTick(1.1f);
		ASSERT_THAT(AreEqual(1, StationActor->StationComponent->GetQueue().Num()));

		StationActor->StationComponent->SimulateTick(1.1f);
		ASSERT_THAT(AreEqual(0, StationActor->StationComponent->GetQueue().Num(),
			TEXT("Queue should be empty after all 3 complete")));

		TArray<const FArcItemData*> OutputItems = StationActor->OutputStore->GetItems();
		ASSERT_THAT(AreEqual(3, OutputItems.Num(), TEXT("Should have 3 crafted items")));
	}
};

// ===================================================================
// TEST: Instigator Store source — items from player inventory
// ===================================================================

TEST_CLASS(ArcCraftStation_InstigatorStoreTests, "ArcCraft.Station.InstigatorStore")
{
	FActorTestSpawner Spawner;

	TObjectPtr<UArcItemDefinition> IronOreDef;
	TObjectPtr<UArcItemDefinition> WoodDef;
	TObjectPtr<UArcItemDefinition> SwordDef;
	TObjectPtr<UArcRecipeDefinition> SwordRecipe;

	AArcCraftStationTestableActor* StationActor = nullptr;
	AArcCraftStationTestInstigator* Instigator = nullptr;

	BEFORE_EACH()
	{
		Spawner.GetWorld();

		IronOreDef = CraftStationTestHelpers::CreateTransientItemDef(TEXT("TestDef_InsIronOre"));
		WoodDef = CraftStationTestHelpers::CreateTransientItemDef(TEXT("TestDef_InsWood"));
		SwordDef = CraftStationTestHelpers::CreateTransientItemDef(TEXT("TestDef_InsSword"));

		FGameplayTagContainer ForgeTags;
		ForgeTags.AddTag(TAG_StationTest_Station_Forge);

		SwordRecipe = CraftStationTestHelpers::CreateTransientRecipe(
			TEXT("TestRecipe_InsSword"),
			{ IronOreDef, WoodDef },
			SwordDef,
			1.0f, ForgeTags);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = *FGuid::NewGuid().ToString();
		StationActor = &Spawner.SpawnActor<AArcCraftStationTestableActor>(SpawnParams);

		CraftStationTestHelpers::ConfigureInstigatorStore(
			StationActor->StationComponent, ForgeTags);
		StationActor->StationComponent->SetTimeMode(EArcCraftStationTimeMode::AutoTick);

		// Register transient definitions for output resolution
		StationActor->StationComponent->RegisterTransientDefinition(IronOreDef);
		StationActor->StationComponent->RegisterTransientDefinition(WoodDef);
		StationActor->StationComponent->RegisterTransientDefinition(SwordDef);

		FActorSpawnParameters InstigatorParams;
		InstigatorParams.Name = *FGuid::NewGuid().ToString();
		Instigator = &Spawner.SpawnActor<AArcCraftStationTestInstigator>(InstigatorParams);
	}

	// ---------------------------------------------------------------
	// Player has items, select recipe — succeeds
	// ---------------------------------------------------------------

	TEST_METHOD(PlayerHasItems_QueueRecipe_Succeeds)
	{
		CraftStationTestHelpers::AddItemToStore(
			Instigator->InventoryStore, IronOreDef);
		CraftStationTestHelpers::AddItemToStore(
			Instigator->InventoryStore, WoodDef);

		ASSERT_THAT(AreEqual(2, Instigator->InventoryStore->GetItems().Num(),
			TEXT("Player should have 2 items")));

		bool bQueued = StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator);
		ASSERT_THAT(IsTrue(bQueued, TEXT("Should succeed with player items")));

		// Items consumed from player inventory
		ASSERT_THAT(AreEqual(0, Instigator->InventoryStore->GetItems().Num(),
			TEXT("Player inventory should be empty after queuing")));
	}

	// ---------------------------------------------------------------
	// Player has no items — fails
	// ---------------------------------------------------------------

	TEST_METHOD(PlayerHasNoItems_QueueRecipe_Fails)
	{
		bool bQueued = StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator);
		ASSERT_THAT(IsFalse(bQueued, TEXT("Should fail without player items")));
	}

	// ---------------------------------------------------------------
	// Craft delivers to player inventory
	// ---------------------------------------------------------------

	TEST_METHOD(CraftDelivers_ToPlayerInventory)
	{
		CraftStationTestHelpers::AddItemToStore(
			Instigator->InventoryStore, IronOreDef);
		CraftStationTestHelpers::AddItemToStore(
			Instigator->InventoryStore, WoodDef);

		StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator);
		StationActor->StationComponent->SimulateTick(1.1f);

		TArray<const FArcItemData*> PlayerItems = Instigator->InventoryStore->GetItems();
		ASSERT_THAT(AreEqual(1, PlayerItems.Num(), TEXT("Player should have 1 crafted item")));
		ASSERT_THAT(IsTrue(PlayerItems[0]->GetItemDefinitionId() == SwordDef->GetPrimaryAssetId(),
			TEXT("Crafted item should be a sword")));
	}

	// ---------------------------------------------------------------
	// Partial items → fail → add rest → succeed
	// ---------------------------------------------------------------

	TEST_METHOD(PartialPlayerInventory_ThenComplete_Succeeds)
	{
		// Only iron
		CraftStationTestHelpers::AddItemToStore(
			Instigator->InventoryStore, IronOreDef);

		ASSERT_THAT(IsFalse(
			StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator),
			TEXT("Should fail with partial items")));

		// Iron should still be there (not consumed on fail)
		ASSERT_THAT(AreEqual(1, Instigator->InventoryStore->GetItems().Num(),
			TEXT("Iron should still be in inventory")));

		// Add wood
		CraftStationTestHelpers::AddItemToStore(
			Instigator->InventoryStore, WoodDef);

		ASSERT_THAT(IsTrue(
			StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator),
			TEXT("Should succeed with all items")));
	}
};

// ===================================================================
// TEST: InteractionCheck time mode
// ===================================================================

TEST_CLASS(ArcCraftStation_InteractionCheckTests, "ArcCraft.Station.InteractionCheck")
{
	FActorTestSpawner Spawner;

	TObjectPtr<UArcItemDefinition> IronOreDef;
	TObjectPtr<UArcItemDefinition> WoodDef;
	TObjectPtr<UArcItemDefinition> SwordDef;
	TObjectPtr<UArcRecipeDefinition> SwordRecipe;

	AArcCraftStationTestableActor* StationActor = nullptr;
	AArcCraftStationTestInstigator* Instigator = nullptr;

	BEFORE_EACH()
	{
		Spawner.GetWorld();

		IronOreDef = CraftStationTestHelpers::CreateTransientItemDef(TEXT("TestDef_InteractIron"));
		WoodDef = CraftStationTestHelpers::CreateTransientItemDef(TEXT("TestDef_InteractWood"));
		SwordDef = CraftStationTestHelpers::CreateTransientItemDef(TEXT("TestDef_InteractSword"));

		FGameplayTagContainer ForgeTags;
		ForgeTags.AddTag(TAG_StationTest_Station_Forge);

		SwordRecipe = CraftStationTestHelpers::CreateTransientRecipe(
			TEXT("TestRecipe_InteractSword"),
			{ IronOreDef, WoodDef },
			SwordDef,
			1.0f, ForgeTags);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = *FGuid::NewGuid().ToString();
		StationActor = &Spawner.SpawnActor<AArcCraftStationTestableActor>(SpawnParams);

		CraftStationTestHelpers::ConfigureStationStore(
			StationActor->StationComponent, ForgeTags);
		StationActor->StationComponent->SetTimeMode(EArcCraftStationTimeMode::InteractionCheck);

		// Register transient definitions
		StationActor->StationComponent->RegisterTransientDefinition(IronOreDef);
		StationActor->StationComponent->RegisterTransientDefinition(WoodDef);
		StationActor->StationComponent->RegisterTransientDefinition(SwordDef);

		FActorSpawnParameters InstigatorParams;
		InstigatorParams.Name = *FGuid::NewGuid().ToString();
		Instigator = &Spawner.SpawnActor<AArcCraftStationTestInstigator>(InstigatorParams);
	}

	// ---------------------------------------------------------------
	// Queue stamps UTC timestamp with InteractionCheck mode
	// ---------------------------------------------------------------

	TEST_METHOD(QueueRecipe_StampsTimestamp)
	{
		FArcItemSpec IronSpec = CraftStationTestHelpers::MakeItemSpec(IronOreDef);
		StationActor->StationComponent->DepositItem(IronSpec, Instigator);
		FArcItemSpec WoodSpec = CraftStationTestHelpers::MakeItemSpec(WoodDef);
		StationActor->StationComponent->DepositItem(WoodSpec, Instigator);

		int64 BeforeTimestamp = FDateTime::UtcNow().ToUnixTimestamp();
		StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator);
		int64 AfterTimestamp = FDateTime::UtcNow().ToUnixTimestamp();

		const TArray<FArcCraftQueueEntry>& Queue = StationActor->StationComponent->GetQueue();
		ASSERT_THAT(AreEqual(1, Queue.Num()));
		ASSERT_THAT(IsTrue(Queue[0].TimeMode == EArcCraftStationTimeMode::InteractionCheck));
		ASSERT_THAT(IsTrue(Queue[0].StartTimestamp >= BeforeTimestamp,
			TEXT("Timestamp should be >= time before queue")));
		ASSERT_THAT(IsTrue(Queue[0].StartTimestamp <= AfterTimestamp,
			TEXT("Timestamp should be <= time after queue")));
	}

	// ---------------------------------------------------------------
	// Immediate interaction — nothing completed (insufficient time)
	// ---------------------------------------------------------------

	TEST_METHOD(ImmediateInteraction_DoesNotComplete)
	{
		FArcItemSpec IronSpec = CraftStationTestHelpers::MakeItemSpec(IronOreDef);
		StationActor->StationComponent->DepositItem(IronSpec, Instigator);
		FArcItemSpec WoodSpec = CraftStationTestHelpers::MakeItemSpec(WoodDef);
		StationActor->StationComponent->DepositItem(WoodSpec, Instigator);

		StationActor->StationComponent->QueueRecipe(SwordRecipe, Instigator);

		// Interact immediately — CraftTime is 1.0s, nearly no time has passed
		bool bCompleted = StationActor->StationComponent->TryCompleteOnInteraction(Instigator);
		ASSERT_THAT(IsFalse(bCompleted, TEXT("Should not complete immediately")));

		ASSERT_THAT(AreEqual(1, StationActor->StationComponent->GetQueue().Num(),
			TEXT("Queue entry should remain")));
	}
};

// ===================================================================
// TEST: Edge cases and validation
// ===================================================================

TEST_CLASS(ArcCraftStation_EdgeCaseTests, "ArcCraft.Station.EdgeCases")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
	}

	TEST_METHOD(NullRecipe_QueueFails)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = *FGuid::NewGuid().ToString();
		AArcCraftStationTestableActor& Actor =
			Spawner.SpawnActor<AArcCraftStationTestableActor>(SpawnParams);

		FActorSpawnParameters InstigatorParams;
		InstigatorParams.Name = *FGuid::NewGuid().ToString();
		AArcCraftStationTestInstigator& Ins =
			Spawner.SpawnActor<AArcCraftStationTestInstigator>(InstigatorParams);

		ASSERT_THAT(IsFalse(Actor.StationComponent->QueueRecipe(nullptr, &Ins),
			TEXT("Should fail with null recipe")));
	}

	TEST_METHOD(ZeroAmount_QueueFails)
	{
		TObjectPtr<UArcItemDefinition> EdgeDef =
			CraftStationTestHelpers::CreateTransientItemDef(TEXT("TestDef_Edge"));

		TObjectPtr<UArcRecipeDefinition> Recipe = CraftStationTestHelpers::CreateTransientRecipe(
			TEXT("TestRecipe_Edge"), {}, EdgeDef, 1.0f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = *FGuid::NewGuid().ToString();
		AArcCraftStationTestableActor& Actor =
			Spawner.SpawnActor<AArcCraftStationTestableActor>(SpawnParams);

		FGameplayTagContainer Tags;
		CraftStationTestHelpers::ConfigureStationStore(Actor.StationComponent, Tags);

		FActorSpawnParameters InstigatorParams;
		InstigatorParams.Name = *FGuid::NewGuid().ToString();
		AArcCraftStationTestInstigator& Ins =
			Spawner.SpawnActor<AArcCraftStationTestInstigator>(InstigatorParams);

		ASSERT_THAT(IsFalse(Actor.StationComponent->QueueRecipe(Recipe, &Ins, 0),
			TEXT("Should fail with zero amount")));
	}

	TEST_METHOD(CancelNonExistentEntry_ReturnsFalse)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = *FGuid::NewGuid().ToString();
		AArcCraftStationTestableActor& Actor =
			Spawner.SpawnActor<AArcCraftStationTestableActor>(SpawnParams);

		ASSERT_THAT(IsFalse(Actor.StationComponent->CancelQueueEntry(FGuid::NewGuid()),
			TEXT("Should fail for non-existent entry")));
	}

	TEST_METHOD(NoItemSourceConfigured_QueueFails)
	{
		TObjectPtr<UArcItemDefinition> NoSourceDef =
			CraftStationTestHelpers::CreateTransientItemDef(TEXT("TestDef_NoSource"));

		TObjectPtr<UArcRecipeDefinition> Recipe = CraftStationTestHelpers::CreateTransientRecipe(
			TEXT("TestRecipe_NoSource"), {}, NoSourceDef, 1.0f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = *FGuid::NewGuid().ToString();
		AArcCraftStationTestableActor& Actor =
			Spawner.SpawnActor<AArcCraftStationTestableActor>(SpawnParams);

		FActorSpawnParameters InstigatorParams;
		InstigatorParams.Name = *FGuid::NewGuid().ToString();
		AArcCraftStationTestInstigator& Ins =
			Spawner.SpawnActor<AArcCraftStationTestInstigator>(InstigatorParams);

		// No source configured — default TInstancedStruct is empty
		ASSERT_THAT(IsFalse(Actor.StationComponent->QueueRecipe(Recipe, &Ins),
			TEXT("Should fail without item source configured")));
	}

	TEST_METHOD(EmptyQueue_TryComplete_ReturnsFalse)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = *FGuid::NewGuid().ToString();
		AArcCraftStationTestableActor& Actor =
			Spawner.SpawnActor<AArcCraftStationTestableActor>(SpawnParams);

		FActorSpawnParameters InstigatorParams;
		InstigatorParams.Name = *FGuid::NewGuid().ToString();
		AArcCraftStationTestInstigator& Ins =
			Spawner.SpawnActor<AArcCraftStationTestInstigator>(InstigatorParams);

		ASSERT_THAT(IsFalse(Actor.StationComponent->TryCompleteOnInteraction(&Ins),
			TEXT("Should return false for empty queue")));
	}

	TEST_METHOD(RecipeWithNoRequiredStationTags_CanBeQueuedAnywhere)
	{
		TObjectPtr<UArcItemDefinition> NoTagDef =
			CraftStationTestHelpers::CreateTransientItemDef(TEXT("TestDef_NoStationTag"));

		TObjectPtr<UArcRecipeDefinition> Recipe = CraftStationTestHelpers::CreateTransientRecipe(
			TEXT("TestRecipe_NoStationTag"),
			{},
			NoTagDef,
			0.5f,
			FGameplayTagContainer()); // No station tag requirements

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = *FGuid::NewGuid().ToString();
		AArcCraftStationTestableActor& Actor =
			Spawner.SpawnActor<AArcCraftStationTestableActor>(SpawnParams);

		FGameplayTagContainer ForgeTags;
		ForgeTags.AddTag(TAG_StationTest_Station_Forge);
		CraftStationTestHelpers::ConfigureStationStore(Actor.StationComponent, ForgeTags);

		// Register transient definition for output
		Actor.StationComponent->RegisterTransientDefinition(NoTagDef);

		FActorSpawnParameters InstigatorParams;
		InstigatorParams.Name = *FGuid::NewGuid().ToString();
		AArcCraftStationTestInstigator& Ins =
			Spawner.SpawnActor<AArcCraftStationTestInstigator>(InstigatorParams);

		// Recipe has no required station tags and no ingredients — should succeed
		ASSERT_THAT(IsTrue(Actor.StationComponent->QueueRecipe(Recipe, &Ins),
			TEXT("Recipe with no station tag requirements should be queueable anywhere")));
	}
};
