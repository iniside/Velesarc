// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcQuickBarTestHelpers.h"

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

using namespace ArcQuickBarTestHelpers;

// ---------------------------------------------------------------------------
// Slot management: AddAndActivateQuickSlot, RemoveQuickSlot,
//                  HandleSlotActivated, HandleSlotDeactivated
// ---------------------------------------------------------------------------
TEST_CLASS(ArcQuickBar_SlotManagement, "ArcCore.QuickBar.Slot")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	// ----- AddAndActivateQuickSlot ------------------------------------------

	TEST_METHOD(AddAndActivate_AutoActivateOnly_SlotBecomesActive)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		ASSERT_THAT(IsTrue(ItemId.IsValid(), TEXT("Item should be created")));

		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);

		ASSERT_THAT(IsTrue(QB->IsItemOnQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Item should be assigned to slot")));
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot should be active in AutoActivateOnly mode")));
	}

	TEST_METHOD(AddAndActivate_Cyclable_AutoSelectSlot_Activates)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::Cyclable,
			{ MakeSlot(TAG_QBTest_Slot01, /*bAutoSelect=*/true) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);

		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("AutoSelect slot should activate on Cyclable bar")));
	}

	TEST_METHOD(AddAndActivate_Cyclable_NonAutoSelectSlot_DoesNotActivate)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::Cyclable,
			{ MakeSlot(TAG_QBTest_Slot01, /*bAutoSelect=*/false) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);

		ASSERT_THAT(IsTrue(QB->IsItemOnQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Item should be assigned to slot")));
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Non-AutoSelect slot should NOT activate on Cyclable bar")));
	}

	TEST_METHOD(AddAndActivate_MultipleSlots_AutoActivateOnly_AllActive)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01), MakeSlot(TAG_QBTest_Slot02) });

		FArcItemId Item1 = AddTestItem(Actor.ItemsStore);
		FArcItemId Item2 = AddTestItem(Actor.ItemsStore);

		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, Item1);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot02, Item2);

		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot01 should be active")));
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot02),
			TEXT("Slot02 should be active")));

		TArray<FGameplayTag> ActiveSlots = QB->GetActiveSlots(TAG_QBTest_Bar01);
		ASSERT_THAT(AreEqual(2, ActiveSlots.Num(), TEXT("Both slots should be active")));
	}

	TEST_METHOD(AddAndActivate_ItemsStoreIntegration_CanFindItem)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);

		// QuickBar can find the item data through the store
		const FArcItemData* ItemData = QB->FindQuickSlotItem(TAG_QBTest_Bar01, TAG_QBTest_Slot01);
		ASSERT_THAT(IsNotNull(ItemData, TEXT("FindQuickSlotItem should return valid data")));

		// ItemsStore still holds the item
		const FArcItemData* StoreItem = Actor.ItemsStore->GetItemPtr(ItemId);
		ASSERT_THAT(IsNotNull(StoreItem, TEXT("Item should still exist in store")));

		// GetItemStoreComponent resolves the correct store
		UArcItemsStoreComponent* Store = QB->GetItemStoreComponent(TAG_QBTest_Bar01);
		ASSERT_THAT(IsNotNull(Store, TEXT("GetItemStoreComponent should find the store")));
	}

	// ----- RemoveQuickSlot --------------------------------------------------

	TEST_METHOD(RemoveQuickSlot_DeactivatesAndRemovesItem)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01)));

		QB->RemoveQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01);

		ASSERT_THAT(IsFalse(QB->IsItemOnQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot should have no item after removal")));
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot should not be active after removal")));
	}

	TEST_METHOD(RemoveQuickSlot_ItemStillExistsInStore)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);
		QB->RemoveQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01);

		// Removing from QuickBar should NOT remove from the ItemsStore
		const FArcItemData* StoreItem = Actor.ItemsStore->GetItemPtr(ItemId);
		ASSERT_THAT(IsNotNull(StoreItem, TEXT("Item should still exist in store after QuickBar removal")));
	}

	// ----- HandleSlotActivated / HandleSlotDeactivated ----------------------

	TEST_METHOD(HandleSlotActivated_ManuallyActivatesInactiveSlot)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		// Cyclable + no autoSelect = item gets added but NOT activated
		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::Cyclable,
			{ MakeSlot(TAG_QBTest_Slot01, /*bAutoSelect=*/false) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Precondition: slot should not be active")));

		bool bResult = QB->HandleSlotActivated(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);

		ASSERT_THAT(IsTrue(bResult, TEXT("HandleSlotActivated should return true")));
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot should be active after HandleSlotActivated")));
	}

	TEST_METHOD(HandleSlotDeactivated_DeactivatesActiveSlot)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Precondition: slot should be active")));

		QB->HandleSlotDeactivated(TAG_QBTest_Bar01, TAG_QBTest_Slot01);

		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot should be inactive after HandleSlotDeactivated")));
		// Item is still assigned to the slot
		ASSERT_THAT(IsTrue(QB->IsItemOnQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Item should still be on slot after deactivation")));
	}

	TEST_METHOD(HandleSlotActivated_FailsWhenNoItemInStore)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::Cyclable,
			{ MakeSlot(TAG_QBTest_Slot01, false) });

		// Try to activate with an invalid item ID (nothing in store)
		bool bResult = QB->HandleSlotActivated(TAG_QBTest_Bar01, TAG_QBTest_Slot01, FArcItemId::InvalidId);

		ASSERT_THAT(IsFalse(bResult, TEXT("Activation should fail with no item in store")));
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot should not be active")));
	}

	TEST_METHOD(ActivateDeactivateRoundtrip_SlotReturnsToOriginalState)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::Cyclable,
			{ MakeSlot(TAG_QBTest_Slot01, false) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);

		// Activate
		QB->HandleSlotActivated(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01)));

		// Deactivate
		QB->HandleSlotDeactivated(TAG_QBTest_Bar01, TAG_QBTest_Slot01);
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01)));

		// Re-activate
		QB->HandleSlotActivated(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot should be re-activatable")));
	}
};
